/*
 *   display.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gießelink
 *
 */
#include "pch.h"
#include "resource.h"
#include "Emu42.h"
#include "ops.h"
#include "io.h"
#include "kml.h"

#define DISPLAY_FREQ	16					// display update 1/frequency (1/64) in ms

#define B 0x00000000						// black
#define W 0x00FFFFFF						// white
#define I 0xFFFFFFFF						// ignore

// Sacajawea
#define PIX_PER_COL		7					// pixel per column
#define COL_PER_CHAR	5					// column per character

// Lewis
#define LCD_ROW			(35*4)				// max. pixel per line (131 or 137)

#define ROP_PSDPxax 0x00B8074A				// ternary ROP
#define ROP_PDSPxax 0x00D80745

// convert color from RGBQUAD to COLORREF
#define RGBtoCOLORREF(c) ((((c) & 0xFF0000) >> 16) \
						| (((c) & 0xFF)     << 16) \
						|  ((c) & 0xFF00))

UINT nBackgroundX = 0;
UINT nBackgroundY = 0;
UINT nBackgroundW = 0;
UINT nBackgroundH = 0;
UINT nLcdX = 0;
UINT nLcdY = 0;

UINT nLcdZoom = 1;							// display for Lewis
UINT nLcdyZoom = (UINT) -1;					// not initialized

UINT nLcdXOff = 0;							// display for Sacajawea
UINT nLcdYOff = 0;
UINT nLcdXPixel = 1;
UINT nLcdYPixel = 1;
UINT nLcdDistance = 0;

HDC  hLcdDC = NULL;
HDC  hMainDC = NULL;
HDC  hAnnunDC = NULL;						// annunciator DC

static HBITMAP hLcdBitmap;
static HBITMAP hMainBitmap;
static HBITMAP hAnnunBitmap;

static HDC     hMaskDC;						// mask bitmap
static HBITMAP hMaskBitmap;
static LPBYTE  pbyMask;

static HDC     hBmpBkDC;					// current background
static HBITMAP hBmpBk;

static HBRUSH  hBrush = NULL;				// old drawing brush

static UINT nLcdXSize = 0;					// display size
static UINT nLcdYSize = 0;

static UINT nLcdxZoom = (UINT) -1;			// x zoom factor

static UINT uLcdTimerId = 0;

static DWORD dwKMLColor[64] =				// color table loaded by KML script
{
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,
	I,I,I,I,I,I,I,I,I,I,I,I,I,I,I,I
};

