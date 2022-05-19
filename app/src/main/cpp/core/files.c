/*
 *   files.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gießelink
 *
 */
#include "pch.h"
#include "Emu42.h"
#include "ops.h"
#include "io.h"								// I/O register definitions
#include "rpl.h"
#include "kml.h"
#include "debugger.h"
#include "stegano.h"
#include "lodepng.h"

#pragma intrinsic(abs,labs)

TCHAR  szEmuDirectory[MAX_PATH];
TCHAR  szCurrentDirectory[MAX_PATH];
TCHAR  szCurrentKml[MAX_PATH];
TCHAR  szBackupKml[MAX_PATH];
TCHAR  szCurrentFilename[MAX_PATH];
TCHAR  szBackupFilename[MAX_PATH];
TCHAR  szBufferFilename[MAX_PATH];

BOOL   bDocumentAvail = FALSE;				// document not available

LPBYTE mNCE2 = NULL;						// pointer to NCE2 master memory
LPBYTE mNCE3 = NULL;						// pointer to NCE3 master memory
LPBYTE sNCE2 = NULL;						// pointer to NCE2 slave  memory
LPBYTE sNCE3 = NULL;						// pointer to NCE3 slave  memory

LPBYTE pbyRom = NULL;
DWORD  dwRomSize = 0;
WORD   wRomCrc = 0;							// fingerprint of patched ROM
UINT   nCurrentHardware = HDW_UNKNOWN;		// Hardware -> emulator platform
BYTE   cCurrentRomType = 0;					// Model -> hardware
UINT   nCurrentClass = 0;					// Class -> derivate

// document signatures
static BYTE pbySignature[] = "Emu42 Document\xFE";
static HANDLE hCurrentFile = NULL;

static CHIPSET_M BackupChipset;
static CHIPSET_S BackupChipsetS;
static LPBYTE    Backup_mNCE2;
static LPBYTE    Backup_mNCE3;
static LPBYTE    Backup_sNCE2;
static LPBYTE    Backup_sNCE3;

BOOL   bBackup = FALSE;

//################
//#
//#    Window Position Tools
//#
//################

VOID SetWindowLocation(HWND hWnd,INT nPosX,INT nPosY)
{
	WINDOWPLACEMENT wndpl;
	RECT *pRc = &wndpl.rcNormalPosition;

	wndpl.length = sizeof(wndpl);
	GetWindowPlacement(hWnd,&wndpl);
	pRc->right = pRc->right - pRc->left + nPosX;
	pRc->bottom = pRc->bottom - pRc->top + nPosY;
	pRc->left = nPosX;
	pRc->top = nPosY;
	SetWindowPlacement(hWnd,&wndpl);
	return;
}



//################
//#
//#    Filename Title Helper Tool
//#
//################

DWORD GetCutPathName(LPCTSTR szFileName, LPTSTR szBuffer, DWORD dwBufferLength, INT nCutLength)
{
	TCHAR  cPath[_MAX_PATH];				// full file name
	TCHAR  cDrive[_MAX_DRIVE];
	TCHAR  cDir[_MAX_DIR];
	TCHAR  cFname[_MAX_FNAME];
	TCHAR  cExt[_MAX_EXT];

	_ASSERT(nCutLength >= 0);				// 0 = only drive and name

	// split original file name into parts
	_tsplitpath(szFileName,cDrive,cDir,cFname,cExt);

	if (*cDir != 0)							// contain directory part
	{
		LPTSTR lpFilePart;					// address of file name in path
		INT    nNameLen,nPathLen,nMaxPathLen;

		GetFullPathName(szFileName,ARRAYSIZEOF(cPath),cPath,&lpFilePart);
		_tsplitpath(cPath,cDrive,cDir,cFname,cExt);

		// calculate size of drive/name and path
		nNameLen = lstrlen(cDrive) + lstrlen(cFname) + lstrlen(cExt);
		nPathLen = lstrlen(cDir);

		// maximum length for path
		nMaxPathLen = nCutLength - nNameLen;

		if (nPathLen > nMaxPathLen)			// have to cut path
		{
			TCHAR cDirTemp[_MAX_DIR] = _T("");
			LPTSTR szPtr;

			// UNC name
			if (cDir[0] == _T('\\') && cDir[1] == _T('\\'))
			{
				// skip server
				if ((szPtr = _tcschr(cDir + 2,_T('\\'))) != NULL)
				{
					// skip share
					if ((szPtr = _tcschr(szPtr + 1,_T('\\'))) != NULL)
					{
						INT nLength = (INT) (szPtr - cDir);

						*szPtr = 0;			// set EOS behind share

						// enough room for \\server\\share and "\...\"
						if (nLength + 5 <= nMaxPathLen)
						{
							lstrcpyn(cDirTemp,cDir,ARRAYSIZEOF(cDirTemp));
							nMaxPathLen -= nLength;
						}

					}
				}
			}

			lstrcat(cDirTemp,_T("\\..."));
			nMaxPathLen -= 5;				// need 6 chars for additional "\..." + "\"
			if (nMaxPathLen < 0) nMaxPathLen = 0;

			// get earliest possible '\' character
			szPtr = &cDir[nPathLen - nMaxPathLen];
			szPtr = _tcschr(szPtr,_T('\\'));
			// not found
			if (szPtr == NULL) szPtr = _T("");

			lstrcat(cDirTemp,szPtr);		// copy path with preample to dir buffer
			lstrcpyn(cDir,cDirTemp,ARRAYSIZEOF(cDir));
		}
	}

	_tmakepath(cPath,cDrive,cDir,cFname,cExt);
	lstrcpyn(szBuffer,cPath,dwBufferLength);
	return lstrlen(szBuffer);
}

VOID SetWindowPathTitle(LPCTSTR szFileName)
{
	TCHAR cPath[MAX_PATH];
	RECT  rectClient;

	if (*szFileName != 0)					// set new title
	{
		_ASSERT(hWnd != NULL);
		VERIFY(GetClientRect(hWnd,&rectClient));
		GetCutPathName(szFileName,cPath,ARRAYSIZEOF(cPath),rectClient.right/11);
		SetWindowTitle(cPath);
	}
	return;
}



//################
//#
//#    BEEP Patch check
//#
//################

BOOL CheckForBeepPatch(VOID)
{
	typedef struct beeppatch
	{
		const DWORD dwAddress;				// patch address
		const BYTE  byPattern[4];			// patch pattern
	} BEEPPATCH, *PBEEPPATCH;

	// known beep patches
	const BEEPPATCH s17[]   = { { 0x02194, { 0x8, 0x1, 0xB, 0x0 } } };
	const BEEPPATCH s17_2[] = { { 0x02284, { 0x8, 0x1, 0xB, 0x0 } } };
	const BEEPPATCH s19_2[] = { { 0x01F46, { 0x8, 0x1, 0xB, 0x0 } } };
	const BEEPPATCH s27[]   = { { 0x0224A, { 0x8, 0x1, 0xB, 0x0 } } };
	const BEEPPATCH s28[]   = { { 0x22AED, { 0x8, 0x1, 0xB, 0x1 } } };

	const BEEPPATCH *psData;
	UINT nDataItems;
	BOOL bMatch;

	switch (cCurrentRomType)
	{
	case 'M':								// HP27S
		psData = s27;
		nDataItems = ARRAYSIZEOF(s27);
		break;
	case 'O':								// HP28S
		psData = s28;
		nDataItems = ARRAYSIZEOF(s28);
		break;
	case 'T':								// HP17B
		psData = s17;
		nDataItems = ARRAYSIZEOF(s17);
		break;
	case 'U':								// HP17BII
		psData = s17_2;
		nDataItems = ARRAYSIZEOF(s17_2);
		break;
	case 'Y':								// HP19BII
		psData = s19_2;
		nDataItems = ARRAYSIZEOF(s19_2);
		break;
	default:
		psData = NULL;
		nDataItems = 0;
	}

	// check if one data set match
	for (bMatch = FALSE; !bMatch && nDataItems > 0; --nDataItems)
	{
		_ASSERT(pbyRom != NULL && psData != NULL);

		// pattern matching?
		bMatch =  (psData->dwAddress + ARRAYSIZEOF(psData->byPattern) < dwRomSize)
			   && (memcmp(&pbyRom[psData->dwAddress],psData->byPattern,ARRAYSIZEOF(psData->byPattern))) == 0;
		++psData;							// next data set
	}
	return bMatch;
}



//################
//#
//#    Patch
//#
//################

// Sacajawea CRC Pass 0
#define _a0	0x1C							// Start Address
#define _d0	(_n0*16)						// Address offset
#define _n0	0x155							// Reads/Half-Sector
#define _s0	3								// #Sectors (Sector Size=2*d)

// Sacajawea CRC Pass 1
#define _a1	0x0								// Start Address
#define _d1	(_n1*16)						// Address offset
#define _n1	1								// #Reads/Half-Sector
#define _s1	0x400							// #Sectors (Sector Size=2*d)

// Lewis CRC Pass 0
#define a0	0x1C							// Start Address
#define d0	(n0*16)							// Address offset
#define n0	0x555							// Reads/Half-Sector
#define s0	3								// #Sectors (Sector Size=2*d)

// Lewis CRC Pass 1
#define a1	0x0								// Start Address
#define d1	(n1*16)							// Address offset
#define n1	1								// #Reads/Half-Sector
#define s1	0x1000							// #Sectors (Sector Size=2*d)

// rebuild of the calculator =CHECKSUM function for the Bert chip ROM
static WORD ChecksumChk(LPBYTE pbyROM, DWORD dwChkAddr)
{
	DWORD i;

	WORD wChk = 0;							// reset checksum

	for (i = 0; i < dwChkAddr; ++i)
	{
		// checksum calculation
		wChk <<= 4;
		wChk += pbyROM[i];
		wChk += ((wChk >> 8)  & 0xF) + 1;
		wChk += ((wChk >> 12) & 0xF) + 1;
	}
	return wChk;
}

// rebuild of the calculator =CHECKSUM function for the Sacajawea and the Lewis chip ROM
static WORD Checksum(LPBYTE pbyROM, DWORD dwStart, DWORD dwOffset, INT nReads, INT nSector)
{
	int i,j;

	WORD wCrc = 0;

	for (;nSector > 0; --nSector)			// evaluate each sector
	{
		DWORD dwAddr1 = dwStart;
		DWORD dwAddr2 = dwStart + dwOffset;

		for (i = 0; i < nReads; ++i)		// no. of reads in sector
		{
			for (j = 0; j < 16; ++j) wCrc = UpCRC(wCrc,pbyROM[dwAddr1++]);
			for (j = 0; j < 16; ++j) wCrc = UpCRC(wCrc,pbyROM[dwAddr2++]);
		}

		dwStart += 2 * dwOffset;			// next start page
	}
	return wCrc;
}

static VOID CorrectCrc(BYTE *a, WORD wCrc)
{
	INT s;

	wCrc = ~wCrc;							// correction word to 0xFFFF

	// unpack and write CRC
	for (s = 3; s >= 0; --s) { a[s] = (BYTE)(wCrc&0xf); wCrc>>=4; }
}

static VOID RebuildRomCrc(VOID)
{
	if (dwRomSize == _KB(10))				// Bert chip
	{
		// patch Bert chip
		Nunpack(&pbyRom[0x4FFC],ChecksumChk(pbyRom,0x4FFC),4);
	}
	if (dwRomSize == _KB(16))				// Sacajawea chip
	{
		// patch Sacajawea chip
		ZeroMemory(&pbyRom[0x7FF8],8);		// delete old Crc's
		CorrectCrc(&pbyRom[0x7FF8],Checksum(pbyRom,_a0,_d0,_n0,_s0));
		CorrectCrc(&pbyRom[0x7FFC],Checksum(pbyRom,_a1,_d1,_n1,_s1));
	}
	if (dwRomSize >= _KB(64))				// Lewis chip
	{
		// patch master Lewis chip
		ZeroMemory(&pbyRom[0x1FFF8],8);		// delete old Crc's
		CorrectCrc(&pbyRom[0x1FFF8],Checksum(pbyRom,a0,d0,n0,s0));
		CorrectCrc(&pbyRom[0x1FFFC],Checksum(pbyRom,a1,d1,n1,s1));
	}
	if (dwRomSize == _KB(96))				// with external ROM
	{
		ZeroMemory(&pbyRom[0x2FFFC],4);		// delete old Crc
		CorrectCrc(&pbyRom[0x2FFFC],Checksum(pbyRom,a1+_KB(64),d1,n1,s1/2));
	}
	if (dwRomSize == _KB(128))				// with slave Lewis chip
	{
		ZeroMemory(&pbyRom[0x3FFF8],8);		// delete old Crc's
		CorrectCrc(&pbyRom[0x3FFF8],Checksum(pbyRom,a0+_KB(64),d0,n0,s0));
		CorrectCrc(&pbyRom[0x3FFFC],Checksum(pbyRom,a1+_KB(64),d1,n1,s1));
	}
	return;
}

