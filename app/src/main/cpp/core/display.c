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

static HDC     hMaskDC = NULL;				// mask bitmap
static HBITMAP hMaskBitmap;
static LPBYTE  pbyMask;

static HDC     hBmpBkDC = NULL;				// current background
static HBITMAP hBmpBk;

static HBRUSH  hBrush = NULL;				// old drawing brush

static UINT nLcdXSize = 0;					// display size
static UINT nLcdYSize = 0;

static UINT nLcdxZoom = (UINT) -1;			// x zoom factor

static UINT uLcdTimerId = 0;

// segment type -> reference to annunciator
static CONST BYTE byLcdTypeBert[] =
{
	0x00, 0x00, 0x0A, 0x0B, 0x00, 0x00, 0x00, 0x00,	// A000, LSB first
	0x07, 0x05, 0x04, 0x0C, 0x00, 0x00, 0x00, 0x06,	// A002, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A004, LSB first
	0x07, 0x05, 0x04, 0x0D, 0x00, 0x00, 0x00, 0x06,	// A006, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A008, LSB first
	0x07, 0x05, 0x04, 0x0E, 0x00, 0x00, 0x00, 0x06,	// A00A, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A00C, LSB first
	0x07, 0x05, 0x04, 0x0F, 0x00, 0x00, 0x00, 0x06,	// A00E, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A010, LSB first
	0x07, 0x05, 0x04, 0x10, 0x00, 0x00, 0x00, 0x06,	// A012, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A014, LSB first
	0x07, 0x05, 0x04, 0x11, 0x00, 0x00, 0x00, 0x06,	// A016, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A018, LSB first
	0x07, 0x05, 0x04, 0x12, 0x00, 0x00, 0x00, 0x06,	// A01A, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A01C, LSB first
	0x07, 0x05, 0x04, 0x13, 0x00, 0x00, 0x00, 0x06,	// A01E, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A020, LSB first
	0x07, 0x05, 0x04, 0x14, 0x00, 0x00, 0x00, 0x06,	// A022, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A024, LSB first
	0x07, 0x05, 0x04, 0x15, 0x00, 0x00, 0x00, 0x06,	// A026, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A028, LSB first
	0x07, 0x05, 0x04, 0x16, 0x00, 0x00, 0x00, 0x06,	// A02A, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A02C, LSB first
	0x07, 0x05, 0x04, 0x17, 0x00, 0x00, 0x00, 0x06,	// A02E, LSB first
	0x02, 0x03, 0x08, 0x09, 0x00, 0x00, 0x00, 0x01,	// A030, LSB first
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00	// A032, LSB first
};

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

VOID (*UpdateContrast)(VOID) = NULL;

static VOID UpdateContrastBert(VOID);
static VOID UpdateContrastSaca(VOID);
static VOID UpdateContrastLewis(VOID);

static VOID (*WritePixel)(BYTE *p, BYTE a) = NULL;

static VOID WritePixelZoom4(BYTE *p, BYTE a);
static VOID WritePixelZoom3(BYTE *p, BYTE a);
static VOID WritePixelZoom2(BYTE *p, BYTE a);
static VOID WritePixelZoom1(BYTE *p, BYTE a);
static VOID WritePixelBYTE(BYTE *p, BYTE a);
static VOID WritePixelWORD(BYTE *p, BYTE a);
static VOID WritePixelDWORD(BYTE *p, BYTE a);

static VOID(*UpdateDisplayMem)(VOID) = NULL;

static VOID UpdateDisplayClamshell(VOID);
static VOID UpdateDisplayPioneer(VOID);
static VOID UpdateDisplaySacajawea(VOID);
static VOID UpdateDisplayBert(VOID);

