/*
 *   rpl.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gieﬂelink
 *
 */
#include "pch.h"
#include "Emu42.h"
#include "ops.h"
#include "rpl.h"

static DWORD RPL28_GarbageCol(VOID)			// RPL variables must be in system RAM
{
	CHIPSET_M OrgChipset;
	CHIPSET_S OrgChipsetS;
	DWORD     dwAVMEM;

	_ASSERT(cCurrentRomType == 'O');		// HP28S (Orlando)

	OrgChipset  = Chipset;					// save original chipset
	OrgChipsetS = ChipsetS;

	// entry for =GARBAGECOL
	Chipset.P = 0;							// P=0
	Chipset.mode_dec = FALSE;				// hex mode
	Chipset.pc = O_GARBAGECOL;				// =GARBAGECOL entry
	rstkpush(0xFFFFF);						// return address for stopping

	while (Chipset.pc != 0xFFFFF)			// wait for stop address
	{
		EvalOpcode(FASTPTR(Chipset.pc));	// execute opcode
	}

	dwAVMEM = Npack(Chipset.C,5);			// available AVMEM
	Chipset  = OrgChipset;					// restore original chipset
	ChipsetS = OrgChipsetS;
	return dwAVMEM;
}

static DWORD RPL28_SkipOb(DWORD d)
{
	BYTE X[8];
	DWORD n, l;

	_ASSERT(cCurrentRomType == 'O');		// HP28S (Orlando)

	Npeek(X,d,5);
	n = Npack(X, 5); // read prolog
	switch (n)
	{
	case O_DOBINT: l = 10; break; // System Binary
	case O_DOREAL: l = 21; break; // Real
	case O_DOEREL: l = 26; break; // Long Real
	case O_DOCMP:  l = 37; break; // Complex
	case O_DOECMP: l = 47; break; // Long Complex
	case O_DOCHAR: l =  7; break; // Character
	case O_DOROMP: l = 11; break; // XLIB Name
	case O_DOLIST: // List
	case O_DOSYMB: // Algebraic
	case O_DOCOL:  // Program
		n=d+5;
		do
		{
			d=n; n=RPL28_SkipOb(d);
		} while (d!=n);
		return n+5;
	case O_SEMI: return d; // ;
	case O_DOIDNT: // Global Name
	case O_DOLAM:  // Local Name
		Npeek(X,d+5,2); n = 7 + Npack(X,2)*2;
		return RPL28_SkipOb(d+n);
	case O_DORRP: // RAMROMPAIR
		d+=8;
		n = Read5(d);
		if (n==0)
		{
			return d+5;
		}
		else
		{
			d+=n;
			Npeek(X,d,2);
			n = Npack(X,2)*2 + 4;
			return RPL28_SkipOb(d+n);
		}
	case O_DOEXT:     // Reserved
	case O_DOARRY:    // Array
	case O_DOLNKARRY: // Linked Array
	case O_DOCSTR:    // String
	case O_DOHSTR:    // Binary Integer
	case O_DOCODE:    // Code
		l = 5+Read5(d+5);
		break;
	default: return d+5;
	}
	return d+l;
}

