#ifndef HPT366_H
#define HPT366_H

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/ide.h>

#define DISPLAY_HPT366_TIMINGS

/* various tuning parameters */
#define HPT_RESET_STATE_ENGINE
#undef HPT_DELAY_INTERRUPT
#undef HPT_SERIALIZE_IO

const char *quirk_drives[] = {
	"QUANTUM FIREBALLlct08 08",
	"QUANTUM FIREBALLP KA6.4",
	"QUANTUM FIREBALLP LM20.4",
	"QUANTUM FIREBALLP LM20.5",
        NULL
};

const char *bad_ata100_5[] = {
	"IBM-DTLA-307075",
	"IBM-DTLA-307060",
	"IBM-DTLA-307045",
	"IBM-DTLA-307030",
	"IBM-DTLA-307020",
	"IBM-DTLA-307015",
	"IBM-DTLA-305040",
	"IBM-DTLA-305030",
	"IBM-DTLA-305020",
	"IC35L010AVER07-0",
	"IC35L020AVER07-0",
	"IC35L030AVER07-0",
	"IC35L040AVER07-0",
	"IC35L060AVER07-0",
	"WDC AC310200R",
	NULL
};

const char *bad_ata66_4[] = {
	"IBM-DTLA-307075",
	"IBM-DTLA-307060",
	"IBM-DTLA-307045",
	"IBM-DTLA-307030",
	"IBM-DTLA-307020",
	"IBM-DTLA-307015",
	"IBM-DTLA-305040",
	"IBM-DTLA-305030",
	"IBM-DTLA-305020",
	"IC35L010AVER07-0",
	"IC35L020AVER07-0",
	"IC35L030AVER07-0",
	"IC35L040AVER07-0",
	"IC35L060AVER07-0",
	"WDC AC310200R",
	NULL
};

const char *bad_ata66_3[] = {
	"WDC AC310200R",
	NULL
};

const char *bad_ata33[] = {
	"Maxtor 92720U8", "Maxtor 92040U6", "Maxtor 91360U4", "Maxtor 91020U3", "Maxtor 90845U3", "Maxtor 90650U2",
	"Maxtor 91360D8", "Maxtor 91190D7", "Maxtor 91020D6", "Maxtor 90845D5", "Maxtor 90680D4", "Maxtor 90510D3", "Maxtor 90340D2",
	"Maxtor 91152D8", "Maxtor 91008D7", "Maxtor 90845D6", "Maxtor 90840D6", "Maxtor 90720D5", "Maxtor 90648D5", "Maxtor 90576D4",
	"Maxtor 90510D4",
	"Maxtor 90432D3", "Maxtor 90288D2", "Maxtor 90256D2",
	"Maxtor 91000D8", "Maxtor 90910D8", "Maxtor 90875D7", "Maxtor 90840D7", "Maxtor 90750D6", "Maxtor 90625D5", "Maxtor 90500D4",
	"Maxtor 91728D8", "Maxtor 91512D7", "Maxtor 91303D6", "Maxtor 91080D5", "Maxtor 90845D4", "Maxtor 90680D4", "Maxtor 90648D3", "Maxtor 90432D2",
	NULL
};

struct chipset_bus_clock_list_entry {
	byte		xfer_speed;
	unsigned int	chipset_settings;
};

/* key for bus clock timings
 * bit
 * 0:3    data_high_time. inactive time of DIOW_/DIOR_ for PIO and MW
 *        DMA. cycles = value + 1
 * 4:8    data_low_time. active time of DIOW_/DIOR_ for PIO and MW
 *        DMA. cycles = value + 1
 * 9:12   cmd_high_time. inactive time of DIOW_/DIOR_ during task file
 *        register access.
 * 13:17  cmd_low_time. active time of DIOW_/DIOR_ during task file
 *        register access.
 * 18:21  udma_cycle_time. clock freq and clock cycles for UDMA xfer.
 *        during task file register access.
 * 22:24  pre_high_time. time to initialize 1st cycle for PIO and MW DMA
 *        xfer.
 * 25:27  cmd_pre_high_time. time to initialize 1st PIO cycle for task
 *        register access.
 * 28     UDMA enable
 * 29     DMA enable
 * 30     PIO_MST enable. if set, the chip is in bus master mode during
 *        PIO.
 * 31     FIFO enable.
 */
