/*
 *   Emu42.h
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gieﬂelink
 *
 */
#include "types.h"

#define	ARRAYSIZEOF(a)	(sizeof(a) / sizeof(a[0]))

#define	_B(a)			((a)*2)
#define	_KB(a)			((a)*2*1024)

#define HARDWARE		"Lewis"				// emulator hardware

// valid calculator models
#define MODELS			(MODELS_BERT MODELS_SACA MODELS_LEWIS)

#define MODELS_BERT		_T("CEF")			// valid calculator models for Bert hardware
#define MODELS_SACA		_T("AILN")			// valid calculator models for Sacajawea hardware
#define MODELS_LEWIS	_T("DMOTUY")		// valid calculator models for Lewis hardware

#define OBJECT_EN		_T("DLNO")			// valid calculator models for Load/Save Object enable
#define STACK_C_EN		_T("ACDEFILMNOTUY")	// valid calculator models for Stack Copy enable
#define STACK_P_EN		_T("DLNO")			// valid calculator models for Stack Paste enable

#define BINARYHEADER28S "HPHP28-S"

#define RAW_FILTER		"User Code Files (*.RAW)\0*.RAW\0All Files (*.*)\0*.*\0"
#define HP_FILTER		"HP Binary Object (*.HP)\0*.HP\0All Files (*.*)\0*.*\0"

#define CF_HPOBJ		"CF_HPOBJ"			// clipboard format for DDE

#define T2CYCLES		(dwCyclesRate)		// CPU cycles in 16384 Hz time frame

#define SM_RUN			0					// states of cpu emulation thread
#define SM_INVALID		1
#define SM_RETURN		2
#define SM_SLEEP		3

#define S_ERR_NO		0					// stack errorcodes
#define S_ERR_OBJECT	1
#define S_ERR_BINARY	2
#define S_ERR_ASCII		3

#define BAD_OB			(0xFFFFFFFF)		// bad object

#define HP_MNEMONICS	FALSE				// disassembler mnenomics mode
#define CLASS_MNEMONICS	TRUE

#define MACRO_OFF	0						// macro recorder off
#define MACRO_NEW	1
#define MACRO_PLAY	2

// window styles
#define STYLE_TITLE		(WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_OVERLAPPED)
#define STYLE_NOTITLE	(WS_POPUP|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPSIBLINGS)

// WM_COPYDATA identifier
#define CDID_FILENAME	1					// send file name

// macro to check for valid calculator model
#define isModelValid(m)	(m != 0 && _tcschr(MODELS,      m) != NULL)
#define isModelLewis(m)	(m != 0 && _tcschr(MODELS_LEWIS,m) != NULL)
#define isModelSaca(m)	(m != 0 && _tcschr(MODELS_SACA, m) != NULL)
#define isModelBert(m)	(m != 0 && _tcschr(MODELS_BERT, m) != NULL)

// macro to check for menu enabled
#define isObjectEn(m)	(m != 0 && _tcschr(OBJECT_EN, m) != NULL)
#define isStackCEn(m)	(m != 0 && _tcschr(STACK_C_EN,m) != NULL)
#define isStackPEn(m)	(m != 0 && _tcschr(STACK_P_EN,m) != NULL)

// values for emulator hardware platform
enum HDW_PLATFORM { HDW_UNKNOWN = 0, HDW_LEWIS, HDW_SACA, HDW_BERT };

// values for selection of Lewis chip
enum CHIP { MASTER = 0, SLAVE };

// values for mapping area
enum MMUMAP { M_NONE,
			  M_REG,  M_DISP,  M_NCE2,  M_NCE3,  M_ROM,
			  M_REGS, M_DISPS, M_NCE2S, M_NCE3S, M_ROMS,
			  B_CTRL, B_DISP,  B_RAM,   B_ROM
			};

// values for disassembler memory mapping modes
enum MEM_MAPPING { MEM_MMU,
				   MEM_NCE1_M, MEM_NCE2_M, MEM_NCE3_M,
				   MEM_NCE1_S, MEM_NCE2_S, MEM_NCE3_S
				 };