static struct
{
	BITMAPINFOHEADER Lcd_bmih;
	RGBQUAD bmiColors[2];
} bmiLcd =
{
	{ sizeof(BITMAPINFOHEADER),0/*x*/,0/*y*/,1,8,BI_RGB,0,0,0,ARRAYSIZEOF(bmiLcd.bmiColors),0 },
	{ {0xFF,0xFF,0xFF,0x00},{0x00,0x00,0x00,0x00} }
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

VOID (*UpdateContrast)(VOID) = NULL;

static VOID UpdateContrastSaca(VOID);
static VOID UpdateContrastLewis(VOID);

static VOID (*WritePixel)(BYTE *p, BYTE a) = NULL;
static VOID (*UpdateDisplayMem)(VOID) = NULL;

static VOID WritePixelZoom4(BYTE *p, BYTE a);
static VOID WritePixelZoom3(BYTE *p, BYTE a);
static VOID WritePixelZoom2(BYTE *p, BYTE a);
static VOID WritePixelZoom1(BYTE *p, BYTE a);
static VOID WritePixelBYTE(BYTE *p, BYTE a);
static VOID WritePixelWORD(BYTE *p, BYTE a);
static VOID WritePixelDWORD(BYTE *p, BYTE a);

static VOID UpdateDisplayClamshell(VOID);
static VOID UpdateDisplayPioneer(VOID);
static VOID UpdateDisplaySacajawea(VOID);

//
// calculate size of Sacajawea LCD bitmap from KML script settings
//
static __inline VOID GetDimLcdBitmap(VOID)
{
	UINT nX,nY;
	INT  i;

	// get dimension of dot matrix part
	nLcdXSize = nLcdXOff + (nLcdXPixel * COL_PER_CHAR) + (12-1) * nLcdDistance;
	nLcdYSize = nLcdYOff + (nLcdYPixel * PIX_PER_COL);

	// scan all annunciators
	for (i = 0; i < ARRAYSIZEOF(pAnnunciator); ++i)
	{
		nX = pAnnunciator[i].nOx + pAnnunciator[i].nCx;
		if (nX > nLcdXSize) nLcdXSize = nX;	// new X-max value?

		nY = pAnnunciator[i].nOy + pAnnunciator[i].nCy;
		if (nY > nLcdYSize) nLcdYSize = nY;	// new Y-max value?
	}
	return;
}

//
// create Sacajawea background bitmap with segments
//
static VOID SetBkBitmap(VOID)
{
	// current annunciator DC
	HDC hSelAnnunDC = hAnnunDC != NULL ? hAnnunDC : hMainDC;

	_ASSERT(hMainDC);						// check for background
	_ASSERT(hBmpBkDC);						// check for LCD background
	_ASSERT(hMaskDC);						// check for dot matrix symbols
	_ASSERT(hSelAnnunDC);					// check for annunciator symbols

	EnterCriticalSection(&csGDILock);
	{
		// get original background from bitmap
		BitBlt(hBmpBkDC, 0, 0, nLcdXSize, nLcdYSize, hMainDC, nLcdX, nLcdY, SRCCOPY);

		// display on and background contrast defined ?
		if ((Chipset.IORam[DSPCTL]&DON) && dwKMLColor[Chipset.IORam[CONTRAST]+16] != I)
		{
			DWORD d;

			HBRUSH hBrush = CreateSolidBrush(RGBtoCOLORREF(dwKMLColor[Chipset.IORam[CONTRAST]+16]));
			hBrush = (HBRUSH) SelectObject(hBmpBkDC,hBrush);

			// scan complete display area
			for (d = PDISP_BEGIN; d <= PDISP_END; d += 2)
			{
				INT nByteNo = d >> 1;

				// column pixel
				BitBlt(hBmpBkDC,
					   (((nByteNo) / COL_PER_CHAR) * nLcdDistance + ((nByteNo) % COL_PER_CHAR) * nLcdXPixel + nLcdXOff),
					   nLcdYOff,
					   nLcdXSize, PIX_PER_COL * nLcdYPixel,
					   hMaskDC,
					   0x7F * nLcdXPixel, 0,
					   ROP_PSDPxax);

				// annunciators
				if (pAnnunciator[nByteNo].nCx > 0)
				{
					BitBlt(hBmpBkDC,
						   pAnnunciator[nByteNo].nOx, pAnnunciator[nByteNo].nOy,
						   pAnnunciator[nByteNo].nCx, pAnnunciator[nByteNo].nCy,
						   hSelAnnunDC,
						   pAnnunciator[nByteNo].nDx, pAnnunciator[nByteNo].nDy,
						   ROP_PSDPxax);
				}
			}

			DeleteObject(SelectObject(hBmpBkDC,hBrush));
		}
		GdiFlush();
	}
	LeaveCriticalSection(&csGDILock);
	return;
}

static VOID UpdateContrastSaca(VOID)
{
	BYTE byContrast;

	_ASSERT(nCurrentHardware == HDW_SACA);

	byContrast = Chipset.IORam[CONTRAST];

	_ASSERT(hLcdDC != NULL);
	EnterCriticalSection(&csGDILock);		// asynchronous display update needs brush
	{
		if (hBrush)							// has already a brush
		{
			// delete it first
			DeleteObject(SelectObject(hLcdDC,hBrush));
			hBrush = NULL;
		}

		if (dwKMLColor[byContrast] != I)	// have brush color?
		{
			// set brush for display pattern
			VERIFY(hBrush = (HBRUSH) SelectObject(hLcdDC,CreateSolidBrush(RGBtoCOLORREF(dwKMLColor[byContrast]))));
		}

		SetBkBitmap();						// update background bitmap
	}
	LeaveCriticalSection(&csGDILock);
	return;
}

static VOID UpdateContrastLewis(VOID)
{
	BYTE byContrast;

	BOOL bOn = (Chipset.IORam[DSPCTL] & DON) != 0;

	_ASSERT(nCurrentHardware == HDW_LEWIS);

	// when display is on fetch contrast value
	byContrast = (bOn)
			   ? ((Chipset.IORam[DSPCTL] & CONT4) << 3) | Chipset.IORam[CONTRAST]
			   : 0;

	// update palette information
	EnterCriticalSection(&csGDILock);		// asynchronous display update!
	{
		// display on and background contrast defined?
		if (bOn && dwKMLColor[byContrast+32] != I)
		{
			DWORD dwColor = RGBtoCOLORREF(dwKMLColor[byContrast+32]);

			HBRUSH hBrush = (HBRUSH) SelectObject(hBmpBkDC,CreateSolidBrush(dwColor));
			PatBlt(hBmpBkDC, 0, 0, nLcdXSize, nLcdYSize, PATCOPY);
			DeleteObject(SelectObject(hBmpBkDC,hBrush));
		}
		else
		{
			// get original background from bitmap
			BitBlt(hBmpBkDC,
				   0, 0, nLcdXSize, nLcdYSize,
				   hMainDC,
				   nLcdX, nLcdY,
				   SRCCOPY);
		}

		_ASSERT(hLcdDC);

		if (hBrush)							// has already a brush
		{
			// delete it first
			DeleteObject(SelectObject(hLcdDC,hBrush));
			hBrush = NULL;
		}

		if (dwKMLColor[byContrast] != I)	// have brush color?
		{
			// set brush for display pattern
			VERIFY(hBrush = (HBRUSH) SelectObject(hLcdDC,CreateSolidBrush(RGBtoCOLORREF(dwKMLColor[byContrast]))));
		}
		GdiFlush();
	}
	LeaveCriticalSection(&csGDILock);
	UpdateAnnunciators();					// adjust annunciator color
	return;
}

VOID SetLcdColor(UINT nId, UINT nRed, UINT nGreen, UINT nBlue)
{
	dwKMLColor[nId&0x3F] = ((nRed&0xFF)<<16)|((nGreen&0xFF)<<8)|(nBlue&0xFF);
	return;
}

VOID GetSizeLcdBitmap(INT *pnX, INT *pnY)
{
	*pnX = nLcdXSize;
	*pnY = nLcdYSize;
	return;
}

VOID CreateLcdBitmap(VOID)
{
	INT  i;

	BOOL bEmpty = TRUE;

	_ASSERT(nCurrentHardware != HDW_UNKNOWN);

	if (nCurrentHardware == HDW_LEWIS)
	{
		INT  nLcdWidth,nLcdHeight;
		UINT nPatSize;

		_ASSERT(nCurrentHardware == HDW_LEWIS);

		// create LCD bitmap
		if (cCurrentRomType != 'O' && cCurrentRomType != 'Y')
		{
			nLcdWidth  = 131;
			nLcdHeight = 16;

			// set display update routine
			UpdateDisplayMem = UpdateDisplayPioneer;

			if (nLcdyZoom == (UINT) -1)		// not initialized
			{
				// pixel aspect ratio = 0.733
				nLcdyZoom = (nLcdZoom * 1000 + 500) / 733;
			}
		}
		else
		{
			nLcdWidth  = 137;
			nLcdHeight = 32;

			// set display update routine
			UpdateDisplayMem = UpdateDisplayClamshell;

			if (nLcdyZoom == (UINT) -1)		// not initialized
			{
				nLcdyZoom = nLcdZoom;
			}
		}

		// select pixel drawing routine
		switch (nLcdZoom)
		{
		case 1:
			WritePixel = WritePixelZoom1;
			break;
		case 2:
			WritePixel = WritePixelZoom2;
			break;
		case 3:
			WritePixel = WritePixelZoom3;
			break;
		case 4:
			WritePixel = WritePixelZoom4;
			break;
		default:
			// select pixel pattern size (BYTE, WORD, DWORD)
			nLcdxZoom = nLcdZoom;			// BYTE pattern size adjusted x-Zoom
			nPatSize = sizeof(BYTE);		// use BYTE pattern
			while ((nLcdxZoom & 0x1) == 0 && nPatSize < sizeof(DWORD))
			{
				nLcdxZoom >>= 1;
				nPatSize <<= 1;
			}
			switch (nPatSize)
			{
			case sizeof(BYTE):
				WritePixel = WritePixelBYTE;
				break;
			case sizeof(WORD):
				WritePixel = WritePixelWORD;
				break;
			case sizeof(DWORD):
				WritePixel = WritePixelDWORD;
				break;
			default:
				_ASSERT(FALSE);
			}
		}

		// all KML contrast palette fields undefined?
		for (i = 0; bEmpty && i < ARRAYSIZEOF(dwKMLColor); ++i)
		{
			bEmpty = (dwKMLColor[i] == I);
		}
		if (bEmpty)							// preset KML contrast palette
		{
			// black on character
			for (i = 0; i < ARRAYSIZEOF(dwKMLColor) / 2; ++i)
			{
				_ASSERT(i < ARRAYSIZEOF(dwKMLColor));
				dwKMLColor[i] = B;
			}
		}

		// display contrast routine
		UpdateContrast = UpdateContrastLewis;

		nLcdXSize = nLcdWidth  * nLcdZoom;
		nLcdYSize = nLcdHeight * nLcdyZoom;

		// initialize background bitmap
		VERIFY(hBmpBkDC = CreateCompatibleDC(hWindowDC));
		VERIFY(hBmpBk = CreateCompatibleBitmap(hWindowDC,nLcdXSize,nLcdYSize));
		VERIFY(hBmpBk = (HBITMAP) SelectObject(hBmpBkDC,hBmpBk));
		_ASSERT(hBmpBk != HGDI_ERROR);

		// create mask bitmap
		bmiLcd.Lcd_bmih.biWidth = LCD_ROW * nLcdZoom;
		bmiLcd.Lcd_bmih.biHeight = -(LONG) nLcdYSize;
		VERIFY(hMaskDC = CreateCompatibleDC(hWindowDC));
		VERIFY(hMaskBitmap = CreateDIBSection(hWindowDC,(BITMAPINFO*)&bmiLcd,DIB_RGB_COLORS,(VOID **)&pbyMask,NULL,0));
		VERIFY(hMaskBitmap = (HBITMAP) SelectObject(hMaskDC,hMaskBitmap));

		// create LCD bitmap
		_ASSERT(hLcdDC == NULL);
		VERIFY(hLcdDC = CreateCompatibleDC(hWindowDC));
		VERIFY(hLcdBitmap = CreateCompatibleBitmap(hWindowDC,nLcdXSize,nLcdYSize));
		VERIFY(hLcdBitmap = (HBITMAP) SelectObject(hLcdDC,hLcdBitmap));

		_ASSERT(hPalette != NULL);
		SelectPalette(hLcdDC,hPalette,FALSE); // set palette for LCD DC
		RealizePalette(hLcdDC);				// realize palette

		UpdateContrast();					// initialize background
		EnterCriticalSection(&csGDILock);	// solving NT GDI problems
		{
			_ASSERT(hMainDC);				// background bitmap must be loaded

			// get original background from bitmap
			BitBlt(hLcdDC,
				   0, 0,
				   nLcdXSize, nLcdYSize,
				   hMainDC,
				   nLcdX, nLcdY,
				   SRCCOPY);
		}
		LeaveCriticalSection(&csGDILock);
	}
	else									// Sacajavea
	{
		_ASSERT(nCurrentHardware == HDW_SACA);

		GetDimLcdBitmap();					// get LCD bitmap dimension from annunciators

		// all KML contrast palette fields undefined?
		for (i = 0; bEmpty && i < 32; ++i)
		{
			bEmpty = (dwKMLColor[i] == I);
		}
		if (bEmpty)							// preset KML contrast palette
		{
			// black on character
			for (i = 0; i < 16; ++i)
			{
				_ASSERT(i < ARRAYSIZEOF(dwKMLColor));
				dwKMLColor[i] = B;
			}
		}

		// set display update routine
		UpdateDisplayMem = UpdateDisplaySacajawea;

		// display contrast routine
		UpdateContrast = UpdateContrastSaca;

		// initialize background bitmap
		VERIFY(hBmpBkDC = CreateCompatibleDC(hWindowDC));
		VERIFY(hBmpBk = CreateCompatibleBitmap(hWindowDC,nLcdXSize,nLcdYSize));
		VERIFY(hBmpBk = (HBITMAP) SelectObject(hBmpBkDC,hBmpBk));
		_ASSERT(hBmpBk != HGDI_ERROR);

		// create segment bitmap
		bmiLcd.Lcd_bmih.biWidth = (1 << PIX_PER_COL) * nLcdXPixel;
		bmiLcd.Lcd_bmih.biHeight = -PIX_PER_COL      * nLcdYPixel;
		VERIFY(hMaskDC = CreateCompatibleDC(hWindowDC));
		VERIFY(hMaskBitmap = CreateDIBSection(hWindowDC,(BITMAPINFO*)&bmiLcd,DIB_RGB_COLORS,(VOID **)&pbyMask,NULL,0));
		VERIFY(hMaskBitmap = (HBITMAP) SelectObject(hMaskDC,hMaskBitmap));
		_ASSERT(hMaskBitmap != HGDI_ERROR);
		// draw segment bitmap
		{
			UINT y,x,z;

			#if !defined NDEBUG
			LPBYTE pbyBase = pbyMask;
			#endif

			for (y = 0; y < PIX_PER_COL; ++y) // 7 pixel per row
			{
				for (x = 0; x < (1 << PIX_PER_COL); ++x) // draw line
				{
					BYTE byCol = (x >> y) & 1; // pixel color

					// draw pixel
					for (z = 0; z < nLcdXPixel; ++z)
						*pbyMask++ = byCol;
				}

				// check if scan line ends on a LONG data-type boundary
				_ASSERT((DWORD) pbyMask % sizeof(LONG) == 0);

				// copy lines
				for (z = 1; z < nLcdYPixel; ++z)
				{
					CopyMemory(pbyMask,pbyMask-bmiLcd.Lcd_bmih.biWidth,bmiLcd.Lcd_bmih.biWidth);
					pbyMask += bmiLcd.Lcd_bmih.biWidth;
				}

				// check if scan line ends on a LONG data-type boundary
				_ASSERT((DWORD) pbyMask % sizeof(LONG) == 0);
			}

			// check for buffer overflow
			_ASSERT(pbyMask - pbyBase <= bmiLcd.Lcd_bmih.biWidth * -bmiLcd.Lcd_bmih.biHeight);
		}

		// create LCD bitmap
		_ASSERT(hLcdDC == NULL);
		VERIFY(hLcdDC = CreateCompatibleDC(hWindowDC));
		VERIFY(hLcdBitmap = CreateCompatibleBitmap(hWindowDC,nLcdXSize,nLcdYSize));
		VERIFY(hLcdBitmap = (HBITMAP) SelectObject(hLcdDC,hLcdBitmap));
		_ASSERT(hLcdBitmap != HGDI_ERROR);

		// set brush for display pattern
		VERIFY(hBrush = (HBRUSH) SelectObject(hLcdDC,CreateSolidBrush(RGBtoCOLORREF(dwKMLColor[Chipset.IORam[CONTRAST]]))));
		_ASSERT(hBrush != HGDI_ERROR);

		_ASSERT(hPalette != NULL);
		SelectPalette(hLcdDC,hPalette,FALSE); // set palette for LCD DC
		RealizePalette(hLcdDC);				// realize palette

		UpdateContrast();					// initialize background

		EnterCriticalSection(&csGDILock);	// solving NT GDI problems
		{
			_ASSERT(hMainDC);				// background bitmap must be loaded

			// get original background from bitmap
			BitBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, hMainDC, nLcdX, nLcdY, SRCCOPY);
			GdiFlush();
		}
		LeaveCriticalSection(&csGDILock);
	}
	return;
}

