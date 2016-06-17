/* 
   Common Flash Interface probe code.
   (C) 2000 Red Hat. GPL'd.
   $Id: jedec_probe.c,v 1.19 2002/11/12 13:12:10 dwmw2 Exp $
   See JEDEC (http://www.jedec.org/) standard JESD21C (section 3.5)
   for the standard this probe goes back to.
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <linux/mtd/map.h>
#include <linux/mtd/cfi.h>
#include <linux/mtd/gen_probe.h>

/* Manufacturers */
#define MANUFACTURER_AMD	0x0001
#define MANUFACTURER_ATMEL	0x001f
#define MANUFACTURER_FUJITSU	0x0004
#define MANUFACTURER_INTEL	0x0089
#define MANUFACTURER_MACRONIX	0x00C2
#define MANUFACTURER_ST		0x0020
#define MANUFACTURER_SST	0x00BF
#define MANUFACTURER_TOSHIBA	0x0098


/* AMD */
#define AM29F800BB	0x2258
#define AM29F800BT	0x22D6
#define AM29LV800BB	0x225B
#define AM29LV800BT	0x22DA
#define AM29LV160DT	0x22C4
#define AM29LV160DB	0x2249
#define AM29F017D	0x003D
#define AM29F016	0x00AD
#define AM29F080	0x00D5
#define AM29F040	0x00A4
#define AM29LV040B	0x004F
#define AM29F032B	0x0041

/* Atmel */
#define AT49BV512	0x0003
#define AT29LV512	0x003d
#define AT49BV16X	0x00C0
#define AT49BV16XT	0x00C2
#define AT49BV32X	0x00C8
#define AT49BV32XT	0x00C9

/* Fujitsu */
#define MBM29LV650UE	0x22D7
#define MBM29LV320TE	0x22F6
#define MBM29LV320BE	0x22F9
#define MBM29LV160TE	0x22C4
#define MBM29LV160BE	0x2249
#define MBM29LV800BA	0x225B
#define MBM29LV800TA	0x22DA

/* Intel */
#define I28F004B3T	0x00d4
#define I28F004B3B	0x00d5
#define I28F400B3T	0x8894
#define I28F400B3B	0x8895
#define I28F008S5	0x00a6
#define I28F016S5	0x00a0
#define I28F008SA	0x00a2
#define I28F008B3T	0x00d2
#define I28F008B3B	0x00d3
#define I28F800B3T	0x8892
#define I28F800B3B	0x8893
#define I28F016S3	0x00aa
#define I28F016B3T	0x00d0
#define I28F016B3B	0x00d1
#define I28F160B3T	0x8890
#define I28F160B3B	0x8891
#define I28F320B3T	0x8896
#define I28F320B3B	0x8897
#define I28F640B3T	0x8898
#define I28F640B3B	0x8899
#define I82802AB	0x00ad
#define I82802AC	0x00ac

/* Macronix */
#define MX29LV160T	0x22C4
#define MX29LV160B	0x2249
#define MX29F016	0x00AD
#define MX29F004T	0x0045
#define MX29F004B	0x0046

/* ST - www.st.com */
#define M29W800T	0x00D7
#define M29W160DT	0x22C4
#define M29W160DB	0x2249
#define M29W040B	0x00E3

/* SST */
#define SST29EE512	0x005d
#define SST29LE512	0x003d
#define SST39LF800	0x2781
#define SST39LF160	0x2782
#define SST39LF512	0x00D4
#define SST39LF010	0x00D5
#define SST39LF020	0x00D6
#define SST39LF040	0x00D7
#define SST39SF010A	0x00B5
#define SST39SF020A	0x00B6
#define SST49LF030A	0x001C
#define SST49LF040A	0x0051
#define SST49LF080A	0x005B

/* Toshiba */
#define TC58FVT160	0x00C2
#define TC58FVB160	0x0043
#define TC58FVT321	0x009A
#define TC58FVB321	0x009C
#define TC58FVT641	0x0093
#define TC58FVB641	0x0095


struct amd_flash_info {
	const __u16 mfr_id;
	const __u16 dev_id;
	const char *name;
	const int DevSize;
	const int InterfaceDesc;
	const int NumEraseRegions;
	const int CmdSet;
	const ulong regions[4];
};

