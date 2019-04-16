/*
 *   settings.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gieﬂelink
 *
 */
#include "pch.h"
#include "Emu42.h"

// #define REGISTRY							// use registry instead of *.ini file

//################
//#
//#    Low level subroutines
//#
//################

#if !defined REGISTRY

// INI-file handling

#if !defined EMU42_INI
	#define EMU42_INI "Emu42.ini"
#endif

#define ReadString(sec,key,dv,v,sv) GetPrivateProfileString(sec,key,dv,v,sv,_T(EMU42_INI))
#define ReadInt(sec,key,dv)         GetPrivateProfileInt(sec,key,dv,_T(EMU42_INI));
#define WriteString(sec,key,v)      WritePrivateProfileString(sec,key,v,_T(EMU42_INI))
#define WriteInt(sec,key,v)         WritePrivateProfileInt(sec,key,v,_T(EMU42_INI))
#define DelKey(sec,key)             WritePrivateProfileString(sec,key,NULL,_T(EMU42_INI))

static BOOL WritePrivateProfileInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, int nValue, LPCTSTR lpszFilename)
{
	TCHAR s[16];
	wsprintf(s,_T("%i"),nValue);
	return WritePrivateProfileString(lpszSection, lpszEntry, s, lpszFilename);
}

#else

// registry handling

#if !defined REGISTRYKEY
	#define REGISTRYKEY "Software\\Emu42"
#endif

#define ReadString(sec,key,dv,v,sv) GetRegistryString(sec,key,dv,v,sv)
#define ReadInt(sec,key,dv)         GetRegistryInt(sec,key,dv)
#define WriteString(sec,key,v)      WriteReg(sec,key,REG_SZ,(BYTE *) v,(lstrlen(v)+1) * sizeof(*v))
#define WriteInt(sec,key,v)         WriteReg(sec,key,REG_DWORD,(BYTE *) &v,sizeof(int))
#define DelKey(sec,key)             DelReg(sec,key)

static VOID ReadReg(LPCTSTR lpSubKey, LPCTSTR lpValueName, LPBYTE lpData, DWORD *pdwSize)
{
	TCHAR lpKey[256] = _T(REGISTRYKEY) _T("\\");

	DWORD retCode,dwType;
	HKEY  hKey;

	lstrcat(lpKey, lpSubKey);				// full registry key

	retCode = RegOpenKeyEx(HKEY_CURRENT_USER,
						   lpKey,
						   0,
						   KEY_QUERY_VALUE,
						   &hKey);
	if (retCode == ERROR_SUCCESS)
	{
		retCode = RegQueryValueEx(hKey,lpValueName,NULL,&dwType,lpData,pdwSize);
		RegCloseKey(hKey);
	}

	if (retCode != ERROR_SUCCESS)			// registry entry not found
		*pdwSize = 0;						// return zero size
	return;
}

static BOOL WriteReg(LPCTSTR lpSubKey, LPCTSTR lpValueName, DWORD dwType, CONST BYTE *lpData, DWORD cbData)
{
	TCHAR lpKey[256] = _T(REGISTRYKEY) _T("\\");

	DWORD retCode;
	HKEY  hKey;
	DWORD dwDisposition;

	lstrcat(lpKey, lpSubKey);				// full registry key

	retCode = RegCreateKeyEx(HKEY_CURRENT_USER,
							 lpKey,
							 0,_T(""),
							 REG_OPTION_NON_VOLATILE,
							 KEY_WRITE,
							 NULL,
							 &hKey,
							 &dwDisposition);
	_ASSERT(retCode == ERROR_SUCCESS);

	RegSetValueEx(hKey,lpValueName,0,dwType,lpData,cbData);
	RegCloseKey(hKey);
	return retCode == ERROR_SUCCESS;
}

static BOOL DelReg(LPCTSTR lpSubKey, LPCTSTR lpValueName)
{
	TCHAR lpKey[256] = _T(REGISTRYKEY) _T("\\");

	DWORD retCode;
	HKEY  hKey;

	lstrcat(lpKey, lpSubKey);				// full registry key

	retCode = RegOpenKeyEx(HKEY_CURRENT_USER,
						   lpKey,
						   0,
						   KEY_SET_VALUE,
						   &hKey);
	if (retCode == ERROR_SUCCESS) 
	{
		retCode = RegDeleteValue(hKey,lpValueName);
		RegCloseKey(hKey);
	}
	return retCode == ERROR_SUCCESS;
}

