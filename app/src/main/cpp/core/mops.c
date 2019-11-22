/*
 *   mops.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gieﬂelink
 *
 */
#include "pch.h"
#include "Emu42.h"
#include "ops.h"
#include "opcodes.h"
#include "io.h"

// #define DEBUG_IO							// switch for I/O debug purpose
// #define DEBUG_MEMACC						// switch for MEMORY access debug purpose

// Bert MMU defines
#define MMU_ROM_BASE	0x0000
#define MMU_ROM_MASK	0x8000
#define MMU_ROM_SIZE	0x8000
#define MMU_DRAM_BASE	0x8000
#define MMU_DRAM_MASK	0xE000
#define MMU_DRAM_SIZE	0x0200
#define MMU_DISP_BASE	0xA000
#define MMU_DISP_MASK	0xE000
#define MMU_DISP_SIZE	0x0040
#define MMU_CTRL_BASE	0xC000
#define MMU_CTRL_MASK	0xE000
#define MMU_CTRL_SIZE	0x0004
#define MMU_NONE_BASE	0xE000
#define MMU_NONE_MASK	0xE000
#define MMU_NONE_SIZE	0x0000

// defines for MODE register
#define LEWIS_MASTER	0x0
#define LEWIS_SLAVE		0x5

// defines for reading an open data bus
#define READEVEN		0x0A
#define READODD			0x03

// CRC calculation
static WORD crc_table[] =
{
	0x0000, 0x6801, 0xD002, 0xB803, 0xC805, 0xA004, 0x1807, 0x7006,
	0xF80B, 0x900A, 0x2809, 0x4008, 0x300E, 0x580F, 0xE00C, 0x880D
};
WORD UpCRC(WORD wCRC, BYTE nib)
{
	return ((wCRC << 4) | nib) ^ crc_table[wCRC >> 12];
}

static __inline UINT MIN(UINT a, UINT b)
{
	return (a<b)?a:b;
}

static __inline UINT MAX(UINT a, UINT b)
{
	return (a>b)?a:b;
}

static __inline VOID NunpackCRC(BYTE *a, DWORD b, UINT s, BOOL bUpdate)
{
	UINT i;
	for (i = 0; i < s; ++i)
	{
		a[i] = (BYTE)(b&0xf);
		if (bUpdate)
		{
			Chipset.crc  = UpCRC(Chipset.crc,a[i]);
			ChipsetS.crc = UpCRC(ChipsetS.crc,0);
		}
		b>>=4;
	}
}

// MMU bit settings
DWORD dwPageBits;							// valid bits for the page
DWORD dwAddrBits;							// valid bits for the address
DWORD dwAddrSize;							// address size (in nibbles)

// port mapping
LPBYTE RMap[1<<11] = {NULL,};
LPBYTE WMap[1<<11] = {NULL,};

// function pointer for mapping
static VOID MapBert(WORD a, WORD b);
static VOID MapLewis(WORD a, WORD b);

VOID (*fnMap)(WORD a, WORD b) = NULL;

// function pointer for IO memory access
static VOID ReadBertIO(BYTE *a, DWORD d, DWORD s, BOOL bUpdate);
static VOID WriteBertIO(BYTE *a, DWORD d, DWORD s);

static VOID ReadSacajaweaIO(BYTE *a, DWORD d, DWORD s, BOOL bUpdate);
static VOID WriteSacajaweaIO(BYTE *a, DWORD d, DWORD s);

static VOID ReadLewisIO(BYTE *a, DWORD b, DWORD s, BOOL bUpdate);
static VOID WriteLewisIO(BYTE *a, DWORD b, DWORD s);
static VOID ReadLewisSlaveIO(BYTE *a, DWORD d, DWORD s, BOOL bUpdate);
static VOID WriteLewisSlaveIO(BYTE *a, DWORD d, DWORD s);

static VOID (*fnReadIO)(BYTE *a, DWORD d, DWORD s, BOOL bUpdate) = NULL;
static VOID (*fnWriteIO)(BYTE *a, DWORD d, DWORD s) = NULL;

// values for chip enables
enum NCEPORT
{
	MASTER_NROM = 0,
	MASTER_NCE2,
	MASTER_NCE3,
	MASTER_NDISP,
	SLAVE_NROM,
	SLAVE_NCE2,
	SLAVE_NCE3,
	SLAVE_NDISP
};

typedef struct
{
	DWORD  *pdwBase;						// base address
	DWORD  *pdwMask;						// don't care mask
	DWORD  *pdwSize;						// real size of module in Nibbles
	LPBYTE *ppNCE;							// pointer to memory
} NCEDATA;

static LPBYTE pbyNull = NULL;				// for ROM mapping

// references to the display/timer area for mapping
static DWORD  dwMasterDispSize = sizeof(Chipset.IORam);
static LPBYTE pbyMasterDisp = Chipset.IORam;
static DWORD  dwSlaveDispSize = sizeof(ChipsetS.IORam);
static LPBYTE pbySlaveDisp = ChipsetS.IORam;

static const NCEDATA NCE[] =
{
	{ &Chipset.RomBase,   &Chipset.RomMask,   &Chipset.RomSize,   &pbyNull       },
	{ &Chipset.NCE2Base,  &Chipset.NCE2Mask,  &Chipset.NCE2Size,  &mNCE2         },
	{ &Chipset.NCE3Base,  &Chipset.NCE3Mask,  &Chipset.NCE3Size,  &mNCE3         },
	{ &Chipset.DispBase,  &Chipset.DispMask,  &dwMasterDispSize,  &pbyMasterDisp },
	{ &ChipsetS.RomBase,  &ChipsetS.RomMask,  &ChipsetS.RomSize,  &pbyNull       },
	{ &ChipsetS.NCE2Base, &ChipsetS.NCE2Mask, &ChipsetS.NCE2Size, &sNCE2         },
	{ &ChipsetS.NCE3Base, &ChipsetS.NCE3Mask, &ChipsetS.NCE3Size, &sNCE3         },
	{ &ChipsetS.DispBase, &ChipsetS.DispMask, &dwSlaveDispSize,   &pbySlaveDisp  }
};

