/*
 *   stack.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2005 Christoph Gießelink
 *
 */
#include "pch.h"
#include "Emu42.h"
#include "io.h"
#include "rpl.h"

BOOL bDetectClpObject = TRUE;				// try to detect clipboard object

//################
//#
//#    Low level subroutines
//#
//################

static LPTSTR Trim(LPCTSTR cp)
{
	LPCTSTR pcWs = _T(" \t\n\r");			// valid whitespace characters

	LPTSTR pc;
	DWORD  dwFirst,dwLast;

	dwLast = lstrlen(cp);					// last position in string (EOS)

	// trim leading and tailing whitespace characters
	dwFirst = (DWORD) _tcsspn(cp,pcWs);		// position of 1st non whitespace character

	// search for position behind last non whitespace character 
	while (dwLast > dwFirst && _tcschr(pcWs,cp[dwLast-1]) != NULL)
		--dwLast;

	dwLast = 1 + dwLast - dwFirst;			// calculate buffer length

	if ((pc = (LPTSTR) malloc(dwLast * sizeof(*pc))) != NULL)
	{
		lstrcpyn(pc,&cp[dwFirst],dwLast);	// copy relevant data + EOS
	}
	return pc;
}

static INT RPL_GetBcd(BYTE CONST *pbyNum,INT nMantLen,INT nExpLen,CONST TCHAR cDec,LPTSTR cp,INT nSize)
{
	BYTE byNib;
	LONG v,lExp;
	BOOL bPflag,bExpflag;
	INT  i;

	lExp = 0;
	for (v = 1; nExpLen--; v *= 10)			// fetch exponent
	{
		lExp += (LONG) *pbyNum++ * v;		// calc. exponent
	}

	if (lExp > v / 2) lExp -= v;			// negative exponent

	lExp -= nMantLen - 1;					// set decimal point to end of mantissa

	i = 0;									// first character
	bPflag = FALSE;							// show no decimal point
	bExpflag = FALSE;						// show no exponent

	// scan mantissa
	for (v = (LONG) nMantLen - 1; v >= 0 || bPflag; v--)
	{
		if (v >= 0L)						// still mantissa digits left
			byNib = *pbyNum++;
		else
			byNib = 0;						// zero for negativ exponent

		if (!i)								// still delete zeros at end
		{
			if (byNib == 0 && lExp && v > 0) // delete zeros
			{
				lExp++;						// adjust exponent
				continue;
			}

			// TRUE at x.E
			bExpflag = v + lExp >= nMantLen || lExp < -nMantLen;
			bPflag = !bExpflag && v < -lExp; // decimal point flag at neg. exponent
		}

		// set decimal point
		if ((bExpflag && v == 0) || (!lExp && i))
		{
			if (i >= nSize) return 0;		// dest buffer overflow
			cp[i++] = cDec;					// write decimal point
			if (v < 0)						// no mantissa digits any more
			{
				if (i >= nSize) return 0;	// dest buffer overflow
				cp[i++] = _T('0');			// write heading zero
			}
			bPflag = FALSE;					// finished with negativ exponents
		}

		if (v >= 0 || bPflag)
		{
			if (i >= nSize) return 0;		// dest buffer overflow
			cp[i++] = (TCHAR) byNib + _T('0'); // write character
		}

		lExp++;								// next position
	}

	if (*pbyNum == 9)						// negative number
	{
		if (i >= nSize) return 0;			// dest buffer overflow
		cp[i++] = _T('-');					// write sign
	}

	if (i >= nSize) return 0;				// dest buffer overflow
	cp[i] = 0;								// set EOS

	for (v = 0; v < (i / 2); v++)			// reverse string
	{
		TCHAR cNib = cp[v];					// swap chars
		cp[v] = cp[i-v-1];
		cp[i-v-1] = cNib;
	}

	// write number with exponent
	if (bExpflag)
	{
		if (i + 5 >= nSize) return 0;		// dest buffer overflow
		i += wsprintf(&cp[i],_T("E%d"),lExp-1);
	}
	return i;
}