static DWORD GetRegistryString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpDefault, LPTSTR lpData, DWORD dwSize)
{
	// buffer size in bytes
	DWORD dwBufSize = dwSize * sizeof(*lpData);

	ReadReg(lpszSection,lpszEntry,(LPBYTE) lpData,&dwBufSize);
	if (dwBufSize == 0)
	{
		lstrcpyn(lpData,lpDefault,dwSize);
		dwSize = lstrlen(lpData);
	}
	else
	{
		dwSize = (dwBufSize / sizeof(*lpData)) - 1;
	}
	return dwSize;
}

static UINT GetRegistryInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, INT nDefault)
{
	UINT  nValue;
	DWORD dwSize = sizeof(nValue);

	ReadReg(lpszSection,lpszEntry,(LPBYTE) &nValue,&dwSize);
	return dwSize ? nValue : nDefault;
}

#endif


//################
//#
//#    Public functions
//#
//################

VOID ReadSettings(VOID)
{
	// Files
	ReadString(_T("Files"),_T("Emu42Directory"),szCurrentDirectory,szEmuDirectory,ARRAYSIZEOF(szEmuDirectory));
	bAutoSave          = ReadInt(_T("Files"),_T("AutoSave"),bAutoSave);
	bAutoSaveOnExit    = ReadInt(_T("Files"),_T("AutoSaveOnExit"),bAutoSaveOnExit);
	bSaveDefConfirm    = ReadInt(_T("Files"),_T("SaveDefaultConfirm"),bSaveDefConfirm);
	bStartupBackup     = ReadInt(_T("Files"),_T("StartupBackup"),bStartupBackup);
	bLoadObjectWarning = ReadInt(_T("Files"),_T("LoadObjectWarning"),bLoadObjectWarning);
	// KML
	bAlwaysDisplayLog = ReadInt(_T("KML"),_T("AlwaysDisplayLog"),bAlwaysDisplayLog);
	// Debugger
	wInstrSize = ReadInt(_T("Debugger"),_T("LastInstrBufSize"),wInstrSize);
	// Disassembler
	disassembler_mode = ReadInt(_T("Disassembler"),_T("Mnemonics"),disassembler_mode);
	disassembler_symb = ReadInt(_T("Disassembler"),_T("Symbolic"),disassembler_symb);
	// Emulator
	bShowTitle       = ReadInt(_T("Emulator"),_T("ShowTitle"),bShowTitle);
	bShowMenu        = ReadInt(_T("Emulator"),_T("ShowMenu"),bShowMenu);
	bAlwaysOnTop     = ReadInt(_T("Emulator"),_T("AlwaysOnTop"),bAlwaysOnTop);
	bActFollowsMouse = ReadInt(_T("Emulator"),_T("ActivationFollowsMouse"),bActFollowsMouse);
	bClientWinMove   = ReadInt(_T("Emulator"),_T("ClientWinMove"),bClientWinMove);
	bSingleInstance  = ReadInt(_T("Emulator"),_T("SingleInstance"),bSingleInstance);
	bRealSpeed       = ReadInt(_T("Emulator"),_T("RealSpeed"),bRealSpeed);
	dwSacaCycles     = ReadInt(_T("Emulator"),_T("SacaCycles"),dwSacaCycles);
	dwLewisCycles    = ReadInt(_T("Emulator"),_T("LewisCycles"),dwLewisCycles);
	dwKeyMinDelay    = ReadInt(_T("Emulator"),_T("KeyMinDelay"),dwKeyMinDelay);
	dwWakeupDelay    = ReadInt(_T("Emulator"),_T("WakeupDelay"),dwWakeupDelay);
	uWaveDevId       = ReadInt(_T("Emulator"),_T("WaveDeviceId"),uWaveDevId);
	dwWaveVol        = ReadInt(_T("Emulator"),_T("WaveVolume"),dwWaveVol);
	dwWaveTime       = ReadInt(_T("Emulator"),_T("WaveTime"),dwWaveTime);
	// LowBat
	bLowBatDisable = ReadInt(_T("LowBat"),_T("Disable"),bLowBatDisable);
	// Macro
	bMacroRealSpeed = ReadInt(_T("Macro"),_T("RealSpeed"),bMacroRealSpeed);
	nMacroTimeout   = ReadInt(_T("Macro"),_T("ReplayTimeout"),nMacroTimeout);
	dwMacroMinDelay = ReadInt(_T("Macro"),_T("KeyMinDelay"),dwMacroMinDelay);
	// IrPrinter
	ReadString(_T("IrPrinter"),_T("Address"),szUdpServer,szUdpServer,ARRAYSIZEOF(szUdpServer));
	wUdpPort = ReadInt(_T("IrPrinter"),_T("Port"),wUdpPort);
	return;
}

