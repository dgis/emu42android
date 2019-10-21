/*
 *   stegano.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2012 Christoph Gieﬂelink
 *
 */
#include "pch.h"
#include "stegano.h"

#define BDAT				0x54414442		// stegano marker "BDAT"

#define WIDTHBYTES(bits) ((((bits) + 31) / 32) * 4)

typedef struct _BMPDIM
{
	LONG lLines;							// no. of lines
	LONG lColPerLine;						// colors per line (24 bit)
	LONG lBytesPerLine;						// bytes per line
	WORD biBitCount;						// bits per color
} BMPDIM, *PBMPDIM;

// CCITT-16 x^16+x^12+x^5+1 Polynom
static WORD xcrc(BYTE c, WORD wCrc)
{
	INT i;

	wCrc ^= (WORD) c << 8;

	for (i = 0; i < 8; ++i)
	{
		if (wCrc & 0x8000)
			wCrc = (wCrc << 1) ^ 0x1021;
		else
			wCrc <<= 1;
	}
	return wCrc;
}

static LONG StegBmpOff(CONST BMPDIM *pBmpDim, LONG lPos)
{
	LONG y = lPos / pBmpDim->lColPerLine;	// coordinate in bitmap
	LONG x = lPos % pBmpDim->lColPerLine;

	if (pBmpDim->biBitCount == 32)			// 32 bit bitmap
	{
		x += (x / 3);						// skip 4th byte
	}

	if (pBmpDim->lLines > 0)				// bottom up DIB
	{
		y = pBmpDim->lLines - y - 1;
	}
	return y * pBmpDim->lBytesPerLine + x;
}

#ifdef STG_COMPILE_DECODER
// stegano decoder
static VOID StegDataRead(CONST BMPDIM *pBmpDim, LPBYTE pbyBase, LONG *plPos, INT nSrcBitPerByte, LPBYTE pbyData, DWORD dwLength, INT nOutBitPerByte, WORD *pwCrc)
{
	DWORD dwPos = 0;

	BYTE bySrcMask = (1 << nSrcBitPerByte) - 1;
	BYTE byOutMask = (1 << nOutBitPerByte) - 1;

	_ASSERT(nOutBitPerByte == 8 || nOutBitPerByte == 4 || nOutBitPerByte == 2 || nOutBitPerByte == 1);

	while (dwPos < dwLength)
	{
		INT nBits;

		BYTE byVal = 0;

		for (nBits = 0; nBits < 8; nBits += nSrcBitPerByte)
		{
			byVal <<= nSrcBitPerByte;
			byVal |= (pbyBase[StegBmpOff(pBmpDim,(*plPos)++)] & bySrcMask);
		}

		if (pwCrc != NULL)					// update crc
		{
			*pwCrc = xcrc(byVal,*pwCrc);
		}
		if (pbyData != NULL)				// have target buffer
		{
			for (nBits = 0; nBits < 8; nBits += nOutBitPerByte)
			{
				pbyData[dwPos++] = byVal & byOutMask;
				byVal >>= nOutBitPerByte;
			}
		}
		else								// no target buffer
		{
			// update position counter
			dwPos += (8 / nOutBitPerByte);
		}
	}
	return;
}

enum STG_ERRCODE SteganoDecodeDib(LPBYTE *ppbyData, DWORD *pdwDataSize, INT nOutBitPerByte, LPBYTE byDib, DWORD dwDibSize)
{
	enum STG_ERRCODE eErr;

	PBITMAPINFOHEADER pbmiHeader;
	BMPDIM sBmpDim;
	LPBYTE pbyData;
	BYTE   byMask;
	WORD   wCrc;
	WORD   wLength;
	DWORD  dwAllSize;
	DWORD  dwMarker;
	DWORD  dwBmpSize;
	INT    nBitPerByte;
	LONG   lTargetPos;

	*ppbyData = NULL;						// no memory allocated

	_ASSERT(byDib != NULL);