#define ERASEINFO(size,blocks) (size<<8)|(blocks-1)

#define SIZE_64KiB  16
#define SIZE_128KiB 17
#define SIZE_256KiB 18
#define SIZE_512KiB 19
#define SIZE_1MiB   20
#define SIZE_2MiB   21
#define SIZE_4MiB   22
#define SIZE_8MiB   23

static const struct amd_flash_info jedec_table[] = {
	{
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29F032B,
		name: "AMD AM29F032B",
		DevSize: SIZE_4MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,64)
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29LV160DT,
		name: "AMD AM29LV160DT",
		DevSize: SIZE_2MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,31),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29LV160DB,
		name: "AMD AM29LV160DB",
		DevSize: SIZE_2MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x04000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x10000,31)
		}
	}, {
		mfr_id: MANUFACTURER_TOSHIBA,
		dev_id: TC58FVT160,
		name: "Toshiba TC58FVT160",
		DevSize: SIZE_2MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,31),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id: MANUFACTURER_TOSHIBA,
		dev_id: TC58FVB160,
		name: "Toshiba TC58FVB160",
		DevSize: SIZE_2MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x04000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x10000,31)
		}
	}, {
		mfr_id: MANUFACTURER_TOSHIBA,
		dev_id: TC58FVB321,
		name: "Toshiba TC58FVB321",
		DevSize: SIZE_4MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x02000,8),
			  ERASEINFO(0x10000,63)
		}
	}, {
		mfr_id: MANUFACTURER_TOSHIBA,
		dev_id: TC58FVT321,
		name: "Toshiba TC58FVT321",
		DevSize: SIZE_4MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x10000,63),
			  ERASEINFO(0x02000,8)
		}
	}, {
		mfr_id: MANUFACTURER_TOSHIBA,
		dev_id: TC58FVB641,
		name: "Toshiba TC58FVB641",
		DevSize: SIZE_8MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x02000,8),
			  ERASEINFO(0x10000,127)
		}
	}, {
		mfr_id: MANUFACTURER_TOSHIBA,
		dev_id: TC58FVT641,
		name: "Toshiba TC58FVT641",
		DevSize: SIZE_8MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x10000,127),
			  ERASEINFO(0x02000,8)
		}
	}, {
		mfr_id: MANUFACTURER_FUJITSU,
		dev_id: MBM29LV650UE,
		name: "Fujitsu MBM29LV650UE",
		DevSize: SIZE_8MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,128)
		}
	}, {
		mfr_id: MANUFACTURER_FUJITSU,
		dev_id: MBM29LV320TE,
		name: "Fujitsu MBM29LV320TE",
		DevSize: SIZE_4MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x10000,63),
			  ERASEINFO(0x02000,8)
		}
	}, {
		mfr_id: MANUFACTURER_FUJITSU,
		dev_id: MBM29LV320BE,
		name: "Fujitsu MBM29LV320BE",
		DevSize: SIZE_4MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x02000,8),
			  ERASEINFO(0x10000,63)
		}
	}, {
		mfr_id: MANUFACTURER_FUJITSU,
		dev_id: MBM29LV160TE,
		name: "Fujitsu MBM29LV160TE",
		DevSize: SIZE_2MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,31),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id: MANUFACTURER_FUJITSU,
		dev_id: MBM29LV160BE,
		name: "Fujitsu MBM29LV160BE",
		DevSize: SIZE_2MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x04000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x10000,31)
		}
	}, {
		mfr_id: MANUFACTURER_FUJITSU,
		dev_id: MBM29LV800BA,
		name: "Fujitsu MBM29LV800BA",
		DevSize: SIZE_1MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x04000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x10000,15)
		}
	}, {
		mfr_id: MANUFACTURER_FUJITSU,
		dev_id: MBM29LV800TA,
		name: "Fujitsu MBM29LV800TA",
		DevSize: SIZE_1MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,15),
 			  ERASEINFO(0x08000,1),
 			  ERASEINFO(0x02000,2),
 			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29LV800BB,
		name: "AMD AM29LV800BB",
		DevSize: SIZE_1MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x04000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x10000,15),
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29F800BB,
		name: "AMD AM29F800BB",
		DevSize: SIZE_1MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x04000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x10000,15),
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29LV800BT,
		name: "AMD AM29LV800BT",
		DevSize: SIZE_1MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,15),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29F800BT,
		name: "AMD AM29F800BT",
		DevSize: SIZE_1MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,15),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29LV800BB,
		name: "AMD AM29LV800BB",
		DevSize: SIZE_1MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,15),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F004B3B,
		name:			"Intel 28F004B3B",
		DevSize:		SIZE_512KiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x02000, 8),
			ERASEINFO(0x10000, 7),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F004B3T,
		name:			"Intel 28F004B3T",
		DevSize:		SIZE_512KiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x10000, 7),
			ERASEINFO(0x02000, 8),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F400B3B,
		name:			"Intel 28F400B3B",
		DevSize:		SIZE_512KiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x02000, 8),
			ERASEINFO(0x10000, 7),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F400B3T,
		name:			"Intel 28F400B3T",
		DevSize:		SIZE_512KiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x10000, 7),
			ERASEINFO(0x02000, 8),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F008B3B,
		name:			"Intel 28F008B3B",
		DevSize:		SIZE_1MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x02000, 8),
			ERASEINFO(0x10000, 15),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F008B3T,
		name:			"Intel 28F008B3T",
		DevSize:		SIZE_1MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x10000, 15),
			ERASEINFO(0x02000, 8),
		}
	}, {
		mfr_id: MANUFACTURER_INTEL,
		dev_id: I28F008S5,
		name: "Intel 28F008S5",
		DevSize: SIZE_1MiB,
		CmdSet: P_ID_INTEL_EXT,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,16),
		}
	}, {
		mfr_id: MANUFACTURER_INTEL,
		dev_id: I28F016S5,
		name: "Intel 28F016S5",
		DevSize: SIZE_2MiB,
		CmdSet: P_ID_INTEL_EXT,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,32),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F008SA,
		name:			"Intel 28F008SA",
		DevSize:		SIZE_1MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	1,
		regions: {
			ERASEINFO(0x10000, 16),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F800B3B,
		name:			"Intel 28F800B3B",
		DevSize:		SIZE_1MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x02000, 8),
			ERASEINFO(0x10000, 15),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F800B3T,
		name:			"Intel 28F800B3T",
		DevSize:		SIZE_1MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x10000, 15),
			ERASEINFO(0x02000, 8),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F016B3B,
		name:			"Intel 28F016B3B",
		DevSize:		SIZE_2MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x02000, 8),
			ERASEINFO(0x10000, 31),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F016S3,
		name:			"Intel I28F016S3",
		DevSize:		SIZE_2MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	1,
		regions: {
			ERASEINFO(0x10000, 32),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F016B3T,
		name:			"Intel 28F016B3T",
		DevSize:		SIZE_2MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x10000, 31),
			ERASEINFO(0x02000, 8),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F160B3B,
		name:			"Intel 28F160B3B",
		DevSize:		SIZE_2MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x02000, 8),
			ERASEINFO(0x10000, 31),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F160B3T,
		name:			"Intel 28F160B3T",
		DevSize:		SIZE_2MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x10000, 31),
			ERASEINFO(0x02000, 8),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F320B3B,
		name:			"Intel 28F320B3B",
		DevSize:		SIZE_4MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x02000, 8),
			ERASEINFO(0x10000, 63),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F320B3T,
		name:			"Intel 28F320B3T",
		DevSize:		SIZE_4MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x10000, 63),
			ERASEINFO(0x02000, 8),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F640B3B,
		name:			"Intel 28F640B3B",
		DevSize:		SIZE_8MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x02000, 8),
			ERASEINFO(0x10000, 127),
		}
	}, {
		mfr_id:			MANUFACTURER_INTEL,
		dev_id:			I28F640B3T,
		name:			"Intel 28F640B3T",
		DevSize:		SIZE_8MiB,
		CmdSet:			P_ID_INTEL_STD,
		NumEraseRegions:	2,
		regions: {
			ERASEINFO(0x10000, 127),
			ERASEINFO(0x02000, 8),
		}
	}, {
		mfr_id: MANUFACTURER_INTEL,
		dev_id: I82802AB,
		name: "Intel 82802AB",
		DevSize: SIZE_512KiB,
		CmdSet: P_ID_INTEL_EXT,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,8),
		}
	}, {
		mfr_id: MANUFACTURER_INTEL,
		dev_id: I82802AC,
		name: "Intel 82802AC",
		DevSize: SIZE_1MiB,
		CmdSet: P_ID_INTEL_EXT,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,16),
		}
	}, {
		mfr_id: MANUFACTURER_ST,
		dev_id: M29W800T,
		name: "ST M29W800T",
		DevSize: SIZE_1MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,15),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id: MANUFACTURER_ST,
		dev_id: M29W160DT,
		name: "ST M29W160DT",
		DevSize: SIZE_2MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,31),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id: MANUFACTURER_ST,
		dev_id: M29W160DB,
		name: "ST M29W160DB",
		DevSize: SIZE_2MiB,
		CmdSet:	P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x04000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x10000,31)
		}
	}, {
		mfr_id: MANUFACTURER_ATMEL,
		dev_id: AT49BV512,
		name: "Atmel AT49BV512",
		DevSize: SIZE_64KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,1)
		}
	}, {
		mfr_id: MANUFACTURER_ATMEL,
		dev_id: AT29LV512,
		name: "Atmel AT29LV512",
		DevSize: SIZE_64KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {
			ERASEINFO(0x80,256),
			ERASEINFO(0x80,256)
		}
	}, {
		mfr_id: MANUFACTURER_ATMEL,
		dev_id: AT49BV16X,
		name: "Atmel AT49BV16X",
		DevSize: SIZE_2MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x02000,8),
			  ERASEINFO(0x10000,31)
		}
	}, {
		mfr_id: MANUFACTURER_ATMEL,
		dev_id: AT49BV16XT,
		name: "Atmel AT49BV16XT",
		DevSize: SIZE_2MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x10000,31),
			  ERASEINFO(0x02000,8)
		}
	}, {
		mfr_id: MANUFACTURER_ATMEL,
		dev_id: AT49BV32X,
		name: "Atmel AT49BV32X",
		DevSize: SIZE_4MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x02000,8),
			  ERASEINFO(0x10000,63)
		}
	}, {
		mfr_id: MANUFACTURER_ATMEL,
		dev_id: AT49BV32XT,
		name: "Atmel AT49BV32XT",
		DevSize: SIZE_4MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 2,
		regions: {ERASEINFO(0x10000,63),
			  ERASEINFO(0x02000,8)
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29F017D,
		name: "AMD AM29F017D",
		DevSize: SIZE_2MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,32),
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29F016,
		name: "AMD AM29F016",
		DevSize: SIZE_2MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,32),
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29F080,
		name: "AMD AM29F080",
		DevSize: SIZE_1MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,16),
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29F040,
		name: "AMD AM29F040",
		DevSize: SIZE_512KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,8),
		}
	}, {
		mfr_id: MANUFACTURER_AMD,
		dev_id: AM29LV040B,
		name: "AMD AM29LV040B",
		DevSize: SIZE_512KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,8),
		}
        }, {
		mfr_id: MANUFACTURER_ST,
		dev_id: M29W040B,
		name: "ST M29W040B",
		DevSize: SIZE_512KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,8),
		}
	}, {
		mfr_id: MANUFACTURER_MACRONIX,
		dev_id: MX29LV160T,
		name: "MXIC MX29LV160T",
		DevSize: SIZE_2MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,31),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1)
		}
	}, {
		mfr_id: MANUFACTURER_MACRONIX,
		dev_id: MX29LV160B,
		name: "MXIC MX29LV160B",
		DevSize: SIZE_2MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x04000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x10000,31)
		}
	}, {
		mfr_id: MANUFACTURER_MACRONIX,
		dev_id: MX29F016,
		name: "Macronix MX29F016",
		DevSize: SIZE_2MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x10000,32),
		}
        }, {
		mfr_id: MANUFACTURER_MACRONIX,
		dev_id: MX29F004T,
		name: "Macronix MX29F004T",
		DevSize: SIZE_512KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x10000,7),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x04000,1),
		}
        }, {
		mfr_id: MANUFACTURER_MACRONIX,
		dev_id: MX29F004B,
		name: "Macronix MX29F004B",
		DevSize: SIZE_512KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 4,
		regions: {ERASEINFO(0x04000,1),
			  ERASEINFO(0x02000,2),
			  ERASEINFO(0x08000,1),
			  ERASEINFO(0x10000,7),
		}
        }, {
		mfr_id: MANUFACTURER_SST,
		dev_id: SST39LF512,
		name: "SST 39LF512",
		DevSize: SIZE_64KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x01000,16),
		}
        }, {
		mfr_id: MANUFACTURER_SST,
		dev_id: SST39LF010,
		name: "SST 39LF010",
		DevSize: SIZE_128KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x01000,32),
		}
        }, {
		mfr_id: MANUFACTURER_SST,
		dev_id: SST39LF020,
		name: "SST 39LF020",
		DevSize: SIZE_256KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x01000,64),
		}
        }, {
		mfr_id: MANUFACTURER_SST,
		dev_id: SST39LF040,
		name: "SST 39LF040",
		DevSize: SIZE_512KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x01000,128),
		}
        }, {
		mfr_id: MANUFACTURER_SST,
		dev_id: SST39SF010A,
		name: "SST 39SF010A",
		DevSize: SIZE_128KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x01000,32),
		}
        }, {
		mfr_id: MANUFACTURER_SST,
		dev_id: SST39SF020A,
		name: "SST 39SF020A",
		DevSize: SIZE_256KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x01000,64),
		}
	}, {
		mfr_id: MANUFACTURER_SST,
		dev_id: SST49LF030A,
		name: "SST 49LF030A",
		DevSize: SIZE_512KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x01000,96),
		}
	}, {
		mfr_id: MANUFACTURER_SST,
		dev_id: SST49LF040A,
		name: "SST 49LF040A",
		DevSize: SIZE_512KiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x01000,128),
		}
	}, {
		mfr_id: MANUFACTURER_SST,
		dev_id: SST49LF080A,
		name: "SST 49LF080A",
		DevSize: SIZE_1MiB,
		CmdSet: P_ID_AMD_STD,
		NumEraseRegions: 1,
		regions: {ERASEINFO(0x01000,256),
		}
	} 
};


