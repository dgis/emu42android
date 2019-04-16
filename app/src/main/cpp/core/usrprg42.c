/*
 *   Usrprg42.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gieﬂelink
 *   Copyright (C) HP41 user code parts by Warren Furlow and Leo Duran
 *
 */
#include "pch.h"
#include "resource.h"
#include "Emu42.h"
#include "rpl.h"

enum										// for decode UC byte
{
	FIRST,									// unknown previous
	SINGLE,									// single byte inst
	DOUBLE1,								// first byte of double byte inst
	DOUBLE2,								// 2 of 2
	TRIPLE1,								// 1 of 3
	TRIPLE2,								// 2 of 3
	TRIPLE3,								// 3 of 3
	TEXT,									// alpha string
	JUMP_TEXT,								// first byte of gto, xeq, w
	GLOBAL1,								// first byte of global
	GLOBAL2,								// second byte of global
	GLOBAL_LBL,								// third byte of label
	GLOBAL_END,								// third byte of end
	GLOBAL_KEYCODE,							// fourth byte of label
	LOCAL1,									// special case of DOUBLE for local jumps
	LOCAL2									// 2 of 2
};

enum										// for LoopStatus variable in Catalog()
{
	FIND,									// looking for a LBL
	DONE,									// finished decoding LBL
	NEXT									// go to next global
};

// user code load/save
typedef struct
{
	BYTE  byText[16];						// max. 7 chars + EOS
	DWORD dwStartAddr;
	DWORD dwEndAddr;
} CatLabel42;

//################
//#
//#    Low level subroutines
//#
//################

//
// get .END. address
//
static __inline DWORD GetEndAddr(VOID)
{
	DWORD dwAddr;

	dwAddr =  Read5(D_CONTEXT);
	dwAddr += Read5(dwAddr + 5) - 1;
	return dwAddr;
}

//
// =ROOM
//
static __inline DWORD GetROOM(VOID)
{
	return Read5(D_DSKTOP) - Read5(D_RSKTOP);
}

//
// =LINKADJ
//
static __inline VOID LINKADJ(DWORD dwAddr, DWORD dwSize)
{
	// dwAddr = RAM-WORD object
	DWORD dwSRRP, dwContSRRP;
	DWORD dwLRW,  dwPrevLRW;

	dwSRRP = Read5(D_USEROB);				// SYSRAMROMPAIR
	if (dwSRRP == dwAddr) return;			// RAM-WORD object = SYSRAMROMPAIR?
	dwSRRP += 8;							// last o-pair pointer
	dwContSRRP = Read5(dwSRRP);				// offset to LASTRAM-WORD
	if (dwContSRRP == 0) return;			// no offset
	dwLRW = dwSRRP + dwContSRRP;			// LASTRAM-WORD ?
	if (dwLRW < dwAddr) return;				// not in RAM-WORD object
	do
	{
		dwPrevLRW = dwLRW;					// RAM-WORD
		dwPrevLRW -= 5;						// PREVRAM-WORD link
		dwLRW = dwPrevLRW - Read5(dwPrevLRW); // next RAM-WORD
	}
	while (dwLRW >= dwAddr);				// wasn't last RAM-WORD
	Write5(dwPrevLRW,Read5(dwPrevLRW)+dwSize); // adjust LASTRAM-WORD pointer
	Write5(dwSRRP,Read5(dwSRRP)+dwSize);	// adjust last o-pair pointer
	return;
}