struct chipset_bus_clock_list_entry forty_base_hpt366[] = {
	{	XFER_UDMA_4,	0x900fd943	},
	{	XFER_UDMA_3,	0x900ad943	},
	{	XFER_UDMA_2,	0x900bd943	},
	{	XFER_UDMA_1,	0x9008d943	},
	{	XFER_UDMA_0,	0x9008d943	},

	{	XFER_MW_DMA_2,	0xa008d943	},
	{	XFER_MW_DMA_1,	0xa010d955	},
	{	XFER_MW_DMA_0,	0xa010d9fc	},

	{	XFER_PIO_4,	0xc008d963	},
	{	XFER_PIO_3,	0xc010d974	},
	{	XFER_PIO_2,	0xc010d997	},
	{	XFER_PIO_1,	0xc010d9c7	},
	{	XFER_PIO_0,	0xc018d9d9	},
	{	0,		0x0120d9d9	}
};

struct chipset_bus_clock_list_entry thirty_three_base_hpt366[] = {
	{	XFER_UDMA_4,	0x90c9a731	},
	{	XFER_UDMA_3,	0x90cfa731	},
	{	XFER_UDMA_2,	0x90caa731	},
	{	XFER_UDMA_1,	0x90cba731	},
	{	XFER_UDMA_0,	0x90c8a731	},

	{	XFER_MW_DMA_2,	0xa0c8a731	},
	{	XFER_MW_DMA_1,	0xa0c8a732	},	/* 0xa0c8a733 */
	{	XFER_MW_DMA_0,	0xa0c8a797	},

	{	XFER_PIO_4,	0xc0c8a731	},
	{	XFER_PIO_3,	0xc0c8a742	},
	{	XFER_PIO_2,	0xc0d0a753	},
	{	XFER_PIO_1,	0xc0d0a7a3	},	/* 0xc0d0a793 */
	{	XFER_PIO_0,	0xc0d0a7aa	},	/* 0xc0d0a7a7 */
	{	0,		0x0120a7a7	}
};

struct chipset_bus_clock_list_entry twenty_five_base_hpt366[] = {

	{	XFER_UDMA_4,	0x90c98521	},
	{	XFER_UDMA_3,	0x90cf8521	},
	{	XFER_UDMA_2,	0x90cf8521	},
	{	XFER_UDMA_1,	0x90cb8521	},
	{	XFER_UDMA_0,	0x90cb8521	},

	{	XFER_MW_DMA_2,	0xa0ca8521	},
	{	XFER_MW_DMA_1,	0xa0ca8532	},
	{	XFER_MW_DMA_0,	0xa0ca8575	},

	{	XFER_PIO_4,	0xc0ca8521	},
	{	XFER_PIO_3,	0xc0ca8532	},
	{	XFER_PIO_2,	0xc0ca8542	},
	{	XFER_PIO_1,	0xc0d08572	},
	{	XFER_PIO_0,	0xc0d08585	},
	{	0,		0x01208585	}
};

/* from highpoint documentation. these are old values */
struct chipset_bus_clock_list_entry thirty_three_base_hpt370[] = {
/*	{	XFER_UDMA_5,	0x1A85F442,	0x16454e31	}, */
	{	XFER_UDMA_5,	0x16454e31	},
	{	XFER_UDMA_4,	0x16454e31	},
	{	XFER_UDMA_3,	0x166d4e31	},
	{	XFER_UDMA_2,	0x16494e31	},
	{	XFER_UDMA_1,	0x164d4e31	},
	{	XFER_UDMA_0,	0x16514e31	},