static int cfi_jedec_setup(struct cfi_private *p_cfi, int index);

static int jedec_probe_chip(struct map_info *map, __u32 base,
			    struct flchip *chips, struct cfi_private *cfi);

struct mtd_info *jedec_probe(struct map_info *map);

static inline u32 jedec_read_mfr(struct map_info *map, __u32 base, 
	struct cfi_private *cfi)
{
	u32 result, mask;
	mask = (1 << (cfi->device_type * 8)) -1;
	result = cfi_read(map, base);
	result &= mask;
	return result;
}

static inline u32 jedec_read_id(struct map_info *map, __u32 base, 
	struct cfi_private *cfi)
{
	int osf;
	u32 result, mask;
	osf = cfi->interleave *cfi->device_type;
	mask = (1 << (cfi->device_type * 8)) -1;
	result = cfi_read(map, base + osf);
	result &= mask;
	return result;
}

static inline void jedec_reset(u32 base, struct map_info *map, 
	struct cfi_private *cfi)
{
	/* Reset */
	cfi_send_gen_cmd(0xF0, 0, base, map, cfi, cfi->device_type, NULL);
	/* Some misdesigned intel chips do not respond for 0xF0 for a reset,
	 * so ensure we're in read mode.  Send both the Intel and the AMD command
	 * for this.  Intel uses 0xff for this, AMD uses 0xff for NOP, so
	 * this should be safe.
	 */ 
	cfi_send_gen_cmd(0xFF, 0, base, map, cfi, cfi->device_type, NULL);

}
static int cfi_jedec_setup(struct cfi_private *p_cfi, int index)
{
	int i,num_erase_regions;

	printk("Found: %s\n",jedec_table[index].name);

	num_erase_regions = jedec_table[index].NumEraseRegions;
	
	p_cfi->cfiq = kmalloc(sizeof(struct cfi_ident) + num_erase_regions * 4, GFP_KERNEL);
	if (!p_cfi->cfiq) {
		//xx printk(KERN_WARNING "%s: kmalloc failed for CFI ident structure\n", map->name);
		return 0;
	}

	memset(p_cfi->cfiq,0,sizeof(struct cfi_ident));	

	p_cfi->cfiq->P_ID = jedec_table[index].CmdSet;
	p_cfi->cfiq->NumEraseRegions = jedec_table[index].NumEraseRegions;
	p_cfi->cfiq->DevSize = jedec_table[index].DevSize;
	p_cfi->cfi_mode = CFI_MODE_JEDEC;

	for (i=0; i<num_erase_regions; i++){
		p_cfi->cfiq->EraseRegionInfo[i] = jedec_table[index].regions[i];
	}
	p_cfi->cmdset_priv = 0;
	return 1; 	/* ok */
}

