/*
 *   dispnum.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2022 Christoph Gießelink
 *
 */
#include "pch.h"
#include "Emu42.h"
#include "ops.h"
#include "io.h"

// #define DEBUG_PATTERN					// switch for pattern output of segments

// translation table for Bert segment type
static CONST struct
{
	WORD  wImage;							// segment image
	TCHAR cChar;							// corresponding character
} sSegtabBert[] =
{
	0x0000, _T(' '),
	0x0001, _T('-'),
	0x8386, _T('0'),
	0x0300, _T('1'),
	0x8107, _T('2'),
	0x8305, _T('3'),
	0x0381, _T('4'),
	0x8285, _T('5'),
	0x8287, _T('6'),
	0x8300, _T('7'),
	0x8387, _T('8'),
	0x8385, _T('9'),
	0x8383, _T('A'),
	0x8086, _T('C'),
	0x8087, _T('E'),
	0x8083, _T('F'),
	0x0383, _T('H'),
	0x0086, _T('L'),
	0x8183, _T('P'),
	0x0385, _T('Y'),
	0x0287, _T('b'),
	0x0007, _T('c'),
	0x0307, _T('d'),
	0x0002, _T('i'),
	0x0203, _T('n'),
	0x0207, _T('o'),
	0x0003, _T('r'),
	0x0087, _T('t'),
	0x0206, _T('u'),
	0x0004, _T('_')
};

// translation table for Sacajawea segment type
static CONST struct
{
	QWORD qwImage;							// segment image
	TCHAR cChar;							// corresponding character
} sSegtabSaca[] =
{
	CLL(0x0000000000), _T(' '),
	CLL(0x000017C000), _T('!'),
	CLL(0x0232623262), _T('%'),
	CLL(0x000388A080), _T('('),
	CLL(0x00038FB180), _T('('),				// wide bracket variant
	CLL(0x0008288E00), _T(')'),
	CLL(0x000C6F8E00), _T(')'),				// wide bracket variant
	CLL(0x0222820A22), _T('*'),
	CLL(0x00810F8408), _T('+'),
	CLL(0x0081020408), _T('-'),
	CLL(0x0000408102), _T('-'),				// negative exponent
	CLL(0x00810A8408), _T('/'),				// divide
	CLL(0x0404040404), _T('/'),
	CLL(0x03EA3262BE), _T('0'),
	CLL(0x00085FE000), _T('1'),
	CLL(0x062A3264C6), _T('2'),
	CLL(0x02293264B6), _T('3'),
	CLL(0x018284BF90), _T('4'),
	CLL(0x0278B162B9), _T('5'),
	CLL(0x03C95264B0), _T('6'),
	CLL(0x001E224283), _T('7'),
	CLL(0x03693264B6), _T('8'),
	CLL(0x006932549E), _T('9'),
	CLL(0x000D9B0000), _T(':'),
	CLL(0x008288A080), _T('<'),
	CLL(0x006932549E), _T('<'),
	CLL(0x0142850A14), _T('='),
	CLL(0x0414450400), _T('>'),
	CLL(0x0020344486), _T('?'),
	CLL(0x07E12244FE), _T('A'),
	CLL(0x07F93264B6), _T('B'),
	CLL(0x03E83060A2), _T('C'),
	CLL(0x07F830511C), _T('D'),
	CLL(0x07F93264C1), _T('E'),
	CLL(0x03E54A9100), _T('E'),				// exponent
	CLL(0x07F1224481), _T('F'),
	CLL(0x03E83068F2), _T('G'),
	CLL(0x07F102047F), _T('H'),
	CLL(0x00083FE080), _T('I'),
	CLL(0x030810203F), _T('J'),
	CLL(0x07F1051141), _T('K'),
	CLL(0x07F8102040), _T('L'),
	CLL(0x07F043017F), _T('M'),
	CLL(0x07F082087F), _T('N'),
	CLL(0x03E83060BE), _T('O'),
	CLL(0x07F1224486), _T('P'),
	CLL(0x03E83450DE), _T('Q'),
	CLL(0x07F12654C6), _T('R'),
	CLL(0x02693264B2), _T('S'),
	CLL(0x00103FC081), _T('T'),
	CLL(0x03F810203F), _T('U'),
	CLL(0x0073180C07), _T('V'),
	CLL(0x07F406107F), _T('W'),
	CLL(0x0632820A63), _T('X'),
	CLL(0x00309E0203), _T('Y'),
	CLL(0x061A3262C3), _T('Z'),
	CLL(0x0040404104), _T('^'),
	CLL(0x0408102040), _T('_'),
	CLL(0x07F9122430), _T('b'),
	CLL(0x0388912244), _T('c'),
	CLL(0x038A952A18), _T('e'),
	CLL(0x00089F6000), _T('i'),
	CLL(0x07C0860278), _T('m'),
	CLL(0x07C0810278), _T('n'),
	CLL(0x07C1010204), _T('r'),
	CLL(0x048A952A24), _T('s'),
	CLL(0x03880C2038), _T('w'),
	CLL(0x04850C2848), _T('x'),
	CLL(0x048A080808), _T('y'),
	CLL(0x0000E14380), _T('°'),
	CLL(0x0192A48000), _T('²')
};