// Emu42.c
extern HPALETTE			hPalette;
extern HPALETTE			hOldPalette;
extern HANDLE			hEventShutdn;
extern LPTSTR			szAppName;
extern LPTSTR			szTopic;
extern LPTSTR			szTitle;
extern CRITICAL_SECTION	csGDILock;
extern CRITICAL_SECTION csLcdLock;
extern CRITICAL_SECTION	csKeyLock;
extern CRITICAL_SECTION	csIOLock;
extern CRITICAL_SECTION	csT1Lock;
extern CRITICAL_SECTION	csT2Lock;
extern CRITICAL_SECTION csSlowLock;
extern CRITICAL_SECTION csDbgLock;
extern INT				nArgc;
extern LPCTSTR			*ppArgv;
extern LARGE_INTEGER	lFreq;
extern DWORD			idDdeInst;
extern UINT				uCF_HpObj;
extern HINSTANCE		hApp;
extern HWND				hWnd;
extern HWND				hDlgDebug;
extern HWND				hDlgFind;
extern HWND				hDlgProfile;
extern HWND				hDlgRplObjView;
extern HDC				hWindowDC;
extern DWORD			dwTColor;
extern DWORD			dwTColorTol;
extern HRGN				hRgn;
extern HCURSOR			hCursorArrow;
extern HCURSOR			hCursorHand;
extern UINT				uWaveDevId;
extern DWORD			dwWakeupDelay;
extern BOOL				bAutoSave;
extern BOOL				bAutoSaveOnExit;
extern BOOL				bSaveDefConfirm;
extern BOOL				bStartupBackup;
extern BOOL				bAlwaysDisplayLog;
extern BOOL				bLoadObjectWarning;
extern BOOL				bShowTitle;
extern BOOL				bShowMenu;
extern BOOL				bAlwaysOnTop;
extern BOOL				bActFollowsMouse;
extern BOOL				bClientWinMove;
extern BOOL				bSingleInstance;
extern HANDLE			hThread;
extern VOID				SetWindowTitle(LPCTSTR szString);
extern VOID				ForceForegroundWindow(HWND hWnd);
extern VOID				CopyItemsToClipboard(HWND hWnd);

// mru.c
extern BOOL    MruInit(UINT nNum);
extern VOID    MruCleanup(VOID);
extern VOID    MruAdd(LPCTSTR lpszEntry);
extern VOID    MruRemove(UINT nIndex);
extern VOID    MruMoveTop(UINT nIndex);
extern UINT    MruEntries(VOID);
extern UINT    MruID(LPCTSTR lpszEntry);
extern VOID    MruFilename(UINT nIndex, LPTSTR szFilename, UINT nBuffersize);
extern VOID    MruUpdateMenu(HMENU hMenu);
extern VOID    MruWriteList(VOID);
extern VOID    MruReadList(VOID);

// Settings.c
extern VOID ReadSettings(VOID);
extern VOID WriteSettings(VOID);
extern VOID ReadLastDocument(LPTSTR szFileName, DWORD nSize);
extern VOID WriteLastDocument(LPCTSTR szFilename);
extern VOID ReadSettingsString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPCTSTR lpDefault, LPTSTR lpData, DWORD dwSize);
extern VOID WriteSettingsString(LPCTSTR lpszSection, LPCTSTR lpszEntry, LPTSTR lpData);
extern INT  ReadSettingsInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, INT nDefault);
extern VOID WriteSettingsInt(LPCTSTR lpszSection, LPCTSTR lpszEntry, INT nValue);
extern VOID DelSettingsKey(LPCTSTR lpszSection, LPCTSTR lpszEntry);

// Display.c
extern UINT   nBackgroundX;
extern UINT   nBackgroundY;
extern UINT   nBackgroundW;
extern UINT   nBackgroundH;
extern UINT   nLcdX;
extern UINT   nLcdY;
extern UINT   nLcdZoom;
extern UINT   nLcdyZoom;
extern UINT   nLcdXOff;
extern UINT   nLcdYOff;
extern UINT   nLcdXPixel;
extern UINT   nLcdYPixel;
extern UINT   nLcdDistance;
extern HDC    hLcdDC;
extern HDC    hMainDC;
extern HDC    hAnnunDC;
extern VOID   (*UpdateContrast)(VOID);
extern VOID   SetLcdColor(UINT nId, UINT nRed, UINT nGreen, UINT nBlue);
extern VOID   GetSizeLcdBitmap(INT *pnX, INT *pnY);
extern VOID   CreateLcdBitmap(VOID);
extern VOID   DestroyLcdBitmap(VOID);
extern BOOL   CreateMainBitmap(LPCTSTR szFilename);
extern VOID   DestroyMainBitmap(VOID);
extern BOOL   CreateAnnunBitmap(LPCTSTR szFilename);
extern BOOL   CreateAnnunBitmapFromMain(VOID);
extern VOID   SetAnnunBitmap(HDC hDC, HBITMAP hBitmap);
extern VOID   DestroyAnnunBitmap(VOID);
extern VOID   StartDisplay(VOID);
extern VOID   StopDisplay(VOID);
extern VOID   UpdateAnnunciators(DWORD dwUpdateMask);
extern VOID   ResizeWindow(VOID);