static int jedec_probe_chip(struct map_info *map, __u32 base,
			      struct flchip *chips, struct cfi_private *cfi)
{
	int i;
	int unlockpass = 0;

	if (!cfi->numchips) {
		switch (cfi->device_type) {
		case CFI_DEVICETYPE_X8:
			cfi->addr_unlock1 = 0x555; 
			cfi->addr_unlock2 = 0x2aa; 
			break;
		case CFI_DEVICETYPE_X16:
			cfi->addr_unlock1 = 0xaaa;
			if (map->buswidth == cfi->interleave) {
				/* X16 chip(s) in X8 mode */
				cfi->addr_unlock2 = 0x555;
			} else {
				cfi->addr_unlock2 = 0x554;
			}
			break;
		case CFI_DEVICETYPE_X32:
			cfi->addr_unlock1 = 0x1555; 
			cfi->addr_unlock2 = 0xaaa; 
			break;
		default:
			printk(KERN_NOTICE "Eep. Unknown jedec_probe device type %d\n", cfi->device_type);
		return 0;
		}
	}

 retry:
	/* Make certain we aren't probing past the end of map */
	if (base >= map->size) {
		printk(KERN_NOTICE
			"Probe at base(0x%08x) past the end of the map(0x%08lx)\n",
			base, map->size -1);
		return 0;
		
	}
	if ((base + cfi->addr_unlock1) >= map->size) {
		printk(KERN_NOTICE
			"Probe at addr_unlock1(0x%08x + 0x%08x) past the end of the map(0x%08lx)\n",
			base, cfi->addr_unlock1, map->size -1);

		return 0;
	}
	if ((base + cfi->addr_unlock2) >= map->size) {
		printk(KERN_NOTICE
			"Probe at addr_unlock2(0x%08x + 0x%08x) past the end of the map(0x%08lx)\n",
			base, cfi->addr_unlock2, map->size -1);
		return 0;
		
	}