	{	XFER_MW_DMA_2,	0x26514e21	},
	{	XFER_MW_DMA_1,	0x26514e33	},
	{	XFER_MW_DMA_0,	0x26514e97	},

	{	XFER_PIO_4,	0x06514e21	},
	{	XFER_PIO_3,	0x06514e22	},
	{	XFER_PIO_2,	0x06514e33	},
	{	XFER_PIO_1,	0x06914e43	},
	{	XFER_PIO_0,	0x06914e57	},
	{	0,		0x06514e57	}
};

struct chipset_bus_clock_list_entry sixty_six_base_hpt370[] = {
	{       XFER_UDMA_5,    0x14846231      },
	{       XFER_UDMA_4,    0x14886231      },
	{       XFER_UDMA_3,    0x148c6231      },
	{       XFER_UDMA_2,    0x148c6231      },
	{       XFER_UDMA_1,    0x14906231      },
	{       XFER_UDMA_0,    0x14986231      },
	
	{       XFER_MW_DMA_2,  0x26514e21      },
	{       XFER_MW_DMA_1,  0x26514e33      },
	{       XFER_MW_DMA_0,  0x26514e97      },
	
	{       XFER_PIO_4,     0x06514e21      },
	{       XFER_PIO_3,     0x06514e22      },
	{       XFER_PIO_2,     0x06514e33      },
	{       XFER_PIO_1,     0x06914e43      },
	{       XFER_PIO_0,     0x06914e57      },
	{       0,              0x06514e57      }
};

/* these are the current (4 sep 2001) timings from highpoint */
struct chipset_bus_clock_list_entry thirty_three_base_hpt370a[] = {
        {       XFER_UDMA_5,    0x12446231      },
        {       XFER_UDMA_4,    0x12446231      },
        {       XFER_UDMA_3,    0x126c6231      },
        {       XFER_UDMA_2,    0x12486231      },
        {       XFER_UDMA_1,    0x124c6233      },
        {       XFER_UDMA_0,    0x12506297      },

        {       XFER_MW_DMA_2,  0x22406c31      },
        {       XFER_MW_DMA_1,  0x22406c33      },
        {       XFER_MW_DMA_0,  0x22406c97      },

        {       XFER_PIO_4,     0x06414e31      },
        {       XFER_PIO_3,     0x06414e42      },
        {       XFER_PIO_2,     0x06414e53      },
        {       XFER_PIO_1,     0x06814e93      },
        {       XFER_PIO_0,     0x06814ea7      },
        {       0,              0x06814ea7      }
};

/* 2x 33MHz timings */
struct chipset_bus_clock_list_entry sixty_six_base_hpt370a[] = {
	{       XFER_UDMA_5,    0x1488e673       },
	{       XFER_UDMA_4,    0x1488e673       },
	{       XFER_UDMA_3,    0x1498e673       },
	{       XFER_UDMA_2,    0x1490e673       },
	{       XFER_UDMA_1,    0x1498e677       },
	{       XFER_UDMA_0,    0x14a0e73f       },

	{       XFER_MW_DMA_2,  0x2480fa73       },
	{       XFER_MW_DMA_1,  0x2480fa77       }, 
	{       XFER_MW_DMA_0,  0x2480fb3f       },

	{       XFER_PIO_4,     0x0c82be73       },
	{       XFER_PIO_3,     0x0c82be95       },
	{       XFER_PIO_2,     0x0c82beb7       },
	{       XFER_PIO_1,     0x0d02bf37       },
	{       XFER_PIO_0,     0x0d02bf5f       },
	{       0,              0x0d02bf5f       }
};

struct chipset_bus_clock_list_entry fifty_base_hpt370a[] = {
	{       XFER_UDMA_5,    0x12848242      },
	{       XFER_UDMA_4,    0x12ac8242      },
	{       XFER_UDMA_3,    0x128c8242      },
	{       XFER_UDMA_2,    0x120c8242      },
	{       XFER_UDMA_1,    0x12148254      },
	{       XFER_UDMA_0,    0x121882ea      },