//
// =MOVEDSD
//
static __inline BOOL MOVEDSD(DWORD dwAddr, DWORD dwSize)
{
	BYTE  byBuffer[16];
	DWORD dwPointer, dwContent, dwBlockLength;
	INT   i;

	DWORD dwDSKTOP = Read5(D_DSKTOP);

	if (dwSize > GetROOM()) return FALSE;	// not enough room

	// PTRADJUST, adjust all pointers

	// DSKTOP (Update Data Stack)
	for (dwPointer = Read5(D_DSKTOP); (dwContent = Read5(dwPointer)) != 0; dwPointer += 5)
	{
		// CKBOUNDS
		if (dwContent >= dwDSKTOP && dwContent < dwAddr)
			Write5(dwPointer,dwContent-dwSize);
	}

	// TEMPENV (Update Temporary Environments)
	dwPointer = Read5(D_TEMPENV);
	while ((dwContent = Read5(dwPointer)) != 0)
	{
		DWORD dwVal;

		dwContent -= 10;
		dwPointer += 10;

		do
		{
			dwPointer += 5;
			dwVal = Read5(dwPointer);

			// CKBOUNDS
			if (dwVal >= dwDSKTOP && dwVal < dwAddr)
				Write5(dwPointer,dwVal-dwSize);

			dwPointer += 5;
			dwContent -= 10;
		}
		while (dwContent != 0);
	}

	// TEMPTOP (Update System Pointers)
	dwPointer = D_TEMPTOP;
	for (i = 0; i < 19; ++i)
	{
		DWORD dwContent = Read5(dwPointer);

		// CKBOUNDS
		if (dwContent >= dwDSKTOP && dwContent < dwAddr)
			Write5(dwPointer,dwContent-dwSize);

		dwPointer += 5;
	}

	// MOVEDOWN
	dwBlockLength = dwAddr - dwDSKTOP;
	while (dwBlockLength > 0)
	{
		DWORD dwLen = dwBlockLength;
		if (dwLen > ARRAYSIZEOF(byBuffer)) dwLen = ARRAYSIZEOF(byBuffer);

		// move data
		Npeek(byBuffer,dwDSKTOP,dwLen);
		Nwrite(byBuffer,dwDSKTOP-dwSize,dwLen);

		dwDSKTOP += dwLen;
		dwBlockLength -= dwLen;
	}

	// ADJMEM
	Write5(D_AVMEM,GetROOM() / 5);
	return TRUE;
}

//
// this decoder returns the type of the current byte
// based on it's value and the previous byte types
//
static INT DecodeUCByte(INT nPrevType, BYTE byCurrByte)
{
	static INT nTextCount = 0;

	INT nCurrType;

	switch (nPrevType)
	{
	case FIRST:
	case SINGLE:
		// 2 byte
		if (   (byCurrByte >= 0x90 && byCurrByte <= 0xB0)
			|| (byCurrByte >= 0xCE && byCurrByte <= 0xCF))
		{
			nCurrType = DOUBLE1;
			break;
		}

		// GTO 00 .. GTO 14 2 byte
		if (byCurrByte >= 0xB1 && byCurrByte <= 0xBF)
		{
			nCurrType = LOCAL1;
			break;
		}

		// GTO, XEQ 3 byte
		if (byCurrByte >= 0xD0 && byCurrByte <= 0xEF)
		{
			nCurrType = TRIPLE1;
			break;
		}

		// text
		if (byCurrByte >= 0xF0 && byCurrByte <= 0xFF)
		{
			nTextCount = byCurrByte & 0x0F;
			nCurrType = TEXT;
			break;
		}

		// GTO, XEQ, W text
		if (byCurrByte >= 0x1D && byCurrByte <= 0x1F)
		{
			nCurrType = JUMP_TEXT;
			break;
		}

		// global
		if (byCurrByte >= 0xC0 && byCurrByte <= 0xCD)
		{
			nCurrType = GLOBAL1;
			break;
		}

		nCurrType = SINGLE;
		break;
	case LOCAL1:
		nCurrType = LOCAL2;
		break;
	case DOUBLE1:
		nCurrType = DOUBLE2;
		break;
	case LOCAL2:
	case DOUBLE2:
		nCurrType = DecodeUCByte(FIRST, byCurrByte);
		break;
	case TRIPLE1:
		nCurrType = TRIPLE2;
		break;
	case TRIPLE2:
		nCurrType = TRIPLE3;
		break;
	case TRIPLE3:
		nCurrType = DecodeUCByte(FIRST, byCurrByte);
		break;
	case TEXT:
		if (nTextCount)
		{
			--nTextCount;
			nCurrType = TEXT;
		}
		else
		{
			nCurrType = DecodeUCByte(FIRST, byCurrByte);
		}
		break;
	case JUMP_TEXT:
		if (byCurrByte > 0xF0)
		{
			nTextCount = byCurrByte & 0x0F;
			nCurrType = TEXT;
		}
		else								// no text after
		{
			nCurrType = DecodeUCByte(FIRST, byCurrByte);
		}
		break;
	case GLOBAL1:
		nCurrType = GLOBAL2;
		break;
	case GLOBAL2:
		if (byCurrByte >= 0xF0)				// it's a LBL
		{
			nTextCount = byCurrByte & 0x0F;
			nCurrType = GLOBAL_LBL;
		}
		else								// it's the END
		{
			nCurrType = GLOBAL_END;
		}
		break;
	case GLOBAL_LBL:
		if (nTextCount)
		{
			--nTextCount;
			nCurrType = GLOBAL_KEYCODE;
		}
		else
		{
			nCurrType = DecodeUCByte(FIRST, byCurrByte);
		}
		break;
	case GLOBAL_END:
		nCurrType = DecodeUCByte(FIRST, byCurrByte);
		break;
	case GLOBAL_KEYCODE:
		nCurrType = DecodeUCByte(TEXT, byCurrByte);
		break;
	}
	return nCurrType;
}

