/*
 *   Usrprg32.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2006 Christoph Gieﬂelink
 *
 */
#include "pch.h"
#include "resource.h"
#include "Emu42.h"
#include "ops.h"

// HP32SII RAM entries
#define PGMEND		0x200E9
#define VAREND		0x200F2
#define PGMST		0x20100
#define IntgMem		0x202E8
#define SolveMem	0x203BD
#define EndURAM		0x20400

// HP32SII status register defines
#define sSlvIntg	0x00007

// HP32SII program codes
#define NUMBER		0x000					// code for number
#define XNEY		0x179					// code x<>y?
#define XLEY		0x17A					// code x<=y?
#define XLTY		0x17B					// code x<y?
#define XGTY		0x17C					// code x>y?
#define XGEY		0x17D					// code x>=y?
#define XEQY		0x17E					// code x=y?
#define XNE0		0x17F					// code x<>0?
#define XLE0		0x180					// code x<=0?
#define XLT0		0x181					// code x<0?
#define XGT0		0x182					// code x>0?
#define XGE0		0x183					// code x>=0?
#define XEQ0		0x184					// code x=0?
#define RTN			0x185					// code RTN
#define FS_0		0x5A1					// code FS? 0
#define FS_11		0x5AC					// code FS? 11
#define LBL_A		0x6A1					// code LaBeL A
#define LBL_Z		0x6BA					// code LaBeL Z
#define ISG_A		0x701					// code ISG A
#define ISG_Z		0x71A					// code ISG Z
#define ISG_I		0x71B					// code ISG i
#define ISG_(I)		0x71C					// code ISG(i)
#define DSE_A		0x721					// code DSE A
#define DSE_Z		0x73A					// code DSE Z
#define DSE_I		0x73B					// code DSE i
#define DSE_(I)		0x73C					// code DSE(i)

enum										// for LoopStatus variable in Catalog()
{
	FIND,									// looking for a LBL
	RETURN,									// stop decoding at next LBL
	DONE									// finished decoding LBL
};

// user code load/save
typedef struct
{
	TCHAR cLabel[32];
	DWORD dwStartAddr;
	DWORD dwEndAddr;
} CatLabel32;

// macro for checking bit in CPU status register
#define ST_BIT(b) ((Chipset.ST[b>>2] & (1<<(b&3))) != 0)

static CONST BYTE pbySignature[] = "RAW32";	// object signature

//################
//#
//#    Low level subroutines
//#
//################

static LPBYTE pbyBuffer = NULL;

static DWORD ReadBuffer3(DWORD d)
{
	return Npack(&pbyBuffer[d],3);
}

static DWORD Read3(DWORD d)
{
	BYTE p[3];

	Npeek(p,d,3);
	return Npack(p,3);
}

static VOID Write3(DWORD d, DWORD n)
{
	BYTE p[3];

	Nunpack(p,n,3);
	Nwrite(p,d,3);
	return;
}

//
// get last address
//
static __inline DWORD GetPgmEnd(VOID)
{
	DWORD dwAddr = PGMEND;
	return (dwAddr & 0xFF000) | Read3(dwAddr);
}

//
// =MEMCHK
//
static __inline BOOL MEMCHK(
	DWORD dwOffset)							// C[X], offset
{
	DWORD dwBound;

	if (ST_BIT(sSlvIntg))					// inside SOLVE
	{
		dwOffset = Read5(VAREND);
		dwOffset *= 4;
		dwBound = ((dwOffset & 0x100000) != 0) ? SolveMem : IntgMem;
	}
	else
	{
		dwOffset += Read3(VAREND);
		dwBound = EndURAM;
	}

	return (dwBound & 0xFFF) < (dwOffset & 0xFFF);
}

//
// =ALLOC
//
static __inline BOOL ALLOC(
	DWORD dwAddr,							// D0,   insert addr
	DWORD dwOffset)							// C[X], size
{
	BYTE  byVal;
	DWORD dwSource,dwDest;
	INT   i;

	if (MEMCHK(dwOffset)) return TRUE;	// check if there is enough room

	dwSource = ((VAREND & 0xFF000) | Read3(VAREND));
	dwDest = dwSource + dwOffset;		// new end

	// move bytes, works only with positive offsets!!
	for (i = dwSource - dwAddr; i > 0; --i)
	{
		--dwSource;
		--dwDest;
		Npeek(&byVal,dwSource,1);
		Nwrite(&byVal,dwDest,1);
	}
	return FALSE;
}