static __inline INT SetBcd(LPCTSTR cp,INT nMantLen,INT nExpLen,CONST TCHAR cDec,LPBYTE pbyNum,INT nSize)
{
	TCHAR cVc[] = _T(".0123456789eE+-");

	BYTE byNum[80];
	INT  i,nIp,nDp,nMaxExp;
	LONG lExp;

	cVc[0] = cDec;							// replace decimal char

	if (   nMantLen + nExpLen >= nSize		// destination buffer too small
		|| !*cp								// empty string
		|| _tcsspn(cp,cVc) != (SIZE_T) lstrlen(cp) // real doesn't contain only these numbers
		|| (SIZE_T) lstrlen(cp) >= ARRAYSIZEOF(byNum)) // ignore too long reals
		return 0;

	byNum[0] = (*cp != _T('-')) ? 0 : 9;	// set sign nibble
	if (*cp == _T('-') || *cp == _T('+'))	// skip sign nibble
		cp++;

	// only '.', '0' .. '9' are valid here
	if (!((*cp == cDec) || (*cp >= _T('0')) || (*cp <= _T('9'))))
		return 0;

	nIp = 0;								// length of integer part
	if (*cp != cDec)						// no decimal point
	{
		// count integer part
		while (*cp >= _T('0') && *cp <= _T('9'))
			byNum[++nIp] = *cp++ - _T('0');
		if (!nIp) return 0;
	}

	// only '.', 'E', 'e' or end are valid here
	if (!(!*cp || (*cp == cDec) || (*cp == _T('E')) || (*cp == _T('e'))))
		return 0;

	nDp = 0;								// length of decimal part
	if (*cp == cDec)						// decimal point
	{
		cp++;								// skip '.'

		// count decimal part
		while (*cp >= _T('0') && *cp <= _T('9'))
			byNum[nIp + ++nDp] = *cp++ - _T('0');
	}

	// count number of heading zeros in mantissa
	for (i = 0; byNum[i+1] == 0 && i + 1 < nIp + nDp; ++i) { }

	if (i > 0)								// have to normalize
	{
		INT j;

		nIp -= i;							// for later ajust of exponent
		for (j = 1; j <= nIp + nDp; ++j)	// normalize mantissa
			byNum[j] = byNum[j + i];
	}

	if(byNum[1] == 0)						// number is 0
	{
		ZeroMemory(pbyNum,nMantLen + nExpLen + 1);
		return nMantLen + nExpLen + 1;
	}

	for (i = nIp + nDp; i < nMantLen;)		// fill rest of mantissa with 0
		byNum[++i] = 0;

	// must be 'E', 'e' or end
	if (!(!*cp || (*cp == _T('E')) || (*cp == _T('e'))))
		return 0;

	lExp = 0;
	if (*cp == _T('E') || *cp == _T('e'))
	{
		cp++;								// skip 'E'

		i = FALSE;							// positive exponent
		if (*cp == _T('-') || *cp == _T('+'))
		{
			i = (*cp++ == _T('-'));			// adjust exponent sign
		}

		// exponent symbol must be followed by number
		if (*cp < _T('0') || *cp > _T('9')) return 0;

		while (*cp >= _T('0') && *cp <= _T('9'))
			lExp = lExp * 10 + *cp++ - _T('0');

		if(i) lExp = -lExp;
	}

	if (*cp != 0) return 0;

	// adjust exponent value with exponent from normalized mantissa
	lExp += nIp - 1;

	// calculate max. posive exponent
	for (nMaxExp = 5, i = 1; i < nExpLen; ++i)
		nMaxExp *= 10;

	// check range of exponent
	if ((lExp < 0 && -lExp >= nMaxExp) || (lExp >= nMaxExp))
		return 0;

	if (lExp < 0) lExp += 2 * nMaxExp;		// adjust negative offset

	for (i = nExpLen; i > 0; --i)			// convert number into digits
	{
		byNum[nMantLen + i] = (BYTE) (lExp % 10);
		lExp /= 10;
	}

	// copy to target in reversed order
	for (i = nMantLen + nExpLen; i >= 0; --i)
		*pbyNum++ = byNum[i];

	return nMantLen + nExpLen + 1;
}

static INT RPL_SetBcd(LPCTSTR cp,INT nMantLen,INT nExpLen,CONST TCHAR cDec,LPBYTE pbyNum,INT nSize)
{
	LPTSTR pszData;

	INT s = 0;

	if ((pszData = Trim(cp)) != NULL)		// create a trimmed working copy of the string
	{
		s = SetBcd(pszData,nMantLen,nExpLen,cDec,pbyNum,nSize);
		free(pszData);
	}
	return s;
}