//
// calculate size of Bert LCD bitmap from KML script settings
//
static __inline VOID GetDimLcdBitmapBert(VOID)
{
	UINT nX,nY,nLastDigit;
	INT  i;

	nLcdXSize = nLcdYSize = 0;

	nLastDigit = (12-1) * nLcdDistance;		// last digit offset

	// scan all annunciators
	for (i = 0; i < ARRAYSIZEOF(pAnnunciator); ++i)
	{
		if (i >= 9) nLastDigit = 0;			// no digits any more

		nX = pAnnunciator[i].nOx + pAnnunciator[i].nCx + nLastDigit;
		if (nX > nLcdXSize) nLcdXSize = nX;	// new X-max value?

		nY = pAnnunciator[i].nOy + pAnnunciator[i].nCy;
		if (nY > nLcdYSize) nLcdYSize = nY;	// new Y-max value?
	}
	return;
}

//
// calculate size of Sacajawea LCD bitmap from KML script settings
//
static __inline VOID GetDimLcdBitmapSaca(VOID)
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
				const INT nByteNo = d >> 1;

				// column pixel
				BitBlt(hBmpBkDC,
					   ((nByteNo / COL_PER_CHAR) * nLcdDistance + (nByteNo % COL_PER_CHAR) * nLcdXPixel + nLcdXOff),
					   nLcdYOff,
					   nLcdXPixel, PIX_PER_COL * nLcdYPixel,
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

static VOID UpdateContrastBert(VOID)
{
	_ASSERT(hLcdDC != NULL);
	EnterCriticalSection(&csGDILock);		// asynchronous display update needs brush
	{
		// replace old with new brush
		DeleteObject(SelectObject(hLcdDC,CreateSolidBrush(
			RGBtoCOLORREF(dwKMLColor[Chipset.b.IORam[BCONTRAST] & (DC2|DC1|DC0)]))));
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
	UpdateAnnunciators(0x7F);				// adjust annunciator color
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

		if (hAnnunDC == NULL)				// no external LCD bitmap
		{
			CreateAnnunBitmapFromMain();	// create annunciator bitmap from background bitmap
		}

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
	else if (nCurrentHardware == HDW_SACA)	// Sacajavea
	{
		_ASSERT(nCurrentHardware == HDW_SACA);

		GetDimLcdBitmapSaca();				// get LCD bitmap dimension from annunciators

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
				_ASSERT((DWORD_PTR) pbyMask % sizeof(LONG) == 0);

				// copy lines
				for (z = 1; z < nLcdYPixel; ++z)
				{
					CopyMemory(pbyMask,pbyMask-bmiLcd.Lcd_bmih.biWidth,bmiLcd.Lcd_bmih.biWidth);
					pbyMask += bmiLcd.Lcd_bmih.biWidth;
				}

				// check if scan line ends on a LONG data-type boundary
				_ASSERT((DWORD_PTR) pbyMask % sizeof(LONG) == 0);
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

		if (hAnnunDC == NULL)				// no external LCD bitmap
		{
			CreateAnnunBitmapFromMain();	// create annunciator bitmap from background bitmap
		}

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
	else
	{
		_ASSERT(nCurrentHardware == HDW_BERT);

		GetDimLcdBitmapBert();				// get LCD bitmap dimension from annunciators

		// set all undefined KML contrast palette fields to black
		for (i = 0; i <= (DC2|DC1|DC0); ++i)
		{
			if (dwKMLColor[i] == I)
			{
				dwKMLColor[i] = B;
			}
		}

		// create LCD bitmap
		_ASSERT(hLcdDC == NULL);
		VERIFY(hLcdDC = CreateCompatibleDC(hWindowDC));
		VERIFY(hLcdBitmap = CreateCompatibleBitmap(hWindowDC,nLcdXSize,nLcdYSize));
		VERIFY(hLcdBitmap = (HBITMAP) SelectObject(hLcdDC,hLcdBitmap));
		_ASSERT(hLcdBitmap != HGDI_ERROR);

		// set brush for display pattern
		VERIFY(hBrush = CreateSolidBrush(RGBtoCOLORREF(dwKMLColor[Chipset.b.IORam[BCONTRAST] & (DC2|DC1|DC0)])));
		VERIFY(hBrush = (HBRUSH) SelectObject(hLcdDC,hBrush));
		_ASSERT(hBrush != HGDI_ERROR);

		_ASSERT(hPalette != NULL);
		SelectPalette(hLcdDC,hPalette,FALSE); // set palette for LCD DC
		RealizePalette(hLcdDC);				// realize palette

		// set display update routine
		UpdateDisplayMem = UpdateDisplayBert;

		// display contrast routine
		UpdateContrast = UpdateContrastBert;

		if (hAnnunDC == NULL)				// no external LCD bitmap
		{
			CreateAnnunBitmapFromMain();	// create annunciator bitmap from background bitmap
		}

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
		if (hMaskDC)
		{
			DeleteObject(SelectObject(hMaskDC,hMaskBitmap));
			DeleteDC(hMaskDC);
		}

		// destroy display pattern brush
		if (hBrush)
		{
			DeleteObject(SelectObject(hLcdDC,hBrush));
		}

		// destroy background bitmap
		if (hBmpBkDC)
		{
			DeleteObject(SelectObject(hBmpBkDC,hBmpBk));
			DeleteDC(hBmpBkDC);
		}

		// destroy LCD bitmap
		DeleteObject(SelectObject(hLcdDC,hLcdBitmap));
		DeleteDC(hLcdDC);
		hMaskDC = NULL;
		hBrush = NULL;
		hBmpBkDC = NULL;
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
	hMainBitmap = LoadBitmapFile(szFilename,TRUE);
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
	hAnnunBitmap = LoadBitmapFile(szFilename,FALSE);
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
// create annunciator bitmap from background bitmap
//
BOOL CreateAnnunBitmapFromMain(VOID)
{
	BITMAPINFO bmi;
	LPDWORD pdwData, pdwRGBPixel;
	BYTE    byLuminanceMin, byLuminanceMax, byLuminanceMean;
	UINT    i, nAnnIdx, nPixelSize;

	// annunciator bitmap
	ZeroMemory(&bmi,sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;			// use 32 bit bitmap for easier buffer calculation
	bmi.bmiHeader.biCompression = BI_RGB;

	// 1st pass, get size for annunciator bitmap
	for (nAnnIdx = 0, i = 0; i < ARRAYSIZEOF(pAnnunciator); ++i)
	{
		// annunciator has a dimension
		if (pAnnunciator[i].nCx * pAnnunciator[i].nCy > 0)
		{
			// quit for opaque annunciators
			if (pAnnunciator[i].bOpaque == TRUE)
				return TRUE;				// quit with success

			bmi.bmiHeader.biWidth += pAnnunciator[i].nCx;
			if ((LONG) pAnnunciator[i].nCy > bmi.bmiHeader.biHeight)
			{
				bmi.bmiHeader.biHeight = pAnnunciator[i].nCy;
				nAnnIdx = i;				// index of annunciator with maximum height
			}
		}
	}

	// no. of Pixel
	nPixelSize = bmi.bmiHeader.biHeight * bmi.bmiHeader.biWidth;

	if (nPixelSize == 0)					// annunciator size = 0
		return TRUE;						// success

	// create annunciator bitmap buffer
	VERIFY(hAnnunBitmap = CreateDIBSection(hWindowDC,
										   &bmi,
										   DIB_RGB_COLORS,
										   (VOID **)&pdwData,
										   NULL,
										   0));
	if (hAnnunBitmap == NULL) return FALSE;

	// check pixel buffer alignment for ARM CPU
	_ASSERT((DWORD_PTR) pdwData % sizeof(DWORD) == 0);

	VERIFY(hAnnunDC = CreateCompatibleDC(hWindowDC));
	hAnnunBitmap = (HBITMAP) SelectObject(hAnnunDC,hAnnunBitmap);

	EnterCriticalSection(&csGDILock);		// solving NT GDI problems
	{
		// copy one annunciator pixel to begin of bitmap buffer
		BitBlt(hAnnunDC, 0, bmi.bmiHeader.biHeight - 1, 1, 1,
			   hMainDC, pAnnunciator[nAnnIdx].nDx, pAnnunciator[nAnnIdx].nDy, SRCCOPY);
		GdiFlush();
	}
	LeaveCriticalSection(&csGDILock);

	// fill annunciator bitmap with a annunciator color
	{
		// get annunciator color (doesn't matter if fore- or background color)
		CONST DWORD dwColor = *pdwData;		// bottom up bitmap

		for (pdwRGBPixel = pdwData, i = nPixelSize; i > 1; --i)
		{
			*++pdwRGBPixel = dwColor;		// and fill
		}
	}

	EnterCriticalSection(&csGDILock);		// solving NT GDI problems
	{
		UINT x = 0;							// bitmap target position

		// 2nd pass, copy annunciators into annunciator bitmap
		for (i = 0; i < ARRAYSIZEOF(pAnnunciator); ++i)
		{
			// annunciator has a dimension
			if (pAnnunciator[i].nCx * pAnnunciator[i].nCy > 0)
			{
				_ASSERT((LONG) x < bmi.bmiHeader.biWidth);
				_ASSERT((LONG) pAnnunciator[i].nCy <= bmi.bmiHeader.biHeight);

				// copy annunciator to annunciator bitmap
				BitBlt(hAnnunDC, x, 0, pAnnunciator[i].nCx, pAnnunciator[i].nCy,
					   hMainDC, pAnnunciator[i].nDx, pAnnunciator[i].nDy, SRCCOPY);

				pAnnunciator[i].nDx = x;	// new annunciator down position
				pAnnunciator[i].nDy = 0;

				x += pAnnunciator[i].nCx;	// next position
			}
		}
		GdiFlush();
	}
	LeaveCriticalSection(&csGDILock);

	// convert to grayscale and get min/max value
	byLuminanceMin = 0xFF; byLuminanceMax = 0x00;
	for (pdwRGBPixel = pdwData, i = nPixelSize; i > 0; --i)
	{
		CONST LPBYTE pbyRGBPixel = (LPBYTE) pdwRGBPixel;

		// NTSC formula for grayscale: 0.299 * Red + 0.587 * Green + 0.114 * Blue
		DWORD dwLuminance = pbyRGBPixel[0] * 114		// blue
						  + pbyRGBPixel[1] * 587		// green
						  + pbyRGBPixel[2] * 299;		// red
		*pbyRGBPixel = (BYTE) (dwLuminance / 1000);		// set grayscale value to blue

		if (*pbyRGBPixel < byLuminanceMin) byLuminanceMin = *pbyRGBPixel;
		if (*pbyRGBPixel > byLuminanceMax) byLuminanceMax = *pbyRGBPixel;

		++pdwRGBPixel;						// next pixel
	}

	// convert to black & white bitmap
	byLuminanceMean = (BYTE) (((WORD) byLuminanceMin + (WORD) byLuminanceMax) / 2);
	for (pdwRGBPixel = pdwData, i = nPixelSize; i > 0; --i)
	{
		// paint white or black dependent on luminance of blue
		*pdwRGBPixel = (*(LPBYTE) pdwRGBPixel >= byLuminanceMean) ? W : B;
		++pdwRGBPixel;						// next pixel
	}

	#if defined DEBUG_WRITEBMP
	// write mask to file
	{
		HANDLE hFile;

		hFile = CreateFile(_T("amask.bmp"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			BITMAPFILEHEADER bfh;
			DWORD  dwWritten;

			bfh.bfType = 0x4D42;
			bfh.bfSize = sizeof(bfh) + sizeof(bmi) + nPixelSize * 4;
			bfh.bfReserved1 = 0;
			bfh.bfReserved2 = 0;
			bfh.bfOffBits = sizeof(bfh) + sizeof(bmi);

			WriteFile(hFile,&bfh,sizeof(bfh),&dwWritten,NULL);
			WriteFile(hFile,&bmi,sizeof(bmi),&dwWritten,NULL);
			WriteFile(hFile,pdwData,nPixelSize * 4,&dwWritten,NULL);
			CloseHandle(hFile);
		}
	}
	#endif
	return TRUE;							// success
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

		_ASSERT((DWORD_PTR) p % sizeof(DWORD) == 0); // check alignment for ARM CPU

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

		_ASSERT((DWORD_PTR) p % sizeof(WORD) == 0); // check alignment for ARM CPU

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
	_ASSERT((DWORD_PTR) p % sizeof(DWORD) == 0); // check alignment for ARM CPU

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
	_ASSERT((DWORD_PTR) p % sizeof(WORD) == 0);	// check alignment for ARM CPU

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
				DWORD i;
				INT   nValue;

				const UINT nByteNo = d >> 1;

				// get column content
				const WORD w = *(WORD *) &Chipset.IORam[d];

				// translate pixel column over row driver table
				for (nValue = 0, i = PROW_END - 1; i >= PROW_START; i -= 2)
				{
					const WORD c = (w & *(WORD *) &Chipset.IORam[i]);
					nValue = (nValue << 1) | (!!c);
				}

				// column pixel
				BitBlt(hLcdDC,
					   ((nByteNo / COL_PER_CHAR) * nLcdDistance + (nByteNo % COL_PER_CHAR) * nLcdXPixel + nLcdXOff),
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

static VOID UpdateDisplayBert(VOID)
{
	EnterCriticalSection(&csGDILock);		// solving NT GDI problems
	{
		HDC hSelAnnunDC = hAnnunDC != NULL ? hAnnunDC : hMainDC;

		// set mask bitmap to background
		PatBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, WHITENESS);

		if ((Chipset.b.IORam[BCONTRAST]&DON))
		{
			DWORD d;

			// scan complete display area
			for (d = BDISP_BEGIN; d <= BDISP_END; ++d)
			{
				// x-offset for digit, dp or cm pattern
				UINT nDigitOff = (UINT) (((d + 2) >> 2) - 1) * nLcdDistance;

				CONST BYTE *pbyLcdType = &byLcdTypeBert[d*4];

				BYTE byValue = Chipset.b.DspRam[d];
				_ASSERT((byValue & 0xF0) == 0);

				for (; byValue; byValue >>= 1)
				{
					BYTE bySegType = *pbyLcdType++;

					// segment connected and on
					if (bySegType && (byValue & 1) != 0)
					{
						UINT nOx = pAnnunciator[--bySegType].nOx;

						if (bySegType <= 8)	// digit, dp or cm
						{
							// offset to next display position
							nOx += nDigitOff;
						}

						BitBlt(hLcdDC,
							   nOx, pAnnunciator[bySegType].nOy,
							   pAnnunciator[bySegType].nCx, pAnnunciator[bySegType].nCy,
							   hSelAnnunDC,
							   pAnnunciator[bySegType].nDx, pAnnunciator[bySegType].nDy,
							   SRCAND);
					}
				}
			}
		}

		// mask segment mask with background and brush
		BitBlt(hLcdDC, 0, 0, nLcdXSize, nLcdYSize, hMainDC, nLcdX, nLcdY, ROP_PDSPxax);
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
	BYTE byDspOn;

	if (uLcdTimerId)						// LCD update timer running
		return;								// -> quit

	byDspOn = (nCurrentHardware == HDW_BERT) ? Chipset.b.IORam[BCONTRAST] : Chipset.IORam[DSPCTL];

	if (byDspOn & DON)						// display on?
	{
		_ASSERT(nCurrentHardware != HDW_UNKNOWN);

		if (nCurrentHardware == HDW_LEWIS)
		{
			_ASSERT(nCurrentHardware == HDW_LEWIS);
			UpdateAnnunciators(0x7F);		// switch on annunciators
		}
		else if (nCurrentHardware == HDW_SACA)
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
VOID UpdateAnnunciators(DWORD dwUpdateMask)
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
		if ((dwUpdateMask & 0x1) != 0)
		{
			nCount = 0;

			if (bAnnOn)						// annunciators on
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
		dwUpdateMask >>= 1;
	}
	_ASSERT(dwUpdateMask == 0);
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