VOID WriteSettings(VOID)
{
	// Files
	WriteString(_T("Files"),_T("Emu42Directory"),szEmuDirectory);
	WriteInt(_T("Files"),_T("AutoSave"),bAutoSave);
	WriteInt(_T("Files"),_T("AutoSaveOnExit"),bAutoSaveOnExit);
	WriteInt(_T("Files"),_T("SaveDefaultConfirm"),bSaveDefConfirm);
	WriteInt(_T("Files"),_T("StartupBackup"),bStartupBackup);
	WriteInt(_T("Files"),_T("LoadObjectWarning"),bLoadObjectWarning);
	// KML
	WriteInt(_T("KML"),_T("AlwaysDisplayLog"),bAlwaysDisplayLog);
	// Debugger
	WriteInt(_T("Debugger"),_T("LastInstrBufSize"),wInstrSize);
	// Disassembler
	WriteInt(_T("Disassembler"),_T("Mnemonics"),disassembler_mode);
	WriteInt(_T("Disassembler"),_T("Symbolic"),disassembler_symb);
	// Emulator
	WriteInt(_T("Emulator"),_T("ShowTitle"),bShowTitle);
	WriteInt(_T("Emulator"),_T("ShowMenu"),bShowMenu);
	WriteInt(_T("Emulator"),_T("AlwaysOnTop"),bAlwaysOnTop);
	WriteInt(_T("Emulator"),_T("ActivationFollowsMouse"),bActFollowsMouse);
	WriteInt(_T("Emulator"),_T("ClientWinMove"),bClientWinMove);
	WriteInt(_T("Emulator"),_T("SingleInstance"),bSingleInstance);
	WriteInt(_T("Emulator"),_T("RealSpeed"),bRealSpeed);
	WriteInt(_T("Emulator"),_T("SacaCycles"),dwSacaCycles);
	WriteInt(_T("Emulator"),_T("LewisCycles"),dwLewisCycles);
	WriteInt(_T("Emulator"),_T("KeyMinDelay"),dwKeyMinDelay);
	WriteInt(_T("Emulator"),_T("WakeupDelay"),dwWakeupDelay);
	WriteInt(_T("Emulator"),_T("WaveDeviceId"),uWaveDevId);
	WriteInt(_T("Emulator"),_T("WaveVolume"),dwWaveVol);
	WriteInt(_T("Emulator"),_T("WaveTime"),dwWaveTime);
	// LowBat
	WriteInt(_T("LowBat"),_T("Disable"),bLowBatDisable);
	// Macro
	WriteInt(_T("Macro"),_T("RealSpeed"),bMacroRealSpeed);
	WriteInt(_T("Macro"),_T("ReplayTimeout"),nMacroTimeout);
	WriteInt(_T("Macro"),_T("KeyMinDelay"),dwMacroMinDelay);
	// IrPrinter
	WriteString(_T("IrPrinter"),_T("Address"),szUdpServer);
	WriteInt(_T("IrPrinter"),_T("Port"),wUdpPort);
	return;
}

VOID ReadLastDocument(LPTSTR szFilename, DWORD nSize)
{
	ReadString(_T("Files"),_T("LastDocument"),_T(""),szFilename,nSize);
	return;
}

VOID WriteLastDocument(LPCTSTR szFilename)
{
	WriteString(_T("Files"),_T("LastDocument"),szFilename);
	return;
}

VOID ReadSettingsString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpDefault, LPTSTR lpData, DWORD dwSize)
{
	ReadString(lpszSection,lpszEntry,lpDefault,lpData,dwSize);
	return;
}

VOID WriteSettingsString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPTSTR lpData)
{
	WriteString(lpszSection,lpszEntry,lpData);
	return;
}

INT ReadSettingsInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, INT nDefault)
{
	return ReadInt(lpszSection,lpszEntry,nDefault);
}

VOID WriteSettingsInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, INT nValue)
{
	WriteInt(lpszSection,lpszEntry,nValue);
	return;
}

VOID DelSettingsKey(LPCTSTR lpszSection, LPCTSTR lpszEntry)
{
	DelKey(lpszSection,lpszEntry);
	return;
}