	pbmiHeader = (PBITMAPINFOHEADER) byDib;

	// no true color bitmap
	if (pbmiHeader->biBitCount != 24 && pbmiHeader->biBitCount != 32)
	{
		eErr = STG_ERR_24COL;
		goto quit;
	}

	// bitmap dimension information
	sBmpDim.lLines = pbmiHeader->biHeight;
	sBmpDim.lColPerLine = 3 * pbmiHeader->biWidth;
	sBmpDim.lBytesPerLine = WIDTHBYTES(pbmiHeader->biWidth * pbmiHeader->biBitCount);
	sBmpDim.biBitCount = pbmiHeader->biBitCount;

	pbyData = byDib + sizeof(*pbmiHeader);	// bitmap data bytes
	if (pbmiHeader->biCompression == BI_BITFIELDS)
	{
		pbyData += 3 * sizeof(RGBQUAD);		// additional color masks
	}

	// check if bitmap dimension is smaller than file size
	if (dwDibSize < (DWORD) labs(sBmpDim.lLines * sBmpDim.lBytesPerLine))
	{
		eErr = STG_ERR_DATALENGTH;
		goto quit;							// illegal data length
	}

	lTargetPos = 0;

	// bitwidth info
	// bitwidth : XXX0 = 1
	// bitwidth : XX01 = 2
	// bitwidth : X011 = 3
	// bitwidth : 0111 = 4
	nBitPerByte = 1;
	byMask = pbyData[StegBmpOff(&sBmpDim,lTargetPos++)] & 0xF;

	// check position of 0 bit
	for (; (byMask & 0x1) != 0; byMask >>= 1)
	{
		++nBitPerByte;
	}

	if (nBitPerByte > 4)
	{
		eErr = STG_ERR_BITWIDTH;
		goto quit;							// illegal bitwidth info
	}

	// check if data length fit into bitmap data
	// no of color bytes
	dwBmpSize = sBmpDim.lColPerLine * labs(sBmpDim.lLines);

	// size for data bytes without bitwidth info field
	dwBmpSize = (dwBmpSize * nBitPerByte) / 8 - 1;

	wCrc = 0;								// reset CRC

	// stegano marker
	StegDataRead(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,(LPBYTE) &dwMarker,sizeof(dwMarker),8,&wCrc);
	if (dwMarker != BDAT)
	{
		eErr = STG_ERR_STGMARKER;
		goto quit;							// illegal stegano marker
	}

	// extra length field (unused)
	StegDataRead(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,(LPBYTE) &wLength,sizeof(wLength),8,&wCrc);

	dwAllSize = sizeof(dwMarker)			// check if extra data is inside BMP file
			  + sizeof(wLength) + wLength;

	if (dwBmpSize < dwAllSize)
	{
		eErr = STG_ERR_DATALENGTH;
		goto quit;							// illegal data length
	}

	// skip extra data
	StegDataRead(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,NULL,wLength,8,&wCrc);

	// data length
	StegDataRead(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,(LPBYTE) pdwDataSize,sizeof(*pdwDataSize),8,&wCrc);

	dwAllSize += sizeof(*pdwDataSize) + *pdwDataSize
			  +  sizeof(wCrc);

	if (dwBmpSize < dwAllSize)
	{
		eErr = STG_ERR_DATALENGTH;
		goto quit;							// illegal data length
	}

	*pdwDataSize *= 8 / nOutBitPerByte;		// new length bit/byte adjusted

	// allocate mem
	if ((*ppbyData = (LPBYTE) malloc(*pdwDataSize)) != NULL)
	{
		WORD wCrcData;

		// data
		StegDataRead(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,*ppbyData,*pdwDataSize,nOutBitPerByte,&wCrc);

		// CRC
		StegDataRead(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,(LPBYTE) &wCrcData,sizeof(wCrcData),8,NULL);

		if (wCrc != wCrcData)				// CRC error
		{
			eErr = STG_ERR_CRC;
			free(*ppbyData);				// free memory
			*ppbyData = NULL;
		}
		else
		{
			eErr = STG_NOERROR;
		}

		// check read data length from calculated one
		_ASSERT(lTargetPos == (LONG) dwAllSize * 8 / nBitPerByte + 1);
	}
	else
	{
		eErr = STG_ERR_MALLOC;
	}

quit:
	return eErr;
}