//
// returns the relative offset in HP41 register/byte format
//
static VOID CalcOffset(DWORD dwActAddr,DWORD dwLowerAddr,BYTE *pbyRegOff,BYTE *pbyByteOff)
{
	if (dwLowerAddr == 0)					// there is no prev addr
	{
		*pbyRegOff = 0;
		*pbyByteOff = 0;
	}
	else
	{
		// distance in bytes
		dwActAddr = (dwActAddr - dwLowerAddr + 4) / 2;

		// split into register and byte
		*pbyByteOff = (BYTE) ((dwActAddr % 7) << 1);
		dwActAddr /= 7;
		_ASSERT(dwActAddr < 0x200);
		*pbyByteOff |= ((dwActAddr & 0x100) >> 8);
		_ASSERT(*pbyByteOff < 0x0E);
		*pbyRegOff  = (BYTE) (dwActAddr & 0xFF);
	}
}

//
// calculate position of the previous global
//
static DWORD PrevGlobal(DWORD dwCurrAddr)
{
	DWORD dwAddr,dwRegOff;

	// extract the offsets from the current global
	dwAddr   = Read2(dwCurrAddr);
	dwRegOff = Read2(dwCurrAddr + 2);

	dwRegOff += (dwAddr & 0x01) << 8;
	dwRegOff *= 14;
	dwRegOff += (dwAddr & 0x0E);

	// calc the absolute address of the prev global
	if (dwRegOff == 0)						// if there is no prev global
	{
		dwAddr = 0;
	}
	else
	{
		dwAddr = dwCurrAddr - dwRegOff + 4;
	}
	return dwAddr;
}

//
// check previous global object for GLOBAL_END,
// if object is GLOBAL_END return program address behind
// previous GLOBAL_END else address of previous object
//
static BOOL IsPrevGlobalEnd(DWORD dwCurrAddr, DWORD *pdwStartAddr)
{
	BOOL bGlobalEnd;
	BYTE p[6];

	// check if previous object is a GLOBAL_END 
	*pdwStartAddr = PrevGlobal(dwCurrAddr);	// get address of previous global
	Npeek(p,*pdwStartAddr,6);				// read 6 nibbles (3 bytes)
	bGlobalEnd = (   *pdwStartAddr == 0		// end of chain
				  || (   DecodeUCByte(FIRST,  (BYTE)(p[0]|(p[1]<<4))) == GLOBAL1
					  && DecodeUCByte(GLOBAL1,(BYTE)(p[2]|(p[3]<<4))) == GLOBAL2
					  && DecodeUCByte(GLOBAL2,(BYTE)(p[4]|(p[5]<<4))) == GLOBAL_END
					 )
				 );

	if (bGlobalEnd)							// current program begins directly after GLOBAL_END or at end of chain
	{
		*pdwStartAddr = (*pdwStartAddr != 0) ? (*pdwStartAddr + 6) : (Read5(D_CONTEXT) + 10);
	}
	return bGlobalEnd;
}