VOID DestroyLcdBitmap(VOID)
{
	// set contrast palette to startup colors
	WORD i = 0; while(i < ARRAYSIZEOF(dwKMLColor)) dwKMLColor[i++] = I;

	UpdateContrast = NULL;
	UpdateDisplayMem = NULL;

	if (hLcdDC != NULL)
	{
		// destroy mask bitmap
		DeleteObject(SelectObject(hMaskDC,hMaskBitmap));
		DeleteDC(hMaskDC);

		// destroy display pattern brush
		if (hBrush)
		{
			DeleteObject(SelectObject(hLcdDC,hBrush));
		}

		// destroy background bitmap
		DeleteObject(SelectObject(hBmpBkDC,hBmpBk));
		DeleteDC(hBmpBkDC);

		// destroy LCD bitmap
		DeleteObject(SelectObject(hLcdDC,hLcdBitmap));
		DeleteDC(hLcdDC);
		hBrush = NULL;
		hLcdDC = NULL;
		hLcdBitmap = NULL;
	}

	nLcdXSize = 0;
	nLcdYSize = 0;
	nLcdxZoom = (UINT) -1;
	WritePixel = NULL;
	return;
}

BOOL CreateMainBitmap(LPCTSTR szFilename)
{
	_ASSERT(hWindowDC != NULL);
	VERIFY(hMainDC = CreateCompatibleDC(hWindowDC));
	if (hMainDC == NULL) return FALSE;		// quit if failed
	_ASSERT(hPalette == NULL);				// resource free
	hMainBitmap = LoadBitmapFile(szFilename);
	if (hMainBitmap == NULL)
	{
		DeleteDC(hMainDC);
		hMainDC = NULL;
		return FALSE;
	}
	hMainBitmap = (HBITMAP) SelectObject(hMainDC,hMainBitmap);
	_ASSERT(hPalette != NULL);
	VERIFY(SelectPalette(hMainDC,hPalette,FALSE));
	RealizePalette(hMainDC);
	return TRUE;
}