// stegano HBITMAP interface
#ifdef STG_COMPILE_HBITMAP
enum STG_ERRCODE SteganoDecodeHBm(LPBYTE *ppbyData, DWORD *pdwDataSize, INT nOutBitPerByte, HBITMAP hBmp)
{
	enum STG_ERRCODE eErr;

	LPBITMAPINFO bi;
	DWORD dwAlignedWidthBytes;
	DWORD dwDataOff;

	HDC hDC = GetDC(NULL);

	// allocate memory for extended image information incl. RGBQUAD color table
	bi = (LPBITMAPINFO) calloc(1,sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	bi->bmiHeader.biSize = sizeof(bi->bmiHeader);
	_ASSERT(bi->bmiHeader.biBitCount == 0); // for query without color table

	// get information about image
	GetDIBits(hDC,hBmp,0,0,NULL,bi,DIB_RGB_COLORS);

	// DWORD aligned bitmap width in BYTES
	dwAlignedWidthBytes = WIDTHBYTES(  bi->bmiHeader.biWidth
									 * bi->bmiHeader.biPlanes
									 * bi->bmiHeader.biBitCount);

	// biSizeImage is empty
	if (bi->bmiHeader.biSizeImage == 0 && bi->bmiHeader.biCompression == BI_RGB)
	{
		bi->bmiHeader.biSizeImage = dwAlignedWidthBytes * bi->bmiHeader.biHeight;
	}

	// no true color bitmap
	if (bi->bmiHeader.biBitCount != 24 && bi->bmiHeader.biBitCount != 32)
	{
		ReleaseDC(NULL,hDC);
		return STG_ERR_24COL;
	}

	dwDataOff = sizeof(bi->bmiHeader);		// begin of data
	if (bi->bmiHeader.biCompression == BI_BITFIELDS)
	{
		dwDataOff += 3 * sizeof(RGBQUAD);	// additional color masks
	}

	// realloc size for BITMAPINFO and bitmap data
	bi = (LPBITMAPINFO) realloc(bi,dwDataOff + bi->bmiHeader.biSizeImage);

	// fill bits buffer
	GetDIBits(hDC,hBmp,0,bi->bmiHeader.biHeight,(LPBYTE) bi + dwDataOff,bi,DIB_RGB_COLORS);

	// decode data in DIB
	eErr = SteganoDecodeDib(ppbyData, pdwDataSize, nOutBitPerByte,
		(LPBYTE) bi, dwDataOff + bi->bmiHeader.biSizeImage);

	ReleaseDC(NULL,hDC);					// release DC
	free(bi);								// free DIB image
	return eErr;
}
#endif // STG_COMPILE_HBITMAP

// stegano disk interface
#ifdef STG_COMPILE_DISK
enum STG_ERRCODE SteganoDecodeBmpFile(LPBYTE *ppbyData, DWORD *pdwDataSize, INT nOutBitPerByte, LPCTSTR szFilename)
{
	DWORD dwFileSize;

	HANDLE hFile = INVALID_HANDLE_VALUE;
	HANDLE hMap = NULL;
	LPBYTE pbyFile = NULL;

	enum STG_ERRCODE eErr = STG_ERR_FILE;

	*ppbyData = NULL;						// no memory allocated

	// open existing BMP file
	hFile = CreateFile(szFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		// current file size
		dwFileSize = GetFileSize(hFile, NULL);

		hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
		if (hMap != NULL)
		{
			pbyFile = (LPBYTE) MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
		}
	}