	/* Reset */
	jedec_reset(base, map, cfi);

	/* Autoselect Mode */
	if(cfi->addr_unlock1) {
		cfi_send_gen_cmd(0xaa, cfi->addr_unlock1, base, map, cfi, CFI_DEVICETYPE_X8, NULL);
		cfi_send_gen_cmd(0x55, cfi->addr_unlock2, base, map, cfi, CFI_DEVICETYPE_X8, NULL);
	}
	cfi_send_gen_cmd(0x90, cfi->addr_unlock1, base, map, cfi, CFI_DEVICETYPE_X8, NULL);

	if (!cfi->numchips) {
		/* This is the first time we're called. Set up the CFI 
		   stuff accordingly and return */
		
		cfi->mfr = jedec_read_mfr(map, base, cfi);
		cfi->id = jedec_read_id(map, base, cfi);
		printk(KERN_INFO "Search for id:(%02x %02x) interleave(%d) type(%d)\n", 
			cfi->mfr, cfi->id, cfi->interleave, cfi->device_type);
		for (i=0; i<sizeof(jedec_table)/sizeof(jedec_table[0]); i++) {
			if (cfi->mfr == jedec_table[i].mfr_id &&
			    cfi->id == jedec_table[i].dev_id) {
				if (!cfi_jedec_setup(cfi, i))
					return 0;
				goto ok_out;
			}
		}
		switch(unlockpass++) {
		case 0:
			cfi->addr_unlock1 |= cfi->addr_unlock1 << 4;
			cfi->addr_unlock2 |= cfi->addr_unlock2 << 4;
			goto retry;
		case 1:
			cfi->addr_unlock1 = cfi->addr_unlock2 = 0;
			goto retry;
		}
		return 0;
	} else {
		__u16 mfr;
		__u16 id;

		/* Make sure it is a chip of the same manufacturer and id */
		mfr = jedec_read_mfr(map, base, cfi);
		id = jedec_read_id(map, base, cfi);

		if ((mfr != cfi->mfr) || (id != cfi->id)) {
			printk(KERN_DEBUG "%s: Found different chip or no chip at all (mfr 0x%x, id 0x%x) at 0x%x\n",
			       map->name, mfr, id, base);
			jedec_reset(base, map, cfi);
			return 0;
		}
	}
	