//
// =MEMADJ (restricted version)
//
static __inline VOID MEMADJ(
	DWORD dwAddr,							// C[X], insert addr
	DWORD dwOffset)							// B[X], size
{
	INT i;
	DWORD dwContent;

	DWORD dwPointer = VAREND;

	dwAddr &= 0xFFF;						// only last three nibble

	for (i = 0; i < 4; ++i)					// adjust 4 pointer
	{
		dwContent = Read3(dwPointer);

		if (dwContent == 0 || dwAddr <= dwContent)
		{
			Write3(dwPointer,dwContent + dwOffset);
		}

		dwPointer -= 3;
	}
	return;
}

static __inline DWORD GetProgLength(LPBYTE pbyProg,DWORD dwLastEnd)
{
	DWORD dwCurrAddr;
	INT   nCmdType;							// type of current command

	dwCurrAddr = 0;

	while (dwCurrAddr + 3 <= dwLastEnd)
	{
		nCmdType =  pbyProg[dwCurrAddr++];
		nCmdType |= pbyProg[dwCurrAddr++] << 4;
		nCmdType |= pbyProg[dwCurrAddr++] << 8;

		if (nCmdType == NUMBER)
		{
			dwCurrAddr += (3+12+1);			// skip exp., mant. and sign

			if (dwCurrAddr > dwLastEnd)		// ovl. decoding number
				return 0;
		}
	}
	
	return dwCurrAddr;
}

static VOID GetUsedLabels(
	DWORD  dwStartAddr,
	DWORD  dwEndAddr,
	DWORD  (*fnread)(DWORD dwAddr),
	LPTSTR lpszLabels)
{
	INT nCmdType;							// type of current command

	INT i = 0;

	while (dwStartAddr < dwEndAddr)
	{
		nCmdType = fnread(dwStartAddr);
		dwStartAddr += 3;

		if (nCmdType == NUMBER)
		{
			dwStartAddr += (3+12+1);		// skip exp., mant. and sign
			continue;
		}

		if (nCmdType >= LBL_A && nCmdType <= (LBL_A + _T('Z') - _T('A')))
		{
			lpszLabels[i++] = _T('A') + (nCmdType - LBL_A);
		}
	}
	lpszLabels[i++] = 0;					// EOS
	return;
}

//
// traverse catalog and return labels one at a time (reverse order)
// returns FALSE when there are no more
//
static BOOL Catalog(BOOL *pbFirst, CatLabel32 *pLbl)
{
	static DWORD dwCurrAddr;
	static DWORD dwLastEnd;					// save the last END addr

	INT  nCmdType;							// type of current command
	INT  nLoopStatus;						// state variable for loop

	INT  nLblPos = 0;						// position in LBL string
	BOOL bCondCode = FALSE;					// conditional jump code

	// first call, get .END. address
	if (*pbFirst)
	{
		dwCurrAddr = PGMST;
		dwLastEnd = GetPgmEnd();
		*pbFirst = FALSE;
	}

	pLbl->cLabel[nLblPos++] = _T('0');		// default label name for program at PRGM TOP
	pLbl->cLabel[nLblPos] = 0;				// no label (EOS)
	pLbl->dwStartAddr = dwCurrAddr;			// start address of program in memory

	nLoopStatus = FIND;						// state variable for loop
	while (nLoopStatus != DONE)
	{
		if (dwCurrAddr >= dwLastEnd) break;	// end of program memory

		nCmdType = Read3(dwCurrAddr);
		dwCurrAddr += 3;

		if (nCmdType == NUMBER)				// floating point number
		{
			dwCurrAddr += (3+12+1);			// skip exp., mant. and sign
		}

		// found label
		if (nCmdType >= LBL_A && nCmdType <= (LBL_A + _T('Z') - _T('A')))
		{
			if (nLoopStatus == FIND)		// label
			{
				// first label in context
				if (dwCurrAddr == pLbl->dwStartAddr + 3)
					nLblPos = 0;			// overwrite "0"

				pLbl->cLabel[nLblPos++] = _T('A') + (nCmdType - LBL_A);
				pLbl->cLabel[nLblPos] = 0;	// EOS
			}

			if (nLoopStatus == RETURN)		// label behind a RTN
			{
				dwCurrAddr -= 3;			// don't save this label
				nLoopStatus = DONE;			// finished decoding LBL
			}
		}

		if (!bCondCode && nCmdType == RTN)	// found a non conditional RTN
		{
			nLoopStatus = RETURN;			// RTN, leave on next label
		}

		// check for conditional jump code
		bCondCode = ((nCmdType >= XNEY)  && (nCmdType <= XEQ0))
				  | ((nCmdType >= FS_0)  && (nCmdType <= FS_11))
				  | ((nCmdType >= ISG_A) && (nCmdType <= ISG_(I)))
				  | ((nCmdType >= DSE_A) && (nCmdType <= DSE_(I)));
	}

	pLbl->dwEndAddr = dwCurrAddr;			// save actual end address
	return pLbl->dwStartAddr != pLbl->dwEndAddr;
}