	if (pbyFile != NULL)					// file open
	{
		PBITMAPFILEHEADER pBmfh;

		// check if bitmap headers are complete inside file mapping
		if (dwFileSize < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
		{
			eErr = STG_ERR_DATALENGTH;
			goto quit;						// illegal data length
		}

		pBmfh = (PBITMAPFILEHEADER) pbyFile;
		if (pBmfh->bfType != 0x4D42)
		{
			eErr = STG_ERR_BMPHEAD;
			goto quit;						// "BM"
		}

		// decode data in DIB
		eErr = SteganoDecodeDib(ppbyData, pdwDataSize, nOutBitPerByte,
			pbyFile+sizeof(BITMAPFILEHEADER), dwFileSize-sizeof(BITMAPFILEHEADER));
	}

quit:
	if (pbyFile != NULL)               UnmapViewOfFile(pbyFile);
	if (hMap != NULL)                  CloseHandle(hMap);
	if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
	return eErr;
}
#endif // STG_COMPILE_DISK
#endif // STG_COMPILE_DECODER

#ifdef STG_COMPILE_ENCODER
// stegano encoder
static VOID StegDataWrite(CONST BMPDIM *pBmpDim, LPBYTE pbyBase, LONG *plPos, INT nBitPerByte, LPBYTE pbyData, DWORD dwLength, WORD *pwCrc)
{
	LPBYTE pbyBmpAddr;
	DWORD  dwPos;

	BYTE byMask = (1 << nBitPerByte) - 1;	// bitmask

	for (dwPos = 0; dwPos < dwLength; ++dwPos)
	{
		INT nBits;

		BYTE byVal = pbyData[dwPos];

		if (pwCrc != NULL)					// update crc
		{
			*pwCrc = xcrc(byVal,*pwCrc);
		}

		for (nBits = 8 - nBitPerByte; nBits >= 0; nBits -= nBitPerByte)
		{
			// address in bitmap
			pbyBmpAddr = pbyBase + StegBmpOff(pBmpDim,(*plPos)++);

			// patch value
			*pbyBmpAddr = (*pbyBmpAddr & ~byMask) | ((byVal >> nBits) & byMask);
		}
	}
	return;
}

enum STG_ERRCODE SteganoEncodeDib(LPBYTE byDib, DWORD dwDibSize, LPBYTE byData, DWORD dwDataSize)
{
	enum STG_ERRCODE eErr;

	PBITMAPINFOHEADER pbmiHeader;
	BMPDIM sBmpDim;
	LPBYTE pbyData;
	LPBYTE pbyBmpAddr;
	BYTE   byMask;
	WORD   wCrc;
	WORD   wLength;
	DWORD  dwAllSize;
	DWORD  dwMarker;
	DWORD  dwBmpSize;
	INT    nBitPerByte;
	LONG   lTargetPos;

	wLength = 0;							// no extra data

	_ASSERT(byDib != NULL);

	pbmiHeader = (PBITMAPINFOHEADER) byDib;

	// no true color bitmap
	if (pbmiHeader->biBitCount != 24 && pbmiHeader->biBitCount != 32)
	{
		eErr = STG_ERR_24COL;
		goto quit;
	}

	// bitmap dimension information
	sBmpDim.lLines = pbmiHeader->biHeight;
	sBmpDim.lColPerLine = 3 * pbmiHeader->biWidth;
	sBmpDim.lBytesPerLine = WIDTHBYTES(pbmiHeader->biWidth * pbmiHeader->biBitCount);
	sBmpDim.biBitCount = pbmiHeader->biBitCount;

	pbyData = byDib + sizeof(*pbmiHeader);	// bitmap data bytes
	if (pbmiHeader->biCompression == BI_BITFIELDS)
	{
		pbyData += 3 * sizeof(RGBQUAD);		// additional color masks
	}

	// check if bitmap dimension is smaller than DIB size
	if (dwDibSize < (DWORD) labs(sBmpDim.lLines * sBmpDim.lBytesPerLine))
	{
		eErr = STG_ERR_DATALENGTH;
		goto quit;							// illegal data length
	}