// Dispnum.c
extern VOID GetLcdNumber(LPTSTR szContent);

// Engine.c
extern CHIPSET_M Chipset;
extern CHIPSET_S ChipsetS;
extern BOOL      bInterrupt;
extern UINT      nState;
extern UINT      nNextState;
extern BOOL      bEnableSlow;
extern BOOL      bRealSpeed;
extern BOOL      bKeySlow;
extern BOOL      bSoundSlow;
extern UINT      nOpcSlow;
extern DWORD     dwSacaCycles;
extern DWORD     dwLewisCycles;
extern DWORD     dwCyclesRate;
extern HANDLE    hEventDebug;
extern BOOL      bDbgAutoStateCtrl;
extern INT       nDbgState;
extern BOOL      bDbgNOP3;
extern BOOL      bDbgRPL;
extern BOOL      bDbgCode;
extern BOOL      bDbgSkipInt;
extern DWORD     dwDbgStopPC;
extern DWORD     dwDbgRplPC;
extern DWORD     dwDbgRstkp;
extern DWORD     dwDbgRstk;
extern DWORD     *pdwInstrArray;
extern WORD      wInstrSize;
extern WORD      wInstrWp;
extern WORD      wInstrRp;
extern VOID      (*fnOutTrace)(VOID);
extern VOID      SuspendDebugger(VOID);
extern VOID      ResumeDebugger(VOID);
extern VOID      InitAdjustSpeed(VOID);
extern VOID      AdjKeySpeed(VOID);
extern VOID      SetSpeed(BOOL bAdjust);
extern BOOL      WaitForSleepState(VOID);
extern UINT      SwitchToState(UINT nNewState);
extern UINT      WorkerThread(LPVOID pParam);

// Fetch.c
extern VOID    EvalOpcode(LPBYTE I);

// Files.c
extern TCHAR   szEmuDirectory[MAX_PATH];
extern TCHAR   szCurrentDirectory[MAX_PATH];
extern TCHAR   szCurrentKml[MAX_PATH];
extern TCHAR   szBackupKml[MAX_PATH];
extern TCHAR   szCurrentFilename[MAX_PATH];
extern TCHAR   szBackupFilename[MAX_PATH];
extern TCHAR   szBufferFilename[MAX_PATH];
extern BOOL    bDocumentAvail;
extern LPBYTE  mNCE2;
extern LPBYTE  mNCE3;
extern LPBYTE  sNCE2;
extern LPBYTE  sNCE3;
extern LPBYTE  pbyRom;
extern DWORD   dwRomSize;
extern WORD    wRomCrc;
extern UINT    nCurrentHardware;
extern BYTE    cCurrentRomType;
extern UINT    nCurrentClass;
extern BOOL    bBackup;
extern VOID    SetWindowLocation(HWND hWnd,INT nPosX,INT nPosY);
extern DWORD   GetCutPathName(LPCTSTR szFileName,LPTSTR szBuffer,DWORD dwBufferLength,INT nCutLength);
extern VOID    SetWindowPathTitle(LPCTSTR szFileName);
extern BOOL    CheckForBeepPatch(VOID);
extern BOOL    PatchRom(LPCTSTR szFilename);
extern BOOL    CrcRom(WORD *pwChk);
extern BOOL    MapRom(LPCTSTR szFilename);
extern BOOL    MapRomBmp(HBITMAP hBmp);
extern VOID    UnmapRom(VOID);
extern VOID    ResetDocument(VOID);
extern BOOL    NewDocument(VOID);
extern BOOL    OpenDocument(LPCTSTR szFilename);
extern BOOL    SaveDocument(VOID);
extern BOOL    SaveDocumentAs(LPCTSTR szFilename);
extern BOOL    SaveBackup(VOID);
extern BOOL    RestoreBackup(VOID);
extern BOOL    ResetBackup(VOID);
extern BOOL    GetOpenFilename(VOID);
extern BOOL    GetSaveAsFilename(VOID);
extern BOOL    GetLoadObjectFilename(LPCTSTR lpstrFilter,LPCTSTR lpstrDefExt);
extern BOOL    GetSaveObjectFilename(LPCTSTR lpstrFilter,LPCTSTR lpstrDefExt);
extern WORD    WriteStack(UINT nStkLevel,LPBYTE lpBuf,DWORD dwSize);
extern BOOL    LoadObject(LPCTSTR szFilename);
extern BOOL    SaveObject(LPCTSTR szFilename);
extern BOOL    LoadIconFromFile(LPCTSTR szFilename);
extern VOID    LoadIconDefault(VOID);
extern HBITMAP LoadBitmapFile(LPCTSTR szFilename,BOOL bPalette);
extern HRGN    CreateRgnFromBitmap(HBITMAP hBmp,COLORREF color,DWORD dwTol);

