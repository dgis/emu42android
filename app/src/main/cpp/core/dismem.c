/*
 *   dismem.c
 *
 *   This file is part of Emu42
 *
 *   Copyright (C) 2012 Christoph Gieﬂelink
 *
 */
#include "pch.h"
#include "Emu42.h"

typedef struct								// type of model memory mapping
{
	BYTE         byType;					// calculator type
	CONST LPBYTE *ppbyNCE1;					// NCE1 data (master)
	CONST DWORD  *pdwNCE1Size;				// NCE1 size (master)
	CONST LPBYTE *ppbyNCE2;					// NCE2 data (master)
	CONST DWORD  *pdwNCE2Size;				// NCE2 size (master)
	CONST LPBYTE *ppbyNCE3;					// NCE3 data (master)
	CONST DWORD  *pdwNCE3Size;				// NCE3 size (master)
	CONST LPBYTE *ppbyNCE1S;				// NCE1 data (slave)
	CONST DWORD  *pdwNCE1SizeS;				// NCE1 size (slave)
	CONST LPBYTE *ppbyNCE2S;				// NCE2 data (slave)
	CONST DWORD  *pdwNCE2SizeS;				// NCE2 size (slave)
	CONST LPBYTE *ppbyNCE3S;				// NCE3 data (slave)
	CONST DWORD  *pdwNCE3SizeS;				// NCE3 size (slave)
} MODEL_MAP_T;

static CONST MODEL_MAP_T MemMap[] =
{
	{
		0,									// default
		&pbyRom, &Chipset.RomSize,			// mask ROM
		&mNCE2,  &Chipset.NCE2Size,			// memory controller 1
		&mNCE3,  &Chipset.NCE3Size,			// memory controller 2
		&pbyRom, &ChipsetS.RomSize,			// mask ROM
		&sNCE2,  &ChipsetS.NCE2Size,		// memory controller 1
		&sNCE3,  &ChipsetS.NCE3Size			// memory controller 2
	}
};

static MODEL_MAP_T CONST *pMapping = MemMap; // model specific memory mapping
static enum MEM_MAPPING eMapType = MEM_MMU;	// MMU memory mapping

static LPBYTE pbyMapData = NULL;
static DWORD  dwMapDataSize = 0;
static DWORD  dwMapDataMask = 0;

BOOL SetMemRomType(BYTE cCurrentRomType)
{
	UINT i;

	pMapping = MemMap;						// init default mapping

	// scan for all table entries
	for (i = 0; i < ARRAYSIZEOF(MemMap); ++i)
	{
		if (MemMap[i].byType == cCurrentRomType)
		{
			pMapping = &MemMap[i];			// found entry
			return TRUE;
		}
	}
	return FALSE;
}

BOOL SetMemMapType(enum MEM_MAPPING eType)
{
	BOOL bSucc = TRUE;

	eMapType = eType;

	switch (eMapType)
	{
	case MEM_MMU:	
		pbyMapData = NULL;					// data
		dwMapDataSize = 512 * 1024 * 2;		// CPU address range
		dwMapDataMask = dwMapDataSize - 1;	// size mask
		break;
	case MEM_NCE1_M:
		pbyMapData = *pMapping->ppbyNCE1;
		dwMapDataSize = *pMapping->pdwNCE1Size;
		dwMapDataMask = dwMapDataSize - 1;	// size mask
		break;
	case MEM_NCE2_M:
		pbyMapData = *pMapping->ppbyNCE2 ? *pMapping->ppbyNCE2 : (pbyRom + Chipset.RomSize);
		dwMapDataSize = *pMapping->pdwNCE2Size;
		dwMapDataMask = dwMapDataSize - 1;	// size mask
		break;
	case MEM_NCE3_M:
		pbyMapData = *pMapping->ppbyNCE3 ? *pMapping->ppbyNCE3 : (pbyRom + Chipset.RomSize);
		dwMapDataSize = *pMapping->pdwNCE3Size;
		dwMapDataMask = dwMapDataSize - 1;	// size mask
		break;
	case MEM_NCE1_S:
		pbyMapData = *pMapping->ppbyNCE1S + ChipsetS.RomSize;
		dwMapDataSize = *pMapping->pdwNCE1SizeS;
		dwMapDataMask = dwMapDataSize - 1;	// size mask
		break;
	case MEM_NCE2_S:
		pbyMapData = *pMapping->ppbyNCE2S ? *pMapping->ppbyNCE2S : (pbyRom + Chipset.RomSize);
		dwMapDataSize = *pMapping->pdwNCE2SizeS;
		dwMapDataMask = dwMapDataSize - 1;	// size mask
		break;
	case MEM_NCE3_S:
		pbyMapData = *pMapping->ppbyNCE3S ? *pMapping->ppbyNCE3S : (pbyRom + Chipset.RomSize);
		dwMapDataSize = *pMapping->pdwNCE3SizeS;
		dwMapDataMask = dwMapDataSize - 1;	// size mask
		break;
	default: _ASSERT(FALSE);
		pbyMapData = NULL;
		dwMapDataSize = 0;
		dwMapDataMask = 0;
		bSucc = FALSE;
	}
	return bSucc;
}

enum MEM_MAPPING GetMemMapType(VOID)
{
	return eMapType;
}

BOOL GetMemAvail(enum MEM_MAPPING eType)
{
	switch (eType)
	{
	case MEM_MMU:  return TRUE;
	case MEM_NCE1_M: return Chipset.RomMask   != 0;
	case MEM_NCE2_M: return Chipset.NCE2Mask  != 0 && Chipset.NCE2Size  != 0;
	case MEM_NCE3_M: return Chipset.NCE3Mask  != 0 && Chipset.NCE3Size  != 0;
	case MEM_NCE1_S: return ChipsetS.RomMask  != 0;
	case MEM_NCE2_S: return ChipsetS.NCE2Mask != 0 && ChipsetS.NCE2Size != 0;
	case MEM_NCE3_S: return ChipsetS.NCE3Mask != 0 && ChipsetS.NCE3Size != 0;
	default: _ASSERT(FALSE);
	}
	return FALSE;
}

DWORD GetMemDataSize(VOID)
{
	return dwMapDataSize;
}

DWORD GetMemDataMask(VOID)
{
	return dwMapDataMask;
}

BYTE GetMemNib(DWORD *p)
{
	BYTE byVal;

	if (pbyMapData == NULL)
	{
		Npeek(&byVal, *p, 1);
	}
	else
	{
		byVal = pbyMapData[*p];
	}
	*p = (*p + 1) & dwMapDataMask;
	return byVal;
}

VOID GetMemPeek(BYTE *a, DWORD d, UINT s)
{
	if (pbyMapData == NULL)
	{
		Npeek(a, d, s);
	}
	else
	{
		for (; s > 0; --s, ++d)
		{
			*a++ = pbyMapData[d & dwMapDataMask];
		}
	}
	return;
}