static DWORD RPL28_ObjectSize(BYTE *o,DWORD s)
{
	DWORD n, l = 0;

	_ASSERT(cCurrentRomType == 'O');		// HP28S (Orlando)

	if (s < 5) return BAD_OB;				// size too small for prolog
	n = Npack(o, 5);						// read prolog
	switch (n)
	{
	case O_DOBINT: l = 10; break; // System Binary
	case O_DOREAL: l = 21; break; // Real
	case O_DOEREL: l = 26; break; // Long Real
	case O_DOCMP:  l = 37; break; // Complex
	case O_DOECMP: l = 47; break; // Long Complex
	case O_DOCHAR: l =  7; break; // Character
	case O_DOROMP: l = 11; break; // XLIB Name
	case O_DOLIST: // List
	case O_DOSYMB: // Algebraic
	case O_DOCOL:  // Program
		n = 5;								// prolog length
		do
		{
			l += n;
			if (l > s) return BAD_OB;		// prevent negative size argument
			n = RPL28_ObjectSize(o+l,s-l);	// get new object
			if (n == BAD_OB) return BAD_OB;	// buffer overflow
		}
		while (n);
		l += 5;
		break;
	case O_SEMI: l = 0; break; // ;
	case O_DOIDNT: // Global Name
	case O_DOLAM:  // Local Name
		if (s < 5 + 2) return BAD_OB;
		l = 7 + Npack(o+5,2) * 2;			// prolog + name length
		if (l > s) return BAD_OB;			// prevent negative size argument
		n = RPL28_ObjectSize(o+l,s-l);		// get new object
		if (n == BAD_OB) return BAD_OB;		// buffer overflow
		l += n;
		break;
	case O_DORRP: // RAMROMPAIR
		if (s < 8 + 5) return BAD_OB;
		n = Npack(o+8,5);
		if (n == 0)							// empty RAMROMPAIR
		{
			l = 13;
		}
		else
		{
			l = 8 + n;
			if (s < l + 2) return BAD_OB;
			n = Npack(o+l,2) * 2 + 4;
			l += n;
			if (l > s) return BAD_OB;		// prevent negative size argument
			n = RPL28_ObjectSize(o+l,s-l);	// next rrp
			if (n == BAD_OB) return BAD_OB;	// buffer overflow
			l += n;
		}
		break;
	case O_DOEXT:     // Reserved
	case O_DOARRY:    // Array
	case O_DOLNKARRY: // Linked Array
	case O_DOCSTR:    // String
	case O_DOHSTR:    // Binary Integer
	case O_DOCODE:    // Code
		if (s < 5 + 5) return BAD_OB;
		l = 5 + Npack(o+5,5);
		break;
	default: l=5;
	}
	return (s >= l) ? l : BAD_OB;
}

static DWORD RPL42_SkipOb(DWORD d)
{
	BYTE X[8];
	DWORD n, l;

	_ASSERT(cCurrentRomType == 'D');		// HP42S (Davinci)

	Npeek(X,d,5);
	n = Npack(X, 5); // read prolog
	switch (n)
	{
	case D_DOBINT: l = 10; break; // System Binary
	case D_DOREAL: l = 21; break; // Real
	case D_DOEREL: l = 26; break; // Long Real
	case D_DOCMP:  l = 37; break; // Complex
	case D_DOECMP: l = 47; break; // Long Complex
	case D_DOCOL:  // Program
		n=d+5;
		do
		{
			d=n; n=RPL42_SkipOb(d);
		} while (d!=n);
		return n+5;
	case D_SEMI: return d; // ;
	case D_DOIDNT: // Global Name
		Npeek(X,d+5,2); n = 7 + Npack(X,2)*2;
		return RPL42_SkipOb(d+n);
	case D_DORRP: // RAMROMPAIR
		d+=8;
		n = Read5(d);
		if (n==0)
		{
			return d+5;
		}
		else
		{
			d+=n;
			Npeek(X,d,2);
			n = Npack(X,2)*2 + 4;
			return RPL42_SkipOb(d+n);
		}
	case D_DOARRY: // Array
	case D_DOHSTR: // Binary Integer
	case D_DOCODE: // Code
		l = 5+Read5(d+5);
		break;
	default: return d+5;
	}
	return d+l;
}

