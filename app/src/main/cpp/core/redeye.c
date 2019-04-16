/*
 *   redeye.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2011 Christoph Gießelink
 *
 */
#include "pch.h"
#include "Emu42.h"

// #define DEBUG_REDEYE_CYCLES				// switch for redeye cycles debug purpose
// #define DEBUG_REDEYE_DATA				// switch for redeye data   debug purpose

#define ERR_CHAR	127						// character for transfer error

#define H1  0x78
#define H2  0xE6
#define H3  0xD5
#define H4  0x8B

// HP redeye correction masks
static CONST BYTE byEmask[] = { H1, H2, H3, H4 };

static __inline UINT MAX(UINT a, UINT b)
{
	return (a>b)?a:b;
}

static __inline BYTE Parity(BYTE b)
{
	b ^= (b >> 4);
	b ^= (b >> 2);
	b ^= (b >> 1);
	return b & 1;
}

static __inline BYTE CreateCorrectionBits(BYTE b)
{
	UINT i;
	BYTE byVal = 0;

	for (i = 0; i < ARRAYSIZEOF(byEmask);++i)
	{
		byVal <<= 1;
		byVal |= Parity((BYTE) (b & byEmask[i]));
	}
	return byVal;
}

static __inline WORD CorrectData(WORD wData,WORD wMissed)
{
	while ((wMissed & 0xFF) != 0)			// clear every missed bit in data area
	{
		BYTE byBitMask;

		// detect valid H(i) mask
		WORD wMi = 0x800;					// first M(i) bit
		INT  i = 0;							// index to first H(i) mask

		while (TRUE)
		{
			if ((wMissed & wMi) == 0)		// possible valid mask
			{
				_ASSERT(i < ARRAYSIZEOF(byEmask));

				// select bit to correct
				byBitMask = wMissed & byEmask[i];

				if (Parity(byBitMask))		// only one bit set (parity odd)
					break;					// -> valid H(i) mask
			}

			wMi >>= 1;						// next M(i) bit
			i++;							// next H(i) mask
		}

		// correct bit with H(i) mask
		wMissed ^= byBitMask;				// clear this missed bit

		// parity odd -> wrong data value
		if (Parity((BYTE) ((wData & byEmask[i]) ^ ((wData & wMi) >> 8))))
			wData ^= byBitMask;				// correct value
	}
	return wData & 0xFF;					// only data byte is correct
}