static INT RPL_GetComplex(BYTE CONST *pbyNum,INT nMantLen,INT nExpLen,CONST TCHAR cDec,LPTSTR cp,INT nSize)
{
	INT   nLen,nPos;
	TCHAR cSep;

	cSep = (cDec == _T('.'))				// current separator
		 ? _T(',')							// radix mark '.' -> ',' separator
		 : _T(';');							// radix mark ',' -> ';' separator

	nPos = 0;								// write buffer position

	if (nSize < 5) return 0;				// target buffer to small
	nSize -= 4;								// reserved room for (,)\0

	cp[nPos++] = _T('(');					// start of complex number

	// real part
	nLen = RPL_GetBcd(pbyNum,nMantLen,nExpLen,cDec,&cp[1],nSize);
	if (nLen == 0) return 0;				// target buffer to small

	_ASSERT(nLen <= nSize);

	nPos += nLen;							// actual buffer postion
	nSize -= nLen;							// remainder target buffer size

	cp[nPos++] = cSep;						// write of complex number seperator

	// imaginary part
	nLen = RPL_GetBcd(&pbyNum[16],nMantLen,nExpLen,cDec,&cp[nPos],nSize);
	if (nLen == 0) return 0;				// target buffer to small

	nPos += nLen;							// actual buffer postion

	cp[nPos++] = _T(')');					// end of complex number
	cp[nPos] = 0;							// EOS

	_ASSERT(lstrlen(cp) == nPos);

	return nPos;
}

static INT RPL_SetComplex(LPCTSTR cp,INT nMantLen,INT nExpLen,CONST TCHAR cDec,LPBYTE pbyNum,INT nSize)
{
	LPTSTR pcSep,pszData;
	INT    nLen;
	TCHAR  cSep;

	nLen = 0;								// read data length

	cSep = (cDec == _T('.'))				// current separator
		 ? _T(',')							// radix mark '.' -> ',' separator
		 : _T(';');							// radix mark ',' -> ';' separator

	if ((pszData = Trim(cp)) != NULL)		// create a trimmed working copy of the string
	{
		INT nStrLength = lstrlen(pszData);	// string length

		// complex number with brackets around
		if (   nStrLength > 0
			&& pszData[0]              == _T('(')
			&& pszData[nStrLength - 1] == _T(')'))
		{
			pszData[--nStrLength] = 0;		// replace ')' with EOS

			// search for number separator
			if ((pcSep = _tcschr(pszData+1,cSep)) != NULL)
			{
				INT nLen1st;

				*pcSep = 0;					// set EOS for 1st substring

				// decode 1st substring
				nLen1st = RPL_SetBcd(pszData+1,nMantLen,nExpLen,cDec,&pbyNum[0],nSize);
				if (nLen1st > 0)
				{
					// decode 2nd substring
					nLen = RPL_SetBcd(pcSep+1,nMantLen,nExpLen,cDec,&pbyNum[nMantLen+nExpLen+1],nSize-nLen1st);
					if (nLen > 0)
					{
						nLen += nLen1st;	// complete Bcd length
					}
				}
			}
		}
		free(pszData);
	}
	return nLen;
}


//################
//#
//#    Object subroutines
//#
//################