	/* Check each previous chip to see if it's an alias */
	for (i=0; i<cfi->numchips; i++) {
		/* This chip should be in read mode if it's one
		   we've already touched. */
		if (jedec_read_mfr(map, chips[i].start, cfi) == cfi->mfr &&
		    jedec_read_id(map, chips[i].start, cfi) == cfi->id) {
			/* Eep. This chip also looks like it's in autoselect mode.
			   Is it an alias for the new one? */
			jedec_reset(chips[i].start, map, cfi);

			/* If the device IDs go away, it's an alias */
			if (jedec_read_mfr(map, base, cfi) != cfi->mfr ||
			    jedec_read_id(map, base, cfi) != cfi->id) {
				printk(KERN_DEBUG "%s: Found an alias at 0x%x for the chip at 0x%lx\n",
				       map->name, base, chips[i].start);
				return 0;
			}
			
			/* Yes, it's actually got the device IDs as data. Most
			 * unfortunate. Stick the new chip in read mode
			 * too and if it's the same, assume it's an alias. */
			/* FIXME: Use other modes to do a proper check */
			jedec_reset(base, map, cfi);
			if (jedec_read_mfr(map, base, cfi) == cfi->mfr &&
			    jedec_read_id(map, base, cfi) == cfi->id) {
				printk(KERN_DEBUG "%s: Found an alias at 0x%x for the chip at 0x%lx\n",
				       map->name, base, chips[i].start);
				return 0;
			}
		}
	}
		