static __inline BYTE Asc2Nib(BYTE c)
{
	if (c<'0') return 0;
	if (c<='9') return c-'0';
	if (c<'A') return 0;
	if (c<='F') return c-'A'+10;
	if (c<'a') return 0;
	if (c<='f') return c-'a'+10;
	return 0;
}

BOOL PatchRom(LPCTSTR szFilename)
{
	HANDLE hFile = NULL;
	DWORD  dwFileSizeLow = 0;
	DWORD  dwFileSizeHigh = 0;
	DWORD  lBytesRead = 0;
	PSZ    lpStop,lpBuf = NULL;
	DWORD  dwAddress = 0;
	UINT   nPos = 0;
	BOOL   bPatched = FALSE;
	BOOL   bSucc = TRUE;

	if (pbyRom == NULL) return FALSE;
	SetCurrentDirectory(szEmuDirectory);
	hFile = CreateFile(szFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	SetCurrentDirectory(szCurrentDirectory);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;
	dwFileSizeLow = GetFileSize(hFile, &dwFileSizeHigh);
	if (dwFileSizeHigh != 0 || dwFileSizeLow == 0)
	{ // file is too large or empty
		CloseHandle(hFile);
		return FALSE;
	}
	lpBuf = (PSZ) malloc(dwFileSizeLow+1);
	if (lpBuf == NULL)
	{
		CloseHandle(hFile);
		return FALSE;
	}
	ReadFile(hFile, lpBuf, dwFileSizeLow, &lBytesRead, NULL);
	CloseHandle(hFile);
	lpBuf[dwFileSizeLow] = 0;
	nPos = 0;
	while (lpBuf[nPos])
	{
		// skip whitespace characters
		nPos += (UINT) strspn(&lpBuf[nPos]," \t\n\r");

		if (lpBuf[nPos] == ';')				// comment?
		{
			do
			{
				nPos++;
				if (lpBuf[nPos] == '\n')
				{
					nPos++;
					break;
				}
			} while (lpBuf[nPos]);
			continue;
		}
		dwAddress = strtoul(&lpBuf[nPos], &lpStop, 16);
		nPos = (UINT) (lpStop - lpBuf);		// position of lpStop

		if (*lpStop != 0)					// data behind address
		{
			if (*lpStop != ':')				// invalid syntax
			{
				// skip to end of line
				while (lpBuf[nPos] != '\n' && lpBuf[nPos] != 0)
				{
					++nPos;
				}
				bSucc = FALSE;
				continue;
			}

			while (lpBuf[++nPos])
			{
				if (isxdigit(lpBuf[nPos]) == FALSE) break;
				if (dwAddress < dwRomSize)	// patch ROM
				{
					pbyRom[dwAddress] = Asc2Nib(lpBuf[nPos]);
					bPatched = TRUE;
				}
				++dwAddress;
			}
		}
	}
	_ASSERT(nPos <= dwFileSizeLow);			// buffer overflow?
	free(lpBuf);
	if (bPatched)							// ROM has been patched
	{
		RebuildRomCrc();					// rebuild the ROM CRC's
	}
	return bSucc;
}



//################
//#
//#    ROM
//#
//################

// lodepng allocators
void* lodepng_malloc(size_t size)
{
  return malloc(size);
}

void* lodepng_realloc(void* ptr, size_t new_size)
{
  return realloc(ptr, new_size);
}

void lodepng_free(void* ptr)
{
  free(ptr);
}

BOOL CrcRom(WORD *pwChk)					// calculate fingerprint of ROM
{
	DWORD *pdwData,dwSize;
	DWORD dwChk = 0;

	if (pbyRom == NULL) return TRUE;		// ROM CRC isn't available

	_ASSERT(pbyRom);						// view on ROM
	pdwData = (DWORD *) pbyRom;

	_ASSERT((dwRomSize % sizeof(*pdwData)) == 0);
	dwSize = dwRomSize / sizeof(*pdwData);	// file size in DWORD's

	// use checksum, because it's faster
	while (dwSize-- > 0)
	{
		DWORD dwData = *pdwData++;
		if ((dwData & 0xF0F0F0F0) != 0)		// data packed?
			return FALSE;
		dwChk += dwData;
	}

	*pwChk = (WORD) ((dwChk >> 16) + (dwChk & 0xFFFF));
	return TRUE;
}

BOOL MapRom(LPCTSTR szFilename)
{
	HANDLE hRomFile;
	DWORD  dwSize,dwFileSize,dwRead;

	if (pbyRom != NULL) return FALSE;
	SetCurrentDirectory(szEmuDirectory);
	hRomFile = CreateFile(szFilename,
						  GENERIC_READ,
						  FILE_SHARE_READ,
						  NULL,
						  OPEN_EXISTING,
						  FILE_FLAG_SEQUENTIAL_SCAN,
						  NULL);
	SetCurrentDirectory(szCurrentDirectory);
	if (hRomFile == INVALID_HANDLE_VALUE) return FALSE;
	dwFileSize = GetFileSize(hRomFile, NULL);

	// read the first 4 bytes
	ReadFile(hRomFile,&dwSize,sizeof(dwSize),&dwRead,NULL);
	if (dwRead < sizeof(dwSize))
	{ // file is too small.
		CloseHandle(hRomFile);
		hRomFile = NULL;
		dwRomSize = 0;
		return FALSE;
	}

	#if defined ROM_BITMAP
	if ((dwSize & 0xFFFF) == 0x4D42)		// check for bmp file header "BM"
	{
		struct tagBMPDIM
		{
			LONG biWidth;
			LONG biHeight;
		} sDim;

		// read bitmap dimension
		SetFilePointer(hRomFile,
					   sizeof(BITMAPFILEHEADER) + (size_t) &(((PBITMAPINFOHEADER) 0)->biWidth),
					   NULL,
					   FILE_BEGIN);
		ReadFile(hRomFile,&sDim,sizeof(sDim),&dwRead,NULL);

		// calculate data size (don't care about DWORD adjust of biWidth)
		_ASSERT((sDim.biWidth % sizeof(DWORD)) == 0);
		dwFileSize = sDim.biWidth * sDim.biHeight;

		// goto bitmap data position
		SetFilePointer(hRomFile,-(LONG)dwFileSize,NULL,FILE_END);

		// read the first 4 bytes of bitmap data
		ReadFile(hRomFile,&dwSize,sizeof(dwSize),&dwRead,NULL);
	}
	#endif

	dwRomSize = dwFileSize;					// calculate ROM image buffer size
	if ((dwSize & 0xF0F0F0F0) != 0)			// packed ROM image ->
		dwRomSize *= 2;						// unpacked ROM image has double size

	pbyRom = (LPBYTE) malloc(dwRomSize);
	if (pbyRom == NULL)
	{
		CloseHandle(hRomFile);
		dwRomSize = 0;
		return FALSE;
	}

	*(DWORD *) pbyRom = dwSize;				// save first 4 bytes

	// load rest of file content
	ReadFile(hRomFile,&pbyRom[sizeof(dwSize)],dwFileSize - sizeof(dwSize),&dwRead,NULL);
	_ASSERT(dwFileSize - sizeof(dwSize) == dwRead);
	CloseHandle(hRomFile);

	if (dwRomSize != dwFileSize)			// packed ROM image
	{
		dwSize = dwRomSize;					// destination start address
		while (dwFileSize > 0)				// unpack source
		{
			BYTE byValue = pbyRom[--dwFileSize];
			_ASSERT(dwSize >= 2);
			pbyRom[--dwSize] = byValue >> 4;
			pbyRom[--dwSize] = byValue & 0xF;
		}
	}
	return TRUE;
}

BOOL MapRomBmp(HBITMAP hBmp)
{
	// look for an integrated ROM image
	BOOL bBitmapROM = SteganoDecodeHBm(&pbyRom,&dwRomSize,8,hBmp) == STG_NOERROR;

	if (bBitmapROM)							// has data inside
	{
		DWORD dwSrc,dwDest;
		unsigned uError;

		LPBYTE pbyOutData = NULL;
		size_t nOutData = 0;

		// try to decompress data
		uError = lodepng_zlib_decompress(&pbyOutData,&nOutData,pbyRom,dwRomSize,
										 &lodepng_default_decompress_settings);

		if (uError == 0)					// data decompression successful
		{
			free(pbyRom);					// free compressed data
			pbyRom = pbyOutData;			// use decompressed instead
			dwRomSize = (DWORD) nOutData;
		}

		dwSrc = dwRomSize;					// source start address

		dwRomSize *= 2;						// unpacked ROM image has double size
		pbyRom = (LPBYTE) realloc(pbyRom,dwRomSize);

		dwDest = dwRomSize;					// destination start address
		while (dwSrc > 0)					// unpack source
		{
			BYTE byValue = pbyRom[--dwSrc];
			_ASSERT(dwDest >= 2);
			pbyRom[--dwDest] = byValue >> 4;
			pbyRom[--dwDest] = byValue & 0xF;
		}
	}
	return bBitmapROM;
}

VOID UnmapRom(VOID)
{
	if (pbyRom == NULL) return;
	free(pbyRom);
	pbyRom = NULL;
	dwRomSize = 0;
	wRomCrc = 0;
	return;
}



//################
//#
//#    Documents
//#
//################

static BOOL IsDataPacked(VOID *pMem, DWORD dwSize)
{
	DWORD *pdwMem = (DWORD *) pMem;

	_ASSERT((dwSize % sizeof(DWORD)) == 0);
	if ((dwSize % sizeof(DWORD)) != 0) return TRUE;

	for (dwSize /= sizeof(DWORD); dwSize-- > 0;)
	{
		if ((*pdwMem++ & 0xF0F0F0F0) != 0)
			return TRUE;
	}
	return FALSE;
}

VOID ResetDocument(VOID)
{
	DisableDebugger();
	if (szCurrentKml[0])
	{
		KillKML();
	}
	if (hCurrentFile)
	{
		CloseHandle(hCurrentFile);
		hCurrentFile = NULL;
	}
	szCurrentKml[0] = 0;
	szCurrentFilename[0]=0;

	if (mNCE2) { free(mNCE2); mNCE2 = NULL; }
	if (mNCE3) { free(mNCE3); mNCE3 = NULL; }
	if (sNCE2) { free(sNCE2); sNCE2 = NULL; };
	if (sNCE3) { free(sNCE3); sNCE3 = NULL; };
	ZeroMemory(&Chipset,sizeof(Chipset));
	ZeroMemory(&ChipsetS,sizeof(ChipsetS));
	ZeroMemory(&RMap,sizeof(RMap));			// delete MMU mappings
	ZeroMemory(&WMap,sizeof(WMap));
	bDocumentAvail = FALSE;					// document not available
	return;
}

BOOL NewDocument(VOID)
{
	SaveBackup();
	ResetDocument();

	if (!DisplayChooseKml(0)) goto restore;
	if (!InitKML(szCurrentKml,FALSE)) goto restore;
	Chipset.type = cCurrentRomType;
	CrcRom(&Chipset.wRomCrc);				// save fingerprint of loaded ROM

	if (isModelBert(Chipset.type))			// Pioneer series base on Bert chip
	{
		// MMU mappings default settings
		Chipset.RegBase  = 0x00000;			// MMU Registers (16 bits)
		Chipset.RegMask  = 0x00000;			// n-island mask
		Chipset.DispBase = 0x00000;			// MMU Display/Timer (11 bits)
		Chipset.DispMask = 0x00000;			// n-island mask
		Chipset.NCE2Base = 0x00000;			// MMU NCE2 Memory (11 bits)
		Chipset.NCE2Mask = 0x00000;			// n-island mask
		Chipset.NCE3Base = 0x00000;			// MMU NCE3 Memory (11 bits)
		Chipset.NCE3Mask = 0x00000;			// n-island mask
		Chipset.RomBase  = 0x00000;			// MMU ROM (6 bits)
		Chipset.RomMask  = 0x08000;			// n-island mask

		Chipset.RomSize  = _KB(10);			// size of mask ROM

		Chipset.b.IORam[BSYSTEM_CTRL] = RST; // set ReSeT bit at power up
	}

	if (isModelSaca(Chipset.type))			// Pioneer series base on Sacajawea chip
	{
		// MMU mappings default settings
		Chipset.RegBase  = 0x24300;			// MMU Registers (16 bits)
		Chipset.RegMask  = 0xFFFF0;			// n-island mask
		Chipset.DispBase = 0x24000;			// MMU Display/Timer (10 bits)
		Chipset.DispMask = 0xFFC00;			// n-island mask
		Chipset.NCE2Base = 0x20000;			// MMU NCE2 Memory (10 bits)
		Chipset.NCE2Mask = 0xFFC00;			// n-island mask
		Chipset.NCE3Base = 0x00000;			// MMU NCE3 Memory (10 bits)
		Chipset.NCE3Mask = 0x00000;			// n-island mask
		Chipset.RomBase  = 0x00000;			// MMU ROM (6 bits)
		Chipset.RomMask  = 0xF8000;			// n-island mask

		Chipset.RomSize  = _KB(16);			// size of mask ROM

		Chipset.NCE2Ram  = TRUE;			// RAM
		Chipset.NCE2Size = _B(512);			// 512 byte, integrated DRAM

		Chipset.IORam[LPE] = RST;			// set ReSeT bit at power up
	}

	if (isModelLewis(Chipset.type))			// Pioneer and Clamshell series base on Lewis chip
	{
		// MMU mappings default settings for Pioneer series
		Chipset.RegBase  = 0x40300;			// MMU Registers (16 bits)
		Chipset.RegMask  = 0xFFFF0;			// n-island mask
		Chipset.DispBase = 0x40000;			// MMU Display/Timer (10 bits)
		Chipset.DispMask = 0xFFC00;			// n-island mask
		Chipset.NCE2Base = 0x50000;			// MMU NCE2 Memory (10 bits)
		Chipset.NCE2Mask = 0xF0000;			// n-island mask
		Chipset.NCE3Base = 0x20000;			// MMU NCE3 Memory (10 bits)
		Chipset.NCE3Mask = 0xE0000;			// n-island mask
		Chipset.RomBase  = 0x00000;			// MMU ROM (6 bits)
		Chipset.RomMask  = 0xE0000;			// n-island mask

		Chipset.RomSize  = _KB(64);			// size of mask ROM

		Chipset.NCE3Ram  = FALSE;			// ROM
		Chipset.NCE3Size = _KB(32);			// size of extra ROM (17B/17BII International)

		Chipset.NCE2Ram  = TRUE;			// RAM
		// physical DRAM chip (default 8KB or size of KML Class argument)
		Chipset.NCE2Size = _KB((nCurrentClass == 0) ? 8 : nCurrentClass);

		Chipset.IORam[RATECTL] = 0x7;		// CPU speed (mask-programmed)
		Chipset.IORam[LPE] = RST;			// set ReSeT bit at power up

		if (Chipset.type == 'O')			// Orlando, HP28S
		{
			Chipset.bSlave = TRUE;

			Chipset.RegBase   = 0xFFF00;	// MMU Registers (16 bits)
			Chipset.RegMask   = 0xFFFF0;	// n-island mask
			Chipset.DispBase  = 0xFFC00;	// MMU Display/Timer (10 bits)
			Chipset.DispMask  = 0xFFC00;	// n-island mask
			Chipset.NCE2Base  = 0xC0000;	// MMU NCE2 Memory (10 bits)
			Chipset.NCE2Mask  = 0xE0000;	// n-island mask
			Chipset.NCE3Base  = 0x80000;	// MMU NCE3 Memory (10 bits)
			Chipset.NCE3Mask  = 0xC0000;	// n-island mask
			Chipset.RomBase   = 0x00000;	// MMU ROM (6 bits)
			Chipset.RomMask   = 0xE0000;	// n-island mask

			ChipsetS.RegBase  = 0xFFB00;	// MMU Registers (16 bits)
			ChipsetS.RegMask  = 0xFFFF0;	// n-island mask
			ChipsetS.DispBase = 0xFF800;	// MMU Display/Timer (10 bits)
			ChipsetS.DispMask = 0xFFC00;	// n-island mask
			ChipsetS.NCE2Base = 0x00000;	// MMU NCE2 Memory (10 bits)
			ChipsetS.NCE2Mask = 0x00000;	// n-island mask (unused)
			ChipsetS.NCE3Base = 0x00000;	// MMU NCE3 Memory (10 bits)
			ChipsetS.NCE3Mask = 0x00000;	// n-island mask (unused)
			ChipsetS.RomBase  = 0x20000;	// MMU ROM (6 bits)
			ChipsetS.RomMask  = 0xE0000;	// n-island mask

			Chipset.RomSize   = _KB(64);	// size of master mask ROM
			ChipsetS.RomSize  = _KB(64);	// size of slave  mask ROM

			Chipset.NCE2Ram   = TRUE;		// RAM
			Chipset.NCE2Size  = _KB(32);	// physical DRAM chip

			Chipset.NCE3Ram   = FALSE;		// open data bus
			Chipset.NCE3Size  = 0;

			ChipsetS.IORam[RATECTL] = 0x7;	// CPU speed (mask-programmed)
			ChipsetS.IORam[LPE] = RST;		// set ReSeT bit at power up
		}

		if (Chipset.type == 'Y')			// Tycoon II, HP19BII
		{
			Chipset.bSlave = TRUE;

			Chipset.RegBase   = 0xFFF00;	// MMU Registers (16 bits)
			Chipset.RegMask   = 0xFFFF0;	// n-island mask
			Chipset.DispBase  = 0xFFC00;	// MMU Display/Timer (10 bits)
			Chipset.DispMask  = 0xFFC00;	// n-island mask
			Chipset.NCE2Base  = 0xC4000;	// MMU NCE2 Memory (10 bits)
			Chipset.NCE2Mask  = 0xFC000;	// n-island mask
			Chipset.NCE3Base  = 0x00000;	// MMU NCE3 Memory (10 bits)
			Chipset.NCE3Mask  = 0x00000;	// n-island mask (unused)
			Chipset.RomBase   = 0x00000;	// MMU ROM (6 bits)
			Chipset.RomMask   = 0xE0000;	// n-island mask

			ChipsetS.RegBase  = 0xFFB00;	// MMU Registers (16 bits)
			ChipsetS.RegMask  = 0xFFFF0;	// n-island mask
			ChipsetS.DispBase = 0xFF800;	// MMU Display/Timer (10 bits)
			ChipsetS.DispMask = 0xFFC00;	// n-island mask
			ChipsetS.NCE2Base = 0x00000;	// MMU NCE2 Memory (10 bits)
			ChipsetS.NCE2Mask = 0x00000;	// n-island mask (unused)
			ChipsetS.NCE3Base = 0x00000;	// MMU NCE3 Memory (10 bits)
			ChipsetS.NCE3Mask = 0x00000;	// n-island mask (unused)
			ChipsetS.RomBase  = 0x20000;	// MMU ROM (6 bits)
			ChipsetS.RomMask  = 0xE0000;	// n-island mask

			Chipset.RomSize   = _KB(64);	// size of master mask ROM
			ChipsetS.RomSize  = _KB(64);	// size of slave  mask ROM

			Chipset.NCE2Ram   = TRUE;		// RAM
			Chipset.NCE2Size  = _KB(8);		// physical DRAM chip

			Chipset.NCE3Ram   = FALSE;		// unused
			Chipset.NCE3Size  = 0;

			ChipsetS.IORam[RATECTL] = 0x7;	// CPU speed (mask-programmed)
			ChipsetS.IORam[LPE] = RST;		// set ReSeT bit at power up
		}
	}

	// check MMU controller boundary adjustments
	_ASSERT(Chipset.RegBase   == (Chipset.RegBase   & Chipset.RegMask));
	_ASSERT(Chipset.DispBase  == (Chipset.DispBase  & Chipset.DispMask));
	_ASSERT(Chipset.NCE2Base  == (Chipset.NCE2Base  & Chipset.NCE2Mask));
	_ASSERT(Chipset.NCE3Base  == (Chipset.NCE3Base  & Chipset.NCE3Mask));
	_ASSERT(Chipset.RomBase   == (Chipset.RomBase   & Chipset.RomMask));
	_ASSERT(ChipsetS.RegBase  == (ChipsetS.RegBase  & ChipsetS.RegMask));
	_ASSERT(ChipsetS.DispBase == (ChipsetS.DispBase & ChipsetS.DispMask));
	_ASSERT(ChipsetS.NCE2Base == (ChipsetS.NCE2Base & ChipsetS.NCE2Mask));
	_ASSERT(ChipsetS.NCE3Base == (ChipsetS.NCE3Base & ChipsetS.NCE3Mask));
	_ASSERT(ChipsetS.RomBase  == (ChipsetS.RomBase  & ChipsetS.RomMask));

	// allocate port memory
	if (Chipset.NCE2Ram)
	{
		mNCE2 = (LPBYTE) calloc(Chipset.NCE2Size,sizeof(*mNCE2));
		_ASSERT(mNCE2 != NULL);
	}
	if (Chipset.NCE3Ram)
	{
		mNCE3 = (LPBYTE) calloc(Chipset.NCE3Size,sizeof(*mNCE3));
		_ASSERT(mNCE3 != NULL);
	}
	if (ChipsetS.NCE2Ram)
	{
		sNCE2 = (LPBYTE) calloc(ChipsetS.NCE2Size,sizeof(*sNCE2));
		_ASSERT(sNCE2 != NULL);
	}
	if (ChipsetS.NCE3Ram)
	{
		sNCE3 = (LPBYTE) calloc(ChipsetS.NCE3Size,sizeof(*sNCE3));
		_ASSERT(sNCE3 != NULL);
	}
	LoadBreakpointList(NULL);				// clear debugger breakpoint list
	bDocumentAvail = TRUE;					// document available
	return TRUE;
restore:
	RestoreBackup();
	ResetBackup();
	return FALSE;
}

BOOL OpenDocument(LPCTSTR szFilename)
{
	#define CHECKAREA(s,e) (offsetof(CHIPSET_M,e)-offsetof(CHIPSET_M,s)+sizeof(((CHIPSET_M *)NULL)->e))

	HANDLE  hFile = INVALID_HANDLE_VALUE;
	DWORD   lBytesRead,lSizeofChipset;
	BYTE    pbyFileSignature[sizeof(pbySignature)];
	UINT    ctBytesCompared;
	UINT    nLength;

	// Open file
	if (lstrcmpi(szCurrentFilename,szFilename) == 0)
	{
		if (YesNoMessage(_T("Do you want to reload this document?")) == IDNO)
			return TRUE;
	}

	SaveBackup();
	ResetDocument();

	hFile = CreateFile(szFilename, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		AbortMessage(_T("This file is missing or already loaded in another instance of Emu42."));
		goto restore;
	}

	// Read and Compare Emu42 1.0 format signature
	ReadFile(hFile, pbyFileSignature, sizeof(pbyFileSignature), &lBytesRead, NULL);
	for (ctBytesCompared=0; ctBytesCompared<sizeof(pbyFileSignature); ctBytesCompared++)
	{
		if (pbyFileSignature[ctBytesCompared]!=pbySignature[ctBytesCompared])
		{
			AbortMessage(_T("This file is not a valid Emu42 document."));
			goto restore;
		}
	}

	// read length of KML script name
	ReadFile(hFile,&nLength,sizeof(nLength),&lBytesRead,NULL);

	// KML script name too long for file buffer
	if (nLength >= ARRAYSIZEOF(szCurrentKml))
	{
		// skip heading KML script name characters until remainder fits into file buffer
		UINT nSkip = nLength - (ARRAYSIZEOF(szCurrentKml) - 1);
		SetFilePointer(hFile, nSkip, NULL, FILE_CURRENT);

		nLength = ARRAYSIZEOF(szCurrentKml) - 1;
	}
	#if defined _UNICODE
	{
		LPSTR szTmp = (LPSTR) malloc(nLength);
		if (szTmp == NULL)
		{
			AbortMessage(_T("Memory Allocation Failure."));
			goto restore;
		}
		ReadFile(hFile, szTmp, nLength, &lBytesRead, NULL);
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szTmp, lBytesRead,
							szCurrentKml, ARRAYSIZEOF(szCurrentKml));
		free(szTmp);
	}
	#else
	{
		ReadFile(hFile, szCurrentKml, nLength, &lBytesRead, NULL);
	}
	#endif
	if (nLength != lBytesRead) goto read_err;
	szCurrentKml[nLength] = 0;

	// read chipset size inside file
	ReadFile(hFile, &lSizeofChipset, sizeof(lSizeofChipset), &lBytesRead, NULL);
	if (lBytesRead != sizeof(lSizeofChipset)) goto read_err;
	if (lSizeofChipset <= sizeof(Chipset))	// actual or older chipset version
	{
		// read chipset content
		ZeroMemory(&Chipset,sizeof(Chipset)); // init chipset
		ReadFile(hFile, &Chipset, lSizeofChipset, &lBytesRead, NULL);
	}
	else									// newer chipset version
	{
		// read my used chipset content
		ReadFile(hFile, &Chipset, sizeof(Chipset), &lBytesRead, NULL);

		// skip rest of chipset
		SetFilePointer(hFile, lSizeofChipset-sizeof(Chipset), NULL, FILE_CURRENT);
		lSizeofChipset = sizeof(Chipset);
	}
	if (lBytesRead != lSizeofChipset) goto read_err;

	if (!isModelValid(Chipset.type))		// check for valid model in emulator state file
	{
		AbortMessage(_T("Emulator state file with invalid calculator model."));
		goto restore;
	}

	SetWindowLocation(hWnd,Chipset.nPosX,Chipset.nPosY);

	while (TRUE)
	{
		if (szCurrentKml[0])				// KML file name
		{
			BOOL bOK;

			bOK = InitKML(szCurrentKml,FALSE);
			bOK = bOK && (cCurrentRomType == Chipset.type);
			if (bOK) break;

			KillKML();
		}
		if (!DisplayChooseKml(Chipset.type))
			goto restore;
	}
	// reload old button state
	ReloadButtons(Chipset.Keyboard_Row,ARRAYSIZEOF(Chipset.Keyboard_Row));

	if (Chipset.bSlave)						// 2nd slave controller
	{
		// read slave chipset size inside file
		ReadFile(hFile, &lSizeofChipset, sizeof(lSizeofChipset), &lBytesRead, NULL);
		if (lBytesRead != sizeof(lSizeofChipset)) goto read_err;

		if (lSizeofChipset <= sizeof(ChipsetS))	// actual or older chipset version
		{
			// read chipset content
			ZeroMemory(&ChipsetS,sizeof(ChipsetS)); // init chipset
			ReadFile(hFile, &ChipsetS, lSizeofChipset, &lBytesRead, NULL);
		}
		else								// newer chipset version
		{
			// read my used chipset content
			ReadFile(hFile, &ChipsetS, sizeof(ChipsetS), &lBytesRead, NULL);

			// skip rest of chipset
			SetFilePointer(hFile, lSizeofChipset-sizeof(ChipsetS), NULL, FILE_CURRENT);
			lSizeofChipset = sizeof(ChipsetS);
		}
		if (lBytesRead != lSizeofChipset) goto read_err;
	}

	if (Chipset.NCE2Ram)					// RAM at NCE2 master
	{
		mNCE2 = (LPBYTE) malloc(Chipset.NCE2Size);
		if (mNCE2 == NULL)
		{
			AbortMessage(_T("Memory Allocation Failure."));
			goto restore;
		}

		ReadFile(hFile, mNCE2, Chipset.NCE2Size, &lBytesRead, NULL);
		if (lBytesRead != Chipset.NCE2Size) goto read_err;

		if (IsDataPacked(mNCE2,Chipset.NCE2Size)) goto read_err;
	}
	if (Chipset.NCE3Ram)					// RAM at NCE3 master
	{
		mNCE3 = (LPBYTE) malloc(Chipset.NCE3Size);
		if (mNCE3 == NULL)
		{
			AbortMessage(_T("Memory Allocation Failure."));
			goto restore;
		}

		ReadFile(hFile, mNCE3, Chipset.NCE3Size, &lBytesRead, NULL);
		if (lBytesRead != Chipset.NCE3Size) goto read_err;

		if (IsDataPacked(mNCE3,Chipset.NCE3Size)) goto read_err;
	}
	if (ChipsetS.NCE2Ram)					// RAM at NCE2 slave
	{
		sNCE2 = (LPBYTE) malloc(ChipsetS.NCE2Size);
		if (sNCE2 == NULL)
		{
			AbortMessage(_T("Memory Allocation Failure."));
			goto restore;
		}

		ReadFile(hFile, sNCE2, ChipsetS.NCE2Size, &lBytesRead, NULL);
		if (lBytesRead != ChipsetS.NCE2Size) goto read_err;

		if (IsDataPacked(sNCE2,ChipsetS.NCE2Size)) goto read_err;
	}
	if (ChipsetS.NCE3Ram)					// RAM at NCE3 slave
	{
		sNCE3 = (LPBYTE) malloc(ChipsetS.NCE3Size);
		if (sNCE3 == NULL)
		{
			AbortMessage(_T("Memory Allocation Failure."));
			goto restore;
		}

		ReadFile(hFile, sNCE3, ChipsetS.NCE3Size, &lBytesRead, NULL);
		if (lBytesRead != ChipsetS.NCE3Size) goto read_err;

		if (IsDataPacked(sNCE3,ChipsetS.NCE3Size)) goto read_err;
	}

	if (Chipset.wRomCrc != wRomCrc)			// ROM changed
	{
		Chipset.pc = 0;						// continue from a safe PC address
		Chipset.Shutdn = FALSE;				// automatic restart
	}

	// check CPU main registers
	if (IsDataPacked(Chipset.A,CHECKAREA(A,R4))) goto read_err;

	// check internal IO data area
	if (IsDataPacked(Chipset.IORam,sizeof(Chipset.IORam))) goto read_err;
	if (Chipset.bSlave)						// 2nd slave controller
	{
		if (IsDataPacked(ChipsetS.IORam,sizeof(ChipsetS.IORam))) goto read_err;
	}

	LoadBreakpointList(hFile);				// load debugger breakpoint list

	lstrcpy(szCurrentFilename, szFilename);
	_ASSERT(hCurrentFile == NULL);
	hCurrentFile = hFile;
	#if defined _USRDLL						// DLL version
		// notify main proc about current document file
		if (pEmuDocumentNotify) pEmuDocumentNotify(szCurrentFilename);
	#endif
	SetWindowPathTitle(szCurrentFilename);	// update window title line
	bDocumentAvail = TRUE;					// document available
	return TRUE;

read_err:
	AbortMessage(_T("This file must be truncated, and cannot be loaded."));
restore:
	if (INVALID_HANDLE_VALUE != hFile)		// close if valid handle
		CloseHandle(hFile);
	RestoreBackup();
	ResetBackup();
	return FALSE;
	#undef CHECKAREA
}

