#ifndef __TEST_H__
#define __TEST_H__

#include "ftl_def.h"

#include "ftl_util.h"
#include "ftl_global.h"
#include "flash_if.h"
#include "ftl_if.h"

#define	NANDSIM_TEST			0
#define DRIVER_TEST				1
#define MTD_TEST				0

#define FTL_UNIT_TEST			0
#define FTL_SYSTEM_TEST			0

#define FTL_LOW_FORMAT_TEST		0
#define FTL_INIT_TEST			0
#define FTL_LOOKUP_TABLE_TEST	0
#define FTL_GC_TEST				0
#define FTL_SECTOR_TEST			0

//a physical page memory
extern UINT32 g_spareData[];

extern UINT8 FTL_Malloc(UINT8 zoneAmount,UINT16 vpbPerZone,UINT8 vppPerVPB);
extern UINT8 FTL_Init_f(UINT8 flag);
extern UINT8 FTL_LowFormat_f(void);
extern void unit_test(void);
extern void system_test(void);

#endif