//################
//#
//#    Writing User Code file to calculator
//#
//################

//
// load user code program
//
BOOL GetUserCode32(LPCTSTR szUCFile)
{
	BYTE   byBuffer[16];
	TCHAR  cCalLbl[32],cPrgLbl[32];
	HANDLE hFile;
	DWORD  dwRead;
	DWORD  dwFileSize;						// size of file
	DWORD  dwProgSize;						// size of program in nibbles
	DWORD  dwEndAddr;						// the .END. address
	INT    i;

	BOOL   bOK = FALSE;						// error detected

	hFile = CreateFile(szUCFile,
					   GENERIC_READ,
					   FILE_SHARE_READ,
					   NULL,
					   OPEN_EXISTING,
					   FILE_FLAG_SEQUENTIAL_SCAN,
					   NULL);

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	// read file signature
	_ASSERT(ARRAYSIZEOF(byBuffer) >= ARRAYSIZEOF(pbySignature));
	ReadFile(hFile,byBuffer,ARRAYSIZEOF(pbySignature)-1,&dwRead,NULL);
	if (   dwRead != ARRAYSIZEOF(pbySignature)-1
		|| memcmp(byBuffer,pbySignature,ARRAYSIZEOF(pbySignature)-1) != 0)
	{
		AbortMessage(_T("Detected wrong object file."));
		goto cancel;
	}

	dwFileSize = GetFileSize(hFile,NULL) - ARRAYSIZEOF(pbySignature) + 1;
	dwProgSize = dwFileSize * 2;			// program size

	pbyBuffer = (LPBYTE) malloc(dwProgSize);
	if (pbyBuffer == NULL) goto cancel;

	// load and unpack file content
	ReadFile(hFile,pbyBuffer,dwFileSize,&dwRead,NULL);
	_ASSERT(dwFileSize == dwRead);

	i = dwFileSize - 1;						// source start address
	dwFileSize = dwProgSize - 1;			// destination start address
	while (i >= 0)							// unpack source
	{
		BYTE byValue = pbyBuffer[i--];
		pbyBuffer[dwFileSize--] = byValue >> 4;
		pbyBuffer[dwFileSize--] = byValue & 0xF;
	}

	// get real object size, maybe we have half bytes
	dwProgSize = GetProgLength(pbyBuffer, dwProgSize);

	dwEndAddr = GetPgmEnd();				// last program address

	// get in use calculator labels
	GetUsedLabels(PGMST,dwEndAddr,Read3,cCalLbl);
	// get in use program labels
	GetUsedLabels(0,dwProgSize,ReadBuffer3,cPrgLbl);

	// scan for duplicate labels
	for (i = 0; cPrgLbl[i]; ++i)			// scan for each prog. label
	{
		if (_tcschr(cCalLbl,cPrgLbl[i]))	// found label in calculator
		{
			TCHAR strMsg[256];

			wsprintf(strMsg,_T("Loading aborted, program label conflict:\n")
							_T("\"%s\" %s in use in the calculator,\n")
							_T("\"%s\" %s in use in the object file."),
							cCalLbl,(cCalLbl[1] == 0) ? _T("is") : _T("are"),
							cPrgLbl,(cPrgLbl[1] == 0) ? _T("is") : _T("are"));
			AbortMessage(strMsg);
			goto cancel;
		}
	}

	if (ALLOC(dwEndAddr,dwProgSize))		// allocate memory at end of user program area
	{
		AbortMessage(_T("Not enough free memory."));
		goto cancel;
	}

	MEMADJ(dwEndAddr,dwProgSize);			// adjust memory pointers
	Nwrite(pbyBuffer,dwEndAddr,dwProgSize);	// write data

	bOK = TRUE;								// completed successfully

cancel:
	if (pbyBuffer)
	{
		free(pbyBuffer);
		pbyBuffer = NULL;
	}
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
	HANDLE hFile;
	LPBYTE pbyBuffer;
	DWORD  dwPos,dwNibSize,dwBytesWritten;
	INT    i,nItems;

	BOOL bOK = FALSE;

	nItems = (INT) SendMessage(hWnd,LB_GETCOUNT,0,0);
	if (nItems == 0) return FALSE;

	// create file and write out buffer
	hFile = CreateFile(szUCFile,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		AbortMessage(_T("Unable to create file."));
		return FALSE;
	}

	// calculate no. of nibbles for target buffer
	dwNibSize = 0;
	for (i = 0; i < nItems; ++i)			// scan all elements
	{
		// item selected
		if (SendMessage(hWnd,LB_GETSEL,i,0) > 0)
		{
			CatLabel32 *pLbl = (CatLabel32*) SendMessage(hWnd,LB_GETITEMDATA,i,0);
			dwNibSize += (pLbl->dwEndAddr - pLbl->dwStartAddr);
		}
	}

	// allocate user code buffer + 1 padding nibble
	if ((pbyBuffer = (LPBYTE) malloc(dwNibSize + 1)) != NULL)
	{
		pbyBuffer[dwNibSize] = 0;			// fill padding nibble

		// write the data into the target buffer
		dwPos = 0;							// target buffer position
		for (i = 0; i < nItems; ++i)		// scan all elements
		{
			// item selected
			if (SendMessage(hWnd,LB_GETSEL,i,0) > 0)
			{
				DWORD dwLength;

				CatLabel32 *pLbl = (CatLabel32*) SendMessage(hWnd,LB_GETITEMDATA,i,0);

				// data length of label
				dwLength = (pLbl->dwEndAddr - pLbl->dwStartAddr);

				// read the data
				Npeek(&pbyBuffer[dwPos],pLbl->dwStartAddr,dwLength);
				dwPos += dwLength;
			}
		}

		// pack target buffer
		for (dwPos = 0; dwPos < dwNibSize; dwPos += 2)
		{
			pbyBuffer[dwPos/2] = pbyBuffer[dwPos] | (pbyBuffer[dwPos+1] << 4);
		}

		// write file signature
		WriteFile(hFile,pbySignature,ARRAYSIZEOF(pbySignature)-1,&dwBytesWritten,NULL);
		_ASSERT(ARRAYSIZEOF(pbySignature)-1 == dwBytesWritten);

		// write data
		WriteFile(hFile,pbyBuffer,dwPos/2,&dwBytesWritten,NULL);
		_ASSERT(dwPos / 2 == dwBytesWritten);

		free(pbyBuffer);

		bOK = TRUE;
	}

	CloseHandle(hFile);
	return bOK;
}