static TCHAR GetRadix(VOID)
{
	BYTE byFlag;
	BOOL bPoint;

	// get locale decimal point
	// GetLocaleInfo(LOCALE_USER_DEFAULT,LOCALE_SDECIMAL,&cDecimal,1);

	// get calculator decimal point
	switch (cCurrentRomType)
	{
	case 'D': // HP42S (Davinci)
		Npeek(&byFlag,D_CALCLFLAGS,sizeof(byFlag));
		bPoint = (byFlag & D_sRADIX) != 0;
		break;
	case 'I': // HP14B (Midas)
		Npeek(&byFlag,I_SysFlags,sizeof(byFlag));
		bPoint = (byFlag & I_nRadix) == 0;
		break;
	case 'M': // HP27S (Mentor)
		Npeek(&byFlag,M_CALCSTATUS,sizeof(byFlag));
		bPoint = (byFlag & (1 << M_sRADIX)) == 0;
		break;
	case 'N': // HP32SII (Nardo)
		Npeek(&byFlag,N_SysFlags,sizeof(byFlag));
		bPoint = (byFlag & N_nRadix) == 0;
		break;
	case 'O': // HP28S (Orlando)
		{
			DWORD dwAddr = O_USERFLAGS + (O_fnRadix - 1) / 4;
			BYTE  byMask = 1 << ((O_fnRadix - 1) & 0x3);

			Npeek(&byFlag,dwAddr,sizeof(byFlag));
			bPoint = (byFlag & byMask) == 0;
		}
		break;
	case 'T': // HP17B (Trader)
		Npeek(&byFlag,T_CALCSTATUS,sizeof(byFlag));
		bPoint = (byFlag & (1 << T_sRADIX)) == 0;
		break;
	case 'U': // HP17BII (Trader II)
		Npeek(&byFlag,U_CALCSTATUS,sizeof(byFlag));
		bPoint = (byFlag & (1 << U_sRADIX)) == 0;
		break;
	case 'Y': // HP19BII (Tycoon II)
		Npeek(&byFlag,Y_CALCSTATUS,sizeof(byFlag));
		bPoint = (byFlag & (1 << Y_sRADIX)) == 0;
		break;
	default: _ASSERT(FALSE);
		bPoint = TRUE;
	}
	return bPoint ? _T('.') : _T(',');
}

static INT DoReal(DWORD dwAddress,LPTSTR cp,INT nSize)
{
	BYTE byNumber[16];

	// get real object content and decode it
	Npeek(byNumber,dwAddress,ARRAYSIZEOF(byNumber));
	return RPL_GetBcd(byNumber,12,3,GetRadix(),cp,nSize);
}

static INT DoComplex(DWORD dwAddress,LPTSTR cp,INT nSize)
{
	BYTE byNumber[32];

	// get complex object content and decode it
	Npeek(byNumber,dwAddress,ARRAYSIZEOF(byNumber));
	return RPL_GetComplex(byNumber,12,3,GetRadix(),cp,nSize);
}


//################
//#
//#    Stack routines
//#
//################