VOID DestroyMainBitmap(VOID)
{
	if (hMainDC != NULL)
	{
		// destroy Main bitmap
		DeleteObject(SelectObject(hMainDC,hMainBitmap));
		DeleteDC(hMainDC);
		hMainDC = NULL;
		hMainBitmap = NULL;
	}
	return;
}

//
// load annunciator bitmap
//
BOOL CreateAnnunBitmap(LPCTSTR szFilename)
{
	_ASSERT(hWindowDC != NULL);
	VERIFY(hAnnunDC = CreateCompatibleDC(hWindowDC));
	if (hAnnunDC == NULL) return FALSE;		// quit if failed
	hAnnunBitmap = LoadBitmapFile(szFilename);
	if (hAnnunBitmap == NULL)
	{
		DeleteDC(hAnnunDC);
		hAnnunDC = NULL;
		return FALSE;
	}
	hAnnunBitmap = (HBITMAP) SelectObject(hAnnunDC,hAnnunBitmap);
	return TRUE;
}

//
// set annunciator bitmap
//
VOID SetAnnunBitmap(HDC hDC, HBITMAP hBitmap)
{
	hAnnunDC = hDC;
	hAnnunBitmap = hBitmap;
	return;
}

//
// destroy annunciator bitmap
//
VOID DestroyAnnunBitmap(VOID)
{
	if (hAnnunDC != NULL)
	{
		DeleteObject(SelectObject(hAnnunDC,hAnnunBitmap));
		DeleteDC(hAnnunDC);
		hAnnunDC = NULL;
		hAnnunBitmap = NULL;
	}
	return;
}