static DWORD RPL42_ObjectSize(BYTE *o,DWORD s)
{
	DWORD n, l = 0;

	_ASSERT(cCurrentRomType == 'D');		// HP42S (Davinci)

	if (s < 5) return BAD_OB;				// size too small for prolog
	n = Npack(o, 5);						// read prolog
	switch (n)
	{
	case D_DOBINT: l = 10; break; // System Binary
	case D_DOREAL: l = 21; break; // Real
	case D_DOEREL: l = 26; break; // Long Real
	case D_DOCMP:  l = 37; break; // Complex
	case D_DOECMP: l = 47; break; // Long Complex
	case D_DOCOL:  // Program
		n = 5;								// prolog length
		do
		{
			l += n;
			if (l > s) return BAD_OB;		// prevent negative size argument
			n = RPL42_ObjectSize(o+l,s-l);	// get new object
			if (n == BAD_OB) return BAD_OB;	// buffer overflow
		}
		while (n);
		l += 5;
		break;
	case D_SEMI: l = 0; break; // ;
	case D_DOIDNT: // Global Name
		if (s < 5 + 2) return BAD_OB;
		l = 7 + Npack(o+5,2) * 2;			// prolog + name length
		if (l > s) return BAD_OB;			// prevent negative size argument
		n = RPL42_ObjectSize(o+l,s-l);		// get new object
		if (n == BAD_OB) return BAD_OB;		// buffer overflow
		l += n;
		break;
	case D_DORRP: // RAMROMPAIR
		if (s < 8 + 5) return BAD_OB;
		n = Npack(o+8,5);
		if (n == 0)							// empty RAMROMPAIR
		{
			l = 13;
		}
		else
		{
			l = 8 + n;
			if (s < l + 2) return BAD_OB;
			n = Npack(o+l,2) * 2 + 4;
			l += n;
			if (l > s) return BAD_OB;		// prevent negative size argument
			n = RPL42_ObjectSize(o+l,s-l);	// next rrp
			if (n == BAD_OB) return BAD_OB;	// buffer overflow
			l += n;
		}
		break;
	case D_DOARRY: // Array
	case D_DOHSTR: // Binary Integer
	case D_DOCODE: // Code
		if (s < 5 + 5) return BAD_OB;
		l = 5 + Npack(o+5,5);
		break;
	default: l=5;
	}
	return (s >= l) ? l : BAD_OB;
}

DWORD RPL_SkipOb(DWORD d)
{
#if 0
	_ASSERT(   cCurrentRomType == 'D'		// HP42S (Davinci)
			|| cCurrentRomType == 'O');		// HP28S (Orlando)

	if (cCurrentRomType == 'D')				// HP42S (Davinci)
	{
		return RPL42_SkipOb(d);
	}
	else									// HP28S (Orlando)
#endif
	{
		return RPL28_SkipOb(d);
	}
}

DWORD RPL_ObjectSize(BYTE *o,DWORD s)
{
#if 0
	_ASSERT(   cCurrentRomType == 'D'		// HP42S (Davinci)
			|| cCurrentRomType == 'O');		// HP28S (Orlando)

	if (cCurrentRomType == 'D')				// HP42S (Davinci)
	{
		return RPL42_ObjectSize(o,s);
	}
	else									// HP28S (Orlando)
#endif
	{
		return RPL28_ObjectSize(o,s);
	}
}