BOOL SaveDocument(VOID)
{
	DWORD           lBytesWritten;
	DWORD           lSizeofChipset;
	UINT            nLength;
	WINDOWPLACEMENT wndpl;

	if (hCurrentFile == NULL) return FALSE;

	_ASSERT(hWnd);							// window open
	wndpl.length = sizeof(wndpl);			// update saved window position
	GetWindowPlacement(hWnd, &wndpl);
	Chipset.nPosX = (SWORD) wndpl.rcNormalPosition.left;
	Chipset.nPosY = (SWORD) wndpl.rcNormalPosition.top;

	SetFilePointer(hCurrentFile,0,NULL,FILE_BEGIN);

	if (!WriteFile(hCurrentFile, pbySignature, sizeof(pbySignature), &lBytesWritten, NULL))
	{
		AbortMessage(_T("Could not write into file !"));
		return FALSE;
	}

	CrcRom(&Chipset.wRomCrc);				// save fingerprint of ROM

	nLength = lstrlen(szCurrentKml);
	WriteFile(hCurrentFile, &nLength, sizeof(nLength), &lBytesWritten, NULL);
	#if defined _UNICODE
	{
		LPSTR szTmp = (LPSTR) malloc(nLength);
		if (szTmp != NULL)
		{
			WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK,
								szCurrentKml, nLength,
								szTmp, nLength, NULL, NULL);
			WriteFile(hCurrentFile, szTmp, nLength, &lBytesWritten, NULL);
			free(szTmp);
		}
	}
	#else
	{
		WriteFile(hCurrentFile, szCurrentKml, nLength, &lBytesWritten, NULL);
	}
	#endif
	lSizeofChipset = sizeof(Chipset);
	WriteFile(hCurrentFile, &lSizeofChipset, sizeof(lSizeofChipset), &lBytesWritten, NULL);
	WriteFile(hCurrentFile, &Chipset, lSizeofChipset, &lBytesWritten, NULL);
	if (Chipset.bSlave)
	{
		lSizeofChipset = sizeof(ChipsetS);
		WriteFile(hCurrentFile, &lSizeofChipset, sizeof(lSizeofChipset), &lBytesWritten, NULL);
		WriteFile(hCurrentFile, &ChipsetS, lSizeofChipset, &lBytesWritten, NULL);
	}
	if (Chipset.NCE2Ram)  WriteFile(hCurrentFile, mNCE2, Chipset.NCE2Size,  &lBytesWritten, NULL);
	if (Chipset.NCE3Ram)  WriteFile(hCurrentFile, mNCE3, Chipset.NCE3Size,  &lBytesWritten, NULL);
	if (ChipsetS.NCE2Ram) WriteFile(hCurrentFile, sNCE2, ChipsetS.NCE2Size, &lBytesWritten, NULL);
	if (ChipsetS.NCE3Ram) WriteFile(hCurrentFile, sNCE3, ChipsetS.NCE3Size, &lBytesWritten, NULL);
	SaveBreakpointList(hCurrentFile);		// save debugger breakpoint list
	SetEndOfFile(hCurrentFile);				// cut the rest
	return TRUE;
}