// translation table for Pioneer/Clamshell graphic image type
static CONST struct
{
	QWORD qwImage;							// segment image
	TCHAR cChar;							// corresponding character
} sGraphictabLewis[] =
{
	CLL(0x000000000000), _T(' '),
	CLL(0x0000F5000000), _T('!'),
	CLL(0x007000700000), _T('\"'),
	CLL(0x41F741F74100), _T('#'),
	CLL(0x42A2F7A22100), _T('$'),
	CLL(0x323180462600), _T('%'),
	CLL(0x639465020500), _T('&'),
	CLL(0x000070000000), _T('\''),
	CLL(0x00C122140000), _T('('),
	CLL(0x001422C10000), _T(')'),
	CLL(0x80A2C1A28000), _T('*'),
	CLL(0x8080E3808000), _T('+'),
	CLL(0x000B07000000), _T(','),
	CLL(0x808080800000), _T('-'),
	CLL(0x000606000000), _T('.'),
	CLL(0x020180402000), _T('/'),
	CLL(0xE3159454E300), _T('0'),
	CLL(0x0024F7040000), _T('1'),
	CLL(0x261594946400), _T('2'),
	CLL(0x229494946300), _T('3'),
	CLL(0x814121F70100), _T('4'),
	CLL(0x725454549300), _T('5'),
	CLL(0xC3A494940300), _T('6'),
	CLL(0x101790503000), _T('7'),
	CLL(0x639494946300), _T('8'),
	CLL(0x60949492E100), _T('9'),
	CLL(0x006363000000), _T(':'),
	CLL(0x820000000000), _T(':'),			// in stack
	CLL(0x006B67000000), _T(';'),
	CLL(0x804122140000), _T('<'),
	CLL(0x414141414100), _T('='),
	CLL(0x142241800000), _T('>'),
	CLL(0x201015906000), _T('?'),
	CLL(0xE314D555E500), _T('@'),
	CLL(0xE314D594E400), _T('@'),			// alternate
	CLL(0xE7909090E700), _T('A'),
	CLL(0xF79494946300), _T('B'),
	CLL(0xE31414142200), _T('C'),
	CLL(0xF7141422C100), _T('D'),
	CLL(0xF79494941400), _T('E'),
	CLL(0xE3A2A2220000), _T('E'),			// exponent
	CLL(0xF79090901000), _T('F'),
	CLL(0xE31414152700), _T('G'),
	CLL(0xF7808080F700), _T('H'),
	CLL(0x0014F7140000), _T('I'),
	CLL(0x03040404F300), _T('J'),
	CLL(0xF78041221400), _T('K'),
	CLL(0xF70404040400), _T('L'),
	CLL(0xF720C020F700), _T('M'),
	CLL(0xF7408001F700), _T('N'),
	CLL(0xE3141414E300), _T('O'),
	CLL(0xF79090906000), _T('P'),
	CLL(0xE3141512E500), _T('Q'),
	CLL(0xF79091926400), _T('R'),
	CLL(0x629494942300), _T('S'),
	CLL(0x1010F7101000), _T('T'),
	CLL(0xF3040404F300), _T('U'),
	CLL(0x708106817000), _T('V'),
	CLL(0xF7028102F700), _T('W'),
	CLL(0x364180413600), _T('X'),
	CLL(0x304087403000), _T('Y'),
	CLL(0x161594543400), _T('Z'),
	CLL(0x00F714140000), _T('['),
	CLL(0x204080010200), _T('\\'),
	CLL(0x001414F70000), _T(']'),
	CLL(0x402010204000), _T('^'),
	CLL(0x080808080800), _T('_'),
	CLL(0x003040000000), _T('`'),
	CLL(0x024545458700), _T('a'),
	CLL(0xF74444448300), _T('b'),
	CLL(0x834444444400), _T('c'),
	CLL(0x83444444F700), _T('d'),
	CLL(0x834545458500), _T('e'),
	CLL(0x0080E7902000), _T('f'),
	CLL(0x80E790200000), _T('f'),			// alternate
	CLL(0x814A4A4A8700), _T('g'),
	CLL(0xF74040408700), _T('h'),
	CLL(0x0044D7040000), _T('i'),
	CLL(0x00040848D700), _T('j'),
	CLL(0x040848D70000), _T('j'),			// alternate
	CLL(0xF70182440000), _T('k'),
	CLL(0x0014F7040000), _T('l'),
	CLL(0xC7408340C700), _T('m'),
	CLL(0xC74040408700), _T('n'),
	CLL(0x834444448300), _T('o'),
	CLL(0xCF4242428100), _T('p'),
	CLL(0x81424242CF00), _T('q'),
	CLL(0xC78040404000), _T('r'),
	CLL(0x844545454200), _T('s'),
	CLL(0x0040F3440200), _T('t'),
	CLL(0x40F344020000), _T('t'),			// alternate
	CLL(0xC3040404C700), _T('u'),
	CLL(0xC1020402C100), _T('v'),
	CLL(0xC3040304C300), _T('w'),
	CLL(0x448201824400), _T('x'),
	CLL(0xC10A0A0AC700), _T('y'),
	CLL(0x408007804000), _T('y'),			// in stack
	CLL(0x444645C44400), _T('z'),
	CLL(0x806314140000), _T('{'),
	CLL(0x0000F7000000), _T('|'),
	CLL(0x001414638000), _T('}'),
	CLL(0x804080018000), _T('~'),
	CLL(0x84E794142000), _T('£'),
	CLL(0x8041A2412200), _T('«'),
	CLL(0x007050700000), _T('°'),
	CLL(0x00E0A0E00000), _T('°'),			// alternate
	CLL(0xCF0404C30400), _T('µ'),
	CLL(0x2241A2418000), _T('»'),
	CLL(0x038454040200), _T('¿'),
	CLL(0x876151619700), _T('Ã'),
	CLL(0xC7312131C700), _T('Ä'),
	CLL(0x876151618700), _T('Å'),
	CLL(0xE790F7941400), _T('Æ'),
	CLL(0xE11216122100), _T('Ç'),
	CLL(0xC7A01122D700), _T('Ñ'),
	CLL(0xC3342434C300), _T('Ö'),
	CLL(0x224180412200), _T('×'),
	CLL(0xE2159454A300), _T('Ø'),
	CLL(0xC3140414C300), _T('Ü'),
	CLL(0x8080A2808000), _T('÷')
};


