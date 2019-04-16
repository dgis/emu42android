/*
 *   external.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gießelink
 *
 */
#include "pch.h"
#include "Emu42.h"
#include "ops.h"
#include "io.h"

#define CPU_FREQ			524288			// base CPU frequency
#define CPU_FREQNOM			(CPU_FREQ * 2)	// nominal CPU frequency

VOID External0(CHIPSET_M* w)				// =BPUTL patch for HP17BII and HP19BII
{
	enum MMUMAP e;

	DWORD dwF, dwD;
	DWORD freq,dur;

	dwD = Npack(w->C,3);					// get fdd control

	e = MapData(w->pc);						// MMU area of code
	if (M_ROM == e || M_ROMS == e)
		dwF = 79 + 54 * (dwD >> 8);			// F, internally (LEWIS ROM)
	else
		dwF = 107 + 66 * (dwD >> 8);		// F, MEMC (External ROM or RAM)
	dwD = dwF * (4096 - 16 * (dwD & 0xFF));	// D

	// CPU frequency
	freq = ((DWORD) w->IORam[RATECTL] + 1) * CPU_FREQ / 4;

	dur = (dwD * 1000) / freq;				// duration in ms
	freq = freq / (2 * dwF);				// frequency in Hz

	if (freq > 4400) freq = 4400;			// high limit of HP

	SoundBeep(freq,dur);					// beeping

	w->cycles += dur * 1000;				// estimate cpu cycles for beeping time (1MHz)

	// original routine return with...
	w->P = 0;								// P=0
	w->carry = FALSE;						// RTNCC
	w->pc = rstkpop();
	return;
}

VOID External1(CHIPSET_M* w)				// =makebeep patch
{
	DWORD freq,dur,cpufreq;

	freq = Npack(w->D,5);					// frequency in Hz
	dur = Npack(w->C,5);					// duration in ms

	// CPU freqency
	cpufreq = ((DWORD) w->IORam[RATECTL] + 1) * CPU_FREQ / 4;

	// frequency and duration RATE dependend
	freq = (DWORD) (((QWORD) freq * cpufreq) / CPU_FREQNOM);
	dur  = (DWORD) (((QWORD) dur * CPU_FREQNOM) / cpufreq);

	if (freq > 4400) freq = 4400;			// high limit of HP

	SoundBeep(freq,dur);					// beeping

	w->cycles += dur * 1000;				// estimate cpu cycles for beeping time (1MHz)

	// original routine return with...
	w->P = 0;								// P=0
	w->intk = TRUE;							// INTON
	w->carry = FALSE;						// RTNCC
	w->pc = rstkpop();
	return;
}
