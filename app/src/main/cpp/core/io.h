/*
 *   io.h
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2004 Christoph Gieﬂelink
 *
 */

// display/timer I/O addresses without mapping offset (Lewis)
#define DAREA_BEGIN	0x000					// start of display area
#define	DAREA_END	0x25F					// end of display area

#define TIMER1		0x3f7					// Timer1 Lewis (4 bit)
#define TIMER2		0x3f8					// Timer2 Lewis (32 bit, LSB first)
#define TIMER		0x3fa					// Timer Sakajawea (24 bit, LSB first)

// LCD addresses of Mid range Pioneer series without mapping offset
#define PDISP_BEGIN	0x000					// Display begin
#define PDISP_END	0x077					// Display end

#define PROW_START	0x078					// Row Drivers Start
#define PROW_END	0x087					// Row Drivers End

// LCD addresses of High end Pioneer series without mapping offset
#define DISP_BEGIN	0x000					// Display begin
#define DISP_END	0x20B					// Display end
#define LA_ALL		0x210					// Annunciator - all
#define LA_UPDOWN	0x218					// Annunciator - Up/Down Arrow
#define LA_SHIFT	0x220					// Annunciator - Shift
#define LA_PRINTER	0x228					// Annunciator - Printer
#define LA_BUSY		0x230					// Annunciator - Busy
#define LA_BAT		0x238					// Annunciator - Battery
#define LA_G		0x240					// Annunciator - G
#define LA_RAD		0x248					// Annunciator - RAD

// LCD addresses of Clamshell series without mapping offset
#define MDISP_BEGIN	0x000					// Display begin in master
#define MDISP_END	0x227					// Display end in master
#define SDISP_BEGIN	0x040					// Display begin in slave
#define SDISP_END	0x25F					// Display end in slave
#define SLA_BUSY	0x000					// Annunciator - Busy
#define SLA_ALPHA	0x008					// Annunciator - Alpha
#define SLA_BAT		0x010					// Annunciator - Battery
#define SLA_SHIFT	0x018					// Annunciator - Shift
#define SLA_RAD		0x020					// Annunciator - 2*Pi
#define SLA_HALT	0x028					// Annunciator - Halt
#define SLA_PRINTER	0x030					// Annunciator - Printer
#define SLA_ALL		0x038					// Annunciator - all

// register I/O addresses without mapping offset
#define RATECTL		0x300					// Rate Control (speed) (only Lewis)
#define CONTRAST	0x301					// Display contrast control
#define DSPTEST		0x302					// Display test
#define DSPCTL		0x303					// Display control + BIN (Lewis)
#define CTRL		0x303					// Display control + BIN (Sacajawea)
#define CRC			0x304					// Crc (16 bit, LSB first)
#define LPD			0x308					// Low Power Detection
#define LPE			0x309					// Low Power detection Enable
#define MODE		0x30a					// Mode register
#define RAMTST		0x30b					// RAM test
#define INPORT		0x30c					// (only Lewis)
#define LEDOUT		0x30d					// (only Lewis)
#define TIMERCTL_1	0x30e					// Timer1 Control (only Lewis)
#define TIMERCTL_2	0x30f					// Timer2 Control (Lewis)
#define TIMERCTL	0x30f					// Timer  Control (Sacajawea)

// 0x301 Display contrast [CONT3 CONT2 CONT1 CONT0]
#define CONT3		0x08					// Display CONTrast Bit3
#define CONT2		0x04					// Display CONTrast Bit2
#define CONT1		0x02					// Display CONTrast Bit1
#define CONT0		0x01					// Display CONTrast Bit0

// 0x302 Display test [VDIG LID CLTM1 CLTM0]
#define VDIG		0x08					// DIGital Voltages
#define LID			0x04					// Lcd Invert Data
#define CLTM1		0x02					// CoLumn Test Mode [1]
#define CLTM0		0x01					// CoLumn Test Mode [0]

// 0x303 Display contrast and DON [DON SDAT CONT4 BIN] (Lewis)
// 0x303 Control [DON SDAT VLBIS BIN]                  (Sacajawea)
#define DON			0x08					// Display On
#define SDAT		0x04					// Shift DATa
#define CONT4		0x02					// Display CONTrast Bit4 (Lewis)
#define VLBIS		0x02					// VLBI Switch           (Sacajawea)
#define BIN			0x01					// Burn IN (read only)

// 0x308 Low Power Detection [GRAM VRAM VLBI LBI]
#define GRAM		0x08					// Glitch sensitive RAM
#define VRAM		0x04					// Voltage sensitive RAM
#define VLBI		0x02					// Very Low Battery Indicator
#define LBI			0x01					// Low Battery Indicator

// 0x309 Low Power Detection Enable [EGRAM EVRAM EVLBI RST]
#define EGRAM		0x08					// Enable Glitch sensitive RAM
#define EVRAM		0x04					// Enable Voltage sensitive RAM
#define EVLBI		0x02					// Enable Very Low Battery Indicator
#define RST			0x01					// ReSeT

// 0x30b RAM test [PLEV XTRA DDP DPC]
#define PLEV		0x08					// reverse Precharge LEVel
#define XTRA_2		0x04					// unused bit
#define DDP			0x02					// Decrement the DP
#define DPC			0x01					// Decrement the PC

// 0x30c Input Port [RX SREQ ST1 ST0]
#define RX			0x08					// RX - LED pin state
#define SREQ		0x04					// Service REQest
#define ST1			0x02					// ST[1] - Service Type [1]
#define ST0			0x01					// ST[1] - Service Type [0]

// 0x30d LED output [UREG EPD STL DRL]
#define UREG		0x08					// UnREGulated pull down
#define EPD			0x04					// Enable Pull Down
#define STL			0x02					// STrobe Low
#define DRL			0x01					// DRive Low

// 0x30e Timer1 Control [SRQ WKE INT XTRA]
#define SRQ			0x08					// Service ReQuest
#define WKE			0x04					// WaKE up
#define INTR		0x02					// INTeRrupt
#define XTRA_0		0x01					// unused bit

// 0x30f Timer2 Control [SRQ WKE INT RUN]
#define SRQ			0x08					// Service ReQuest
#define WKE			0x04					// WaKE up
#define INTR		0x02					// INTeRrupt
#define RUN			0x01					// RUN timer