	// no of color bytes
	dwBmpSize = sBmpDim.lColPerLine * labs(sBmpDim.lLines);

	// calculate necessary bits in a byte
	dwAllSize = sizeof(dwMarker)
			  + sizeof(wLength)    + wLength
			  + sizeof(dwDataSize) + dwDataSize
			  + sizeof(wCrc);

	nBitPerByte = (dwAllSize * 8 + 1) / dwBmpSize + 1;

	if (nBitPerByte == 3) nBitPerByte = 4;

	if (nBitPerByte > 4)
	{
		eErr = STG_ERR_DATALENGTH;
		goto quit;							// data too large
	}

	lTargetPos = 0;

	// bitwidth info
	// bitwidth : 0    = 1
	// bitwidth : 01   = 2
	// bitwidth : 011  = 3
	// bitwidth : 0111 = 4
	pbyBmpAddr = pbyData + StegBmpOff(&sBmpDim,lTargetPos++);
	byMask = (1 << nBitPerByte) - 1;		// bitmask
	*pbyBmpAddr = (*pbyBmpAddr & ~byMask) | (byMask >> 1);

	wCrc = 0;								// reset CRC

	// stegano marker
	dwMarker = BDAT;
	StegDataWrite(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,(LPBYTE) &dwMarker,sizeof(dwMarker),&wCrc);

	// extra length field (unused)
	StegDataWrite(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,(LPBYTE) &wLength,sizeof(wLength),&wCrc);

	// data length
	StegDataWrite(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,(LPBYTE) &dwDataSize,sizeof(dwDataSize),&wCrc);

	// data
	StegDataWrite(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,byData,dwDataSize,&wCrc);

	// CRC
	StegDataWrite(&sBmpDim,pbyData,&lTargetPos,nBitPerByte,(LPBYTE) &wCrc,sizeof(wCrc),NULL);

	// check written data length from calculated one
	_ASSERT(lTargetPos == (LONG) dwAllSize * 8 / nBitPerByte + 1);

	eErr = STG_NOERROR;

quit:
	return eErr;
}

// stegano disk interface
#ifdef STG_COMPILE_DISK
enum STG_ERRCODE SteganoEncodeBmpFile(LPCTSTR szFilename, LPBYTE byData, DWORD dwDataSize)
{
	DWORD dwFileSize;

	HANDLE hFile = INVALID_HANDLE_VALUE;
	HANDLE hMap = NULL;
	LPBYTE pbyFile = NULL;

	enum STG_ERRCODE eErr = STG_ERR_FILE;

	// open existing BMP file
	hFile = CreateFile(szFilename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		// current file size
		dwFileSize = GetFileSize(hFile, NULL);

		hMap = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 0, NULL);
		if (hMap != NULL)
		{
			pbyFile = (LPBYTE) MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		}
	}

	if (pbyFile != NULL)					// file open
	{
		PBITMAPFILEHEADER pBmfh;

		// check if bitmap headers are complete inside file mapping
		if (dwFileSize < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER))
		{
			eErr = STG_ERR_DATALENGTH;
			goto quit;						// illegal data length
		}

		pBmfh = (PBITMAPFILEHEADER) pbyFile;
		if (pBmfh->bfType != 0x4D42)
		{
			eErr = STG_ERR_BMPHEAD;
			goto quit;						// "BM"
		}

		// encode data in DIB
		eErr = SteganoEncodeDib(pbyFile+sizeof(BITMAPFILEHEADER), dwFileSize-sizeof(BITMAPFILEHEADER),
			byData, dwDataSize);
	}

quit:
	if (pbyFile != NULL)               UnmapViewOfFile(pbyFile);
	if (hMap != NULL)                  CloseHandle(hMap);
	if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
	return eErr;
}
#endif // STG_COMPILE_DISK
#endif // STG_COMPILE_ENCODER