VOID IrPrinter(BYTE c)
{
	static INT  nFrame = 0;					// frame counter

	static DWORD dwTiming[8];				// timing table

	static WORD wData;						// data container
	static WORD wMissed;					// missed bit container

	static DWORD dwHalfBitTime;				// current half bit time

	static DWORD dwStartCyc;				// cycle counter at start to detect overrrun
	static DWORD dwLastValidCyc;			// prior last timer value (fall back sync point)
	static DWORD dwLastCyc;					// last timer value at begin of last bit

	BOOL   bOverrun = FALSE;				// overrun flag

	DWORD dwCycles,dwDiffCycles;
	INT   i;

	if (c == 0) return;						// falling edge, ignore

	_ASSERT(nFrame >= 0);					// only positive framing values

	// actual timestamp
	dwCycles = (DWORD) (Chipset.cycles & 0xFFFFFFFF);

	dwDiffCycles = dwCycles - dwLastCyc;	// time difference from syncpoint

	#if defined DEBUG_REDEYE_CYCLES
	{
		TCHAR buffer[256];
		wsprintf(buffer,_T("Frame %d: Diff %u\n"),nFrame,dwDiffCycles);
		OutputDebugString(buffer);
	}
	#endif

	// rising edge
	switch (nFrame)
	{
	case 0: // init
		// workaraound for HP28S because inaccurate TIMER2 implementation
		// may cause a timer interrupt too early
		Chipset.inte = FALSE;				// disable interrupt

		dwStartCyc = dwLastValidCyc = dwLastCyc = dwCycles;
		break;
	case 1: // 2nd sync
		// CPU cycles at SPEED setting
		//   0    1  ...   4   5   6   7    8  ...  15	SPEED
		// 1836 1857     4015 321 370 425  481     840	Cycles
		if (dwDiffCycles > 1500)			// the difference is too big
		{
			nFrame = 0;						// reset, handle as 1st frame
			dwStartCyc = dwLastValidCyc = dwLastCyc = dwCycles;
		}
		break;
	case 2: // 3rd sync
		// get difference between (dwCycles - dwLastCyc) and (dwLastCyc - dwLastValidCyc)
		_ASSERT (dwLastCyc - dwLastValidCyc == dwDiffCycles);

		// median half bit time
		dwHalfBitTime = (dwCycles - dwLastValidCyc) / 2;

		#if 0
		{
			TCHAR buffer[256];
			wsprintf(buffer,"Halfbit Time = %u\n",dwHalfBitTime);
			OutputDebugString(buffer);
		}
		#endif

		// create timing table
		for (i = 0; i < ARRAYSIZEOF(dwTiming); ++i)
		{
			dwTiming[i] =  dwHalfBitTime * i;
			dwTiming[i] += dwHalfBitTime * MAX(4,i) / 8;
		}

		dwCycles = dwLastCyc;				// set syncpoint
		wData = wMissed = 0;				// preset data register
		break;
	default: // 12 data frames
		// overrun condition
		if ((dwCycles - dwStartCyc) > (30 * dwHalfBitTime))
		{
			_ASSERT(FALSE);					// should not happen with OS caller

			bOverrun = TRUE;				// overrun flag

			for (;nFrame <= 14; ++nFrame)	// frame not complete
			{
				wData <<= 1;				// add 0 to data (missed)
				wMissed = (wMissed << 1) | 1; // add 1 to missed bit
			}
			nFrame = 14;					// last frame
			break;
		}

		// get table entry form timing
		for (i = 0; i < ARRAYSIZEOF(dwTiming); ++i)
		{
			if ((INT) (dwTiming[i] - dwDiffCycles) >= 0)
				break;
		}

		switch (i)							// eval timing information
		{
		case 0: // too short
			return;							// exit without changing cycle references
		case 1: // 2nd burst in same bit
			_ASSERT(nFrame > 3);			// clear only data frames
			do
			{
				--nFrame;					// remove last frame

				wData >>= 1;				// remove last data bit
				wMissed >>= 1;				// remove corresponding missed bit
			}
			while ((wMissed & 0x1) != 0);	// remove all missed bits
			dwLastCyc = dwLastValidCyc;		// restore to last valid syncpoint
			return;							// exit without changing cycle references
		case 6: // XX1
			++nFrame;						// add 1 missed frame
			wData <<= 1;					// add 0 to data (missed)
			wMissed = (wMissed << 1) | 1;	// add 1 to missed bit
			// no break
		case 4: // X1
			++nFrame;						// add 1 missed frame
			wData <<= 1;					// add 0 to data (missed)
			wMissed = (wMissed << 1) | 1;	// add 1 to missed bit
			// no break
		case 2: // 1
			wData = (wData << 1) | 1;		// add 1 to data
			wMissed <<= 1;					// no missed bit
			break;
		case 7: // XX0
			++nFrame;						// add 1 missed frame
			wData <<= 1;					// add 0 to data (missed)
			wMissed = (wMissed << 1) | 1;	// add 1 to missed bit
			// no break
		case 5: // X0
			++nFrame;						// add 1 missed frame
			wData <<= 1;					// add 0 to data (missed)
			wMissed = (wMissed << 1) | 1;	// add 1 to missed bit
			// no break
		case 3: // 0
			wData <<= 1;					// add 0 to data
			wMissed <<= 1;					// no missed bit
			dwCycles -= dwHalfBitTime;
			break;
		default: // too many
			wMissed = 0xFFFF;				// fill missed bit counter
			break;
		}
		break;
	}

	if (nFrame == 14)						// last frame received, decode data
	{
		WORD w;
		INT  nCount;

		nFrame = 0;							// reset for next character

		#if defined DEBUG_REDEYE_DATA
		{
			TCHAR buffer[256];
			wsprintf(buffer,_T("Frame %d: Data = %03X Miss = %03X "),nFrame,wData,wMissed);
			OutputDebugString(buffer);
		}
		#endif

		// count number of missed bits
		for (nCount = 0, w = wMissed; w != 0; ++nCount)
			w &= (w - 1);

		if (nCount <= 2)					// error can be fixed
		{
			BYTE byOrgParity,byNewParity;

			byOrgParity = wData >> 8;		// the original parity information with missed bits
			byNewParity = ~(wMissed >> 8);	// missed bit mask for recalculated parity

			if (nCount > 0)					// error correction
			{
				wData = CorrectData(wData,wMissed);
			}

			wData &= 0xFF;					// remove parity information

			// recalculate parity data
			byNewParity &= CreateCorrectionBits((BYTE) wData);
	
			// wrong parity
			if (byOrgParity != byNewParity)
				wData = ERR_CHAR;			// character for transfer error
		}
		else
		{
			wData = ERR_CHAR;				// character for transfer error
		}
		
		#if defined DEBUG_REDEYE_DATA
		{
			TCHAR buffer[256];
			if (isprint(wData))
				wsprintf(buffer,_T("-> '%c'\n"),wData);
			else
				wsprintf(buffer,_T("-> %02X\n"),wData);
			OutputDebugString(buffer);
		}
		#endif

		SendByteUdp((BYTE) wData);			// send data byte

		if (bOverrun) IrPrinter(c);			// call for start frame
		return;
	}

	++nFrame;								// frame valid, next frame
	dwLastValidCyc = dwLastCyc;				// update sync points
	dwLastCyc = dwCycles;
	return;
}