//################
//#
//#    Pioneer
//#
//################

//
// interprete characters of Bert display
//
static VOID GetLcdNumberBert(LPTSTR szContent)
{
	INT i = 0;

	_ASSERT(nCurrentHardware == HDW_BERT);

	// display on?
	if ((Chipset.b.IORam[BCONTRAST] & DON) != 0)
	{
		INT nAddr;

		// get sign
		if ((Chipset.b.DspRam[0] & 0x4)) szContent[i++] = _T('-');

		// scan complete display area
		for (nAddr = 0x02; nAddr < 0x32; nAddr += 0x04)
		{
			INT j;

			CONST WORD wSeg = (WORD) Npack(&Chipset.b.DspRam[nAddr],4);

			// search for character image in table
			CONST WORD wCharSeg = wSeg & 0x8387;
			for (szContent[i] = _T('·'), j = 0; j < ARRAYSIZEOF(sSegtabBert); ++j)
			{
				if (sSegtabBert[j].wImage == wCharSeg) // found known image
				{
					szContent[i] = sSegtabBert[j].cChar;
					break;
				}
			}
			++i;							// next character

			#if defined DEBUG_PATTERN
			if (j == ARRAYSIZEOF(sSegtabBert))
			{
				TCHAR buffer[256];
				_sntprintf(buffer,ARRAYSIZEOF(buffer),_T("Segment Pattern = 0x%04X\n"),wSeg & 0x8387);
				OutputDebugString(buffer);
			}
			#endif

			switch (wSeg & 0x0C00)				// dp, cm decoding
			{
			case 0x0400: szContent[i++] = _T('.'); break;
			case 0x0C00: szContent[i++] = _T(','); break;
			}
		}
	}
	szContent[i] = 0;						// set EOS
	return;
}