//****************
//*
//* LCD functions
//*
//****************

static VOID WritePixelZoom4(BYTE *p, BYTE a)
{
	BYTE *s;
	UINT y;

	if (a != 0)								// pixel on
	{
		CONST DWORD dwPattern[] = { 0x00000000, 0x01010101 };

		_ASSERT((DWORD) p % sizeof(DWORD) == 0); // check alignment for ARM CPU

		for (y = 0; y < nLcdyZoom; ++y)
		{
			s = p + 4 * LCD_ROW * y;
			*(DWORD *)s = dwPattern[a&1];

			s += 4 * LCD_ROW * nLcdyZoom;
			*(DWORD *)s = dwPattern[(a>>1)&1];

			s += 4 * LCD_ROW * nLcdyZoom;
			*(DWORD *)s = dwPattern[(a>>2)&1];

			s += 4 * LCD_ROW * nLcdyZoom;
			*(DWORD *)s = dwPattern[(a>>3)&1];
		}
	}
	return;
}

static VOID WritePixelZoom3(BYTE *p, BYTE a)
{
	BYTE *s;
	UINT y;

	if (a != 0)								// pixel on
	{
		for (y = 0; y < nLcdyZoom; ++y)
		{
			s = p + 3 * LCD_ROW * y;
			s[0] = s[1] = s[2] = a&1;

			s += 3 * LCD_ROW * nLcdyZoom;
			s[0] = s[1] = s[2] = (a>>1)&1;

			s += 3 * LCD_ROW * nLcdyZoom;
			s[0] = s[1] = s[2] = (a>>2)&1;

			s += 3 * LCD_ROW * nLcdyZoom;
			s[0] = s[1] = s[2] = (a>>3)&1;
		}
	}
	return;
}

static VOID WritePixelZoom2(BYTE *p, BYTE a)
{
	BYTE *s;
	UINT y;

	if (a != 0)								// pixel on
	{
		CONST WORD wPattern[] = { 0x0000, 0x0101 };

		_ASSERT((DWORD) p % sizeof(WORD) == 0); // check alignment for ARM CPU

		for (y = 0; y < nLcdyZoom; ++y)
		{
			s = p + 2 * LCD_ROW * y;
			*(WORD *)s = wPattern[a&1];

			s += 2 * LCD_ROW * nLcdyZoom;
			*(WORD *)s = wPattern[(a>>1)&1];

			s += 2 * LCD_ROW * nLcdyZoom;
			*(WORD *)s = wPattern[(a>>2)&1];

			s += 2 * LCD_ROW * nLcdyZoom;
			*(WORD *)s = wPattern[(a>>3)&1];
		}
	}
	return;
}

static VOID WritePixelZoom1(BYTE *p, BYTE a)
{
	BYTE *s;
	UINT y;

	if (a != 0)								// pixel on
	{
		for (y = 0; y < nLcdyZoom; ++y)
		{
			s = p + LCD_ROW * y;
			*s = a&1;

			s += LCD_ROW * nLcdyZoom;
			*s = (a>>1)&1;

			s += LCD_ROW * nLcdyZoom;
			*s = (a>>2)&1;

			s += LCD_ROW * nLcdyZoom;
			*s = (a>>3)&1;
		}
	}
	return;
}