	{       XFER_MW_DMA_2,  0x22808242      },
	{       XFER_MW_DMA_1,  0x22808254      },
	{       XFER_MW_DMA_0,  0x228082ea      },

	{       XFER_PIO_4,     0x0a81f442      },
	{       XFER_PIO_3,     0x0a81f443      },
	{       XFER_PIO_2,     0x0a81f454      },
	{       XFER_PIO_1,     0x0ac1f465      },
	{       XFER_PIO_0,     0x0ac1f48a      },
	{       0,              0x0ac1f48a      }
};

struct chipset_bus_clock_list_entry thirty_three_base_hpt372[] = {
	{	XFER_UDMA_6,	0x1c81dc62	},
	{	XFER_UDMA_5,	0x1c6ddc62	},
	{	XFER_UDMA_4,	0x1c8ddc62	},
	{	XFER_UDMA_3,	0x1c8edc62	},	/* checkme */
	{	XFER_UDMA_2,	0x1c91dc62	},
	{	XFER_UDMA_1,	0x1c9adc62	},	/* checkme */
	{	XFER_UDMA_0,	0x1c82dc62	},	/* checkme */

	{	XFER_MW_DMA_2,	0x2c829262	},
	{	XFER_MW_DMA_1,	0x2c829266	},	/* checkme */
	{	XFER_MW_DMA_0,	0x2c82922e	},	/* checkme */

	{	XFER_PIO_4,	0x0c829c62	},
	{	XFER_PIO_3,	0x0c829c84	},
	{	XFER_PIO_2,	0x0c829ca6	},
	{	XFER_PIO_1,	0x0d029d26	},
	{	XFER_PIO_0,	0x0d029d5e	},
	{	0,		0x0d029d5e	}
};

struct chipset_bus_clock_list_entry fifty_base_hpt372[] = {
	{	XFER_UDMA_5,	0x12848242	},
	{	XFER_UDMA_4,	0x12ac8242	},
	{	XFER_UDMA_3,	0x128c8242	},
	{	XFER_UDMA_2,	0x120c8242	},
	{	XFER_UDMA_1,	0x12148254	},
	{	XFER_UDMA_0,	0x121882ea	},

	{	XFER_MW_DMA_2,	0x22808242	},
	{	XFER_MW_DMA_1,	0x22808254	},
	{	XFER_MW_DMA_0,	0x228082ea	},

	{	XFER_PIO_4,	0x0a81f442	},
	{	XFER_PIO_3,	0x0a81f443	},
	{	XFER_PIO_2,	0x0a81f454	},
	{	XFER_PIO_1,	0x0ac1f465	},
	{	XFER_PIO_0,	0x0ac1f48a	},
	{	0,		0x0a81f443	}
};

struct chipset_bus_clock_list_entry sixty_six_base_hpt372[] = {
	{	XFER_UDMA_6,	0x1c869c62	},
	{	XFER_UDMA_5,	0x1cae9c62	},
	{	XFER_UDMA_4,	0x1c8a9c62	},
	{	XFER_UDMA_3,	0x1c8e9c62	},
	{	XFER_UDMA_2,	0x1c929c62	},
	{	XFER_UDMA_1,	0x1c9a9c62	},
	{	XFER_UDMA_0,	0x1c829c62	},

	{	XFER_MW_DMA_2,	0x2c829c62	},
	{	XFER_MW_DMA_1,	0x2c829c66	},
	{	XFER_MW_DMA_0,	0x2c829d2e	},

	{	XFER_PIO_4,	0x0c829c62	},
	{	XFER_PIO_3,	0x0c829c84	},
	{	XFER_PIO_2,	0x0c829ca6	},
	{	XFER_PIO_1,	0x0d029d26	},
	{	XFER_PIO_0,	0x0d029d5e	},
	{	0,		0x0d029d26	}
};

