/*
 *   stegano.h
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2012 Christoph Gieﬂelink
 *
 */

#ifndef STEGANO_H
#define STEGANO_H

// stegano decoder
#ifndef STG_NO_COMPILE_DECODER
#define STG_COMPILE_DECODER
#endif
// stegano encoder
#ifndef STG_NO_COMPILE_ENCODER
#define STG_COMPILE_ENCODER
#endif
// stegano HBITMAP interface
#ifndef STG_NO_COMPILE_HBITMAP
#define STG_COMPILE_HBITMAP
#endif
// stegano disk interface
#ifndef STG_NO_COMPILE_DISK
#define STG_COMPILE_DISK
#endif

enum STG_ERRCODE
{
	STG_NOERROR = 0,
	STG_ERR_FILE,
	STG_ERR_BMPHEAD,
	STG_ERR_24COL,
	STG_ERR_BITWIDTH,
	STG_ERR_STGMARKER,
	STG_ERR_DATALENGTH,
	STG_ERR_MALLOC,
	STG_ERR_CRC
};

#ifdef STG_COMPILE_DECODER
	// stegano decoder
	enum STG_ERRCODE SteganoDecodeDib(LPBYTE *ppbyData, DWORD *pdwDataSize, INT nOutBitPerByte, LPBYTE byDib, DWORD dwDibSize);

	#ifdef STG_COMPILE_HBITMAP
		// stegano HBITMAP interface
		enum STG_ERRCODE SteganoDecodeHBm(LPBYTE *ppbyData, DWORD *pdwDataSize, INT nOutBitPerByte, HBITMAP hBmp);
	#endif // STG_COMPILE_HBITMAP

	#ifdef STG_COMPILE_DISK
		// stegano disk interface
		enum STG_ERRCODE SteganoDecodeBmpFile(LPBYTE *ppbyData, DWORD *pdwDataSize, INT nOutBitPerByte, LPCTSTR szFilename);
	#endif // STG_COMPILE_DISK
#endif // STG_COMPILE_DECODER

#ifdef STG_COMPILE_ENCODER
	// stegano encoder
	enum STG_ERRCODE SteganoEncodeDib(LPBYTE byDib, DWORD dwDibSize, LPBYTE byData, DWORD dwDataSize);

	// stegano disk interface
	#ifdef STG_COMPILE_DISK
		enum STG_ERRCODE SteganoEncodeBmpFile(LPCTSTR szFilename, LPBYTE byData, DWORD dwDataSize);
	#endif // STG_COMPILE_DISK
#endif // STG_COMPILE_ENCODER

#endif // STEGANO_H