static VOID WritePixelDWORD(BYTE *p, BYTE a)
{
	CONST DWORD dwPattern[] = { 0x00000000, 0x01010101 };

	UINT x,y;

	_ASSERT(nLcdxZoom > 0);					// x-Zoom factor
	_ASSERT((DWORD) p % sizeof(DWORD) == 0); // check alignment for ARM CPU

	while (a != 0)							// pixel on
	{
		DWORD c = dwPattern[a&1];

		for (y = nLcdyZoom; y > 0; --y)
		{
			LPDWORD pdwPixel = (LPDWORD) p;

			x = nLcdxZoom;
			do
			{
				*pdwPixel++ = c;
			}
			while (--x > 0);
			p += sizeof(DWORD) * LCD_ROW * nLcdxZoom;
		}
		a >>= 1;
	}
	return;
}

static VOID WritePixelWORD(BYTE *p, BYTE a)
{
	CONST WORD wPattern[] = { 0x0000, 0x0101 };

	UINT x,y;

	_ASSERT(nLcdxZoom > 0);					// x-Zoom factor
	_ASSERT((DWORD) p % sizeof(WORD) == 0);	// check alignment for ARM CPU

	while (a != 0)							// pixel on
	{
		WORD c = wPattern[a&1];

		for (y = nLcdyZoom; y > 0; --y)
		{
			LPWORD pwPixel = (LPWORD) p;

			x = nLcdxZoom;
			do
			{
				*pwPixel++ = c;
			}
			while (--x > 0);
			p += sizeof(WORD) * LCD_ROW * nLcdxZoom;
		}
		a >>= 1;
	}
	return;
}

static VOID WritePixelBYTE(BYTE *p, BYTE a)
{
	UINT x,y;

	_ASSERT(nLcdxZoom > 0);					// x-Zoom factor

	while (a != 0)							// pixel on
	{
		BYTE c = a&1;

		for (y = nLcdyZoom; y > 0; --y)
		{
			LPBYTE pbyPixel = p;

			x = nLcdxZoom;
			do
			{
				*pbyPixel++ = c;
			}
			while (--x > 0);
			p += LCD_ROW * nLcdxZoom;
		}
		a >>= 1;
	}
	return;
}

static VOID UpdateDisplayClamshell(VOID)
{
	DWORD d;
	INT   x0,y0;
	BYTE  *p,byContrast;

	BOOL bOn = (Chipset.IORam[DSPCTL] & DON) != 0;

	// when display is on fetch contrast value
	byContrast = (bOn)
			   ? ((Chipset.IORam[DSPCTL] & CONT4) << 3) | Chipset.IORam[CONTRAST]
			   : 0;

	ZeroMemory(pbyMask, bmiLcd.Lcd_bmih.biWidth * -bmiLcd.Lcd_bmih.biHeight);
	byContrast=22;
	if ((Chipset.IORam[DSPCTL]&DON) != 0 && dwKMLColor[byContrast] != I)
	{
		// scan complete display area of master
		for (d = MDISP_BEGIN; d <= MDISP_END; ++d)
		{
			// calculate x/y coordinates
			y0 = (d&0x7)*4;
			x0 = (SDISP_END-SDISP_BEGIN+1+d)>>3;

			// calculate memory position in LCD bitmap
			p = pbyMask + (y0*LCD_ROW*nLcdyZoom + x0)*nLcdZoom;

			// write pixel zoom independent
			WritePixel(p,Chipset.IORam[d]);
		}

		// scan complete display area of slave
		for (d = SDISP_BEGIN; d <= SDISP_END; ++d)
		{
			// calculate x/y coordinates
			y0 = ((d-SDISP_BEGIN)&0x7)*4;
			x0 = (d-SDISP_BEGIN)>>3;

			// calculate memory position in LCD bitmap
			p = pbyMask + (y0*LCD_ROW*nLcdyZoom + x0)*nLcdZoom;

			// write pixel zoom independent
			WritePixel(p,(BYTE)(ChipsetS.bLcdSyncErr ? 0xF : ChipsetS.IORam[d]));
		}
	}

	EnterCriticalSection(&csGDILock);		// solving NT GDI problems
	{
		// load lcd with mask bitmap
		BitBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, hMaskDC, 0, 0, SRCCOPY);

		// mask segment mask with background and brush
		BitBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, hBmpBkDC, 0, 0, ROP_PDSPxax);
		GdiFlush();
	}
	LeaveCriticalSection(&csGDILock);
	return;
}