// Timer.c
extern VOID  SetHPTime(VOID);
extern VOID  StartTimers(enum CHIP eChip);
extern VOID  StopTimers(enum CHIP eChip);
extern DWORD ReadT2(enum CHIP eChip);
extern VOID  SetT2(enum CHIP eChip, DWORD dwValue);
extern BYTE  ReadT1(enum CHIP eChip);
extern VOID  SetT1(enum CHIP eChip, BYTE byValue);
extern VOID  StartTimerBert(VOID);
extern VOID  StopTimerBert(VOID);

// Mops.c
extern DWORD       dwPageBits;
extern DWORD       dwAddrBits;
extern DWORD       dwAddrSize;
extern LPBYTE      RMap[1<<11];
extern LPBYTE      WMap[1<<11];
extern WORD        UpCRC(WORD wCRC, BYTE nib);
extern VOID        InitIO(VOID);
extern VOID        (*fnMap)(WORD a, WORD b);
extern VOID        Config(VOID);
extern VOID        Uncnfg(VOID);
extern enum MMUMAP MapData(DWORD d);
extern VOID        Reset(VOID);
extern VOID        C_Eq_Id(VOID);
extern VOID        CpuReset(VOID);
extern VOID        Npeek(BYTE *a, DWORD d, UINT s);
extern VOID        Nread(BYTE *a, DWORD d, UINT s);
extern VOID        Nwrite(BYTE *a, DWORD d, UINT s);
extern BYTE        Read2(DWORD d);
extern DWORD       Read5(DWORD d);
extern VOID        Write5(DWORD d, DWORD n);
extern VOID        Write2(DWORD d, BYTE n);
extern VOID        IOBit(BYTE *d, BYTE b, BOOL s);

// Lowbat.c
extern BOOL bLowBatDisable;
extern VOID StartBatMeasure(VOID);
extern VOID StopBatMeasure(VOID);
extern VOID GetBatteryState(BOOL *pbLBI, BOOL *pbVLBI);

// Keyboard.c
extern DWORD dwKeyMinDelay;
extern VOID  ScanKeyboard(BOOL bActive, BOOL bReset);
extern VOID  KeyboardEvent(BOOL bPress, UINT out, UINT in);

// Keymacro.c
extern INT     nMacroState;
extern INT     nMacroTimeout;
extern BOOL    bMacroRealSpeed;
extern DWORD   dwMacroMinDelay;
extern VOID    KeyMacroRecord(BOOL bPress, UINT out, UINT in);
extern LRESULT OnToolMacroNew(VOID);
extern LRESULT OnToolMacroPlay(VOID);
extern LRESULT OnToolMacroStop(VOID);
extern LRESULT OnToolMacroSettings(VOID);

// Redeye.c 
extern VOID IrPrinter(BYTE c);

// Udp.c 
extern TCHAR szUdpServer[1024];
extern WORD  wUdpPort;
extern VOID  ResetUdp(VOID);
extern BOOL  SendByteUdp(BYTE byData);

// Stack.c
extern BOOL    bDetectClpObject;
extern LRESULT OnStackCopy(VOID);
extern LRESULT OnStackPaste(VOID);