static VOID MapNCE(enum NCEPORT s, WORD a, WORD b, DWORD *pdwRomOff)
{
	LPBYTE pbyRomPage;
	UINT   i, uAddr, uMask;
	DWORD  dwRomEnd;
	DWORD  p, m;

	_ASSERT(s >= 0 && s < ARRAYSIZEOF(NCE));

	// n-island mask = don't care? or no chip connected
	if (!(*NCE[s].pdwMask && *NCE[s].pdwSize))
		return;

	if (*NCE[s].ppNCE == NULL)				// module is ROM
	{
		if (*pdwRomOff >= dwRomSize)		// ROM file not big enough
			return;							// no mapping

		dwRomEnd = dwRomSize - *pdwRomOff;	// size of remaining ROM
		pbyRomPage = pbyRom + *pdwRomOff;	// base for ROM access
		*pdwRomOff += *NCE[s].pdwSize;		// offset in file for next ROM module
	}

	uAddr = *NCE[s].pdwBase >> dwAddrBits;	// base address in pages
	uMask = *NCE[s].pdwMask >> dwAddrBits;	// address mask in pages

	a = (WORD)MAX(a,uAddr);
	b = (WORD)MIN(b,(*NCE[s].pdwBase + (~*NCE[s].pdwMask & 0xFFFFF)) >> dwAddrBits);
	// check if size a power of 2
	if ((*NCE[s].pdwSize & (*NCE[s].pdwSize - 1)) != 0)
	{
		// get next 2^x boundary of size (for 10KB Bert ROM)
		m = 1;
		for (p = *NCE[s].pdwSize; p != 1; p >>= 1)
		{
			if ((p & 1) == 1) ++p;
			m <<= 1;
		}
		--m;								// mask
	}
	else
	{
		m = *NCE[s].pdwSize - 1;			// mask for power of 2 size
	}

	p = (a << dwAddrBits) & m;				// offset to begin in nibbles
	for (i = a; i <= b; ++i)
	{
		if (   ((i ^ uAddr) & uMask) == 0	// mapping area may have holes
			&& (p + dwAddrSize) <= *NCE[s].pdwSize)
		{
			if (*NCE[s].ppNCE)				// module is RAM
			{
				RMap[i] = *NCE[s].ppNCE + p;
				WMap[i] = *NCE[s].ppNCE + p;
			}
			else							// module is ROM
			{
				if ((p + dwAddrSize) <= dwRomEnd)
				{
					_ASSERT(pbyRomPage);	// ROM page defined
					RMap[i] = pbyRomPage + p;
				}
			}
		}
		p = (p + dwAddrSize) & m;
	}
	return;
}

static VOID MapRAMBert(WORD a, WORD b)
{
	UINT  i, uAddr, uMask;
	DWORD p, m;

	uAddr = MMU_DRAM_BASE >> dwAddrBits;	// base address in pages
	uMask = MMU_DRAM_MASK >> dwAddrBits;	// address mask in pages

	a = (WORD)MAX(a,uAddr);
	b = (WORD)MIN(b,(MMU_DRAM_BASE + (~MMU_DRAM_MASK & 0xFFFFF)) >> dwAddrBits);
	m = MMU_DRAM_SIZE - 1;
	p = (a << dwAddrBits) & m;				// offset to begin in nibbles
	for (i=a; i<=b; ++i)
	{
		if (((i ^ uAddr) & uMask) == 0)		// mapping area may have holes
		{
			RMap[i] = Chipset.b.DRam + p;
			WMap[i] = Chipset.b.DRam + p;
		}
		p = (p + dwAddrSize) & m;
	}
	return;
}

VOID InitIO(VOID)
{
	_ASSERT(nCurrentHardware != HDW_UNKNOWN);

	switch (nCurrentHardware)
	{
	case HDW_LEWIS:	// Lewis/Clamshell
		fnMap = MapLewis;
		fnReadIO  = ReadLewisIO;
		fnWriteIO = WriteLewisIO;
		dwPageBits = 10;					// valid bits for the page
		break;
	case HDW_SACA:	// Sacajavea
		fnMap = MapLewis;
		fnReadIO  = ReadSacajaweaIO;
		fnWriteIO = WriteSacajaweaIO;
		dwPageBits = 10;					// valid bits for the page
		break;
	case HDW_BERT:	// Bert
		fnMap = MapBert;
		fnReadIO  = ReadBertIO;
		fnWriteIO = WriteBertIO;
		dwPageBits = 11;					// valid bits for the page
		break;
	default:
		_ASSERT(nCurrentHardware != HDW_UNKNOWN);
		fnMap = NULL;
		fnReadIO  = NULL;
		fnWriteIO = NULL;
		dwPageBits = 0;
	}

	_ASSERT((1<<dwPageBits) <= (UINT) ARRAYSIZEOF(RMap));
	dwAddrBits = (20-dwPageBits);			// valid bits for the address
	dwAddrSize = (1<<dwAddrBits);			// address size (in nibbles)
	return;
}

static VOID MapBert(WORD a, WORD b)
{
	UINT i;

	DWORD dwOffset = 0;						// ROM file offset

	for (i = a; i <= b; ++i)				// clear area
	{
		RMap[i] = NULL;
		WMap[i] = NULL;
	}

	MapNCE(MASTER_NROM,a,b,&dwOffset);		// ROM (master)
	MapRAMBert(a,b);						// RAM
	return;
}

