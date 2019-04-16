/*
 *   timer.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gieﬂelink
 *
 */
#include "pch.h"
#include "Emu42.h"
#include "io.h"								// I/O definitions

#define AUTO_OFF	10						// Time in minutes for 'auto off'

// Ticks for 'auto off'
#define OFF_TIME	((LONGLONG) (AUTO_OFF * 60) << 13)

// memory address for clock and auto off

// HP17BII, HP42S = 0x50000-0x50019
#define PIONEER_TIME	0x50000				// clock (=NEXTIRQ)

// HP19BII = 0xC4806-C481D
#define TYCOON2_TIME	0xC4806				// clock (=NEXTIRQ)

// HP28S = 0xC0003-0xC000E, 0xC015A
#define ORLANDO_TIME	0xC0003				// clock (=NEXTIRQ)
#define ORLANDO_OFFCNT	0xC015A				// one minute off counter

#define T1_FREQ		62						// Timer1 1/frequency in ms
#define T2_FREQ		8192					// Timer2 frequency

// typecast define for easy access
#define IOReg(c,o) ((pIORam[c])[o])			// independent I/O access

static TIMECAPS tc;							// timer information
static UINT uT2MaxTicks = 0;				// max. timer2 ticks handled by one timer event

static BOOL bAccurateTimer = FALSE;			// flag if accurate timer is used

static BOOL  bStarted[]   = { FALSE, FALSE };
static BOOL  bOutRange[]  = { FALSE, FALSE }; // flag if timer value out of range
static UINT  uT1TimerId[] = { 0, 0 };
static UINT  uT2TimerId[] = { 0, 0 };

static LARGE_INTEGER lT2Ref[2];				// counter value at timer2 start
static DWORD dwT2Ref[2];					// timer2 value at last timer2 access
static DWORD dwT2Cyc[2];					// cpu cycle counter at last timer2 access

// tables for timer access
static BYTE   *pT1[]   = { &Chipset.t1,   &ChipsetS.t1 };
static DWORD  *pT2[]   = { &Chipset.t2,   &ChipsetS.t2 };
static LPBYTE pIORam[] = { Chipset.IORam, ChipsetS.IORam };

