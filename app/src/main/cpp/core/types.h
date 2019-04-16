/*
 *   types.h
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gieﬂelink
 *
 */

// HST bits
#define XM 1
#define SB 2
#define SR 4
#define MP 8

#define	SWORD SHORT							// signed   16 Bit variable
#define	QWORD ULONGLONG						// unsigned 64 Bit variable

typedef struct
{
	SWORD	nPosX;							// position of window
	SWORD	nPosY;
	BYTE	type;							// calculator type

	DWORD	pc;
	DWORD	d0;
	DWORD	d1;
	DWORD	rstkp;
	DWORD	rstk[8];
	BYTE	A[16];
	BYTE	B[16];
	BYTE	C[16];
	BYTE	D[16];
	BYTE	R0[16];
	BYTE	R1[16];
	BYTE	R2[16];
	BYTE	R3[16];
	BYTE	R4[16];
	BYTE	ST[4];
	BYTE	HST;
	BYTE	P;
	WORD	out;
	WORD	in;
	BOOL	SoftInt;
	BOOL	Shutdn;
	BOOL	mode_dec;
	BOOL	inte;							// interrupt status flag (FALSE = int in service)
	BOOL	intk;							// 1 ms keyboard scan flag (TRUE = enable)
	BOOL	intd;							// keyboard interrupt pending (TRUE = int pending)
	BOOL	carry;
	WORD	crc;							// CRC content

	BOOL	bShutdnWake;					// flag for wake up from SHUTDN mode

#if defined _USRDLL							// DLL version
	QWORD	cycles;							// oscillator cycles
#else										// EXE version
	DWORD	cycles;							// oscillator cycles
	DWORD	cycles_reserved;				// reserved for MSB of oscillator cycles
#endif

	WORD	wRomCrc;						// fingerprint of ROM

	BOOL	bSlave;							// slave controller assembled

	DWORD	RegBase,  RegMask;				// MMU Registers (16 bits)
	DWORD	DispBase, DispMask;				// MMU Display/Timer (10 bits)
	DWORD	NCE2Base, NCE2Mask;				// MMU NCE2 Memory (10 bits)
	DWORD	NCE3Base, NCE3Mask;				// MMU NCE3 Memory (10 bits)
	DWORD	RomBase,  RomMask;				// MMU ROM (6 bits)

	BYTE	IORam[0x400];					// display/timer/control registers

	BOOL	NCE2Ram;						// type of memory (FALSE = ROM | TRUE = RAM)
	DWORD	NCE2Size;						// real size of module in Nibbles
	DWORD   dwUnused1;						// not used, was memory pointer NCE2

	BOOL	NCE3Ram;						// type of memory (FALSE = ROM | TRUE = RAM)
	DWORD	NCE3Size;						// real size of module in Nibbles
	DWORD   dwUnused2;						// not used, was memory pointer NCE3

	DWORD	RomSize;						// real size of ROM in Nibbles

	BYTE	t1;								// timer1 content
	DWORD	t2;								// timer2 content

	WORD	Keyboard_Row[10];				// keyboard Out lines
	WORD	IR15X;							// ON-key state

//	BYTE	contrast;						// display contrast setting
} CHIPSET_M;

typedef struct
{
	WORD	crc;							// CRC content

	DWORD	RegBase,  RegMask;				// MMU Registers (16 bits)
	DWORD	DispBase, DispMask;				// MMU Display/Timer (10 bits)
	DWORD	NCE2Base, NCE2Mask;				// MMU NCE2 Memory (10 bits)
	DWORD	NCE3Base, NCE3Mask;				// MMU NCE3 Memory (10 bits)
	DWORD	RomBase,  RomMask;				// MMU ROM (6 bits)

	BYTE	IORam[0x400];					// display/timer/control registers (slave)

	BOOL	NCE2Ram;						// type of memory (FALSE = ROM | TRUE = RAM)
	DWORD	NCE2Size;						// real size of module in Nibbles
	DWORD   dwUnused1;						// not used, was memory pointer NCE2

	BOOL	NCE3Ram;						// type of memory (FALSE = ROM | TRUE = RAM)
	DWORD	NCE3Size;						// real size of module in Nibbles
	DWORD   dwUnused2;						// not used, was memory pointer NCE3

	DWORD	RomSize;						// real size of ROM in Nibbles

	BYTE	t1;								// timer1 content
	DWORD	t2;								// timer2 content

	BOOL	bLcdSyncErr;					// master/slave LCD controller synchronization error
} CHIPSET_S;