//
// interprete characters of Sacajawea display
//
static VOID GetLcdNumberSaca(LPTSTR szContent)
{
	INT i = 0;

	_ASSERT(nCurrentHardware == HDW_SACA);

	// display on?
	if ((Chipset.IORam[DSPCTL] & DON) != 0)
	{
		INT nAddr;

		// get sign
		if ((Chipset.IORam[1] & 0x8)) szContent[i++] = _T('-');

		// scan complete display area
		for (nAddr = PDISP_BEGIN; nAddr <= PDISP_END;)
		{
			INT j;

			// smart copy of dp and cm settings
			CONST BYTE byPoint = Chipset.IORam[nAddr+9];
			CONST BYTE byComma = Chipset.IORam[nAddr+7];

			// build character image (5 column)
			QWORD qwSeg = 0;
			for (j = 5; --j >= 0;)
			{
				qwSeg <<= 7;
				qwSeg |= Chipset.IORam[nAddr++];
				qwSeg |= ((Chipset.IORam[nAddr++] & 0x7) << 4);
			}

			// search for character image in table
			for (szContent[i] = _T('·'), j = 0; j < ARRAYSIZEOF(sSegtabSaca); ++j)
			{
				if (sSegtabSaca[j].qwImage == qwSeg) // found known image
				{
					szContent[i] = sSegtabSaca[j].cChar;
					break;
				}
			}
			++i;							// next character

			#if defined DEBUG_PATTERN
			if (j == ARRAYSIZEOF(sSegtabSaca))
			{
				TCHAR buffer[256];
				_sntprintf(buffer,ARRAYSIZEOF(buffer),_T("Segment Pattern = 0x%010I64X\n"),qwSeg);
				OutputDebugString(buffer);
			}
			#endif

			// decode dp and cm
			if ((byPoint & 0x8))			// dp set
			{
				szContent[i++] = (byComma & 0x8) ? _T(',') : _T('.');
			}
		}
	}
	szContent[i] = 0;						// set EOS
	return;
}

//
// decode character of Lewis display
//
static TCHAR DecodeCharLewis(QWORD qwImage)
{
	UINT i;

	// search for character image in table
	for (i = 0; i < ARRAYSIZEOF(sGraphictabLewis); ++i)
	{
		// found known image
		if (sGraphictabLewis[i].qwImage == qwImage)
		{
			return sGraphictabLewis[i].cChar;
		}
	}

	#if defined DEBUG_PATTERN
	{
		TCHAR buffer[256];
		_sntprintf(buffer,ARRAYSIZEOF(buffer),_T("Segment Pattern = 0x%012I64X\n"),qwImage);
		OutputDebugString(buffer);
	}
	#endif
	return '·';								// unknown character
}