static VOID UpdateDisplayPioneer(VOID)
{
	DWORD d;
	INT   x0,y0;
	BYTE  *p,byContrast;

	BOOL bOn = (Chipset.IORam[DSPCTL] & DON) != 0;

	// when display is on fetch contrast value
	byContrast = (bOn)
			   ? ((Chipset.IORam[DSPCTL] & CONT4) << 3) | Chipset.IORam[CONTRAST]
			   : 0;

	ZeroMemory(pbyMask, bmiLcd.Lcd_bmih.biWidth * -bmiLcd.Lcd_bmih.biHeight);

	if ((Chipset.IORam[DSPCTL]&DON) != 0 && dwKMLColor[byContrast] != I)
	{
		// scan complete display area
		for (d = DISP_BEGIN; d <= DISP_END; ++d)
		{
			// calculate x/y coordinates
			y0 = (d&0x3)*4;
			x0 = 66*((d>>2)&1)+(d>>3);

			// calculate memory position in LCD bitmap
			p = pbyMask + (y0*LCD_ROW*nLcdyZoom + x0)*nLcdZoom;

			// write pixel zoom independent
			WritePixel(p,Chipset.IORam[d]);
		}
	}

	EnterCriticalSection(&csGDILock);		// solving NT GDI problems
	{
		// load lcd with mask bitmap
		BitBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, hMaskDC, 0, 0, SRCCOPY);

		// mask segment mask with background and brush
		BitBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, hBmpBkDC, 0, 0, ROP_PDSPxax);
		GdiFlush();
	}
	LeaveCriticalSection(&csGDILock);
	return;
}

static VOID UpdateDisplaySacajawea(VOID)
{
	// current annunciator DC
	HDC hSelAnnunDC = hAnnunDC != NULL ? hAnnunDC : hMainDC;

	EnterCriticalSection(&csGDILock);		// solving NT GDI problems
	{
		// set mask bitmap to background
		VERIFY(PatBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, WHITENESS));

		if ((Chipset.IORam[DSPCTL]&DON) != 0 && dwKMLColor[Chipset.IORam[CONTRAST]] != I)
		{
			DWORD d;

			// scan complete display area
			for (d = PDISP_BEGIN; d <= PDISP_END; d += 2)
			{
				INT nByteNo = d >> 1;

				INT nValue = (Chipset.IORam[d+1] << 4) | Chipset.IORam[d];

				// column pixel
				BitBlt(hLcdDC,
					   (((nByteNo) / COL_PER_CHAR) * nLcdDistance + ((nByteNo) % COL_PER_CHAR) * nLcdXPixel + nLcdXOff),
					   nLcdYOff,
					   nLcdXPixel, PIX_PER_COL * nLcdYPixel,
					   hMaskDC,
					   (nValue & 0x7F) * nLcdXPixel, 0,
					   SRCCOPY);			// rectangle

				// annunciator set
				if ((nValue & 0x80) != 0 && pAnnunciator[nByteNo].nCx > 0)
				{
					BitBlt(hLcdDC,
						   pAnnunciator[nByteNo].nOx, pAnnunciator[nByteNo].nOy,
						   pAnnunciator[nByteNo].nCx, pAnnunciator[nByteNo].nCy,
						   hSelAnnunDC,
						   pAnnunciator[nByteNo].nDx, pAnnunciator[nByteNo].nDy,
						   SRCCOPY);		// areas don't overlap
				}
			}
		}

		// mask segment mask with background and brush
		BitBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, hBmpBkDC, 0, 0, ROP_PDSPxax);
		GdiFlush();
	}
	LeaveCriticalSection(&csGDILock);
	return;
}

static VOID UpdateMainDisplay(VOID)
{
	_ASSERT(UpdateDisplayMem);				// function not initialized
	UpdateDisplayMem();						// update display memory

	EnterCriticalSection(&csGDILock);		// solving NT GDI problems
	{
		BitBlt(hWindowDC, nLcdX, nLcdY, nLcdXSize, nLcdYSize, hLcdDC, 0, 0, SRCCOPY);
		GdiFlush();
	}
	LeaveCriticalSection(&csGDILock);
	return;
}

static VOID CALLBACK LcdProc(UINT uEventId, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	EnterCriticalSection(&csLcdLock);
	{
		if (uLcdTimerId)					// display update task still active
		{
			UpdateMainDisplay();
		}
	}
	LeaveCriticalSection(&csLcdLock);
	return;
	UNREFERENCED_PARAMETER(uEventId);
	UNREFERENCED_PARAMETER(uMsg);
	UNREFERENCED_PARAMETER(dwUser);
	UNREFERENCED_PARAMETER(dw1);
	UNREFERENCED_PARAMETER(dw2);
}

VOID StartDisplay(VOID)
{
	if (uLcdTimerId)						// LCD update timer running
		return;								// -> quit

	if (Chipset.IORam[DSPCTL]&DON)			// display on?
	{
		_ASSERT(nCurrentHardware != HDW_UNKNOWN);

		if (nCurrentHardware == HDW_LEWIS)
		{
			_ASSERT(nCurrentHardware == HDW_LEWIS);
			UpdateAnnunciators();			// switch on annunciators
		}
		else								// Sacajavea
		{
			_ASSERT(nCurrentHardware == HDW_SACA);
			SetBkBitmap();					// set background
		}

		VERIFY(uLcdTimerId = timeSetEvent(DISPLAY_FREQ,0,(LPTIMECALLBACK)&LcdProc,0,TIME_PERIODIC));
	}
	return;
}

VOID StopDisplay(VOID)
{
	if (uLcdTimerId == 0)					// timer stopped
		return;								// -> quit

	timeKillEvent(uLcdTimerId);				// stop display update
	uLcdTimerId = 0;						// set flag display update stopped

	_ASSERT(nCurrentHardware != HDW_UNKNOWN);

	EnterCriticalSection(&csLcdLock);
	{
		EnterCriticalSection(&csGDILock);	// solving NT GDI problems
		{
			// get original background from bitmap
			BitBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, hMainDC, nLcdX, nLcdY, SRCCOPY);
			GdiFlush();
		}
		LeaveCriticalSection(&csGDILock);
	}
	LeaveCriticalSection(&csLcdLock);
	InvalidateRect(hWnd,NULL,FALSE);
	return;
}