// Usrprg32.c
extern BOOL GetUserCode32(LPCTSTR szUCFile);
extern BOOL OnSelectProgram32(VOID);

// Usrprg42.c
extern BOOL GetUserCode42(LPCTSTR szUCFile);
extern BOOL OnSelectProgram42(VOID);

// RPL.c
extern DWORD RPL_SkipOb(DWORD d);
extern DWORD RPL_ObjectSize(BYTE *o,DWORD s);
extern DWORD RPL_CreateTemp(DWORD l);
extern UINT  RPL_Depth(VOID);
extern DWORD RPL_Pick(UINT l);
extern VOID  RPL_Replace(DWORD n);
extern VOID  RPL_Push(UINT l,DWORD n);
extern VOID  RPL_PushX(DWORD n);

// SndEnum.c
extern VOID SetSoundDeviceList(HWND hWnd,UINT uDeviceID);

// Sound.c
extern DWORD dwWaveVol;
extern DWORD dwWaveTime;
extern BOOL  SoundAvailable(UINT uDeviceID);
extern BOOL  SoundGetDeviceID(UINT *puDeviceID);
extern BOOL  SoundOpen(UINT uDeviceID);
extern VOID  SoundClose(VOID);
extern VOID  SoundOut(CHIPSET_M* w, WORD wOut);
extern VOID  SoundBeep(DWORD dwFrequency, DWORD dwDuration);

// DDEserv.c
extern HDDEDATA CALLBACK DdeCallback(UINT, UINT, HCONV, HSZ, HSZ, HDDEDATA, DWORD, DWORD);

// Dismem.c
extern BOOL             SetMemRomType(BYTE cCurrentRomType);
extern BOOL             SetMemMapType(enum MEM_MAPPING eType);
extern enum MEM_MAPPING GetMemMapType(VOID);
extern BOOL             GetMemAvail(enum MEM_MAPPING eType);
extern DWORD            GetMemDataSize(VOID);
extern DWORD            GetMemDataMask(VOID);
extern BYTE             GetMemNib(DWORD *p);
extern VOID             GetMemPeek(BYTE *a, DWORD d, UINT s);

// Disasm.c
extern BOOL  disassembler_mode;
extern BOOL  disassembler_symb;
extern DWORD disassemble(DWORD addr, LPTSTR out);

// Symbfile.c
extern BOOL    RplTableEmpty(VOID);
extern BOOL    RplLoadTable(LPCTSTR lpszFilename);
extern VOID    RplDeleteTable(VOID);
extern LPCTSTR RplGetName(DWORD dwAddr);
extern BOOL    RplGetAddr(LPCTSTR lpszName, DWORD *pdwAddr);

// Cursor.c
extern HCURSOR CreateHandCursor(VOID);

#if defined _USRDLL							// DLL version
// Emu42dll.c
extern VOID (CALLBACK *pEmuDocumentNotify)(LPCTSTR lpszFilename);
extern BOOL DLLDestroyWnd(VOID);

// Symbfile.c
#define RplGetName(a)		NULL			// for linking
#endif

// Message Boxes
static __inline int InfoMessage(LPCTSTR szMessage)  {return MessageBox(hWnd, szMessage, szTitle, MB_APPLMODAL|MB_OK|MB_ICONINFORMATION|MB_SETFOREGROUND);}
static __inline int AbortMessage(LPCTSTR szMessage) {return MessageBox(hWnd, szMessage, szTitle, MB_APPLMODAL|MB_OK|MB_ICONSTOP|MB_SETFOREGROUND);}
static __inline int YesNoMessage(LPCTSTR szMessage) {return MessageBox(hWnd, szMessage, szTitle, MB_APPLMODAL|MB_YESNO|MB_ICONEXCLAMATION|MB_SETFOREGROUND);}
static __inline int YesNoCancelMessage(LPCTSTR szMessage,UINT uStyle) {return MessageBox(hWnd, szMessage, szTitle, MB_APPLMODAL|MB_YESNOCANCEL|MB_ICONEXCLAMATION|MB_SETFOREGROUND|uStyle);}

// Missing Win32 API calls
static __inline LPTSTR DuplicateString(LPCTSTR szString)
{
	UINT   uLength = lstrlen(szString) + 1;
	LPTSTR szDup   = (LPTSTR) malloc(uLength*sizeof(szDup[0]));
	lstrcpy(szDup,szString);
	return szDup;
}