//
// interprete one character of Lewis Pioneer display
//
static __inline TCHAR GetCharPioneer(INT nXPos, INT nYPos)
{
	QWORD qwImage;
	DWORD dwAddr;
	INT   i;

	// build display memory offset
	nXPos *= 6;								// 6 pixel width
	dwAddr = nXPos % 66;
	dwAddr <<= 1;
	dwAddr |= (nXPos / 66);
	dwAddr <<= 2;
	dwAddr |= (nYPos * 2);
	_ASSERT(dwAddr >= DISP_BEGIN && dwAddr <= DISP_END);

	// build character image
	qwImage = 0;
	for (i = 0; i < 6; ++i)					// pixel width
	{
		if (dwAddr <= DISP_END)				// visible area
		{
			qwImage <<= 4;
			qwImage |= Chipset.IORam[dwAddr];
			qwImage <<= 4;
			qwImage |= Chipset.IORam[dwAddr+1];
		}
		else								// hidden, fill with 0
		{
			qwImage <<= 8;
		}
		dwAddr += (1 << 3);					// next XPos;
	}
	return DecodeCharLewis(qwImage);
}

//
// interprete characters of Lewis Pioneer display
//
static VOID GetLcdNumberPioneer(LPTSTR szContent)
{
	INT i,x,y;

	i = 0;
	for (y = 0; y < 2; ++y)					// row
	{
		for (x = 0; x < 22; ++x)			// column
		{
			szContent[i++] = GetCharPioneer(x,y);
		}
		szContent[i++] = _T('\r');
		szContent[i++] = _T('\n');
	}
	szContent[i] = 0;						// EOS
	return;
}


//################
//#
//#    Clamshell
//#
//################

//
// interprete one character of Lewis Clamshell display
//
static __inline TCHAR GetCharClamshell(INT nXPos, INT nYPos)
{
	QWORD qwImage;
	DWORD dwAddr;
	INT   i;

	nXPos *= 6;								// 6 pixel width

	// build character image
	qwImage = 0;
	for (i = 0; i < 6; ++i)					// pixel width
	{
		dwAddr = (nXPos+i) << 3;

		if (dwAddr >= SDISP_END-SDISP_BEGIN+1)	// master
		{
			dwAddr -= (SDISP_END-SDISP_BEGIN+1);
			dwAddr |= (nYPos * 2);

			if (dwAddr <= MDISP_END)		// visible area
			{
				_ASSERT(dwAddr >= MDISP_BEGIN && dwAddr <= MDISP_END);
				qwImage <<= 4;
				qwImage |= Chipset.IORam[dwAddr];
				qwImage <<= 4;
				qwImage |= Chipset.IORam[dwAddr+1];
			}
			else							// hidden, fill with 0
			{
				qwImage <<= 8;
			}
		}
		else								// slave
		{
			dwAddr += SDISP_BEGIN;
			dwAddr |= (nYPos * 2);
			_ASSERT(dwAddr >= SDISP_BEGIN && dwAddr <= SDISP_END);

			qwImage <<= 4;
			qwImage |= ChipsetS.IORam[dwAddr];
			qwImage <<= 4;
			qwImage |= ChipsetS.IORam[dwAddr+1];
		}
	}
	return DecodeCharLewis(qwImage);
}

//
// interprete characters of Lewis Clamshell display
//
static VOID GetLcdNumberClamshell(LPTSTR szContent)
{
	INT i,x,y;

	i = 0;
	for (y = 0; y < 4; ++y)					// row
	{
		for (x = 0; x < 23; ++x)			// column
		{
			szContent[i++] = GetCharClamshell(x,y);
		}
		szContent[i++] = _T('\r');
		szContent[i++] = _T('\n');
	}
	szContent[i] = 0;						// EOS
	return;
}


//################
//#
//#    Public
//#
//################

//
// get text display string
//
VOID GetLcdNumber(LPTSTR szContent)
{
	switch (nCurrentHardware)
	{
	case HDW_BERT:
		GetLcdNumberBert(szContent);
		break;
	case HDW_SACA:
		GetLcdNumberSaca(szContent);
		break;
	case HDW_LEWIS:
		if (Chipset.bSlave)
			GetLcdNumberClamshell(szContent);
		else
			GetLcdNumberPioneer(szContent);
		break;
	default:
		*szContent = 0;
	}
	return;
}