//
// traverse catalog and return labels one at a time (reverse order)
// returns FALSE when there are no more
//
static BOOL Catalog(BOOL *pbFirst, CatLabel42 *pLbl)
{
	static DWORD dwCurrAddr;
	static DWORD dwLastEnd;					// save the last END addr

	DWORD dwStartAddr;
	DWORD dwAddrIndex;						// register index for loop

	INT nByteType;							// type of current byte
	INT nLabelIndex;						// counts from 0..n for indexing text string
	INT nTextCount;							// counts from n..0 for counting text length
	INT nLoopStatus;						// state variable for loop

	// first call, get .END. address
	if (*pbFirst)
	{
		dwCurrAddr = GetEndAddr();
		*pbFirst = FALSE;
	}

	nLoopStatus = FIND;
	while (nLoopStatus != DONE)
	{
		if (dwCurrAddr == 0) return FALSE;	// no more labels

		// decode current instruction - should be a global LBL or END
		dwAddrIndex = dwCurrAddr;
		nByteType = FIRST;
		nLabelIndex = 0;
		nTextCount = 0;
		nLoopStatus = FIND;
		while (nLoopStatus == FIND)
		{
			BYTE byCurrByte = Read2(dwAddrIndex);
			nByteType = DecodeUCByte(nByteType,byCurrByte);
			switch (nByteType)
			{
			case GLOBAL1:
			case GLOBAL2:
				break;
			case GLOBAL_LBL:
				nTextCount = Read2(dwAddrIndex) & 0x0F;
				nLabelIndex = 0;
				break;
			case GLOBAL_END:
				nLoopStatus = NEXT;
				dwLastEnd = dwCurrAddr;

				// check if previous object is GLOBAL_END -> then we have no GLOBAL_LBL
				if (IsPrevGlobalEnd(dwCurrAddr,&dwStartAddr))
				{
					// between start and end address is data
					_ASSERT(dwStartAddr <= dwLastEnd);
					if (dwStartAddr != dwLastEnd)
					{
						// this code has no GLOBAL_LBL, choose END or .END. as text
						strcpy((LPSTR) pLbl->byText,((byCurrByte & 32) != 0) ? ".END." : "END");
						pLbl->dwStartAddr = dwStartAddr;
						pLbl->dwEndAddr = dwLastEnd;
						nLoopStatus = DONE;	// ready to return this value
					}
				}
				break;
			case GLOBAL_KEYCODE:
				break;
			case TEXT:
				if (nTextCount)
				{
					// enough room in text buffer
					if (nLabelIndex < ARRAYSIZEOF(pLbl->byText) - 1)
						pLbl->byText[nLabelIndex++] = Read2(dwAddrIndex);
					--nTextCount;
				}
				if (nTextCount == 1)		// got label name
				{
					nLoopStatus = NEXT;		// not first GLOBAL_LBL

					// check if previous object is GLOBAL_END -> then this is the first GLOBAL_LBL
					if (IsPrevGlobalEnd(dwCurrAddr,&dwStartAddr))
					{
						pLbl->byText[nLabelIndex] = 0; // null terminate
						pLbl->dwStartAddr = dwStartAddr;
						pLbl->dwEndAddr = dwLastEnd;
						nLoopStatus = DONE;	// ready to return this value
					}
				}
				break;
			default: // we have landed on something other than global so get out
				pLbl->byText[0] = 0;
				pLbl->dwStartAddr = 0;
				pLbl->dwEndAddr = 0;
				nLoopStatus = DONE;
				break;
			}

			dwAddrIndex += 2;				// next byte
		}

		// setup for next time around
		dwCurrAddr = PrevGlobal(dwCurrAddr);
	}
	return TRUE;
}


//################
//#
//#    Writing User Code file to calculator
//#
//################