static void CALLBACK TimeProc(UINT uEventId, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

static DWORD CalcT2(enum CHIP eChip)		// calculate timer2 value
{
	DWORD dwT2 = *pT2[eChip];				// get value from chipset

	if (bStarted[eChip])					// timer2 running
	{
		LARGE_INTEGER lT2Act;
		DWORD         dwT2Dif;

		// timer should run a little bit faster (10%) than maschine in authentic speed mode
		DWORD dwCycPerTick = (9 * T2CYCLES) / 5;

		QueryPerformanceCounter(&lT2Act);	// actual time
		// calculate realtime timer2 ticks since reference point
		dwT2 -= (DWORD)
				(((lT2Act.QuadPart - lT2Ref[eChip].QuadPart) * T2_FREQ)
				/ lFreq.QuadPart);

		dwT2Dif = dwT2Ref[eChip] - dwT2;	// timer2 ticks since last request

		// checking if the MSB of dwT2Dif can be used as sign flag
		_ASSERT((DWORD) tc.wPeriodMax < ((1<<(sizeof(dwT2Dif)*8-1))/8192)*1000);

		// 2nd timer call in a 32ms time frame or elapsed time is negative (Win2k bug)
		if (!Chipset.Shutdn && ((dwT2Dif > 0x01 && dwT2Dif <= 0x100) || (dwT2Dif & 0x80000000) != 0))
		{
			DWORD dwT2Ticks = ((DWORD) (Chipset.cycles & 0xFFFFFFFF) - dwT2Cyc[eChip]) / dwCycPerTick;

			// estimated < real elapsed timer2 ticks or negative time
			if (dwT2Ticks < dwT2Dif || (dwT2Dif & 0x80000000) != 0)
			{
				// real time too long or got negative time elapsed
				dwT2 = dwT2Ref[eChip] - dwT2Ticks;	// estimated timer2 value from CPU cycles
				dwT2Cyc[eChip] += dwT2Ticks * dwCycPerTick; // estimated CPU cycles for the timer2 ticks
			}
			else
			{
				// reached actual time -> new synchronizing
				dwT2Cyc[eChip] = (DWORD) (Chipset.cycles & 0xFFFFFFFF) - dwCycPerTick;
			}
		}
		else
		{
			// valid actual time -> new synchronizing
			dwT2Cyc[eChip] = (DWORD) (Chipset.cycles & 0xFFFFFFFF) - dwCycPerTick;
		}

		// check if timer2 interrupt is active -> no timer2 value below 0xFFFFFFFF
		if (   Chipset.inte
			&& (dwT2 & 0x80000000) != 0
			&& (!Chipset.Shutdn || (IOReg(eChip,TIMERCTL_2)&WKE))
			&& (IOReg(eChip,TIMERCTL_2)&INTR)
		   )
		{
			dwT2 = 0xFFFFFFFF;
			dwT2Cyc[eChip] = (DWORD) (Chipset.cycles & 0xFFFFFFFF) - dwCycPerTick;
		}

		dwT2Ref[eChip] = dwT2;				// new reference time
	}
	return dwT2;
}

static VOID CheckT1(enum CHIP eChip, BYTE nT1)
{
	if ((nT1&8) == 0)						// timer1 MSB not set
	{
		IOReg(eChip,TIMERCTL_1) &= ~SRQ;	// clear SRQ bit
		return;
	}

	_ASSERT((nT1&8) != 0);					// timer1 MSB set

	// timer MSB and INT or WAKE bit is set
	if ((IOReg(eChip,TIMERCTL_1)&(WKE|INTR)) != 0)
		IOReg(eChip,TIMERCTL_1) |= SRQ;		// set SRQ
	// cpu not sleeping and T1 -> Interrupt
	if (   (!Chipset.Shutdn || (IOReg(eChip,TIMERCTL_1)&WKE))
		&& (IOReg(eChip,TIMERCTL_1)&INTR))
	{
		Chipset.SoftInt = TRUE;
		bInterrupt = TRUE;
	}
	// cpu sleeping and T1 -> Wake Up
	if (Chipset.Shutdn && (IOReg(eChip,TIMERCTL_1)&WKE))
	{
		IOReg(eChip,TIMERCTL_1) &= ~WKE;	// clear WKE bit
		Chipset.bShutdnWake = TRUE;			// wake up from SHUTDN mode
		SetEvent(hEventShutdn);				// wake up emulation thread
	}
	return;
}

static VOID CheckT2(enum CHIP eChip, DWORD dwT2)
{
	if ((dwT2&0x80000000) == 0)				// timer2 MSB not set
	{
		IOReg(eChip,TIMERCTL_2) &= ~SRQ;	// clear SRQ bit
		return;
	}

	_ASSERT((dwT2&0x80000000) != 0);		// timer2 MSB set

	// timer MSB and INT or WAKE bit is set
	if ((IOReg(eChip,TIMERCTL_2)&(WKE|INTR)) != 0)
		IOReg(eChip,TIMERCTL_2) |= SRQ;		// set SRQ
	// cpu not sleeping and T2 -> Interrupt
	if (   (!Chipset.Shutdn || (IOReg(eChip,TIMERCTL_2)&WKE))
		&& (IOReg(eChip,TIMERCTL_2)&INTR))
	{
		Chipset.SoftInt = TRUE;
		bInterrupt = TRUE;
	}
	// cpu sleeping and T2 -> Wake Up
	if (Chipset.Shutdn && (IOReg(eChip,TIMERCTL_2)&WKE))
	{
		IOReg(eChip,TIMERCTL_2) &= ~WKE;	// clear WKE bit
		Chipset.bShutdnWake = TRUE;			// wake up from SHUTDN mode
		SetEvent(hEventShutdn);				// wake up emulation thread
	}
	return;
}

static VOID RescheduleT2(enum CHIP eChip, BOOL bRefPoint)
{
	UINT uDelay;
	_ASSERT(uT2TimerId[eChip] == 0);		// timer2 must stopped
	if (bRefPoint)							// save reference time
	{
		dwT2Ref[eChip] = *pT2[eChip];		// timer2 value at last timer2 access
		dwT2Cyc[eChip] = (DWORD) (Chipset.cycles & 0xFFFFFFFF); // cpu cycle counter at last timer2 access
		QueryPerformanceCounter(&lT2Ref[eChip]);// time of corresponding Chipset.t2 value
		uDelay = *pT2[eChip];				// timer value for delay
	}
	else									// called without new refpoint, restart t2 with actual value
	{
		uDelay = CalcT2(eChip);				// actual timer value for delay
	}
	if ((bOutRange[eChip] = uDelay > uT2MaxTicks)) // delay greater maximum delay
		uDelay = uT2MaxTicks;				// wait maximum delay time
	uDelay = (uDelay * 125 + 1023) / 1024;	// timer delay in ms (1000/8192 = 125/1024)
	uDelay = __max(tc.wPeriodMin,uDelay);	// wait minimum delay of timer
	_ASSERT(uDelay <= tc.wPeriodMax);		// inside maximum event delay
	// start timer2; schedule event, when Chipset.t2 will be zero
	VERIFY(uT2TimerId[eChip] = timeSetEvent(uDelay,0,&TimeProc,2,TIME_ONESHOT));
	return;
}

static VOID AbortT2(enum CHIP eChip)
{
	_ASSERT(uT2TimerId[eChip]);
	timeKillEvent(uT2TimerId[eChip]);		// kill event
	uT2TimerId[eChip] = 0;					// then reset var
	return;
}

static void CALLBACK TimeProc(UINT uEventId, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	if (uEventId == 0) return;				// illegal EventId

	if (uEventId == uT1TimerId[MASTER])		// called from master timer1 event (default period 16 Hz)
	{
		EnterCriticalSection(&csT1Lock);
		{
			Chipset.t1 = (Chipset.t1-1)&0xF;// decrement timer value
			CheckT1(MASTER,Chipset.t1);		// test timer1 control bits
		}
		LeaveCriticalSection(&csT1Lock);
		return;
	}
	if (uEventId == uT1TimerId[SLAVE])		// called from slave timer1 event (default period 16 Hz)
	{
		EnterCriticalSection(&csT1Lock);
		{
			ChipsetS.t1 = (ChipsetS.t1-1)&0xF;// decrement timer value
			CheckT1(SLAVE,ChipsetS.t1);		// test timer1 control bits
		}
		LeaveCriticalSection(&csT1Lock);
		return;
	}
	if (uEventId == uT2TimerId[MASTER])		// called from master timer2 event, Chipset.t2 should be zero
	{
		EnterCriticalSection(&csT2Lock);
		{
			uT2TimerId[MASTER] = 0;			// single shot timer master timer2 stopped
			if (!bOutRange[MASTER])			// timer event elapsed
			{
				// timer2 overrun, test timer2 control bits else restart timer2
				Chipset.t2 = CalcT2(MASTER); // calculate new timer2 value
				CheckT2(MASTER,Chipset.t2);	// test master timer2 control bits
			}
			RescheduleT2(MASTER,!bOutRange[MASTER]); // restart master timer2
		}
		LeaveCriticalSection(&csT2Lock);
		return;
	}
	if (uEventId == uT2TimerId[SLAVE])		// called from slave timer2 event, Chipset.t2 should be zero
	{
		EnterCriticalSection(&csT2Lock);
		{
			uT2TimerId[SLAVE] = 0;			// single shot timer slave timer2 stopped
			if (!bOutRange[SLAVE])			// timer event elapsed
			{
				// timer2 overrun, test timer2 control bits else restart timer2
				ChipsetS.t2 = CalcT2(SLAVE); // calculate new timer2 value
				CheckT2(SLAVE,ChipsetS.t2);	// test slave timer2 control bits
			}
			RescheduleT2(SLAVE,!bOutRange[SLAVE]); // restart slave timer2
		}
		LeaveCriticalSection(&csT2Lock);
		return;
	}
	return;
	UNREFERENCED_PARAMETER(uMsg);
	UNREFERENCED_PARAMETER(dwUser);
	UNREFERENCED_PARAMETER(dw1);
	UNREFERENCED_PARAMETER(dw2);
}

VOID SetHPTime(VOID)						// set date and time
{
	SYSTEMTIME ts;
	LONGLONG   ticks, time;
	BYTE       byTicks[sizeof(ticks)*2];	// unpacked ticks buffer
	DWORD      dw;
	WORD       i;

	_ASSERT(sizeof(LONGLONG) == 8);			// check size of datatype

	GetLocalTime(&ts);						// local time, _ftime() cause memory/resource leaks

	// calculate days until 01.01.0000 (Erlang BIF localtime/0)
	dw = (DWORD) ts.wMonth;
	if (dw > 2)
		dw -= 3L;
	else
	{
		dw += 9L;
		--ts.wYear;
	}
	dw = (DWORD) ts.wDay + (153L * dw + 2L) / 5L;
	dw += (146097L * (((DWORD) ts.wYear) / 100L)) / 4L;
	dw +=   (1461L * (((DWORD) ts.wYear) % 100L)) / 4L;
	dw += (-719469L + 719528L);				// days from year 0

	ticks = (ULONGLONG) dw;					// convert to 64 bit

	// convert into seconds and add time
	ticks = ticks * 24L + (ULONGLONG) ts.wHour;
	ticks = ticks * 60L + (ULONGLONG) ts.wMinute;
	ticks = ticks * 60L + (ULONGLONG) ts.wSecond;

	// create timerticks = (s + ms) * 8192
	ticks = (ticks << 13) | (((ULONGLONG) ts.wMilliseconds << 10) / 125);

	ticks += (LONG) Chipset.t2;				// add actual timer2 value

	time = ticks;							// save for calc. timeout

	// unpack timer ticks
	for (i = 0; i < ARRAYSIZEOF(byTicks); ++i)
	{
		byTicks[i] = (BYTE) ticks & 0xf;
		ticks >>= 4;
	}

	if (Chipset.type == 'Y')				// Tycoon II, HP19BII
	{
		Nwrite(byTicks, TYCOON2_TIME, 13);	// write date and time

		// disable auto off (1 nib minute up counter + upper 10 nibs of 1min time
		ZeroMemory(byTicks,11);
		Nwrite(byTicks, TYCOON2_TIME+13, 11);
		return;
	}

	if (Chipset.type == 'O')				// Orlando, HP28S
	{
		Nwrite(byTicks, ORLANDO_TIME, 12);	// write date and time

		// HP address for timeout 1 minute counter
		byTicks[0] = AUTO_OFF;
		Nwrite(byTicks, ORLANDO_OFFCNT, 1);
		return;
	}

	// default section for Pioneer series
	Nwrite(byTicks, PIONEER_TIME, 13);		// write date and time

	time += OFF_TIME;						// add 10 min for auto off
	for (i = 0; i < 13; ++i)				// unpack timeout time
	{
		byTicks[i] = (BYTE) time & 0xf;
		time >>= 4;
	}

	// HP address for timeout (=TIMEOUT)
	Nwrite(byTicks, PIONEER_TIME+13, 13);	// write time for auto off
	return;
}

VOID StartTimers(enum CHIP eChip)
{
	// try to start slave timer in non existing slave controller or master timer stopped
	if (eChip == SLAVE && (!Chipset.bSlave || (Chipset.IORam[TIMERCTL_2]&RUN) == 0))
		return;

	if (IOReg(eChip,TIMERCTL_2)&RUN)		// start timer1 and timer2 ?
	{
		if (bStarted[eChip])				// timer running
			return;							// -> quit

		bStarted[eChip] = TRUE;				// flag timer running

		if (uT2MaxTicks == 0)				// init once
		{
			timeGetDevCaps(&tc,sizeof(tc));	// get timer resolution

			// max. timer2 ticks that can be handled by one timer event
			uT2MaxTicks = __min((0xFFFFFFFF / 1024),tc.wPeriodMax);
			uT2MaxTicks = __min((0xFFFFFFFF - 1023) / 125,uT2MaxTicks * 1024 / 125);
		}

		CheckT1(eChip,*pT1[eChip]);			// check for timer1 interrupts
		CheckT2(eChip,*pT2[eChip]);			// check for timer2 interrupts
		// set timer resolution to greatest possible one
		if (bAccurateTimer == FALSE)
			bAccurateTimer = (timeBeginPeriod(tc.wPeriodMin) == TIMERR_NOERROR);
		// set timer1 with given period
		VERIFY(uT1TimerId[eChip] = timeSetEvent(T1_FREQ,0,&TimeProc,1,TIME_PERIODIC));
		RescheduleT2(eChip,TRUE);			// start timer2
	}
	return;
}

VOID StopTimers(enum CHIP eChip)
{
	if (eChip == MASTER && bStarted[SLAVE])	// want to stop master timer but slave timer is running
		StopTimers(SLAVE);					// stop slave timer first

	if (uT1TimerId[eChip] != 0)				// timer1 running
	{
		// Critical Section handler may cause a dead lock
		timeKillEvent(uT1TimerId[eChip]);	// stop timer1
		uT1TimerId[eChip] = 0;				// set flag timer1 stopped
	}
	if (uT2TimerId[eChip] != 0)				// timer2 running
	{
		EnterCriticalSection(&csT2Lock);
		{
			*pT2[eChip] = CalcT2(eChip);	// update chipset timer2 value
		}
		LeaveCriticalSection(&csT2Lock);
		AbortT2(eChip);						// stop outside critical section
	}
	bStarted[eChip] = FALSE;

	// "Accurate timer" running and timer stopped
	if (bAccurateTimer && !bStarted[MASTER] && !bStarted[SLAVE])
	{
		timeEndPeriod(tc.wPeriodMin);		// finish service
		bAccurateTimer = FALSE;
	}
	return;
}

DWORD ReadT2(enum CHIP eChip)
{
	DWORD dwT2;
	EnterCriticalSection(&csT2Lock);
	{
		dwT2 = CalcT2(eChip);				// calculate timer2 value or if stopped last timer value
		CheckT2(eChip,dwT2);				// update timer2 control bits
	}
	LeaveCriticalSection(&csT2Lock);
	return dwT2;
}

VOID SetT2(enum CHIP eChip, DWORD dwValue)
{
	// calling AbortT2() inside Critical Section handler may cause a dead lock
	if (uT2TimerId[eChip] != 0)				// timer2 running
		AbortT2(eChip);						// stop timer2
	EnterCriticalSection(&csT2Lock);
	{
		*pT2[eChip] = dwValue;				// set new value
		CheckT2(eChip,*pT2[eChip]);			// test timer2 control bits
		if (bStarted[eChip])				// timer running
			RescheduleT2(eChip,TRUE);		// restart timer2
	}
	LeaveCriticalSection(&csT2Lock);
	return;
}

BYTE ReadT1(enum CHIP eChip)
{
	BYTE nT1;
	EnterCriticalSection(&csT1Lock);
	{
		nT1 = *pT1[eChip];					// read timer1 value
		CheckT1(eChip,nT1);					// update timer1 control bits
	}
	LeaveCriticalSection(&csT1Lock);
	return nT1;
}

VOID SetT1(enum CHIP eChip, BYTE byValue)
{
	BOOL bEqual;

	_ASSERT(byValue < 0x10);				// timer1 is only a 4bit counter

	EnterCriticalSection(&csT1Lock);
	{
		bEqual = (*pT1[eChip] == byValue);	// check for same value
	}
	LeaveCriticalSection(&csT1Lock);
	if (bEqual) return;						// same value doesn't restart timer period

	if (uT1TimerId[eChip] != 0)				// timer1 running
	{
		timeKillEvent(uT1TimerId[eChip]);	// stop timer1
		uT1TimerId[eChip] = 0;				// set flag timer1 stopped
	}
	EnterCriticalSection(&csT1Lock);
	{
		*pT1[eChip] = byValue;				// set new timer1 value
		CheckT1(eChip,*pT1[eChip]);			// test timer1 control bits
	}
	LeaveCriticalSection(&csT1Lock);
	if (bStarted[eChip])					// timer running
	{
		// restart timer1 to get full period of frequency
		VERIFY(uT1TimerId[eChip] = timeSetEvent(T1_FREQ,0,&TimeProc,1,TIME_PERIODIC));
	}
	return;
}