struct chipset_bus_clock_list_entry thirty_three_base_hpt374[] = {
	{	XFER_UDMA_6,	0x12808242	},
	{	XFER_UDMA_5,	0x12848242	},
	{	XFER_UDMA_4,	0x12ac8242	},
	{	XFER_UDMA_3,	0x128c8242	},
	{	XFER_UDMA_2,	0x120c8242	},
	{	XFER_UDMA_1,	0x12148254	},
	{	XFER_UDMA_0,	0x121882ea	},

	{	XFER_MW_DMA_2,	0x22808242	},
	{	XFER_MW_DMA_1,	0x22808254	},
	{	XFER_MW_DMA_0,	0x228082ea	},

	{	XFER_PIO_4,	0x0a81f442	},
	{	XFER_PIO_3,	0x0a81f443	},
	{	XFER_PIO_2,	0x0a81f454	},
	{	XFER_PIO_1,	0x0ac1f465	},
	{	XFER_PIO_0,	0x0ac1f48a	},
	{	0,		0x06814e93	}
};

#if 0
struct chipset_bus_clock_list_entry fifty_base_hpt374[] = {
	{	XFER_UDMA_6,	},
	{	XFER_UDMA_5,	},
	{	XFER_UDMA_4,	},
	{	XFER_UDMA_3,	},
	{	XFER_UDMA_2,	},
	{	XFER_UDMA_1,	},
	{	XFER_UDMA_0,	},
	{	XFER_MW_DMA_2,	},
	{	XFER_MW_DMA_1,	},
	{	XFER_MW_DMA_0,	},
	{	XFER_PIO_4,	},
	{	XFER_PIO_3,	},
	{	XFER_PIO_2,	},
	{	XFER_PIO_1,	},
	{	XFER_PIO_0,	},
	{	0,	}
};
#endif
#if 0
struct chipset_bus_clock_list_entry sixty_six_base_hpt374[] = {
	{	XFER_UDMA_6,	0x12406231	},	/* checkme */
	{	XFER_UDMA_5,	0x12446231	},
				0x14846231
	{	XFER_UDMA_4,		0x16814ea7	},
				0x14886231
	{	XFER_UDMA_3,		0x16814ea7	},
				0x148c6231
	{	XFER_UDMA_2,		0x16814ea7	},
				0x148c6231
	{	XFER_UDMA_1,		0x16814ea7	},
				0x14906231
	{	XFER_UDMA_0,		0x16814ea7	},
				0x14986231
	{	XFER_MW_DMA_2,		0x16814ea7	},
				0x26514e21
	{	XFER_MW_DMA_1,		0x16814ea7	},
				0x26514e97
	{	XFER_MW_DMA_0,		0x16814ea7	},
				0x26514e97
	{	XFER_PIO_4,		0x06814ea7	},
				0x06514e21
	{	XFER_PIO_3,		0x06814ea7	},
				0x06514e22
	{	XFER_PIO_2,		0x06814ea7	},
				0x06514e33
	{	XFER_PIO_1,		0x06814ea7	},
				0x06914e43
	{	XFER_PIO_0,		0x06814ea7	},
				0x06914e57
	{	0,		0x06814ea7	}
};
#endif

#define HPT366_DEBUG_DRIVE_INFO		0
#define HPT374_ALLOW_ATA133_6		0
#define HPT371_ALLOW_ATA133_6		0
#define HPT302_ALLOW_ATA133_6		0
#define HPT372_ALLOW_ATA133_6		1
#define HPT370_ALLOW_ATA100_5		1
#define HPT366_ALLOW_ATA66_4		1
#define HPT366_ALLOW_ATA66_3		1
#define HPT366_MAX_DEVS			8

#define F_LOW_PCI_33      0x23
#define F_LOW_PCI_40      0x29
#define F_LOW_PCI_50      0x2d
#define F_LOW_PCI_66      0x42

#if defined(DISPLAY_HPT366_TIMINGS) && defined(CONFIG_PROC_FS)
#include <linux/stat.h>
#include <linux/proc_fs.h>