BOOL SaveDocumentAs(LPCTSTR szFilename)
{
	HANDLE hFile;

	if (hCurrentFile)						// already file in use
	{
		CloseHandle(hCurrentFile);			// close it, even it's same, so data always will be saved
		hCurrentFile = NULL;
	}
	hFile = CreateFile(szFilename, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)		// error, couldn't create a new file
	{
		AbortMessage(_T("This file must be currently used by another instance of Emu42."));
		return FALSE;
	}
	lstrcpy(szCurrentFilename, szFilename);	// save new file name
	hCurrentFile = hFile;					// and the corresponding handle
	#if defined _USRDLL						// DLL version
		// notify main proc about current document file
		if (pEmuDocumentNotify) pEmuDocumentNotify(szCurrentFilename);
	#endif
	SetWindowPathTitle(szCurrentFilename);	// update window title line
	return SaveDocument();					// save current content
}



//################
//#
//#    Backup
//#
//################

BOOL SaveBackup(VOID)
{
	WINDOWPLACEMENT wndpl;

	if (!bDocumentAvail) return FALSE;

	_ASSERT(nState != SM_RUN);				// emulation engine is running

	// save window position
	_ASSERT(hWnd);							// window open
	wndpl.length = sizeof(wndpl);			// update saved window position
	GetWindowPlacement(hWnd, &wndpl);
	Chipset.nPosX = (SWORD) wndpl.rcNormalPosition.left;
	Chipset.nPosY = (SWORD) wndpl.rcNormalPosition.top;

	lstrcpy(szBackupFilename, szCurrentFilename);
	lstrcpy(szBackupKml, szCurrentKml);
	if (Backup_mNCE2) { free(Backup_mNCE2); Backup_mNCE2 = NULL; }
	if (Backup_mNCE3) { free(Backup_mNCE3); Backup_mNCE3 = NULL; }
	if (Backup_sNCE2) { free(Backup_sNCE2); Backup_sNCE2 = NULL; }
	if (Backup_sNCE3) { free(Backup_sNCE3); Backup_sNCE3 = NULL; }
	CopyMemory(&BackupChipset, &Chipset, sizeof(Chipset));
	if (Chipset.bSlave)						// 2nd slave controller
	{
		CopyMemory(&BackupChipsetS, &ChipsetS, sizeof(ChipsetS));
	}
	if (Chipset.NCE2Ram)					// RAM at NCE2 master
	{
		Backup_mNCE2 = (LPBYTE) malloc(Chipset.NCE2Size);
		CopyMemory(Backup_mNCE2, mNCE2, Chipset.NCE2Size);
	}
	if (Chipset.NCE3Ram)					// RAM at NCE3 master
	{
		Backup_mNCE3 = (LPBYTE) malloc(Chipset.NCE3Size);
		CopyMemory(Backup_mNCE3, mNCE3, Chipset.NCE3Size);
	}
	if (ChipsetS.NCE2Ram)					// RAM at NCE2 slave
	{
		Backup_sNCE2 = (LPBYTE) malloc(ChipsetS.NCE2Size);
		CopyMemory(Backup_sNCE2, sNCE2, ChipsetS.NCE2Size);
	}
	if (ChipsetS.NCE3Ram)					// RAM at NCE3 slave
	{
		Backup_sNCE3 = (LPBYTE) malloc(ChipsetS.NCE3Size);
		CopyMemory(Backup_sNCE3, sNCE3, ChipsetS.NCE3Size);
	}
	CreateBackupBreakpointList();
	bBackup = TRUE;
	return TRUE;
}