static VOID MapLewis(WORD a, WORD b)				// maps 512 byte pages
{
	UINT i;

	DWORD dwOffset = 0;						// ROM file offset

	for (i = a; i <= b; ++i)				// clear area
	{
		RMap[i] = NULL;
		WMap[i] = NULL;
	}

	MapNCE(MASTER_NROM,a,b,&dwOffset);		// ROM (master)
	if (Chipset.bSlave)						// slave chip
	{
		MapNCE(SLAVE_NROM,a,b,&dwOffset);	// ROM (slave)
	}
	MapNCE(MASTER_NCE2,a,b,&dwOffset);		// Memory Controller 1 (master)
	MapNCE(MASTER_NCE3,a,b,&dwOffset);		// Memory Controller 2 (master)
	MapNCE(MASTER_NDISP,a,b,&dwOffset);		// Display/Timer (master)
	if (Chipset.bSlave)						// slave chip
	{
		MapNCE(SLAVE_NCE2,a,b,&dwOffset);	// Memory Controller 1 (slave)
		MapNCE(SLAVE_NCE3,a,b,&dwOffset);	// Memory Controller 2 (slave)
		MapNCE(SLAVE_NDISP,a,b,&dwOffset);	// Display/Timer (slave)
	}
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Bus Commands
//
////////////////////////////////////////////////////////////////////////////////

VOID Config()								// configure modules in fixed order
{
	return;
}

VOID Uncnfg()
{
	return;
}

VOID Reset()
{
	return;
}

VOID C_Eq_Id()
{
	memset(Chipset.C, 0, 5);				// clear C[A]
	return;
}

static VOID ChipResetMaster(VOID)
{
	StopTimerBert();						// stop Bert timer chip
	StopTimers(MASTER);						// stop timer
	StopDisplay();							// stop display

	Chipset.pc = 0;
	Chipset.rstkp = 0;
	ZeroMemory(Chipset.rstk,sizeof(Chipset.rstk));
	Chipset.HST = 0;
	Chipset.SoftInt = FALSE;
	Chipset.Shutdn = TRUE;
	Chipset.inte = TRUE;					// enable interrupts
	Chipset.intk = TRUE;					// INTON
	Chipset.intd = FALSE;					// no keyboard interrupts pending
	Chipset.crc = 0;
	ZeroMemory(Chipset.IORam,sizeof(Chipset.IORam));
	if (nCurrentHardware == HDW_LEWIS)
		Chipset.IORam[RATECTL] = 7;			// CPU speed
	Chipset.IORam[LPE] = RST;				// set ReSeT bit at hardware reset
	Chipset.t1 = 0;							// reset timer values
	Chipset.t2 = 0;
	UpdateContrast();						// update contrast
	return;
}

static VOID ChipResetSlave(VOID)
{
	StopTimers(SLAVE);						// stop timer
	ChipsetS.bLcdSyncErr = TRUE;			// preset display with synchronization error

	ChipsetS.crc = 0;
	ZeroMemory(ChipsetS.IORam,sizeof(ChipsetS.IORam));
	ChipsetS.IORam[RATECTL] = 7;			// CPU speed
	ChipsetS.IORam[LPE] = RST;				// set ReSeT bit at hardware reset
	ChipsetS.t1 = 0;						// reset timer values
	ChipsetS.t2 = 0;
	return;
}

VOID CpuReset(VOID)							// register setting after Cpu Reset
{
	StopTimerBert();						// reset Bert timer chip
	ChipResetMaster();						// reset master chip
	if (Chipset.bSlave)						// 2nd display controller
	{
		ChipResetSlave();					// reset slave chip
	}
	return;
}

enum MMUMAP MapData(DWORD d)				// check MMU area
{
	if (nCurrentHardware == HDW_BERT)
	{
		if (((d ^ MMU_ROM_BASE)  & MMU_ROM_MASK)  == 0) return B_ROM;
		if (((d ^ MMU_DRAM_BASE) & MMU_DRAM_MASK) == 0) return B_RAM;
		if (((d ^ MMU_DISP_BASE) & MMU_DISP_MASK) == 0) return B_DISP;
		if (((d ^ MMU_CTRL_BASE) & MMU_CTRL_MASK) == 0) return B_CTRL;
	}
	else
	{
		// if (Chipset.RegMask   && ((d ^ Chipset.RegBase)   & Chipset.RegMask)   == 0) return M_REG;
		if (Chipset.DispMask  && ((d ^ Chipset.DispBase)  & Chipset.DispMask)  == 0) return M_DISP;
		if (Chipset.NCE2Mask  && ((d ^ Chipset.NCE2Base)  & Chipset.NCE2Mask)  == 0) return M_NCE2;
		if (Chipset.NCE3Mask  && ((d ^ Chipset.NCE3Base)  & Chipset.NCE3Mask)  == 0) return M_NCE3;
		if (Chipset.RomMask   && ((d ^ Chipset.RomBase)   & Chipset.RomMask)   == 0) return M_ROM;
		// if (ChipsetS.RegMask  && ((d ^ ChipsetS.RegBase)  & ChipsetS.RegMask)  == 0) return M_REGS;
		if (ChipsetS.DispMask && ((d ^ ChipsetS.DispBase) & ChipsetS.DispMask) == 0) return M_DISPS;
		if (ChipsetS.NCE2Mask && ((d ^ ChipsetS.NCE2Base) & ChipsetS.NCE2Mask) == 0) return M_NCE2S;
		if (ChipsetS.NCE3Mask && ((d ^ ChipsetS.NCE3Base) & ChipsetS.NCE3Mask) == 0) return M_NCE3S;
		if (ChipsetS.RomMask  && ((d ^ ChipsetS.RomBase)  & ChipsetS.RomMask)  == 0) return M_ROMS;
	}
	return M_NONE;
}

static VOID NreadEx(BYTE *a, DWORD d, UINT s, BOOL bUpdate)
{
	enum MMUMAP eMap;
	DWORD u, v;
	UINT  c;
	BYTE *p;

	#if defined DEBUG_MEMACC
	{
		TCHAR buffer[256];
		wsprintf(buffer,_T("%.5lx: Mem %s : %02x,%u\n"),
				 Chipset.pc,(bUpdate) ? _T("read") : _T("peek"),d,s);
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		eMap = MapData(d);					// get active memory controller

		do
		{
			if (B_DISP == eMap)				// Bert display registers
			{
				v = d & (sizeof(Chipset.b.DspRam)-1);
				c = MIN(s,sizeof(Chipset.b.DspRam)-v);
				memcpy(a, Chipset.b.DspRam+v, c);
				break;
			}

			if (B_CTRL == eMap)				// Bert control registers
			{
				v = d & (sizeof(Chipset.b.IORam)-1);
				c = MIN(s,sizeof(Chipset.b.IORam)-v);
				fnReadIO(a, v, c, bUpdate);
				break;
			}

			if (M_DISP == eMap)				// Lewis display/timer/control registers (master)
			{
				v = d & (sizeof(Chipset.IORam)-1);
				c = MIN(s,sizeof(Chipset.IORam)-v);
				fnReadIO(a,v,c,bUpdate);
				break;
			}

			if (M_DISPS == eMap)			// Lewis display/timer/control registers (slave)
			{
				v = d & (sizeof(ChipsetS.IORam)-1);
				c = MIN(s,sizeof(ChipsetS.IORam)-v);
				ReadLewisSlaveIO(a,v,c,bUpdate);
				break;
			}

			u = d >> dwAddrBits;
			v = d & (dwAddrSize-1);
			c = MIN(s,dwAddrSize-v);
			if ((p=RMap[u]) != NULL)		// module mapped
			{
				memcpy(a, p+v, c);
			}
			// simulate open data bus
			else							// open data bus
			{
				if (M_NONE != eMap)			// open data bus
				{
					for (u=0; u<c; ++u)		// fill all nibbles
					{
						if ((v+u) & 1)		// odd address
							a[u] = READODD;
						else				// even address
							a[u] = READEVEN;
					}
				}
				else
				{
					memset(a, 0x00, c);		// fill with 0
				}
			}

			if (bUpdate)
			{
				for (u=0; u<c; u++)			// update CRC
				{
					Chipset.crc  = UpCRC(Chipset.crc,a[u]);
					ChipsetS.crc = UpCRC(ChipsetS.crc,0);
				}
			}
		}
		while (0);

		a+=c;
		d=(d+c)&0xFFFFF;
	} while (s-=c);
	return;
}

__forceinline VOID Npeek(BYTE *a, DWORD d, UINT s)
{
	NreadEx(a, d, s, FALSE);
	return;
}

__forceinline VOID Nread(BYTE *a, DWORD d, UINT s)
{
	NreadEx(a, d, s, TRUE);
	return;
}

VOID Nwrite(BYTE *a, DWORD d, UINT s)
{
	enum MMUMAP eMap;
	DWORD u, v;
	UINT  c;
	BYTE *p;

	#if defined DEBUG_MEMACC
	{
		TCHAR buffer[256];
		wsprintf(buffer,_T("%.5lx: Mem write: %02x,%u\n"),Chipset.pc,d,s);
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		eMap = MapData(d);					// get active memory controller

		do
		{
			if (B_DISP == eMap)				// Bert display registers
			{
				UINT i;
				BYTE byData;

				v = d & (sizeof(Chipset.b.DspRam)-1);
				c = MIN(s,sizeof(Chipset.b.DspRam)-v);

				for (i = 0; i < c; ++i)
				{
					// handle no RAM at bit location
					byData = (((v + i) & 0x1) != 0) ? (a[i] & 0x8) : a[i];
					Chipset.b.DspRam[v+i] = byData;
				}
				break;
			}

			if (B_CTRL == eMap)				// Bert control registers
			{
				v = d & (sizeof(Chipset.b.IORam)-1);
				c = MIN(s,sizeof(Chipset.b.IORam)-v);
				fnWriteIO(a, v, c);
				break;
			}

			if (M_DISP == eMap)				// Lewis display/timer/control registers (master)
			{
				v = d & (sizeof(Chipset.IORam)-1);
				c = MIN(s,sizeof(Chipset.IORam)-v);
				fnWriteIO(a, v, c);
				break;
			}

			if (M_DISPS == eMap)			// Lewis display/timer/control registers (slave)
			{
				v = d & (sizeof(ChipsetS.IORam)-1);
				c = MIN(s,sizeof(ChipsetS.IORam)-v);
				WriteLewisSlaveIO(a, v, c);
				break;
			}

			u = d >> dwAddrBits;
			v = d & (dwAddrSize-1);
			c = MIN(s,dwAddrSize-v);
			if ((p=WMap[u]) != NULL) memcpy(p+v, a, c);
		}
		while (0);

		a+=c;
		d=(d+c)&0xFFFFF;
	} while (s-=c);
	return;
}

DWORD Read5(DWORD d)
{
	BYTE p[5];

	Npeek(p,d,5);
	return Npack(p,5);
}

BYTE Read2(DWORD d)
{
	BYTE p[2];

	Npeek(p,d,2);
	return (BYTE)(p[0]|(p[1]<<4));
}

VOID Write5(DWORD d, DWORD n)
{
	BYTE p[5];

	Nunpack(p,n,5);
	Nwrite(p,d,5);
	return;
}

VOID Write2(DWORD d, BYTE n)
{
	BYTE p[2];

	Nunpack(p,n,2);
	Nwrite(p,d,2);
	return;
}

VOID IOBit(BYTE *d, BYTE b, BOOL s)			// set/clear bit in I/O section
{
	EnterCriticalSection(&csIOLock);
	{
		if (s)
			*d |= b;						// set bit
		else
			*d &= ~b;						// clear bit
	}
	LeaveCriticalSection(&csIOLock);
	return;
}

static DWORD ReadT2Acc(enum CHIP eChip)
{
	static DWORD dwCyc[2] = { 0, 0 };		// CPU cycle counter at last timer2 read access

	DWORD dwCycDif;

	// CPU cycles since last call
	dwCycDif = (DWORD) (Chipset.cycles & 0xFFFFFFFF) - dwCyc[eChip];
	dwCyc[eChip] = (DWORD) (Chipset.cycles & 0xFFFFFFFF);

	// maybe CPU speed measurement, slow down the next 10 CPU opcodes
	if (dwCycDif < 150)
	{
		EnterCriticalSection(&csSlowLock);
		{
			InitAdjustSpeed();				// init variables if necessary
			nOpcSlow = 10;					// slow down next 10 opcodes
		}
		LeaveCriticalSection(&csSlowLock);
	}
	return ReadT2(eChip);
}

// Bert
static VOID ReadBertIO(BYTE *a, DWORD d, DWORD s, BOOL bUpdate)
{
	#if defined DEBUG_IO
	{
		TCHAR buffer[256];
		wsprintf(buffer,_T("%.5lx: IO read : %02x,%u\n"),Chipset.pc,d,s);
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		switch (d)
		{
		case 0x00: // System Control [TE LBI 0 RST]
			_ASSERT((Chipset.b.IORam[d] & XTRA) == 0);
			if (bUpdate)
			{
				Chipset.b.IORam[d] &= ~BLBI; // clear LBI bit

				// display on -> low battery circuit enabled
				if ((Chipset.b.IORam[BCONTRAST] & DON) != 0)
				{
					SYSTEM_POWER_STATUS sSps;

					VERIFY(GetSystemPowerStatus(&sSps));

					// low bat emulation enabled, battery powered and low battery condition on host
					if (   !bLowBatDisable
						&& sSps.ACLineStatus == AC_LINE_OFFLINE
						&& (sSps.BatteryFlag & (BATTERY_FLAG_CRITICAL | BATTERY_FLAG_LOW)) != 0)
					{
						// set Low Battery Indicator
						Chipset.b.IORam[d] |= BLBI;
					}
				}

				*a = Chipset.b.IORam[d];

				// clear RST bit after reading
				Chipset.b.IORam[d] &= ~RST;
			}
			else
			{
				*a = Chipset.b.IORam[d];
			}
			break;
		case 0x01: // Display contrast [DON DC2 DC1 DC0]
			*a = Chipset.b.IORam[d];
			break;
		case 0x02: // System Test [PLEV VDIG DDP DPC]
			*a = Chipset.b.IORam[d];
			break;
		case 0x03: // Mode
			*a = 0;						// Quite Mode
			break;
		default:
			_ASSERT(FALSE);
		}

		d++; a++;
	} while (--s);
	return;
}

static VOID WriteBertIO(BYTE *a, DWORD d, DWORD s)
{
	BYTE c;

	#if defined DEBUG_IO
	{
		TCHAR buffer[256];
		DWORD j;
		int   i;

		i = wsprintf(buffer,_T("%.5lx: IO write: %02x,%u = "),Chipset.pc,d,s);
		for (j = 0;j < s;++j,++i)
		{
			buffer[i] = a[j];
			if (buffer[i] > 9) buffer[i] += _T('a') - _T('9') - 1;
			buffer[i] += _T('0');
		}
		buffer[i++] = _T('\n');
		buffer[i] = 0;
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		c = *a;
		switch (d)
		{
// 00 =  SYSTEM_CTRL
// 00 @  System Control [TE LBI -- RST]
		case 0x00:
			Chipset.b.IORam[d] = (c & ~XTRA);
			if ((c & TE) == 0)				// timer disabled
				Chipset.SReq = 0;			// no responds of timer any more
			break;
// 01 =  CONTRAST
// 01 @  Display contrast [DON DC2 DC1 DC0]
// 01 @  Higher value = darker screen
		case 0x01:
			if ((c^Chipset.b.IORam[d]) != 0) // content changed
			{
				// DON bit changed?
				BOOL bDON = ((c ^ Chipset.b.IORam[d]) & DON) != 0;

				Chipset.b.IORam[d] = c;		// new setting
				if (bDON)					// DON bit changed
				{
					if ((c & DON) != 0)		// set display on
					{
						StartDisplay();		// start display update
					}
					else					// set display off
					{
						StopDisplay();		// stop display update
					}
				}

				UpdateContrast();
			}
			break;
		default: Chipset.b.IORam[d]=c;
		}
		a++; d++;
	} while (--s);
	return;
}

// Sacajawea
static VOID ReadSacajaweaIO(BYTE *a, DWORD d, DWORD s, BOOL bUpdate)
{
	BOOL bLBI,bVLBI;

	#if defined DEBUG_IO
	{
		TCHAR buffer[256];
		wsprintf(buffer,_T("%.5lx: IO read : %02x,%u\n"),Chipset.pc,d,s);
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		switch (d)
		{
		case 0x304: *a = (Chipset.crc    )&0xF; break;
		case 0x305: *a = (Chipset.crc>> 4)&0xF; break;
		case 0x306: *a = (Chipset.crc>> 8)&0xF; break;
		case 0x307: *a = (Chipset.crc>>12)&0xF; break;
		case 0x308:
			// battery measurement is active because CPU is running
			GetBatteryState(&bLBI,&bVLBI);	// get battery state

			// set IO bits
			IOBit(&Chipset.IORam[d],LBI,bLBI);
			IOBit(&Chipset.IORam[d],VLBI,bVLBI);
			*a = Chipset.IORam[d];
			if (bUpdate)					// modify enabled
			{
				// GRAM is cleared after reading
				IOBit(&Chipset.IORam[d],GRAM,FALSE);
			}
			break;
		case 0x309:
			*a = Chipset.IORam[d];
			if (bUpdate)					// modify enabled
			{
				Chipset.IORam[d] &= ~RST;	// clear RST bit after reading
			}
			break;
		case 0x30F:
			ReadT2(MASTER);					// dummy read for update timer2 control register
			*a = Chipset.IORam[d];
			break;
		case 0x3FA: NunpackCRC(a, ReadT2Acc(MASTER)    , s, bUpdate); return;
		case 0x3FB: NunpackCRC(a, ReadT2Acc(MASTER)>> 4, s, bUpdate); return;
		case 0x3FC: NunpackCRC(a, ReadT2Acc(MASTER)>> 8, s, bUpdate); return;
		case 0x3FD: NunpackCRC(a, ReadT2Acc(MASTER)>>12, s, bUpdate); return;
		case 0x3FE: NunpackCRC(a, ReadT2Acc(MASTER)>>16, s, bUpdate); return;
		case 0x3FF: NunpackCRC(a, ReadT2Acc(MASTER)>>20, s, bUpdate); return;
		default:
			if (d > PROW_END && d < CONTRAST) // undefined area 1
			{
				*a = 0;
				break;
			}
			if (d > TIMERCTL && d < TIMER)    // undefined area 2
			{
				*a = 3;
				break;
			}
			*a = Chipset.IORam[d];
		}

		// the first 3 nibbles of the master CRC do not update the master CRC itself
		if (bUpdate && (d < CRC || d > CRC + 2))
			Chipset.crc = UpCRC(Chipset.crc,*a);

		d++; a++;
	} while (--s);
	return;
}

static VOID WriteSacajaweaIO(BYTE *a, DWORD d, DWORD s)
{
	BYTE  c;

	#if defined DEBUG_IO
	{
		TCHAR buffer[256];
		DWORD j;
		int   i;

		i = wsprintf(buffer,_T("%.5lx: IO write: %02x,%u = "),Chipset.pc,d,s);
		for (j = 0;j < s;++j,++i)
		{
			buffer[i] = a[j];
			if (buffer[i] > 9) buffer[i] += _T('a') - _T('9') - 1;
			buffer[i] += _T('0');
		}
		buffer[i++] = _T('\n');
		buffer[i] = 0;
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		c = *a;
		switch (d)
		{
// 00301 =  CONTRAST
// 00301 @  Contrast Control [CONT3 CONT2 CONT1 CONT0]
// 00301 @  Higher value = darker screen
		case 0x301:
			if (c!=Chipset.IORam[d])
			{
				Chipset.IORam[d]=c;
				UpdateContrast();
			}
			break;

// 00303 =  CTRL
// 00303 @  CONTROL [DON SDAT VLBIS BIN]
		case 0x303:
			c &= ~BIN;						// Burn IN bit is read only, normally cleared
			if ((c^Chipset.IORam[d]) != 0)	// content changed
			{
				// DON bit changed?
				BOOL bDON = ((c ^ Chipset.IORam[d]) & DON) != 0;

				Chipset.IORam[d] = c;		// new setting
				if (bDON)					// DON bit changed
				{
					if ((c & DON) != 0)		// set display on
					{
						StartDisplay();		// start display update
					}
					else					// display is off
					{
						StopDisplay();		// stop display update
					}
				}
			}
			break;

// 00304 =  HP:CRC
// 00304 @  16 bit hardware CRC (304-307)
		case 0x304: Chipset.crc = (Chipset.crc&0xfff0)|(c*0x0001); break;
		case 0x305: Chipset.crc = (Chipset.crc&0xff0f)|(c*0x0010); break;
		case 0x306: Chipset.crc = (Chipset.crc&0xf0ff)|(c*0x0100); break;
		case 0x307: Chipset.crc = (Chipset.crc&0x0fff)|(c*0x1000); break;

// 00308 =  LPD
// 00308 @  Low Power Detection [GRAM VRAM VLBI LBI]
		case 0x308:
			// VLBI and LBI are read only (controlled by LowBat thread)
			IOBit(&Chipset.IORam[d],GRAM,(c & GRAM) != 0);
			IOBit(&Chipset.IORam[d],VRAM,(c & VRAM) != 0);
			// chip reset condition?
			if (   (Chipset.IORam[LPD] & GRAM)  != 0
				&& (Chipset.IORam[LPE] & EGRAM) != 0)
			{
				ChipResetMaster();			// reset master chip
				bInterrupt = TRUE;			// leave execution loop for shut down
			}
			break;

// 00309 =  LPE
// 00309 @  Low Power Detection [EGRAM EVRAM EVLBI RST]
		case 0x309:
			Chipset.IORam[d] = c;
			// chip reset condition?
			if (   (Chipset.IORam[LPD] & GRAM)  != 0
				&& (Chipset.IORam[LPE] & EGRAM) != 0)
			{
				ChipResetMaster();			// reset master chip
				bInterrupt = TRUE;			// leave execution loop for shut down
			}
			break;

// 0030F =  NS:TIMERCTL
// 0030F @  TIMER Control [SRQ WKE INT RUN]
		case 0x30F:
			Chipset.IORam[d]=c;
			ReadT2(MASTER);					// dummy read for checking control bits
			if (c&1)
			{
				StartTimers(MASTER);		// start timer
			}
			else
			{
				StopTimers(MASTER);			// stops timer
			}
			break;

// 003FA =  HP:TIMER
// 003FA @  hardware timer (3A-3F), decremented 8192 times/s
		// nothing - fall through to default

		default:
			Chipset.IORam[d]=c;				// write data

			if (d >= TIMER)					// timer update
			{
				Nunpack(Chipset.IORam+TIMER, ReadT2(MASTER), 6);
				memcpy(Chipset.IORam+d, a, s);
				SetT2(MASTER,Npack(Chipset.IORam+TIMER, 6));
				return;
			}
		}
		a++; d++;
	} while (--s);
	return;
}

// Lewis
static VOID ReadLewisIO(BYTE *a, DWORD d, DWORD s, BOOL bUpdate)
{
	BOOL bLBI,bVLBI;

	#if defined DEBUG_IO
	{
		TCHAR buffer[256];
		wsprintf(buffer,_T("%.5lx: IO read : %02x,%u\n"),Chipset.pc,d,s);
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		switch (d)
		{
		case 0x304: *a = (Chipset.crc    )&0xF; break;
		case 0x305: *a = (Chipset.crc>> 4)&0xF; break;
		case 0x306: *a = (Chipset.crc>> 8)&0xF; break;
		case 0x307: *a = (Chipset.crc>>12)&0xF; break;
		case 0x308:
			// battery measurement is active because CPU is running
			GetBatteryState(&bLBI,&bVLBI);	// get battery state

			// set IO bits
			IOBit(&Chipset.IORam[d],LBI,bLBI);
			IOBit(&Chipset.IORam[d],VLBI,bVLBI);
			*a = Chipset.IORam[d];
			if (bUpdate)					// modify enabled
			{
				// GRAM is cleared after reading
				IOBit(&Chipset.IORam[d],GRAM,FALSE);
			}
			break;
		case 0x309:
			*a = Chipset.IORam[d];
			if (bUpdate)					// modify enabled
			{
				Chipset.IORam[d] &= ~RST;	// clear RST bit after reading
			}
			break;
		case 0x30A:
			*a = LEWIS_MASTER;				// master chip
			break;
		case 0x30C:
			*a = RX;						// RX-Line always high
			break;
		case 0x30E:
			ReadT1(MASTER);					// dummy read for update timer1 control register
			*a = Chipset.IORam[d];
			break;
		case 0x30F:
			ReadT2(MASTER);					// dummy read for update timer2 control register
			*a = Chipset.IORam[d];
			break;
		case 0x3F7: *a = ReadT1(MASTER); break;
		case 0x3F8: NunpackCRC(a, ReadT2Acc(MASTER)    , s, bUpdate); return;
		case 0x3F9: NunpackCRC(a, ReadT2Acc(MASTER)>> 4, s, bUpdate); return;
		case 0x3FA: NunpackCRC(a, ReadT2Acc(MASTER)>> 8, s, bUpdate); return;
		case 0x3FB: NunpackCRC(a, ReadT2Acc(MASTER)>>12, s, bUpdate); return;
		case 0x3FC: NunpackCRC(a, ReadT2Acc(MASTER)>>16, s, bUpdate); return;
		case 0x3FD: NunpackCRC(a, ReadT2Acc(MASTER)>>20, s, bUpdate); return;
		case 0x3FE: NunpackCRC(a, ReadT2Acc(MASTER)>>24, s, bUpdate); return;
		case 0x3FF: NunpackCRC(a, ReadT2Acc(MASTER)>>28, s, bUpdate); return;
		default:
			if (d > DAREA_END && d < RATECTL) // undefined area 1
			{
				*a = 0;
				break;
			}
			if (d > TIMERCTL_2 && d < TIMER1) // undefined area 2
			{
				*a = 3;
				break;
			}
			*a = Chipset.IORam[d];
		}

		ChipsetS.crc = UpCRC(ChipsetS.crc,0); // slave CRC update
		// the first 3 nibbles of the master CRC do not update the master CRC itself
		if (bUpdate && (d < CRC || d > CRC + 2))
			Chipset.crc = UpCRC(Chipset.crc,*a);

		d++; a++;
	} while (--s);
	return;
}

static VOID WriteLewisIO(BYTE *a, DWORD d, DWORD s)
{
	BYTE c;
	BOOL bAnnunciator = FALSE;				// no annunciator access

	#if defined DEBUG_IO
	{
		TCHAR buffer[256];
		DWORD j;
		int   i;

		i = wsprintf(buffer,_T("%.5lx: IO write: %02x,%u = "),Chipset.pc,d,s);
		for (j = 0;j < s;++j,++i)
		{
			buffer[i] = a[j];
			if (buffer[i] > 9) buffer[i] += _T('a') - _T('9') - 1;
			buffer[i] += _T('0');
		}
		buffer[i++] = _T('\n');
		buffer[i] = 0;
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		c = *a;
		switch (d)
		{
// 00300 =  RATECTL
// 00300 @  Rate Control
// 00300 @  CPU Frequency = (RATE+1) * 524288Hz, Strobe rate = Frequency / 4
		case 0x300:
			// CPU speed depending on RATECTL content
			dwCyclesRate = (dwLewisCycles * (c + 1) + 4) / 8;
			Chipset.IORam[d]=c;
			break;

// 00301 =  CONTRAST
// 00301 @  Contrast Control [CONT3 CONT2 CONT1 CONT0]
// 00301 @  Higher value = darker screen
		case 0x301:
			if (c!=Chipset.IORam[d])
			{
				Chipset.IORam[d]=c;
				UpdateContrast();
			}
			break;

// 00303 =  DSPCTL
// 00303 @  Display control + BIN [DON SDAT CONT4 BIN]
// 00303 @  1 nib for display contrast MSB, DON=Display ON
		case 0x303:
			c &= ~BIN;						// Burn IN bit is read only, normally cleared
			if ((c^Chipset.IORam[d])&DON)	// DON bit changed
			{
				if ((c & DON) != 0)			// set display on
				{
					// DON bit of slave not set
					ChipsetS.bLcdSyncErr = (Chipset.bSlave && !(ChipsetS.IORam[DSPCTL]&DON));

					Chipset.IORam[d] |= DON;
					StartDisplay();			// start display update
				}
				else						// display is off
				{
					Chipset.IORam[d] &= ~DON;
					StopDisplay();			// stop display update
				}
				UpdateContrast();
			}
			// CONT4 bit changed
			if ((c^Chipset.IORam[d]) & CONT4)
			{
				Chipset.IORam[d]=c;
				UpdateContrast();
			}
			Chipset.IORam[d] = c;
			break;

// 00304 =  HP:CRC
// 00304 @  16 bit hardware CRC (304-307)
		case 0x304: Chipset.crc = (Chipset.crc&0xfff0)|(c*0x0001); break;
		case 0x305: Chipset.crc = (Chipset.crc&0xff0f)|(c*0x0010); break;
		case 0x306: Chipset.crc = (Chipset.crc&0xf0ff)|(c*0x0100); break;
		case 0x307: Chipset.crc = (Chipset.crc&0x0fff)|(c*0x1000); break;

// 00308 =  LPD
// 00308 @  Low Power Detection [GRAM VRAM VLBI LBI]
		case 0x308:
			// VLBI and LBI are read only (controlled by LowBat thread)
			IOBit(&Chipset.IORam[d],GRAM,(c & GRAM) != 0);
			IOBit(&Chipset.IORam[d],VRAM,(c & VRAM) != 0);
			// chip reset condition?
			if (   (Chipset.IORam[LPD] & GRAM)  != 0
				&& (Chipset.IORam[LPE] & EGRAM) != 0)
			{
				ChipResetMaster();			// reset master chip
				bInterrupt = TRUE;			// leave execution loop for shut down
			}
			break;

// 00309 =  LPE
// 00309 @  Low Power Detection Enable [EGRAM EVRAM EVLBI RST]
		case 0x309:
			Chipset.IORam[d] = c;
			// chip reset condition?
			if (   (Chipset.IORam[LPD] & GRAM)  != 0
				&& (Chipset.IORam[LPE] & EGRAM) != 0)
			{
				ChipResetMaster();			// reset master chip
				bInterrupt = TRUE;			// leave execution loop for shut down
			}
			break;

// 0030C =  INPORT
// 0030C @  Input Port [RX SREQ ST1 ST0]
		case 0x30C:
			if (c & SREQ)					// SREQ bit set
			{
				c &= ~SREQ;					// clear SREQ bit
				Chipset.SoftInt = TRUE;
				bInterrupt = TRUE;
			}
			Chipset.IORam[d]=c;
			break;

// 0030D =  LEDOUT
// 0030D @  LED output [UREG EPD STL DRL]
		case 0x30D:
			if ((c^Chipset.IORam[d]) & STL)	// STL bit changed
			{
				IrPrinter((BYTE)(c & STL));
			}
			Chipset.IORam[d]=c;
			break;

// 0030E =  NS:TIMER1CTL
// 0030E @  TIMER1 Control [SRQ WKE INT XTRA]
		case 0x30E:
			Chipset.IORam[d]=c;				// don't clear XTRA bit
			ReadT1(MASTER);					// dummy read for checking control bits
			break;

// 0030F =  NS:TIMER2CTL
// 0030F @  TIMER2 Control [SRQ WKE INT RUN]
		case 0x30F:
			Chipset.IORam[d]=c;
			ReadT2(MASTER);					// dummy read for checking control bits
			if (c&1)
			{
				StartTimers(MASTER);		// start the master timer
				StartTimers(SLAVE);			// is there a waiting slave timer, if so start
			}
			else
			{
				StopTimers(SLAVE);			// master timer stops also slave timer
				StopTimers(MASTER);			// now stops the master
			}
			break;

// 003F7 =  HP:TIMER1
// 003F7 @  Decremented 16 times/s
		case 0x3F7:
			SetT1(MASTER,c);				// set new value
			break;

// 003F8 =  HP:TIMER2
// 003F8 @  hardware timer (38-3F), decremented 8192 times/s
		// nothing - fall through to default
		default:
			Chipset.IORam[d]=c;				// write data

			// Pioneer annunciator area
			bAnnunciator |= !Chipset.bSlave && d >= LA_ALL && d <= LA_RAD + 7;

			if (d >= TIMER2)				// timer2 update
			{
				Nunpack(Chipset.IORam+TIMER2, ReadT2(MASTER), 8);
				memcpy(Chipset.IORam+d, a, s);
				SetT2(MASTER,Npack(Chipset.IORam+TIMER2, 8));
				goto finish;
			}
		}
		a++; d++;
	} while (--s);

finish:
	if (bAnnunciator) UpdateAnnunciators();
	return;
}

static VOID ReadLewisSlaveIO(BYTE *a, DWORD d, DWORD s, BOOL bUpdate)
{
	#if defined DEBUG_IO
	{
		TCHAR buffer[256];
		wsprintf(buffer,_T("%.5lx: IO Slave read : %02x,%u\n"),Chipset.pc,d,s);
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		switch (d)
		{
		case 0x304: *a = (ChipsetS.crc    )&0xF; break;
		case 0x305: *a = (ChipsetS.crc>> 4)&0xF; break;
		case 0x306: *a = (ChipsetS.crc>> 8)&0xF; break;
		case 0x307: *a = (ChipsetS.crc>>12)&0xF; break;
		case 0x308:
			// VLBI and LBI are always 0
			*a = ChipsetS.IORam[d] & (GRAM | VRAM);
			if (bUpdate)					// modify enabled
			{
				ChipsetS.IORam[d] &= ~GRAM;	// GRAM is cleared after reading
			}
			break;
		case 0x309:
			*a = ChipsetS.IORam[d];
			if (bUpdate)					// modify enabled
			{
				ChipsetS.IORam[d] &= ~RST;	// clear RST bit after reading
			}
			break;
		case 0x30A:
			*a = LEWIS_SLAVE;				// slave chip
			break;
		case 0x30E:
			ReadT1(SLAVE);					// dummy read for update timer1 control register
			*a = ChipsetS.IORam[d];
			break;
		case 0x30F:
			ReadT2(SLAVE);					// dummy read for update timer2 control register
			*a = ChipsetS.IORam[d];
			break;
		case 0x3F7: *a = ReadT1(SLAVE); break;
		case 0x3F8: NunpackCRC(a, ReadT2Acc(SLAVE)    , s, bUpdate); return;
		case 0x3F9: NunpackCRC(a, ReadT2Acc(SLAVE)>> 4, s, bUpdate); return;
		case 0x3FA: NunpackCRC(a, ReadT2Acc(SLAVE)>> 8, s, bUpdate); return;
		case 0x3FB: NunpackCRC(a, ReadT2Acc(SLAVE)>>12, s, bUpdate); return;
		case 0x3FC: NunpackCRC(a, ReadT2Acc(SLAVE)>>16, s, bUpdate); return;
		case 0x3FD: NunpackCRC(a, ReadT2Acc(SLAVE)>>20, s, bUpdate); return;
		case 0x3FE: NunpackCRC(a, ReadT2Acc(SLAVE)>>24, s, bUpdate); return;
		case 0x3FF: NunpackCRC(a, ReadT2Acc(SLAVE)>>28, s, bUpdate); return;
		default:
			if (d > DAREA_END && d < RATECTL) // undefined area 1
			{
				*a = 0;
				break;
			}
			if (d > TIMERCTL_2 && d < TIMER1) // undefined area 2
			{
				*a = 3;
				break;
			}
			*a = ChipsetS.IORam[d];
		}

		Chipset.crc = UpCRC(Chipset.crc,*a); // master CRC update
		// the first 3 nibbles of the slave CRC do not update the slave CRC itself
		if (bUpdate && (d < CRC || d > CRC + 2))
			ChipsetS.crc = UpCRC(ChipsetS.crc,0);

		d++; a++;
	} while (--s);
	return;
}

static VOID WriteLewisSlaveIO(BYTE *a, DWORD d, DWORD s)
{
	BYTE c;
	BOOL bAnnunciator = FALSE;				// no annunciator access

	#if defined DEBUG_IO
	{
		TCHAR buffer[256];
		DWORD j;
		int   i;

		i = wsprintf(buffer,_T("%.5lx: IO Slave write: %02x,%u = "),Chipset.pc,d,s);
		for (j = 0;j < s;++j,++i)
		{
			buffer[i] = a[j];
			if (buffer[i] > 9) buffer[i] += _T('a') - _T('9') - 1;
			buffer[i] += _T('0');
		}
		buffer[i++] = _T('\n');
		buffer[i] = 0;
		OutputDebugString(buffer);
	}
	#endif

	do
	{
		c = *a;
		switch (d)
		{
// 00303 =  DSPCTL
// 00303 @  Display control + BIN [DON SDAT CONT4 BIN]
// 00303 @  1 nib for display contrast MSB, DON=Display ON
		case 0x303:
			c &= ~BIN;						// Burn IN bit is read only, normally cleared
			if ((c^ChipsetS.IORam[d])&DON)	// DON bit changed
			{
				if ((c & DON) == 0)			// set display off
				{
					// DON bit of master not cleared
					ChipsetS.bLcdSyncErr = (Chipset.IORam[DSPCTL]&DON) != 0;
				}
				bAnnunciator = TRUE;		// update annunciator setting
			}
			ChipsetS.IORam[d] = c;
			break;

// 00304 =  HP:CRC
// 00304 @  16 bit hardware CRC (304-307)
		case 0x304: ChipsetS.crc = (ChipsetS.crc&0xfff0)|(c*0x0001); break;
		case 0x305: ChipsetS.crc = (ChipsetS.crc&0xff0f)|(c*0x0010); break;
		case 0x306: ChipsetS.crc = (ChipsetS.crc&0xf0ff)|(c*0x0100); break;
		case 0x307: ChipsetS.crc = (ChipsetS.crc&0x0fff)|(c*0x1000); break;

// 00308 =  LPD
// 00308 @  Low Power Detection [GRAM VRAM VLBI LBI]
		case 0x308:
			// VLBI and LBI are read only and return always zero on slave chip
			ChipsetS.IORam[d] = (c & (GRAM | VRAM));
			// chip reset condition?
			if (   (ChipsetS.IORam[LPD] & GRAM)  != 0
				&& (ChipsetS.IORam[LPE] & EGRAM) != 0)
			{
				ChipResetSlave();			// reset slave chip
			}
			break;

// 00309 =  LPE
// 00309 @  Low Power Detection Enable [EGRAM EVRAM EVLBI RST]
		case 0x309:
			ChipsetS.IORam[d] = c;
			// chip reset condition?
			if (   (ChipsetS.IORam[LPD] & GRAM)  != 0
				&& (ChipsetS.IORam[LPE] & EGRAM) != 0)
			{
				ChipResetSlave();			// reset slave chip
			}
			break;

// 0030E =  NS:TIMER1CTL
// 0030E @  TIMER1 Control [SRQ WKE INT XTRA]
		case 0x30E:
			ChipsetS.IORam[d] = c;			// don't clear XTRA bit
			ReadT1(SLAVE);					// dummy read for checking control bits
			break;

// 0030F =  NS:TIMER2CTL
// 0030F @  TIMER2 Control [SRQ WKE INT RUN]
		case 0x30F:
			ChipsetS.IORam[d] = c;
			ReadT2(SLAVE);					// dummy read for checking control bits
			if (c&1)
			{
				StartTimers(SLAVE);
			}
			else
			{
				StopTimers(SLAVE);
			}
			break;

// 003F7 =  HP:TIMER1
// 003F7 @  Decremented 16 times/s
		case 0x3F7:
			SetT1(SLAVE,c);					// set new value
			break;

// 003F8 =  HP:TIMER2
// 003F8 @  hardware timer (38-3F), decremented 8192 times/s
		// nothing - fall through to default
		default:
			ChipsetS.IORam[d] = c;			// write data

			// Clamshell annunciator area
			bAnnunciator |= d >= SLA_BUSY && d <= SLA_ALL + 7;

			if (d >= TIMER2)				// timer2 update
			{
				Nunpack(ChipsetS.IORam+TIMER2, ReadT2(SLAVE), 8);
				memcpy(ChipsetS.IORam+d, a, s);
				SetT2(SLAVE,Npack(ChipsetS.IORam+TIMER2, 8));
				goto finish;
			}
		}
		a++; d++;
	} while (--s);

finish:
	if (bAnnunciator) UpdateAnnunciators();
	return;
}
