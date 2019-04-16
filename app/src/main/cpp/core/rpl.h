/*
 *   rpl.h
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2005 Christoph Gieﬂelink
 *
 */

//| 19B2 | 28S  | 42S  | Name
//       #C005A #500B8 =TEMPTOP
//#C408A #C005F #500BD =RSKTOP   (B)
//#C408F #C0064 #500C2 =DSKTOP   (D1)
//       #C0069 #500C7 =EDITLINE
//#C4111 #C00DC #50117 =AVMEM    (D)
//#C40B7 #C008C #500E0 =INTRPPTR (D0)

// HP28S ROM rev. 2BB entries
#define O_fnRadix		48

#define O_DOBINT		0x02911		// System Binary
#define O_DOREAL		0x02933		// Real
#define O_DOEREL		0x02955		// Long Real
#define O_DOCMP			0x02977		// Complex
#define O_DOECMP		0x0299D		// Long Complex
#define O_DOCHAR		0x029BF		// Character
#define O_DOEXT			0x029E1		// Reserved
#define O_DOARRY		0x02A0A		// Array
#define O_DOLNKARRY		0x02A2C		// Linked Array
#define O_DOCSTR		0x02A4E		// String
#define O_DOHSTR		0x02A70		// Binary Integer
#define O_DOLIST		0x02A96		// List
#define O_DORRP			0x02AB8		// RAMROMPAIR
#define O_DOSYMB		0x02ADA		// Algebraic
#define O_DOCOL			0x02C67		// Program
#define O_DOCODE		0x02C96		// Code
#define O_DOIDNT		0x02D12		// Global Name
#define O_DOLAM			0x02D37		// Local Name
#define O_DOROMP		0x02D5C		// XLIB Name
#define O_SEMI			0x02F90		// ;

#define O_GARBAGECOL	0x04A94		// =GARBAGECOL

#define O_TEMPTOP		0xC005A
#define O_RSKTOP		0xC005F
#define O_DSKTOP		0xC0064
#define O_EDITLINE		0xC0069
#define O_INTRPPTR		0xC008C
#define O_AVMEM			0xC00DC
#define O_USERFLAGS		0xC010F

// HP17B ROM rev. A,B entries
#define T_sRADIX		3

#define T_CALCSTATUS	0x5002A
#define T_X				0x501F4

// HP17BII ROM rev. A,B entries
#define U_sRADIX		3

#define U_CALCSTATUS	0x5002B
#define U_X				0x501F5
#define U_Y				0x50205
#define U_Z				0x50215
#define U_T				0x50225

// HP19BII ROM rev. ? entries
#define Y_sRADIX		3

#define Y_CALCSTATUS	0xC41E1
#define Y_RSKTOP		0xC408A
#define Y_DSKTOP		0xC408F
#define Y_INTRPPTR		0xC40B7
#define Y_AVMEM			0xC4111
#define Y_X				0xC419D
#define Y_Y				0xC41AD
#define Y_Z				0xC41BD
#define Y_T				0xC41CD

// HP27S ROM rev. A,B  entries
#define M_sRADIX		3

#define M_CALCSTATUS	0x50025
#define M_X				0x501F1
#define M_Y				0x50201
#define M_Z				0x50211
#define M_T				0x50221

// HP42S ROM rev. A,B,C entries
#define D_sRADIX		1
#define D_sSTKLIFT		4

#define D_DOBINT		0x02911		// System Binary
#define D_DOREAL		0x02933		// Real
#define D_DOEREL		0x02955		// Long Real
#define D_DOCMP			0x02977		// Complex
#define D_DOECMP		0x0299D		// Long Complex
#define D_DOARRY		0x029BF		// Array
#define D_DOHSTR		0x029E1		// Binary Integer
#define D_DORRP			0x02A1F		// RAMROMPAIR
#define D_DOCOL			0x02B3E		// Program
#define D_DOCODE		0x02B6D		// Code
#define D_DOIDNT		0x02BCB		// Global Name
#define D_SEMI			0x02D23		// ;

#define D_TEMPTOP		0x500B8
#define D_RSKTOP		0x500BD
#define D_DSKTOP		0x500C2
#define D_TEMPENV		0x500C7
#define D_USEROB		0x500D1
#define D_CONTEXT		0x500D6
#define D_INTRPPTR		0x500E0
#define D_StackT		0x500E5
#define D_StackZ		0x500EA
#define D_StackY		0x500EF
#define D_StackX		0x500F4
#define D_StackLastX	0x500F9
#define D_AVMEM			0x50117
#define D_CALCLFLAGS	0x5046B

// HP14B RAM entry
#define I_nRadix		2			// nRadix,

#define I_SysFlags		0x2006B
#define I_X				0x20355

// HP32SII RAM entries
#define N_nRadix		4			// nRadix,

#define N_SysFlags		0x2005D
#define N_StackLastX	0x20096
#define N_StackT		0x200A6
#define N_StackZ		0x200B6
#define N_StackY		0x200C6
#define N_StackX		0x200D6