BOOL RestoreBackup(VOID)
{
	BOOL bDbgOpen;

	if (!bBackup) return FALSE;

	bDbgOpen = (nDbgState != DBG_OFF);		// debugger window open?
	ResetDocument();
	if (!InitKML(szBackupKml,TRUE))
	{
		InitKML(szCurrentKml,TRUE);
		return FALSE;
	}
	lstrcpy(szCurrentKml, szBackupKml);
	lstrcpy(szCurrentFilename, szBackupFilename);
	if (szCurrentFilename[0])
	{
		hCurrentFile = CreateFile(szCurrentFilename, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (hCurrentFile == INVALID_HANDLE_VALUE)
		{
			hCurrentFile = NULL;
			szCurrentFilename[0] = 0;
		}
	}
	CopyMemory(&Chipset, &BackupChipset, sizeof(Chipset));
	if (Chipset.bSlave)						// 2nd slave controller
	{
		CopyMemory(&ChipsetS, &BackupChipsetS, sizeof(ChipsetS));
	}
	if (Chipset.NCE2Ram)					// RAM at NCE2 master
	{
		mNCE2 = (LPBYTE) malloc(Chipset.NCE2Size);
		CopyMemory(mNCE2, Backup_mNCE2, Chipset.NCE2Size);
	}
	if (Chipset.NCE3Ram)					// RAM at NCE3 master
	{
		mNCE3 = (LPBYTE) malloc(Chipset.NCE3Size);
		CopyMemory(mNCE3, Backup_mNCE3, Chipset.NCE3Size);
	}
	if (ChipsetS.NCE2Ram)					// RAM at NCE2 slave
	{
		sNCE2 = (LPBYTE) malloc(ChipsetS.NCE2Size);
		CopyMemory(sNCE2, Backup_sNCE2, ChipsetS.NCE2Size);
	}
	if (ChipsetS.NCE3Ram)					// RAM at NCE3 slave
	{
		sNCE3 = (LPBYTE) malloc(ChipsetS.NCE3Size);
		CopyMemory(sNCE3, Backup_sNCE3, ChipsetS.NCE3Size);
	}
	UpdateContrast();						// update contrast
	SetWindowPathTitle(szCurrentFilename);	// update window title line
	SetWindowLocation(hWnd,Chipset.nPosX,Chipset.nPosY);
	RestoreBackupBreakpointList();			// restore the debugger breakpoint list
	if (bDbgOpen) OnToolDebug();			// reopen the debugger
	bDocumentAvail = TRUE;					// document available
	return TRUE;
}

BOOL ResetBackup(VOID)
{
	if (!bBackup) return FALSE;
	szBackupFilename[0] = 0;
	szBackupKml[0] = 0;
	if (Backup_mNCE2) { free(Backup_mNCE2); Backup_mNCE2 = NULL; }
	if (Backup_mNCE3) { free(Backup_mNCE3); Backup_mNCE3 = NULL; }
	if (Backup_sNCE2) { free(Backup_sNCE2); Backup_sNCE2 = NULL; }
	if (Backup_sNCE3) { free(Backup_sNCE3); Backup_sNCE3 = NULL; }
	ZeroMemory(&BackupChipset,sizeof(BackupChipset));
	ZeroMemory(&BackupChipsetS,sizeof(BackupChipsetS));
	bBackup = FALSE;
	return TRUE;
}



//################
//#
//#    Open File Common Dialog Boxes
//#
//################

static VOID InitializeOFN(LPOPENFILENAME ofn)
{
	ZeroMemory((LPVOID)ofn, sizeof(OPENFILENAME));
	ofn->lStructSize = sizeof(OPENFILENAME);
	ofn->hwndOwner = hWnd;
	ofn->Flags = OFN_EXPLORER|OFN_HIDEREADONLY;
	return;
}

BOOL GetOpenFilename(VOID)
{
	TCHAR szBuffer[ARRAYSIZEOF(szBufferFilename)];
	OPENFILENAME ofn;

	InitializeOFN(&ofn);
	ofn.lpstrFilter =
		_T("Emu42 Files (*.e10;*.e14;*.e17;*.e19;*.e20;*.e21;*.e22;*.e27;*.e28;*.e32;*.e42)\0")
		  _T("*.e10;*.e14;*.e17;*.e19;*.e20;*.e21;*.e22;*.e27;*.e28;*.e32;*.e42\0")
		_T("HP-10B Files (*.e10)\0*.e10\0")
		_T("HP-14B Files (*.e14)\0*.e14\0")
		_T("HP-17B/17BII Files (*.e17)\0*.e17\0")
		_T("HP-19BII Files (*.e19)\0*.e19\0")
		_T("HP-20S Files (*.e20)\0*.e20\0")
		_T("HP-21S Files (*.e21)\0*.e21\0")
		_T("HP-22S Files (*.e22)\0*.e22\0")
		_T("HP-27S Files (*.e27)\0*.e27\0")
		_T("HP-28S Files (*.e28)\0*.e28\0")
		_T("HP-32S/32SII Files (*.e32)\0*.e32\0")
		_T("HP-42S Files (*.e42)\0*.e42\0")
		_T("All Files (*.*)\0*.*\0");
	ofn.nFilterIndex = 1;					// default
	ofn.lpstrFile = szBuffer;
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = ARRAYSIZEOF(szBuffer);
	ofn.Flags |= OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
	if (GetOpenFileName(&ofn) == FALSE) return FALSE;
	_ASSERT(ARRAYSIZEOF(szBufferFilename) == ofn.nMaxFile);
	lstrcpy(szBufferFilename, ofn.lpstrFile);
	return TRUE;
}

BOOL GetSaveAsFilename(VOID)
{
	TCHAR szBuffer[ARRAYSIZEOF(szBufferFilename)];
	OPENFILENAME ofn;

	InitializeOFN(&ofn);
	ofn.lpstrFilter =
		_T("HP-10B Files (*.e10)\0*.e10\0")
		_T("HP-14B Files (*.e14)\0*.e14\0")
		_T("HP-17B/17BII Files (*.e17)\0*.e17\0")
		_T("HP-19BII Files (*.e19)\0*.e19\0")
		_T("HP-20S Files (*.e20)\0*.e20\0")
		_T("HP-21S Files (*.e21)\0*.e21\0")
		_T("HP-22S Files (*.e22)\0*.e22\0")
		_T("HP-27S Files (*.e27)\0*.e27\0")
		_T("HP-28S Files (*.e28)\0*.e28\0")
		_T("HP-32S/32SII Files (*.e32)\0*.e32\0")
		_T("HP-42S Files (*.e42)\0*.e42\0")
		_T("All Files (*.*)\0*.*\0");
	ofn.nFilterIndex = 12;					// default
	if (cCurrentRomType == 'E')				// Ernst, HP10B
	{
		ofn.lpstrDefExt = _T("e10");
		ofn.nFilterIndex = 1;
	}
	if (cCurrentRomType == 'I')				// Midas, HP14B
	{
		ofn.lpstrDefExt = _T("e14");
		ofn.nFilterIndex = 2;
	}
	if (   cCurrentRomType == 'T'			// Trader,    HP17B
		|| cCurrentRomType == 'U')			// Trader II, HP17BII
	{
		ofn.lpstrDefExt = _T("e17");
		ofn.nFilterIndex = 3;
	}
	if (cCurrentRomType == 'Y')				// Tycoon II, HP19BII
	{
		ofn.lpstrDefExt = _T("e19");
		ofn.nFilterIndex = 4;
	}
	if (cCurrentRomType == 'F')				// Erni, HP20S
	{
		ofn.lpstrDefExt = _T("e20");
		ofn.nFilterIndex = 5;
	}
	if (cCurrentRomType == 'C')				// Monte Carlo, HP20S
	{
		ofn.lpstrDefExt = _T("e21");
		ofn.nFilterIndex = 6;
	}
	if (cCurrentRomType == 'A')				// Plato, HP22S
	{
		ofn.lpstrDefExt = _T("e22");
		ofn.nFilterIndex = 7;
	}
	if (cCurrentRomType == 'M')				// Mentor, HP27S
	{
		ofn.lpstrDefExt = _T("e27");
		ofn.nFilterIndex = 8;
	}
	if (cCurrentRomType == 'O')				// Orlando, HP28S
	{
		ofn.lpstrDefExt = _T("e28");
		ofn.nFilterIndex = 9;
	}
	if (   cCurrentRomType == 'L'			// Leonardo, HP32S
		|| cCurrentRomType == 'N')			// Nardo,    HP32SII
	{
		ofn.lpstrDefExt = _T("e32");
		ofn.nFilterIndex = 10;
	}
	if (cCurrentRomType == 'D')				// Davinci, HP42S
	{
		ofn.lpstrDefExt = _T("e42");
		ofn.nFilterIndex = 11;
	}
	ofn.lpstrFile = szBuffer;
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = ARRAYSIZEOF(szBuffer);
	ofn.Flags |= OFN_CREATEPROMPT|OFN_OVERWRITEPROMPT;
	if (GetSaveFileName(&ofn) == FALSE) return FALSE;
	_ASSERT(ARRAYSIZEOF(szBufferFilename) == ofn.nMaxFile);
	lstrcpy(szBufferFilename, ofn.lpstrFile);
	return TRUE;
}

BOOL GetLoadObjectFilename(LPCTSTR lpstrFilter,LPCTSTR lpstrDefExt)
{
	TCHAR szBuffer[ARRAYSIZEOF(szBufferFilename)];
	OPENFILENAME ofn;

	InitializeOFN(&ofn);
	ofn.lpstrFilter = lpstrFilter;
	ofn.lpstrDefExt = lpstrDefExt;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = szBuffer;
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = ARRAYSIZEOF(szBuffer);
	ofn.Flags |= OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
	if (GetOpenFileName(&ofn) == FALSE) return FALSE;
	_ASSERT(ARRAYSIZEOF(szBufferFilename) == ofn.nMaxFile);
	lstrcpy(szBufferFilename, ofn.lpstrFile);
	return TRUE;
}

BOOL GetSaveObjectFilename(LPCTSTR lpstrFilter,LPCTSTR lpstrDefExt)
{
	TCHAR szBuffer[ARRAYSIZEOF(szBufferFilename)];
	OPENFILENAME ofn;

	InitializeOFN(&ofn);
	ofn.lpstrFilter = lpstrFilter;
	ofn.lpstrDefExt = NULL;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = szBuffer;
	ofn.lpstrFile[0] = 0;
	ofn.nMaxFile = ARRAYSIZEOF(szBuffer);
	ofn.Flags |= OFN_CREATEPROMPT|OFN_OVERWRITEPROMPT;
	if (GetSaveFileName(&ofn) == FALSE) return FALSE;
	_ASSERT(ARRAYSIZEOF(szBufferFilename) == ofn.nMaxFile);
	lstrcpy(szBufferFilename, ofn.lpstrFile);
	if (ofn.nFileExtension == 0)			// given filename has no extension
	{
		// actual name length
		UINT nLength = lstrlen(szBufferFilename);

		// destination buffer has room for the default extension
		if (nLength + 1 + lstrlen(lpstrDefExt) < ARRAYSIZEOF(szBufferFilename))
		{
			// add default extension
			szBufferFilename[nLength++] = _T('.');
			lstrcpy(&szBufferFilename[nLength], lpstrDefExt);
		}
	}
	return TRUE;
}



//################
//#
//#    Load and Save HP28S Objects
//#
//################

WORD WriteStack(UINT nStkLevel,LPBYTE lpBuf,DWORD dwSize)	// separated from LoadObject()
{
	BOOL  bBinary;
	DWORD dwAddress, i;

	// check for HP28S binary header
	bBinary = (memcmp(&lpBuf[dwSize],BINARYHEADER28S,8) == 0);

	for (dwAddress = 0, i = 0; i < dwSize; i++)
	{
		BYTE byTwoNibs = lpBuf[i+dwSize];
		lpBuf[dwAddress++] = (BYTE)(byTwoNibs&0xF);
		lpBuf[dwAddress++] = (BYTE)(byTwoNibs>>4);
	}

	dwSize = dwAddress;						// unpacked buffer size

	if (bBinary == TRUE)
	{ // load as binary
		dwSize = RPL_ObjectSize(lpBuf+16,dwSize-16);
		if (dwSize == BAD_OB) return S_ERR_OBJECT;
		dwAddress = RPL_CreateTemp(dwSize);
		if (dwAddress == 0) return S_ERR_BINARY;
		Nwrite(lpBuf+16,dwAddress,dwSize);
	}
	else
	{ // load as string
		dwAddress = RPL_CreateTemp(dwSize+10);
		if (dwAddress == 0) return S_ERR_ASCII;
		Write5(dwAddress,O_DOCSTR);			// String
		Write5(dwAddress+5,dwSize+5);		// length of String
		Nwrite(lpBuf,dwAddress+10,dwSize);	// data
	}
	RPL_Push(nStkLevel,dwAddress);
	return S_ERR_NO;
}

BOOL LoadObject(LPCTSTR szFilename)			// separated stack writing part
{
	HANDLE hFile;
	DWORD  dwFileSizeLow;
	DWORD  dwFileSizeHigh;
	LPBYTE lpBuf;
	WORD   wError;

	hFile = CreateFile(szFilename,
					   GENERIC_READ,
					   FILE_SHARE_READ,
					   NULL,
					   OPEN_EXISTING,
					   FILE_FLAG_SEQUENTIAL_SCAN,
					   NULL);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;
	dwFileSizeLow = GetFileSize(hFile, &dwFileSizeHigh);
	if (dwFileSizeHigh != 0)
	{ // file is too large.
		CloseHandle(hFile);
		return FALSE;
	}
	lpBuf = (LPBYTE) malloc(dwFileSizeLow*2);
	if (lpBuf == NULL)
	{
		CloseHandle(hFile);
		return FALSE;
	}
	ReadFile(hFile, lpBuf+dwFileSizeLow, dwFileSizeLow, &dwFileSizeHigh, NULL);
	CloseHandle(hFile);

	wError = WriteStack(1,lpBuf,dwFileSizeLow);

	if (wError == S_ERR_OBJECT)
		AbortMessage(_T("This isn't a valid binary file."));

	if (wError == S_ERR_BINARY)
		AbortMessage(_T("The calculator does not have enough\nfree memory to load this binary file."));

	if (wError == S_ERR_ASCII)
		AbortMessage(_T("The calculator does not have enough\nfree memory to load this text file."));

	free(lpBuf);
	return (wError == S_ERR_NO);
}

BOOL SaveObject(LPCTSTR szFilename)			// separated stack reading part
{
	HANDLE hFile;
	UINT   uStkLvl;
	DWORD  lBytesWritten;
	DWORD  dwAddress;
	INT    nLength;

	_ASSERT(cCurrentRomType == 'O');

	// write object at stack level 1, if the calculator is off the object is
	// at stack level 2, stack level 1 contain the FALSE object in this case
	uStkLvl = ((Chipset.IORam[DSPCTL]&DON) != 0) ? 1 : 2;

	dwAddress = RPL_Pick(uStkLvl);
	if (dwAddress == 0)
	{
		AbortMessage(_T("Too Few Arguments."));
		return FALSE;
	}

	hFile = CreateFile(szFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		AbortMessage(_T("Cannot open file."));
		return FALSE;
	}

	WriteFile(hFile, BINARYHEADER28S, 8, &lBytesWritten, NULL);

	nLength = RPL_SkipOb(dwAddress) - dwAddress;

	for (; nLength > 0; nLength -= 2)
	{
		BYTE byByte = Read2(dwAddress);
		if (nLength == 1) byByte &= 0xF;
		WriteFile(hFile, &byByte, sizeof(byByte), &lBytesWritten, NULL);
		dwAddress += 2;
	}
	CloseHandle(hFile);
	return TRUE;
}



//################
//#
//#    Load Icon
//#
//################

BOOL LoadIconFromFile(LPCTSTR szFilename)
{
	HANDLE hIcon;

	SetCurrentDirectory(szEmuDirectory);
	// not necessary to destroy because icon is shared
	hIcon = LoadImage(NULL, szFilename, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE|LR_LOADFROMFILE|LR_SHARED);
	SetCurrentDirectory(szCurrentDirectory);

	if (hIcon)
	{
		SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM) hIcon);
		SendMessage(hWnd, WM_SETICON, ICON_BIG,   (LPARAM) hIcon);
	}
	return hIcon != NULL;
}