	/* OK, if we got to here, then none of the previous chips appear to
	   be aliases for the current one. */
	if (cfi->numchips == MAX_CFI_CHIPS) {
		printk(KERN_WARNING"%s: Too many flash chips detected. Increase MAX_CFI_CHIPS from %d.\n", map->name, MAX_CFI_CHIPS);
		/* Doesn't matter about resetting it to Read Mode - we're not going to talk to it anyway */
		return -1;
	}
	chips[cfi->numchips].start = base;
	chips[cfi->numchips].state = FL_READY;
	cfi->numchips++;
		
ok_out:
	/* Put it back into Read Mode */
	jedec_reset(base, map, cfi);

	printk(KERN_INFO "%s: Found %d x%d devices at 0x%x in %d-bit mode\n",
	       map->name, cfi->interleave, cfi->device_type*8, base, 
	       map->buswidth*8);
	
	return 1;
}

static struct chip_probe jedec_chip_probe = {
	name: "JEDEC",
	probe_chip: jedec_probe_chip
};

struct mtd_info *jedec_probe(struct map_info *map)
{
	/*
	 * Just use the generic probe stuff to call our CFI-specific
	 * chip_probe routine in all the possible permutations, etc.
	 */
	return mtd_do_chip_probe(map, &jedec_chip_probe);
}

static struct mtd_chip_driver jedec_chipdrv = {
	probe: jedec_probe,
	name: "jedec_probe",
	module: THIS_MODULE
};

int __init jedec_probe_init(void)
{
	register_mtd_chip_driver(&jedec_chipdrv);
	return 0;
}

static void __exit jedec_probe_exit(void)
{
	unregister_mtd_chip_driver(&jedec_chipdrv);
}

module_init(jedec_probe_init);
module_exit(jedec_probe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Erwin Authried <eauth@softsys.co.at> et al.");
MODULE_DESCRIPTION("Probe code for JEDEC-compliant flash chips");