//
// view and select user program
//
static BOOL CALLBACK SelectProgram(HWND hDlg, UINT message, DWORD wParam, LONG lParam)
{
	HWND       hWnd;
	TCHAR      szBuffer[40];
	BOOL       bFirst;
	CatLabel32 Lbl;
	INT        i,j;

	switch (message)
	{
	case WM_INITDIALOG:
		hWnd = GetDlgItem(hDlg,IDC_USERCODE_SELECT);
		bFirst = TRUE;
		while (Catalog(&bFirst, &Lbl))
		{
			// make a copy of the data
			CatLabel32 *pLbl = (CatLabel32 *) malloc(sizeof(*pLbl));
			if (pLbl == NULL) break;
			CopyMemory(pLbl,&Lbl,sizeof(*pLbl));

			_ASSERT(pLbl->cLabel[0] != 0);
			i = wsprintf(szBuffer,_T("LBL  \"%c\""),pLbl->cLabel[0]);
			if (pLbl->cLabel[1] != 0)		// multiple labels
				wsprintf(&szBuffer[i],_T("  (%s)"),&pLbl->cLabel[1]);
			i = (INT) SendMessage(hWnd,LB_ADDSTRING,0,(LPARAM) szBuffer);

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
				free((HLOCAL) SendMessage(hWnd,LB_GETITEMDATA,j,0));
			}
			EndDialog(hDlg,LOWORD(wParam));
			return TRUE;
		}
	}
	return FALSE;
	UNREFERENCED_PARAMETER(lParam);
}

BOOL OnSelectProgram32(VOID)
{
	if (DialogBox(hApp, MAKEINTRESOURCE(IDD_USERCODE), hWnd, (DLGPROC)SelectProgram) == -1)
		AbortMessage(_T("Select Program Dialog Box Creation Error!"));
	return 0;
}