VOID LoadIconDefault(VOID)
{
	// use window class icon
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM) NULL);
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM) NULL);
	return;
}



//################
//#
//#    Load Bitmap
//#
//################

#define WIDTHBYTES(bits) (((bits) + 31) / 32 * 4)

typedef struct _BmpFile
{
	DWORD  dwPos;							// actual reading pos
	DWORD  dwFileSize;						// file size
	LPBYTE pbyFile;							// buffer
} BMPFILE, FAR *LPBMPFILE, *PBMPFILE;

static __inline WORD DibNumColors(__unaligned BITMAPINFOHEADER CONST *lpbi)
{
	if (lpbi->biClrUsed != 0) return (WORD) lpbi->biClrUsed;

	/* a 24 bitcount DIB has no color table */
	return (lpbi->biBitCount <= 8) ? (1 << lpbi->biBitCount) : 0;
}

static HPALETTE CreateBIPalette(__unaligned BITMAPINFOHEADER CONST *lpbi)
{
	LOGPALETTE* pPal;
	HPALETTE    hpal = NULL;
	WORD        wNumColors;
	BYTE        red;
	BYTE        green;
	BYTE        blue;
	UINT        i;
	__unaligned RGBQUAD* pRgb;

	if (!lpbi || lpbi->biSize != sizeof(BITMAPINFOHEADER))
		return NULL;

	// Get a pointer to the color table and the number of colors in it
	pRgb = (RGBQUAD FAR *)((LPBYTE)lpbi + (WORD)lpbi->biSize);
	wNumColors = DibNumColors(lpbi);

	if (wNumColors)
	{
		// Allocate for the logical palette structure
		pPal = (PLOGPALETTE) malloc(sizeof(LOGPALETTE) + wNumColors * sizeof(PALETTEENTRY));
		if (!pPal) return NULL;

		pPal->palVersion    = 0x300;
		pPal->palNumEntries = wNumColors;

		// Fill in the palette entries from the DIB color table and
		// create a logical color palette.
		for (i = 0; i < pPal->palNumEntries; i++)
		{
			pPal->palPalEntry[i].peRed   = pRgb[i].rgbRed;
			pPal->palPalEntry[i].peGreen = pRgb[i].rgbGreen;
			pPal->palPalEntry[i].peBlue  = pRgb[i].rgbBlue;
			pPal->palPalEntry[i].peFlags = 0;
		}
		hpal = CreatePalette(pPal);
		free(pPal);
	}
	else
	{
		// create halftone palette for 16, 24 and 32 bitcount bitmaps

		// 16, 24 and 32 bitcount DIB's have no color table entries so, set the
		// number of to the maximum value (256).
		wNumColors = 256;
		pPal = (PLOGPALETTE) malloc(sizeof(LOGPALETTE) + wNumColors * sizeof(PALETTEENTRY));
		if (!pPal) return NULL;

		pPal->palVersion    = 0x300;
		pPal->palNumEntries = wNumColors;

		red = green = blue = 0;

		// Generate 256 (= 8*8*4) RGB combinations to fill the palette
		// entries.
		for (i = 0; i < pPal->palNumEntries; i++)
		{
			pPal->palPalEntry[i].peRed   = red;
			pPal->palPalEntry[i].peGreen = green;
			pPal->palPalEntry[i].peBlue  = blue;
			pPal->palPalEntry[i].peFlags = 0;

			if (!(red += 32))
				if (!(green += 32))
					blue += 64;
		}
		hpal = CreatePalette(pPal);
		free(pPal);
	}
	return hpal;
}

static HBITMAP DecodeBmp(LPBMPFILE pBmp,BOOL bPalette)
{
	DWORD dwFileSize;

	HBITMAP hBitmap = NULL;

	// map memory to BITMAPFILEHEADER and BITMAPINFO
	const LPBITMAPFILEHEADER pBmfh = (LPBITMAPFILEHEADER) pBmp->pbyFile;
	const __unaligned LPBITMAPINFO pBmi = (__unaligned LPBITMAPINFO) & pBmfh[1];

	// size of bitmap header information & check for bitmap
	dwFileSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	if (   pBmp->dwFileSize < dwFileSize	// minimum size to read data from BITMAPFILEHEADER + BITMAPINFOHEADER
		|| pBmfh->bfType != 0x4D42)			// "BM"
		return NULL;

	// size with color table
	if (pBmi->bmiHeader.biCompression == BI_BITFIELDS)
	{
		dwFileSize += 3 * sizeof(DWORD);
	}
	else
	{
		dwFileSize += DibNumColors(&pBmi->bmiHeader) * sizeof(RGBQUAD);
	}
	if (dwFileSize != pBmfh->bfOffBits) return NULL;

	// size with bitmap data
	if (pBmi->bmiHeader.biCompression != BI_RGB)
	{
		dwFileSize += pBmi->bmiHeader.biSizeImage;
	}
	else
	{
		dwFileSize += WIDTHBYTES(pBmi->bmiHeader.biWidth * pBmi->bmiHeader.biBitCount)
					* labs(pBmi->bmiHeader.biHeight);
	}
	if (pBmp->dwFileSize < dwFileSize) return NULL;

	VERIFY(hBitmap = CreateDIBitmap(
		hWindowDC,
		&pBmi->bmiHeader,
		CBM_INIT,
		pBmp->pbyFile + pBmfh->bfOffBits,
		pBmi, DIB_RGB_COLORS));
	if (hBitmap == NULL) return NULL;

	if (bPalette && hPalette == NULL)
	{
		hPalette = CreateBIPalette(&pBmi->bmiHeader);
		// save old palette
		hOldPalette = SelectPalette(hWindowDC, hPalette, FALSE);
		RealizePalette(hWindowDC);
	}
	return hBitmap;
}

static BOOL ReadGifByte(LPBMPFILE pGif, INT *n)
{
	// outside GIF file
	if (pGif->dwPos >= pGif->dwFileSize)
		return TRUE;

	*n = pGif->pbyFile[pGif->dwPos++];
	return FALSE;
}

static BOOL ReadGifWord(LPBMPFILE pGif, INT *n)
{
	// outside GIF file
	if (pGif->dwPos + 1 >= pGif->dwFileSize)
		return TRUE;

	*n = pGif->pbyFile[pGif->dwPos++];
	*n |= (pGif->pbyFile[pGif->dwPos++] << 8);
	return FALSE;
}