//
// ID_STACK_COPY
//
LRESULT OnStackCopy(VOID)					// copy data to clipboard
{
	TCHAR  cBuffer[64];
	HANDLE hClipObj;
	LPBYTE lpbyData;
	DWORD  dwAddress,dwObject,dwSize;
	UINT   uClipboardFormat;

	_ASSERT(nState == SM_RUN);				// emulator must be in RUN state
	if (WaitForSleepState())				// wait for cpu SHUTDN then sleep state
	{
		InfoMessage(_T("The emulator is busy."));
		return 0;
	}

	_ASSERT(nState == SM_SLEEP);

	switch (cCurrentRomType)
	{
	case 'D': // HP42S (Davinci)
		dwAddress = Read5(D_StackX);		// pick address of x register object
		dwObject = Read5(dwAddress);		// object prolog
		dwAddress += 5;						// skip prolog
		break;
	case 'I': // HP14B (Midas)
		dwAddress = I_X;					// x register object
		dwObject = D_DOREAL;				// interpret object as real
		break;
	case 'M': // HP27S (Mentor)
		dwAddress = M_X;					// x register object
		dwObject = D_DOREAL;				// interpret object as real
		break;
	case 'N': // HP32SII (Nardo)
		dwAddress = N_StackX;				// x register object
		dwObject = D_DOREAL;				// interpret object as real
		break;
	case 'O': // HP28S (Orlando)
		dwAddress = RPL_Pick(1);			// pick address of level1 object
		dwObject = Read5(dwAddress);		// object prolog
		dwAddress += 5;						// skip prolog
		break;
	case 'T': // HP17B (Trader)
		dwAddress = T_X;					// x register object
		dwObject = D_DOREAL;				// interpret object as real
		break;
	case 'U': // HP17BII (Trader II)
		dwAddress = U_X;					// x register object
		dwObject = D_DOREAL;				// interpret object as real
		break;
	case 'Y': // HP19BII (Tycoon II)
		dwAddress = Y_X;					// x register object
		dwObject = D_DOREAL;				// interpret object as real
		break;
	default:
		_ASSERT(FALSE);
	}

	_ASSERT(D_DOREAL == O_DOREAL);			// check if both objects have same address
	_ASSERT(D_DOCMP == O_DOCMP);			// check if both objects have same address

	switch (dwObject)						// select object
	{
	case O_DOREAL: // real object
	case O_DOCMP:  // complex
		switch (dwObject)
		{
		case O_DOREAL: // real object
			// get real object content and decode it
			dwSize = DoReal(dwAddress,cBuffer,ARRAYSIZEOF(cBuffer));
			break;
		case O_DOCMP: // complex object
			// get complex object content and decode it
			dwSize = DoComplex(dwAddress,cBuffer,ARRAYSIZEOF(cBuffer));
			break;
		}

		// calculate buffer size
		dwSize = (dwSize + 1) * sizeof(*cBuffer);

		// memory allocation for clipboard data
		if ((hClipObj = GlobalAlloc(GMEM_MOVEABLE,dwSize)) == NULL)
			goto error;

		if ((lpbyData = (LPBYTE) GlobalLock(hClipObj)))
		{
			// copy data to memory
			CopyMemory(lpbyData,cBuffer,dwSize);
			GlobalUnlock(hClipObj);			// unlock memory
		}

		#if defined _UNICODE
			uClipboardFormat = CF_UNICODETEXT;
		#else
			uClipboardFormat = CF_TEXT;
		#endif
		break;
	case O_DOCSTR: // string
		dwSize = (Read5(dwAddress) - 5) / 2; // length of string

		// memory allocation for clipboard data
		if ((hClipObj = GlobalAlloc(GMEM_MOVEABLE,dwSize + 1)) == NULL)
			goto error;

		if ((lpbyData = (LPBYTE) GlobalLock(hClipObj))) // lock memory
		{
			// copy data into clipboard buffer
			for (dwAddress += 5;dwSize-- > 0;dwAddress += 2,++lpbyData)
				*lpbyData = Read2(dwAddress);
			*lpbyData = 0;					// set EOS

			GlobalUnlock(hClipObj);			// unlock memory
		}
		uClipboardFormat = CF_TEXT;			// always text
		break;
	default:
		MessageBeep(MB_OK);					// error beep
		goto error;
	}

	if (OpenClipboard(hWnd))
	{
		if (EmptyClipboard())
			SetClipboardData(uClipboardFormat,hClipObj);
		else
			GlobalFree(hClipObj);
		CloseClipboard();
	}
	else									// clipboard open failed
	{
		GlobalFree(hClipObj);
	}

error:
	SwitchToState(SM_RUN);
	return 0;
}