//
// load user code program
//
BOOL GetUserCode42(LPCTSTR szUCFile)
{
	LPBYTE pbyBuffer = NULL;
	HANDLE hFile,hMap;
	DWORD  dwFileSize;						// size of file
	DWORD  dwProgSize;						// size of program in bytes
	DWORD  dwProgSizeNib;					// size of program in nibbles
	DWORD  dwEndAddr;						// the .END. address
	DWORD  dwAddr;							// address for loop
	DWORD  dwPrevGlobAddr;					// register where last global starts
	BYTE   byRegOff;						// register offset to previous global
	BYTE   byByteOff;						// byte offset to previous global
	BOOL   bNumberType, bPrevNumberType;	// state for adding zero byte behind numbers
	BOOL   bGlobEndFound;					// found one global end in stream
	INT    nByteType;						// type of current byte
	INT    i;

	BOOL   bOK = FALSE;						// error detected

	hFile = CreateFile(szUCFile,
					   GENERIC_READ,
					   FILE_SHARE_READ,
					   NULL,
					   OPEN_EXISTING,
					   FILE_ATTRIBUTE_NORMAL,
					   NULL);

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	dwFileSize = GetFileSize(hFile,NULL);

	hMap = CreateFileMapping(hFile, NULL, PAGE_WRITECOPY, 0, 0, NULL);
	if (hMap == NULL) goto cancel;

	pbyBuffer = (LPBYTE) MapViewOfFile(hMap, FILE_MAP_COPY, 0, 0, 0);
	if (pbyBuffer == NULL) goto cancel;

	// get exact program size
	// there can be junk after the END instruction and zero byte seperators can be missing after numbers
	bGlobEndFound = FALSE;
	bNumberType = FALSE;
	nByteType = FIRST;
	for (dwProgSize = dwProgSizeNib = 0; dwProgSize < dwFileSize; ++dwProgSize, ++dwProgSizeNib)
	{
		if (nByteType == GLOBAL_END)		// found global end
		{
			bGlobEndFound = TRUE;
			dwEndAddr = dwProgSize;			// save file position of GLOBAL_END
			dwAddr = dwProgSizeNib;			// save corresponding bytes to allocate
		}

		nByteType = DecodeUCByte(nByteType,pbyBuffer[dwProgSize]);

		if (nByteType == GLOBAL_LBL)		// global label found
			bOK = TRUE;

		// is current byte a number (0-9 [10-19], '.' [1A], 'E' [1B], '+/-' [1C])?
		bPrevNumberType = bNumberType;
		bNumberType = (nByteType == SINGLE && pbyBuffer[dwProgSize] >= 0x10 && pbyBuffer[dwProgSize] <= 0x1C);
		if (bPrevNumberType && !bNumberType) // not a number any more
			++dwProgSizeNib;				// numbers must follow a zero byte

		// single zero byte (NULL command)
		if (nByteType == SINGLE && pbyBuffer[dwProgSize] == 0x00)
			--dwProgSizeNib;				// -> PACK later
	}

	if (nByteType != GLOBAL_END)			// finished without GLOBAL_END
	{
		if (bGlobEndFound)					// but there was a GLOBAL_END earlier
		{
			dwProgSize = dwEndAddr;			// get saved file position of GLOBAL_END
			dwProgSizeNib = dwAddr;			// get saved corresponding bytes to allocate information
		}
	}

	if (!bOK)								// no GLOBAL_LBL found -> warning
	{
		if (IDYES == YesNoMessage(_T("Unkown User Code format, stop loading?")))
			goto cancel;
	}

	bOK = FALSE;							// preset error detected
	dwProgSizeNib *= 2;						// no. of nibbles for allocating

	dwEndAddr = GetEndAddr();				// address of .END.
	dwPrevGlobAddr = PrevGlobal(dwEndAddr);	// address of prior global before allocating memory

	if (dwProgSizeNib > GetROOM())			// not enough room
	{
		AbortMessage(_T("Not enough free memory."));
		goto cancel;
	}

	// allocate memory in RAM
	dwAddr = Read5(D_CONTEXT);
	LINKADJ(dwAddr,dwProgSizeNib);			// adjust RAM-WORD links
	dwAddr += 5;							// adjust RAM-WORD length
	Write5(dwAddr,Read5(dwAddr)+dwProgSizeNib);
	MOVEDSD(dwEndAddr,dwProgSizeNib);		// reserve memory before .END.
	_ASSERT(dwEndAddr == GetEndAddr());		// .END. address must be the same after moving

	// global object positions
	dwAddr = dwEndAddr - dwProgSizeNib;		// start address
	if (dwPrevGlobAddr)						// prior global available?
	{
		dwPrevGlobAddr -= dwProgSizeNib;	// that's the moved position of the prior global
	}

	// move program to RAM
	bNumberType = FALSE;
	nByteType = FIRST;
	for (i = 0; i < (INT) dwProgSize; ++i)
	{
		nByteType = DecodeUCByte(nByteType,pbyBuffer[i]);

		// fixup global chain as we go
		switch (nByteType)
		{
		case GLOBAL1: // if global, link it into previous global
			CalcOffset(dwAddr,dwPrevGlobAddr,&byRegOff,&byByteOff);
			dwPrevGlobAddr = dwAddr;
			pbyBuffer[i] = 0xC0 | byByteOff;
			break;
		case GLOBAL2:
			pbyBuffer[i] = byRegOff;
			break;
		case GLOBAL_END:
			pbyBuffer[i] = 0x09;			// this is 0x09 on the HP42S
			break;
		case GLOBAL_KEYCODE:				// clear any key assignment from global label
		case LOCAL2:						// or this is the short label offset
		case TRIPLE2:						// or this the long label offset
			pbyBuffer[i] = 0;
			break;
		case TRIPLE1:
			pbyBuffer[i] &= 0xF0;			// save opcode, delete part of offset
			break;
		case TRIPLE3:
			pbyBuffer[i] &= 0x7F;			// save label, delete part of offset
			break;
		}

		// is current byte a number (0-9 [10-19], '.' [1A], 'E' [1B], '+/-' [1C])?
		bPrevNumberType = bNumberType;
		bNumberType = (nByteType == SINGLE && pbyBuffer[i] >= 0x10 && pbyBuffer[i] <= 0x1C);
		if (bPrevNumberType && !bNumberType) // not a number any more
		{
			// write general a "end of number marker",
			// old markers are deleted by the packer
			Write2(dwAddr,0x00);			// end of number marker
			dwAddr += 2;					// go to next location
		}

		// remove single zero byte (NULL command) -> packing
		if (nByteType != SINGLE || pbyBuffer[i] != 0x00)
		{
			Write2(dwAddr,pbyBuffer[i]);	// copy byte
			dwAddr += 2;					// go to next location
		}
	}

	_ASSERT(dwAddr == dwEndAddr);			// written nibbles equal to allocated memory

	// recalculate .END.
	CalcOffset(dwEndAddr,dwPrevGlobAddr,&byRegOff,&byByteOff);
	Write2(dwEndAddr+0,(BYTE) (0xC0 | byByteOff));
	Write2(dwEndAddr+2,byRegOff);

	bOK = TRUE;								// completed successfully

cancel:
	if (pbyBuffer) UnmapViewOfFile(pbyBuffer);
	if (hMap)      CloseHandle(hMap);
	CloseHandle(hFile);
	return bOK;
}