static HBITMAP DecodeGif(LPBMPFILE pBmp,DWORD *pdwTransparentColor,BOOL bPalette)
{
	// this implementation base on the GIF image file
	// decoder engine of Free42 (c) by Thomas Okken

	HBITMAP hBitmap;

	typedef struct cmap
	{
		WORD    biBitCount;					// bits used in color map
		DWORD   biClrUsed;					// no of colors in color map
		RGBQUAD bmiColors[256];				// color map
	} CMAP;

	BOOL    bHasGlobalCmap;
	CMAP    sGlb;							// data of global color map

	INT     nWidth,nHeight,nInfo,nBackground,nZero;
	LONG    lBytesPerLine;

	LPBYTE  pbyPixels;

	BITMAPINFO bmi;							// global bitmap info

	BOOL bDecoding = TRUE;

	hBitmap = NULL;

	pBmp->dwPos = 6;						// position behind GIF header

	/* Bits 6..4 of info contain one less than the "color resolution",
	 * defined as the number of significant bits per RGB component in
	 * the source image's color palette. If the source image (from
	 * which the GIF was generated) was 24-bit true color, the color
	 * resolution is 8, so the value in bits 6..4 is 7. If the source
	 * image had an EGA color cube (2x2x2), the color resolution would
	 * be 2, etc.
	 * Bit 3 of info must be zero in GIF87a; in GIF89a, if it is set,
	 * it indicates that the global colormap is sorted, the most
	 * important entries being first. In PseudoColor environments this
	 * can be used to make sure to get the most important colors from
	 * the X server first, to optimize the image's appearance in the
	 * event that not all the colors from the colormap can actually be
	 * obtained at the same time.
	 * The 'zero' field is always 0 in GIF87a; in GIF89a, it indicates
	 * the pixel aspect ratio, as (PAR + 15) : 64. If PAR is zero,
	 * this means no aspect ratio information is given, PAR = 1 means
	 * 1:4 (narrow), PAR = 49 means 1:1 (square), PAR = 255 means
	 * slightly over 4:1 (wide), etc.
	 */

	if (   ReadGifWord(pBmp,&nWidth)
		|| ReadGifWord(pBmp,&nHeight)
		|| ReadGifByte(pBmp,&nInfo)
		|| ReadGifByte(pBmp,&nBackground)
		|| ReadGifByte(pBmp,&nZero)
		|| nZero != 0)
		goto quit;

	ZeroMemory(&bmi,sizeof(bmi));			// init bitmap info
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = nWidth;
	bmi.bmiHeader.biHeight = nHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;			// create a true color DIB
	bmi.bmiHeader.biCompression = BI_RGB;

	ZeroMemory(&sGlb,sizeof(sGlb));			// init global color map
	bHasGlobalCmap = (nInfo & 0x80) != 0;

	sGlb.biBitCount = (nInfo & 7) + 1;		// bits used in global color map
	sGlb.biClrUsed = (1 << sGlb.biBitCount); // no of colors in global color map

	// color table should not exceed 256 colors
	_ASSERT(sGlb.biClrUsed <= ARRAYSIZEOF(sGlb.bmiColors));

	if (bHasGlobalCmap)						// global color map
	{
		DWORD i;

		for (i = 0; i < sGlb.biClrUsed; ++i)
		{
			int r, g, b;

			if (ReadGifByte(pBmp,&r) || ReadGifByte(pBmp,&g) || ReadGifByte(pBmp,&b))
				goto quit;

			sGlb.bmiColors[i].rgbRed   = r;
			sGlb.bmiColors[i].rgbGreen = g;
			sGlb.bmiColors[i].rgbBlue  = b;
		}
	}
	else									// no color map
	{
		DWORD i;

		for (i = 0; i < sGlb.biClrUsed; ++i)
		{
			BYTE k = (BYTE) ((i * sGlb.biClrUsed) / (sGlb.biClrUsed - 1));

			sGlb.bmiColors[i].rgbRed   = k;
			sGlb.bmiColors[i].rgbGreen = k;
			sGlb.bmiColors[i].rgbBlue  = k;
		}
	}

	// bitmap dimensions
	lBytesPerLine = WIDTHBYTES(bmi.bmiHeader.biWidth * bmi.bmiHeader.biBitCount);
	bmi.bmiHeader.biSizeImage = lBytesPerLine * bmi.bmiHeader.biHeight;

	// create top-down DIB
	bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight;

	// allocate buffer for pixels
	VERIFY(hBitmap = CreateDIBSection(hWindowDC,
									  &bmi,
									  DIB_RGB_COLORS,
									  (VOID **)&pbyPixels,
									  NULL,
									  0));
	if (hBitmap == NULL) goto quit;

	// fill pixel buffer with background color
	for (nHeight = 0; nHeight < labs(bmi.bmiHeader.biHeight); ++nHeight)
	{
		LPBYTE pbyLine = pbyPixels + nHeight * lBytesPerLine;

		for (nWidth = 0; nWidth < bmi.bmiHeader.biWidth; ++nWidth)
		{
			*pbyLine++ = sGlb.bmiColors[nBackground].rgbBlue;
			*pbyLine++ = sGlb.bmiColors[nBackground].rgbGreen;
			*pbyLine++ = sGlb.bmiColors[nBackground].rgbRed;
		}

		_ASSERT((DWORD) (pbyLine - pbyPixels) <= bmi.bmiHeader.biSizeImage);
	}

	while (bDecoding)
	{
		INT nBlockType;

		if (ReadGifByte(pBmp,&nBlockType)) goto quit;

		switch (nBlockType)
		{
		case ',': // image
			{
				CMAP sAct;					// data of actual color map

				INT  nLeft,nTop,nWidth,nHeight;
				INT  i,nInfo;

				BOOL bInterlaced;
				INT h,v;
				INT nCodesize;				// LZW codesize in bits
				INT nBytecount;

				SHORT prefix_table[4096];
				SHORT code_table[4096];

				INT  nMaxcode;
				INT  nClearCode;
				INT  nEndCode;

				INT  nCurrCodesize;
				INT  nCurrCode;
				INT  nOldCode;
				INT  nBitsNeeded;
				BOOL bEndCodeSeen;

				// read image dimensions
				if (   ReadGifWord(pBmp,&nLeft)
					|| ReadGifWord(pBmp,&nTop)
					|| ReadGifWord(pBmp,&nWidth)
					|| ReadGifWord(pBmp,&nHeight)
					|| ReadGifByte(pBmp,&nInfo))
					goto quit;

				if (   nTop + nHeight > labs(bmi.bmiHeader.biHeight)
					|| nLeft + nWidth > bmi.bmiHeader.biWidth)
					goto quit;

				/* Bit 3 of info must be zero in GIF87a; in GIF89a, if it
				 * is set, it indicates that the local colormap is sorted,
				 * the most important entries being first. In PseudoColor
				 * environments this can be used to make sure to get the
				 * most important colors from the X server first, to
				 * optimize the image's appearance in the event that not
				 * all the colors from the colormap can actually be
				 * obtained at the same time.
				 */

				if ((nInfo & 0x80) == 0)	// using global color map
				{
					sAct = sGlb;
				}
				else						// using local color map
				{
					DWORD i;

					sAct.biBitCount = (nInfo & 7) + 1;	// bits used in color map
					sAct.biClrUsed = (1 << sAct.biBitCount); // no of colors in color map

					for (i = 0; i < sAct.biClrUsed; ++i)
					{
						int r, g, b;

						if (ReadGifByte(pBmp,&r) || ReadGifByte(pBmp,&g) || ReadGifByte(pBmp,&b))
							goto quit;

						sAct.bmiColors[i].rgbRed   = r;
						sAct.bmiColors[i].rgbGreen = g;
						sAct.bmiColors[i].rgbBlue  = b;
					}
				}

				// interlaced image
				bInterlaced = (nInfo & 0x40) != 0;

				h = 0;
				v = 0;
				if (   ReadGifByte(pBmp,&nCodesize)
					|| ReadGifByte(pBmp,&nBytecount))
					goto quit;

				nMaxcode = (1 << nCodesize);

				// preset LZW table
				for (i = 0; i < nMaxcode + 2; ++i)
				{
					prefix_table[i] = -1;
					code_table[i] = i;
				}
				nClearCode = nMaxcode++;
				nEndCode = nMaxcode++;

				nCurrCodesize = nCodesize + 1;
				nCurrCode = 0;
				nOldCode = -1;
				nBitsNeeded = nCurrCodesize;
				bEndCodeSeen = FALSE;

				while (nBytecount != 0)
				{
					for (i = 0; i < nBytecount; ++i)
					{
						INT nCurrByte;
						INT nBitsAvailable;

						if (ReadGifByte(pBmp,&nCurrByte))
							goto quit;

						if (bEndCodeSeen) continue;

						nBitsAvailable = 8;
						while (nBitsAvailable != 0)
						{
							INT nBitsCopied = (nBitsNeeded < nBitsAvailable)
											? nBitsNeeded
											: nBitsAvailable;

							INT nBits = nCurrByte >> (8 - nBitsAvailable);

							nBits &= 0xFF >> (8 - nBitsCopied);
							nCurrCode |= nBits << (nCurrCodesize - nBitsNeeded);
							nBitsAvailable -= nBitsCopied;
							nBitsNeeded -= nBitsCopied;

							if (nBitsNeeded == 0)
							{
								BYTE byExpanded[4096];
								INT  nExplen;

								do
								{
									if (nCurrCode == nEndCode)
									{
										bEndCodeSeen = TRUE;
										break;
									}

									if (nCurrCode == nClearCode)
									{
										nMaxcode = (1 << nCodesize) + 2;
										nCurrCodesize = nCodesize + 1;
										nOldCode = -1;
										break;
									}

									if (nCurrCode < nMaxcode)
									{
										if (nMaxcode < 4096 && nOldCode != -1)
										{
											INT c = nCurrCode;
											while (prefix_table[c] != -1)
												c = prefix_table[c];
											c = code_table[c];
											prefix_table[nMaxcode] = nOldCode;
											code_table[nMaxcode] = c;
											nMaxcode++;
											if (nMaxcode == (1 << nCurrCodesize) && nCurrCodesize < 12)
												nCurrCodesize++;
										}
									}
									else
									{
										INT c;

										if (nCurrCode > nMaxcode || nOldCode == -1) goto quit;

										_ASSERT(nCurrCode == nMaxcode);

										/* Once maxcode == 4096, we can't get here
										 * any more, because we refuse to raise
										 * nCurrCodeSize above 12 -- so we can
										 * never read a bigger code than 4095.
										 */

										c = nOldCode;
										while (prefix_table[c] != -1)
											c = prefix_table[c];
										c = code_table[c];
										prefix_table[nMaxcode] = nOldCode;
										code_table[nMaxcode] = c;
										nMaxcode++;

										if (nMaxcode == (1 << nCurrCodesize) && nCurrCodesize < 12)
											nCurrCodesize++;
									}
									nOldCode = nCurrCode;

									// output nCurrCode!
									nExplen = 0;
									while (nCurrCode != -1)
									{
										_ASSERT(nExplen < ARRAYSIZEOF(byExpanded));
										byExpanded[nExplen++] = (BYTE) code_table[nCurrCode];
										nCurrCode = prefix_table[nCurrCode];
									}
									_ASSERT(nExplen > 0);

									while (--nExplen >= 0)
									{
										// get color map index
										BYTE byColIndex = byExpanded[nExplen];

										LPBYTE pbyRgbr = pbyPixels + (lBytesPerLine * (nTop + v) + 3 * (nLeft + h));

										_ASSERT(pbyRgbr + 2 < pbyPixels + bmi.bmiHeader.biSizeImage);
										_ASSERT(byColIndex < sAct.biClrUsed);

										*pbyRgbr++ = sAct.bmiColors[byColIndex].rgbBlue;
										*pbyRgbr++ = sAct.bmiColors[byColIndex].rgbGreen;
										*pbyRgbr   = sAct.bmiColors[byColIndex].rgbRed;

										if (++h == nWidth)
										{
											h = 0;
											if (bInterlaced)
											{
												switch (v & 7)
												{
												case 0:
													v += 8;
													if (v < nHeight)
														break;
													/* Some GIF en/decoders go
													 * straight from the '0'
													 * pass to the '4' pass
													 * without checking the
													 * height, and blow up on
													 * 2/3/4 pixel high
													 * interlaced images.
													 */
													if (nHeight > 4)
														v = 4;
													else
														if (nHeight > 2)
															v = 2;
														else
															if (nHeight > 1)
																v = 1;
															else
																bEndCodeSeen = TRUE;
													break;
												case 4:
													v += 8;
													if (v >= nHeight)
														v = 2;
													break;
												case 2:
												case 6:
													v += 4;
													if (v >= nHeight)
														v = 1;
													break;
												case 1:
												case 3:
												case 5:
												case 7:
													v += 2;
													if (v >= nHeight)
														bEndCodeSeen = TRUE;
													break;
												}
												if (bEndCodeSeen)
													break; // while (--nExplen >= 0)
											}
											else // non interlaced
											{
												if (++v == nHeight)
												{
													bEndCodeSeen = TRUE;
													break; // while (--nExplen >= 0)
												}
											}
										}
									}
								}
								while (FALSE);

								nCurrCode = 0;
								nBitsNeeded = nCurrCodesize;
							}
						}
					}

					if (ReadGifByte(pBmp,&nBytecount))
						goto quit;
				}
			}
			break;

		case '!': // extension block
			{
				INT i,nFunctionCode,nByteCount,nDummy;

				if (ReadGifByte(pBmp,&nFunctionCode)) goto quit;
				if (ReadGifByte(pBmp,&nByteCount)) goto quit;

				// Graphic Control Label & correct Block Size
				if (nFunctionCode == 0xF9 && nByteCount == 0x04)
				{
					INT nPackedFields,nColorIndex;

					// packed fields
					if (ReadGifByte(pBmp,&nPackedFields)) goto quit;

					// delay time
					if (ReadGifWord(pBmp,&nDummy)) goto quit;

					// transparent color index
					if (ReadGifByte(pBmp,&nColorIndex)) goto quit;

					// transparent color flag set
					if ((nPackedFields & 0x1) != 0)
					{
						if (pdwTransparentColor != NULL)
						{
							*pdwTransparentColor = RGB(sGlb.bmiColors[nColorIndex].rgbRed,
													   sGlb.bmiColors[nColorIndex].rgbGreen,
													   sGlb.bmiColors[nColorIndex].rgbBlue);
						}
					}

					// block terminator (0 byte)
					if (!(!ReadGifByte(pBmp,&nDummy) && nDummy == 0)) goto quit;
				}
				else // other function
				{
					while (nByteCount != 0)
					{
						for (i = 0; i < nByteCount; ++i)
						{
							if (ReadGifByte(pBmp,&nDummy)) goto quit;
						}

						if (ReadGifByte(pBmp,&nByteCount)) goto quit;
					}
				}
			}
			break;

		case ';': // terminator
			bDecoding = FALSE;
			break;

		default: goto quit;
		}
	}

	_ASSERT(bDecoding == FALSE);			// decoding successful

	// normal decoding exit
	if (bPalette && hPalette == NULL)
	{
		hPalette = CreateBIPalette((PBITMAPINFOHEADER) &bmi);
		// save old palette
		hOldPalette = SelectPalette(hWindowDC, hPalette, FALSE);
		RealizePalette(hWindowDC);
	}

quit:
	if (hBitmap != NULL && bDecoding)		// creation failed
	{
		DeleteObject(hBitmap);				// delete bitmap
		hBitmap = NULL;
	}
	return hBitmap;
}

static HBITMAP DecodePng(LPBMPFILE pBmp,BOOL bPalette)
{
	// this implementation use the PNG image file decoder
	// engine of Copyright (c) 2005-2018 Lode Vandevenne

	HBITMAP hBitmap;

	UINT    uError,uWidth,uHeight;
	INT     nWidth,nHeight;
	LONG    lBytesPerLine;

	LPBYTE  pbyImage;						// PNG RGB image data
	LPBYTE  pbySrc;							// source buffer pointer
	LPBYTE  pbyPixels;						// BMP buffer

	BITMAPINFO bmi;

	hBitmap = NULL;
	pbyImage = NULL;

	// decode PNG image
	uError = lodepng_decode_memory(&pbyImage,&uWidth,&uHeight,pBmp->pbyFile,pBmp->dwFileSize,LCT_RGB,8);
	if (uError) goto quit;

	ZeroMemory(&bmi,sizeof(bmi));			// init bitmap info
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = (LONG) uWidth;
	bmi.bmiHeader.biHeight = (LONG) uHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;			// create a true color DIB
	bmi.bmiHeader.biCompression = BI_RGB;

	// bitmap dimensions
	lBytesPerLine = WIDTHBYTES(bmi.bmiHeader.biWidth * bmi.bmiHeader.biBitCount);
	bmi.bmiHeader.biSizeImage = lBytesPerLine * bmi.bmiHeader.biHeight;

	// allocate buffer for pixels
	VERIFY(hBitmap = CreateDIBSection(hWindowDC,
									  &bmi,
									  DIB_RGB_COLORS,
									  (VOID **)&pbyPixels,
									  NULL,
									  0));
	if (hBitmap == NULL) goto quit;

	pbySrc = pbyImage;						// init source loop pointer
	pbyPixels += bmi.bmiHeader.biSizeImage;	// end of destination bitmap

	// fill bottom up DIB pixel buffer with color information
	for (nHeight = 0; nHeight < bmi.bmiHeader.biHeight; ++nHeight)
	{
		LPBYTE pbyLine;

		pbyPixels -= lBytesPerLine;			// begin of previous row
		pbyLine = pbyPixels;				// row working copy

		for (nWidth = 0; nWidth < bmi.bmiHeader.biWidth; ++nWidth)
		{
			*pbyLine++ = pbySrc[2];			// blue
			*pbyLine++ = pbySrc[1];			// green
			*pbyLine++ = pbySrc[0];			// red
			pbySrc += 3;
		}
	}

	if (bPalette && hPalette == NULL)
	{
		hPalette = CreateBIPalette((PBITMAPINFOHEADER) &bmi);
		// save old palette
		hOldPalette = SelectPalette(hWindowDC, hPalette, FALSE);
		RealizePalette(hWindowDC);
	}

quit:
	if (pbyImage != NULL)					// buffer for PNG image allocated
	{
		free(pbyImage);						// free PNG image data
	}
	return hBitmap;
}