//
// ID_STACK_PASTE
//
LRESULT OnStackPaste(VOID)					// paste data to stack
{
	#if defined _UNICODE
		#define CF_TEXTFORMAT CF_UNICODETEXT
	#else
		#define CF_TEXTFORMAT CF_TEXT
	#endif

	HANDLE hClipObj;

	BOOL bSuccess = FALSE;

	// check if clipboard format is available
	if (!IsClipboardFormatAvailable(CF_TEXTFORMAT))
	{
		MessageBeep(MB_OK);					// error beep
		return 0;
	}

	SuspendDebugger();						// suspend debugger
	bDbgAutoStateCtrl = FALSE;				// disable automatic debugger state control

	if (!(Chipset.IORam[DSPCTL]&DON))		// calculator off, turn on
	{
		KeyboardEvent(TRUE,0,0x8000);
		Sleep(dwWakeupDelay);
		KeyboardEvent(FALSE,0,0x8000);

		// wait for sleep mode
		while(Chipset.Shutdn == FALSE) Sleep(0);
	}

	_ASSERT(nState == SM_RUN);				// emulator must be in RUN state
	if (WaitForSleepState())				// wait for cpu SHUTDN then sleep state
	{
		InfoMessage(_T("The emulator is busy."));
		goto cancel;
	}

	_ASSERT(nState == SM_SLEEP);

	if (OpenClipboard(hWnd))
	{
		if ((hClipObj = GetClipboardData(CF_TEXTFORMAT)))
		{
			LPCTSTR lpstrClipdata;
			LPBYTE  lpbyData;

			if ((lpstrClipdata = (LPCTSTR) GlobalLock(hClipObj)))
			{
				BYTE  byNumber[32];
				DWORD dwAddress;
				INT   s;

				do
				{
					if (bDetectClpObject)	// autodetect clipboard object enabled
					{
						// try to convert string to real format
						_ASSERT(16 <= ARRAYSIZEOF(byNumber));
						s = RPL_SetBcd(lpstrClipdata,12,3,GetRadix(),byNumber,sizeof(byNumber));

						if (s > 0)					// is a real number
						{
							_ASSERT(s <= ARRAYSIZEOF(byNumber));

							// get TEMPOB memory for real object
							dwAddress = RPL_CreateTemp(16+5);
							if ((bSuccess = (dwAddress > 0)))
							{
								// check if both objects have same address
								_ASSERT(D_DOREAL == O_DOREAL);
								Write5(dwAddress,O_DOREAL);
								Nwrite(byNumber,dwAddress+5,s);

								if (cCurrentRomType == 'D') // HP42S (Davinci)
								{
									// push object to stack
									RPL_PushX(dwAddress);
								}
								else				// HP28S (Orlando)
								{
									// push object to stack
									RPL_Push(1,dwAddress);
								}
							}
							break;
						}

						// try to convert string to complex format
						_ASSERT(32 <= ARRAYSIZEOF(byNumber));
						s = RPL_SetComplex(lpstrClipdata,12,3,GetRadix(),byNumber,sizeof(byNumber));

						if (s > 0)			// is a real complex
						{
							_ASSERT(s == 32); // length of complex number BCD coded

							// get TEMPOB memory for complex object
							dwAddress = RPL_CreateTemp(16+16+5);
							if ((bSuccess = (dwAddress > 0)))
							{
								Write5(dwAddress,O_DOCMP); // prolog
								Nwrite(byNumber,dwAddress+5,s); // data

								if (cCurrentRomType == 'D') // HP42S (Davinci)
								{
									// push object to stack
									RPL_PushX(dwAddress);
								}
								else		// HP28S (Orlando)
								{
									// push object to stack
									RPL_Push(1,dwAddress);
								}
							}
							break;
						}
					}

					// any other format
					if (cCurrentRomType == 'O') // HP28S (Orlando)
					{
						DWORD dwSize = lstrlen(lpstrClipdata);
						if ((lpbyData = (LPBYTE) malloc(dwSize * 2)))
						{
							LPBYTE lpbySrc,lpbyDest;
							DWORD  dwLoop;

							#if defined _UNICODE
								// copy data UNICODE -> ASCII
								WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK,
													lpstrClipdata, dwSize,
													(LPSTR) lpbyData+dwSize, dwSize, NULL, NULL);
							#else
								// copy data
								memcpy(lpbyData+dwSize,lpstrClipdata,dwSize);
							#endif

							// unpack data
							lpbySrc = lpbyData+dwSize;
							lpbyDest = lpbyData;
							dwLoop = dwSize;
							while (dwLoop-- > 0)
							{
								BYTE byTwoNibs = *lpbySrc++;
								*lpbyDest++ = (BYTE) (byTwoNibs & 0xF);
								*lpbyDest++ = (BYTE) (byTwoNibs >> 4);
							}

							dwSize *= 2;	// size in nibbles

							// get TEMPOB memory for string object
							dwAddress = RPL_CreateTemp(dwSize+10);
							if ((bSuccess = (dwAddress > 0)))
							{
								Write5(dwAddress,O_DOCSTR); // String
								Write5(dwAddress+5,dwSize+5); // length of String
								Nwrite(lpbyData,dwAddress+10,dwSize); // data

								// push object to stack
								RPL_Push(1,dwAddress);
							}
							free(lpbyData);
						}
					}
				}
				while (FALSE);

				GlobalUnlock(hClipObj);
			}
		}
		CloseClipboard();
	}

	SwitchToState(SM_RUN);					// run state
	while (nState!=nNextState) Sleep(0);
	_ASSERT(nState == SM_RUN);

	if (bSuccess == FALSE)					// data not copied
	{
		MessageBeep(MB_OK);					// error beep
		goto cancel;
	}

	KeyboardEvent(TRUE,0,0x8000);
	Sleep(dwWakeupDelay);
	KeyboardEvent(FALSE,0,0x8000);

	// wait for sleep mode
	while(Chipset.Shutdn == FALSE) Sleep(0);

cancel:
	bDbgAutoStateCtrl = TRUE;				// enable automatic debugger state control
	ResumeDebugger();
	return 0;
	#undef CF_TEXTFORMAT
}