//################
//#
//#    Writing User Code to file
//#
//################

//
// write user code program
//
static BOOL PutUserCode(LPCTSTR szUCFile,HWND hWnd)
{
	HANDLE hUCFile;
	LPBYTE pbyBuffer;
	DWORD  dwBytesWritten;
	DWORD  dwAddr;
	DWORD  dwProgSize;
	INT    nByteType;
	INT    i,nItems;

	BOOL bOK = TRUE;

	nItems = (INT) SendMessage(hWnd,LB_GETCOUNT,0,0);
	if (nItems == 0) return FALSE;

	// create file and write out buffer
	hUCFile = CreateFile(szUCFile,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if (hUCFile == INVALID_HANDLE_VALUE)
	{
		AbortMessage(_T("Unable to create file."));
		return FALSE;
	}

	for (i = nItems; --i >= 0;)				// scan all elements in reversed order
	{
		// item selected
		if (SendMessage(hWnd,LB_GETSEL,i,0) > 0)
		{
			CatLabel42 *pLbl = (CatLabel42*) SendMessage(hWnd,LB_GETITEMDATA,i,0);

			// allocate user code buffer
			if ((pbyBuffer = (LPBYTE) malloc((pLbl->dwEndAddr-pLbl->dwStartAddr)/2+3)) == NULL)
			{
				bOK = FALSE;
				goto error;
			}

			dwAddr = pLbl->dwStartAddr;

			// copy program to buffer
			nByteType = FIRST;
			for (dwProgSize = 0;dwAddr < pLbl->dwEndAddr; ++dwProgSize)
			{
				pbyBuffer[dwProgSize] = Read2(dwAddr); // copy byte

				nByteType = DecodeUCByte(nByteType,pbyBuffer[dwProgSize]);
				switch (nByteType)
				{
				case GLOBAL_KEYCODE:		// clear any key assignment from global label
					pbyBuffer[dwProgSize] = 0;
					break;
				}

				dwAddr += 2;				// next byte
			}

			// write an HP41 compatible GLOBAL END
			pbyBuffer[dwProgSize++] = 0xC0;
			pbyBuffer[dwProgSize++] = 0x00;
			pbyBuffer[dwProgSize++] = 0x0D;

			WriteFile(hUCFile,pbyBuffer,dwProgSize,&dwBytesWritten,NULL);
			free(pbyBuffer);

			if (dwProgSize != dwBytesWritten)
			{
				AbortMessage(_T("Error writing to file."));
				bOK = FALSE;
				goto error;
			}
		}
	}

error:
	CloseHandle(hUCFile);
	return bOK;
}

//
// view and select user program
//
static BOOL CALLBACK SelectProgram(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	HWND       hWnd;
	BOOL       bFirst;
	CatLabel42 Lbl;
	INT        i,j;

	switch (message)
	{
	case WM_INITDIALOG:
		hWnd = GetDlgItem(hDlg,IDC_USERCODE_SELECT);
		bFirst = TRUE;
		while (Catalog(&bFirst, &Lbl))
		{
			// make a copy of the data
			CatLabel42 *pLbl = (CatLabel42*) malloc(sizeof(*pLbl));
			if (pLbl == NULL) break;
			CopyMemory(pLbl,&Lbl,sizeof(*pLbl));

			#if defined _UNICODE
			{
				INT nLength = (INT) (strlen((LPCSTR) pLbl->byText) + 1) * 2;
				LPTSTR szTmp = (LPTSTR) malloc(nLength);
				if (szTmp == NULL) return TRUE; // allocation error
				MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (LPCSTR) pLbl->byText, -1, szTmp, nLength);
				i = (INT) SendMessage(hWnd,LB_ADDSTRING,0,(LPARAM) szTmp);
				free(szTmp);
			}
			#else
			{
				i = (INT) SendMessage(hWnd,LB_ADDSTRING,0,(LPARAM) pLbl->byText);
			}
			#endif

			// add catalog information to listbox item
			SendMessage(hWnd,LB_SETITEMDATA,i,(LPARAM) pLbl);
		}
		return TRUE;
	case WM_COMMAND:
		hWnd = GetDlgItem(hDlg,IDC_USERCODE_SELECT);
		switch(LOWORD(wParam))
		{
		case IDOK:
			// items selected
			if (SendMessage(hWnd,LB_GETSELCOUNT,0,0) > 0)
			{
				// get the user code data
				if (GetSaveObjectFilename(_T(RAW_FILTER),_T("RAW")))
				{
					PutUserCode(szBufferFilename,hWnd);
				}
			}
			// fall into IDCANCEL to clean up reserved memory
		case IDCANCEL:
			// clean up data
			i = (INT) SendMessage(hWnd,LB_GETCOUNT,0,0);
			for (j = 0; j < i; ++j)
			{
				free((LPVOID) SendMessage(hWnd,LB_GETITEMDATA,j,0));
			}
			EndDialog(hDlg,LOWORD(wParam));
			return TRUE;
		}
	}
	return FALSE;
	UNREFERENCED_PARAMETER(lParam);
}

BOOL OnSelectProgram42(VOID)
{
	if (DialogBox(hApp, MAKEINTRESOURCE(IDD_USERCODE), hWnd, (DLGPROC)SelectProgram) == -1)
		AbortMessage(_T("Select Program Dialog Box Creation Error!"));
	return 0;
}