HBITMAP LoadBitmapFile(LPCTSTR szFilename,BOOL bPalette)
{
	HANDLE  hFile;
	HANDLE  hMap;
	BMPFILE Bmp;
	HBITMAP hBitmap;

	SetCurrentDirectory(szEmuDirectory);
	hFile = CreateFile(szFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	SetCurrentDirectory(szCurrentDirectory);
	if (hFile == INVALID_HANDLE_VALUE) return NULL;
	Bmp.dwFileSize = GetFileSize(hFile, NULL);
	hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hMap == NULL)
	{
		CloseHandle(hFile);
		return NULL;
	}
	Bmp.pbyFile = (LPBYTE) MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	if (Bmp.pbyFile == NULL)
	{
		CloseHandle(hMap);
		CloseHandle(hFile);
		return NULL;
	}

	do
	{
		// check for BMP file header "BM"
		if (Bmp.dwFileSize >= 2 && *(WORD *) Bmp.pbyFile == 0x4D42)
		{
			hBitmap = DecodeBmp(&Bmp,bPalette);
			break;
		}

		// check for GIF file header
		if (   Bmp.dwFileSize >= 6
			&& (memcmp(Bmp.pbyFile,"GIF87a",6) == 0 || memcmp(Bmp.pbyFile,"GIF89a",6) == 0))
		{
			hBitmap = DecodeGif(&Bmp,&dwTColor,bPalette);
			break;
		}

		// check for PNG file header
		if (Bmp.dwFileSize >= 8 && memcmp(Bmp.pbyFile,"\x89PNG\r\n\x1a\n",8) == 0)
		{
			hBitmap = DecodePng(&Bmp,bPalette);
			break;
		}

		// unknown file type
		hBitmap = NULL;
	}
	while (FALSE);

	UnmapViewOfFile(Bmp.pbyFile);
	CloseHandle(hMap);
	CloseHandle(hFile);
	return hBitmap;
}

static BOOL AbsColorCmp(DWORD dwColor1,DWORD dwColor2,DWORD dwTol)
{
	DWORD dwDiff;

	dwDiff =  (DWORD) abs((INT) (dwColor1 & 0xFF) - (INT) (dwColor2 & 0xFF));
	dwColor1 >>= 8;
	dwColor2 >>= 8;
	dwDiff += (DWORD) abs((INT) (dwColor1 & 0xFF) - (INT) (dwColor2 & 0xFF));
	dwColor1 >>= 8;
	dwColor2 >>= 8;
	dwDiff += (DWORD) abs((INT) (dwColor1 & 0xFF) - (INT) (dwColor2 & 0xFF));

	return dwDiff > dwTol;					// FALSE = colors match
}

static BOOL LabColorCmp(DWORD dwColor1,DWORD dwColor2,DWORD dwTol)
{
	DWORD dwDiff;
	INT   nDiffCol;

	nDiffCol = (INT) (dwColor1 & 0xFF) - (INT) (dwColor2 & 0xFF);
	dwDiff = (DWORD) (nDiffCol * nDiffCol);
	dwColor1 >>= 8;
	dwColor2 >>= 8;
	nDiffCol = (INT) (dwColor1 & 0xFF) - (INT) (dwColor2 & 0xFF);
	dwDiff += (DWORD) (nDiffCol * nDiffCol);
	dwColor1 >>= 8;
	dwColor2 >>= 8;
	nDiffCol = (INT) (dwColor1 & 0xFF) - (INT) (dwColor2 & 0xFF);
	dwDiff += (DWORD) (nDiffCol * nDiffCol);
	dwTol *= dwTol;

	return dwDiff > dwTol;					// FALSE = colors match
}

static DWORD EncodeColorBits(DWORD dwColorVal,DWORD dwMask)
{
	#define MAXBIT 32
	UINT uLshift = MAXBIT;
	UINT uRshift = 8;
	DWORD dwBitMask = dwMask;

	dwColorVal &= 0xFF;						// the color component using the lowest 8 bit

	// position of highest bit
	while ((dwBitMask & (1<<(MAXBIT-1))) == 0 && uLshift > 0)
	{
		dwBitMask <<= 1;					// next bit
		--uLshift;							// next position
	}

	if (uLshift > 24)						// avoid overflow on 32bit mask
	{
		uLshift -= uRshift;					// normalize left shift
		uRshift = 0;
	}

	return ((dwColorVal << uLshift) >> uRshift) & dwMask;
	#undef MAXBIT
}

HRGN CreateRgnFromBitmap(HBITMAP hBmp,COLORREF color,DWORD dwTol)
{
	#define ADD_RECTS_COUNT  256

	BOOL (*fnColorCmp)(DWORD dwColor1,DWORD dwColor2,DWORD dwTol);

	DWORD dwRed,dwGreen,dwBlue;
	HRGN hRgn;
	LPRGNDATA pRgnData;
	LPBITMAPINFO bi;
	LPBYTE pbyBits;
	LPBYTE pbyColor;
	DWORD dwAlignedWidthBytes;
	DWORD dwBpp;
	DWORD dwRectsCount;
	LONG x,y,xleft;
	BOOL bFoundLeft;
	BOOL bIsMask;

	if (dwTol >= 1000)						// use CIE L*a*b compare
	{
		fnColorCmp = LabColorCmp;
		dwTol -= 1000;						// remove L*a*b compare selector
	}
	else									// use Abs summation compare
	{
		fnColorCmp = AbsColorCmp;
	}

	// allocate memory for extended image information incl. RGBQUAD color table
	bi = (LPBITMAPINFO) calloc(1,sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	bi->bmiHeader.biSize = sizeof(bi->bmiHeader);
	_ASSERT(bi->bmiHeader.biBitCount == 0); // for query without color table

	// get information about image
	GetDIBits(hWindowDC,hBmp,0,0,NULL,bi,DIB_RGB_COLORS);

	// DWORD aligned bitmap width in BYTES
	dwAlignedWidthBytes = WIDTHBYTES(  bi->bmiHeader.biWidth
									 * bi->bmiHeader.biPlanes
									 * bi->bmiHeader.biBitCount);

	// biSizeImage is empty
	if (bi->bmiHeader.biSizeImage == 0 && bi->bmiHeader.biCompression == BI_RGB)
	{
		bi->bmiHeader.biSizeImage = dwAlignedWidthBytes * bi->bmiHeader.biHeight;
	}

	// allocate memory for image data (colors)
	pbyBits = (LPBYTE) malloc(bi->bmiHeader.biSizeImage);

	// fill bits buffer
	GetDIBits(hWindowDC,hBmp,0,bi->bmiHeader.biHeight,pbyBits,bi,DIB_RGB_COLORS);

	// convert color if current DC is 16-bit/32-bit bitfield coded
	if (bi->bmiHeader.biCompression == BI_BITFIELDS)
	{
		dwRed   = *(LPDWORD) &bi->bmiColors[0];
		dwGreen = *(LPDWORD) &bi->bmiColors[1];
		dwBlue  = *(LPDWORD) &bi->bmiColors[2];
	}
	else // RGB coded
	{
		// convert color if current DC is 16-bit RGB coded
		if (bi->bmiHeader.biBitCount == 16)
		{
			// for 15 bit (5:5:5)
			dwRed   = 0x00007C00;
			dwGreen = 0x000003E0;
			dwBlue  = 0x0000001F;
		}
		else
		{
			// convert COLORREF to RGBQUAD color
			dwRed   = 0x00FF0000;
			dwGreen = 0x0000FF00;
			dwBlue  = 0x000000FF;
		}
	}
	color = EncodeColorBits((color >> 16), dwBlue)
		  | EncodeColorBits((color >>  8), dwGreen)
		  | EncodeColorBits((color >>  0), dwRed);

	dwBpp = bi->bmiHeader.biBitCount >> 3;	// bytes per pixel

	// DIB is bottom up image so we begin with the last scanline
	pbyColor = pbyBits + (bi->bmiHeader.biHeight - 1) * dwAlignedWidthBytes;

	dwRectsCount = bi->bmiHeader.biHeight;	// number of rects in allocated buffer

	bFoundLeft = FALSE;						// set when mask has been found in current scan line

	// allocate memory for region data
	pRgnData = (PRGNDATA) malloc(sizeof(RGNDATAHEADER) + dwRectsCount * sizeof(RECT));

	// fill it by default
	ZeroMemory(&pRgnData->rdh,sizeof(pRgnData->rdh));
	pRgnData->rdh.dwSize = sizeof(pRgnData->rdh);
	pRgnData->rdh.iType	 = RDH_RECTANGLES;
	SetRect(&pRgnData->rdh.rcBound,MAXLONG,MAXLONG,0,0);

	for (y = 0; y < bi->bmiHeader.biHeight; ++y)
	{
		LPBYTE pbyLineStart = pbyColor;

		for (x = 0; x < bi->bmiHeader.biWidth; ++x)
		{
			// get color
			switch (bi->bmiHeader.biBitCount)
			{
			case 8:
				bIsMask = fnColorCmp(*(LPDWORD)(&bi->bmiColors)[*pbyColor],color,dwTol);
				break;
			case 16:
				// it makes no sense to allow a tolerance here
				bIsMask = (*(LPWORD)pbyColor != (WORD) color);
				break;
			case 24:
				bIsMask = fnColorCmp((*(LPDWORD)pbyColor & 0x00ffffff),color,dwTol);
				break;
			case 32:
				bIsMask = fnColorCmp(*(LPDWORD)pbyColor,color,dwTol);
			}
			pbyColor += dwBpp;				// shift pointer to next color

			if (!bFoundLeft && bIsMask)		// non transparent color found
			{
				xleft = x;
				bFoundLeft = TRUE;
			}

			if (bFoundLeft)					// found non transparent color in scanline
			{
				// transparent color or last column
				if (!bIsMask || x + 1 == bi->bmiHeader.biWidth)
				{
					// non transparent color and last column
					if (bIsMask && x + 1 == bi->bmiHeader.biWidth)
						++x;

					// save current RECT
					((LPRECT) pRgnData->Buffer)[pRgnData->rdh.nCount].left = xleft;
					((LPRECT) pRgnData->Buffer)[pRgnData->rdh.nCount].top  = y;
					((LPRECT) pRgnData->Buffer)[pRgnData->rdh.nCount].right = x;
					((LPRECT) pRgnData->Buffer)[pRgnData->rdh.nCount].bottom = y + 1;
					pRgnData->rdh.nCount++;

					if (xleft < pRgnData->rdh.rcBound.left)
						pRgnData->rdh.rcBound.left = xleft;

					if (y < pRgnData->rdh.rcBound.top)
						pRgnData->rdh.rcBound.top = y;

					if (x > pRgnData->rdh.rcBound.right)
						pRgnData->rdh.rcBound.right = x;

					if (y + 1 > pRgnData->rdh.rcBound.bottom)
						pRgnData->rdh.rcBound.bottom = y + 1;

					// if buffer full reallocate it with more room
					if (pRgnData->rdh.nCount >= dwRectsCount)
					{
						dwRectsCount += ADD_RECTS_COUNT;

						pRgnData = (LPRGNDATA) realloc(pRgnData,sizeof(RGNDATAHEADER) + dwRectsCount * sizeof(RECT));
					}

					bFoundLeft = FALSE;
				}
			}
		}

		// previous scanline
		pbyColor = pbyLineStart - dwAlignedWidthBytes;
	}
	// release image data
	free(pbyBits);
	free(bi);

	// create region
	hRgn = ExtCreateRegion(NULL,sizeof(RGNDATAHEADER) + pRgnData->rdh.nCount * sizeof(RECT),pRgnData);

	free(pRgnData);
	return hRgn;
	#undef ADD_RECTS_COUNT
}