//
// Lewis chip annunciator implementation
//
VOID UpdateAnnunciators(VOID)
{
	const DWORD dwAnnAddrP[] = { LA_ALL, LA_UPDOWN, LA_SHIFT, LA_PRINTER, LA_BUSY, LA_BAT, LA_G, LA_RAD };
	const DWORD dwAnnAddrO[] = { SLA_ALL, SLA_HALT, SLA_SHIFT, SLA_ALPHA, SLA_BUSY, SLA_BAT, SLA_RAD, SLA_PRINTER };

	DWORD  const *dwAnnAddr;
	BOOL   bAnnOn;
	LPBYTE pbySource;
	INT    i,j,nCount;

	if (nCurrentHardware != HDW_LEWIS) return;

	bAnnOn = (Chipset.IORam[DSPCTL] & DON) != 0;

	if (Chipset.bSlave)						// has slave controller
	{
		dwAnnAddr = dwAnnAddrO;
		pbySource = ChipsetS.IORam;			// slave controller

		bAnnOn = bAnnOn && (ChipsetS.IORam[DSPCTL] & DON) != 0;
	}
	else
	{
		dwAnnAddr = dwAnnAddrP;
		pbySource = Chipset.IORam;			// master controller
	}

	// check all annuncators
	for (i = 1; i < ARRAYSIZEOF(dwAnnAddrP); ++i)
	{
		nCount = 0;

		if (bAnnOn)							// annunciators on
		{
			DWORD dwAnn = Npack(pbySource+dwAnnAddr[i],8) ^ Npack(pbySource+dwAnnAddr[0],8);

			// count the number of set bits
			for (;dwAnn != 0; ++nCount)
			{
				dwAnn &= (dwAnn - 1);
			}
		}

		// contrast table entry of annunciator
		j = ((Chipset.IORam[DSPCTL] & CONT4) << 3) | Chipset.IORam[CONTRAST];
		j = j + nCount - 16;
		if (j < 0)  j = 0;
		if (j > 31) j = 31;

		DrawAnnunciator(i, nCount > 0 && dwKMLColor[j] != I, dwKMLColor[j]);
	}
	return;
}

//
// interprete characters of Sacajawea display
//
VOID GetLcdNumberSaca(LPTSTR szContent)
{
	INT   nAddr;
	INT   i = 0;

	_ASSERT(nCurrentHardware == HDW_SACA);

	// get sign
	if ((Chipset.IORam[1] & 0x8)) szContent[i++] = _T('-');

	// scan complete display area
	for (nAddr = PDISP_BEGIN; nAddr <= PDISP_END; nAddr += 10)
	{
		INT j;
		QWORD qwSeg = 0;

		// build character image
		for (j = 0; j < 10; j += 2)
		{
			qwSeg <<= 3;
			qwSeg |= Chipset.IORam[nAddr+j+1] & 0x7;
			qwSeg <<= 4;
			qwSeg |= Chipset.IORam[nAddr+j];
		}

		// search for character image in table
		for (j = 0; j < ARRAYSIZEOF(sSegtabSaca); ++j)
		{
			if (sSegtabSaca[j].qwImage == qwSeg) // found known image
			{
				szContent[i++] = sSegtabSaca[j].cChar;
				break;
			}
		}

		// decode dp and cm
		if (Chipset.IORam[nAddr+9] & 0x8)	// dp set
		{
			szContent[i++] = (Chipset.IORam[nAddr+7] & 0x8) ? _T(',') : _T('.');
		}
	}
	szContent[i] = 0;
	return;
}

VOID ResizeWindow(VOID)
{
	if (hWnd != NULL)						// if window created
	{
		RECT rectWindow;
		RECT rectClient;

		rectWindow.left   = 0;
		rectWindow.top    = 0;
		rectWindow.right  = nBackgroundW;
		rectWindow.bottom = nBackgroundH;

		AdjustWindowRect(&rectWindow,
			(DWORD) GetWindowLongPtr(hWnd,GWL_STYLE),
			GetMenu(hWnd) != NULL || IsRectEmpty(&rectWindow));
		SetWindowPos(hWnd, bAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0,
			rectWindow.right  - rectWindow.left,
			rectWindow.bottom - rectWindow.top,
			SWP_NOMOVE);

		// check if menu bar wrapped to two or more rows
		GetClientRect(hWnd, &rectClient);
		if (rectClient.bottom < (LONG) nBackgroundH)
		{
			rectWindow.bottom += (nBackgroundH - rectClient.bottom);
			SetWindowPos (hWnd, NULL, 0, 0,
				rectWindow.right  - rectWindow.left,
				rectWindow.bottom - rectWindow.top,
				SWP_NOMOVE | SWP_NOZORDER);
		}

		EnterCriticalSection(&csGDILock);	// solving NT GDI problems
		{
			_ASSERT(hWindowDC);				// move origin of destination window
			VERIFY(SetWindowOrgEx(hWindowDC, nBackgroundX, nBackgroundY, NULL));
			GdiFlush();
		}
		LeaveCriticalSection(&csGDILock);
		InvalidateRect(hWnd,NULL,TRUE);
	}
	return;
}