DWORD RPL_CreateTemp(DWORD l)
{
	DWORD a, b, c;
	BYTE *p;

	_ASSERT(   cCurrentRomType == 'D'		// HP42S (Davinci)
			|| cCurrentRomType == 'O');		// HP28S (Orlando)

	l += 6;									// memory for link field (5) + marker (1) and end
	if (cCurrentRomType == 'D')				// HP42S (Davinci)
	{
		b = Read5(D_RSKTOP);				// tail address of rtn stack
		c = Read5(D_DSKTOP);				// top of data stack
		if ((b+l)>c) return 0;				// check if there's enough memory to move DSKTOP
		a = Read5(D_TEMPTOP);				// tail address of top object
		Write5(D_TEMPTOP, a+l);				// adjust new end of top object
		Write5(D_RSKTOP,  b+l);				// adjust new end of rtn stack
		Write5(D_AVMEM, (c-b-l)/5);			// calculate free memory (*5 nibbles)
	}
	else									// HP28S (Orlando)
	{
		b = Read5(O_RSKTOP);				// tail address of rtn stack
		c = Read5(O_DSKTOP);				// top of data stack
		if ((b+l)>c)						// there's not enough memory to move DSKTOP
		{
			RPL28_GarbageCol();				// do a garbage collection
			b = Read5(O_RSKTOP);			// reload tail address of rtn stack
			c = Read5(O_DSKTOP);			// reload top of data stack
		}
		if ((b+l)>c) return 0;				// check if there's now enough memory to move DSKTOP
		a = Read5(O_TEMPTOP);				// tail address of top object
		Write5(O_TEMPTOP, a+l);				// adjust new end of top object
		Write5(O_RSKTOP,  b+l);				// adjust new end of rtn stack
		Write5(O_AVMEM, (c-b-l)/5);			// calculate free memory (*5 nibbles)
	}
	p = (LPBYTE) malloc(b-a);				// move down rtn stack
	Npeek(p,a,b-a);
	Nwrite(p,a+l,b-a);
	free(p);
	Write5(a+l-5,l);						// set object length field
	return (a+1);							// return base address of new object
}

//
// HP28S stack manipulation routines
//

UINT RPL_Depth(VOID)
{
	return (Read5(O_EDITLINE) - Read5(O_DSKTOP)) / 5 - 1;
}

DWORD RPL_Pick(UINT l)
{
	DWORD stkp;

	_ASSERT(cCurrentRomType == 'O');		// HP28S (Orlando)

	_ASSERT(l > 0);							// first stack element is one
	if (l == 0) return 0;
	if (RPL_Depth() < l) return 0;			// not enough elements on stack
	stkp = Read5(O_DSKTOP) + (l-1)*5;
	return Read5(stkp);
}

VOID RPL_Replace(DWORD n)
{
	DWORD stkp;

	_ASSERT(cCurrentRomType == 'O');		// HP28S (Orlando)

	stkp = Read5(O_DSKTOP);
	Write5(stkp,n);
	return;
}

VOID RPL_Push(UINT l,DWORD n)
{
	UINT  i;
	DWORD stkp, avmem;

	_ASSERT(cCurrentRomType == 'O');		// HP28S (Orlando)

	if (l > RPL_Depth() + 1) return;		// invalid stack level

	avmem = Read5(O_AVMEM);					// amount of free memory
	if (avmem==0) return;					// no memory free
	avmem--;								// fetch memory
	Write5(O_AVMEM,avmem);					// save new amount of free memory

	stkp = Read5(O_DSKTOP) - 5;				// get pointer of new stack level 1
	Write5(O_DSKTOP,stkp);					// save it

	for (i = 1; i < l; ++i)					// move down stack level entries before insert pos
	{
		Write5(stkp,Read5(stkp+5));			// move down stack level entry
		stkp += 5;							// next stack entry
	}

	Write5(stkp,n);							// save pointer of new object on given stack level
	return;
}

//
// HP42S stack manipulation routines
//
VOID RPL_PushX(DWORD n)
{
	BYTE byFlag;

	_ASSERT(cCurrentRomType == 'D');		// HP42S (Davinci)

	// get flag with stack lift bit
	Npeek(&byFlag,D_CALCLFLAGS,sizeof(byFlag));
	if ((byFlag & D_sSTKLIFT) == 0)			// stack lift enabled
	{
		BYTE byStack[5*3];

		// do stack lift, x,y,z -> y,z,t
		Npeek(byStack,D_StackZ,sizeof(byStack));
		Nwrite(byStack,D_StackT,sizeof(byStack));
	}
	// overwrite x-register with object
	Write5(D_StackX,n);
	byFlag &= ~D_sSTKLIFT;					// enable stack lift
	Nwrite(&byFlag,D_CALCLFLAGS,sizeof(byFlag));
	return;
}