static u8 hpt366_proc;

static int hpt366_get_info(char *, char **, off_t, int);

static ide_pci_host_proc_t hpt366_procs[] __initdata = {
	{
		.name		= "hpt366",
		.set		= 1,
		.get_info	= hpt366_get_info,
		.parent		= NULL,
	},
};
#endif  /* defined(DISPLAY_HPT366_TIMINGS) && defined(CONFIG_PROC_FS) */

static void init_setup_hpt366(struct pci_dev *, ide_pci_device_t *);
static void init_setup_hpt37x(struct pci_dev *, ide_pci_device_t *);
static void init_setup_hpt374(struct pci_dev *, ide_pci_device_t *);
static unsigned int init_chipset_hpt366(struct pci_dev *, const char *);
static void init_hwif_hpt366(ide_hwif_t *);
static void init_dma_hpt366(ide_hwif_t *, unsigned long);

static ide_pci_device_t hpt366_chipsets[] __devinitdata = {
	{	/* 0 */
		.vendor		= PCI_VENDOR_ID_TTI,
		.device		= PCI_DEVICE_ID_TTI_HPT366,
		.name		= "HPT366",
		.init_setup	= init_setup_hpt366,
		.init_chipset	= init_chipset_hpt366,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_hpt366,
		.init_dma	= init_dma_hpt366,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= OFF_BOARD,
		.extra		= 240
	},{	/* 1 */
		.vendor		= PCI_VENDOR_ID_TTI,
		.device		= PCI_DEVICE_ID_TTI_HPT372,
		.name		= "HPT372A",
		.init_setup	= init_setup_hpt37x,
		.init_chipset	= init_chipset_hpt366,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_hpt366,
		.init_dma	= init_dma_hpt366,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= OFF_BOARD,
		.extra		= 0
	},{	/* 2 */
		.vendor		= PCI_VENDOR_ID_TTI,
		.device		= PCI_DEVICE_ID_TTI_HPT302,
		.name		= "HPT302",
		.init_setup	= init_setup_hpt37x,
		.init_chipset	= init_chipset_hpt366,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_hpt366,
		.init_dma	= init_dma_hpt366,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= OFF_BOARD,
		.extra		= 0
	},{	/* 3 */
		.vendor		= PCI_VENDOR_ID_TTI,
		.device		= PCI_DEVICE_ID_TTI_HPT371,
		.name		= "HPT371",
		.init_setup	= init_setup_hpt37x,
		.init_chipset	= init_chipset_hpt366,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_hpt366,
		.init_dma	= init_dma_hpt366,
		.channels	= 2,
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= OFF_BOARD,
		.extra		= 0
	},{	/* 4 */
		.vendor		= PCI_VENDOR_ID_TTI,
		.device		= PCI_DEVICE_ID_TTI_HPT374,
		.name		= "HPT374",
		.init_setup	= init_setup_hpt374,
		.init_chipset	= init_chipset_hpt366,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_hpt366,
		.init_dma	= init_dma_hpt366,
		.channels	= 2,	/* 4 */
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= OFF_BOARD,
		.extra		= 0
	},{	/* 5 */
		.vendor		= PCI_VENDOR_ID_TTI,
		.device		= PCI_DEVICE_ID_TTI_HPT372N,
		.name		= "HPT372N",
		.init_setup	= init_setup_hpt37x,
		.init_chipset	= init_chipset_hpt366,
		.init_iops	= NULL,
		.init_hwif	= init_hwif_hpt366,
		.init_dma	= init_dma_hpt366,
		.channels	= 2,	/* 4 */
		.autodma	= AUTODMA,
		.enablebits	= {{0x00,0x00,0x00}, {0x00,0x00,0x00}},
		.bootable	= OFF_BOARD,
		.extra		= 0
	},{
		.vendor		= 0,
		.device		= 0,
		.channels	= 0,
		.bootable	= EOL,
	}
};

#endif /* HPT366_H */
