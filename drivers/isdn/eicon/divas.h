/*
 * External Diva Server driver include file
 *
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.5  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#if !defined(DIVAS_H)
#define DIVAS_H

#include "sys.h"


/* IOCTL commands */

#define	DIA_IOCTL_INIT				(0)
#define	DIA_IOCTL_LOAD				(1)
#define	DIA_IOCTL_CONFIG			(2)
#define	DIA_IOCTL_START				(3)
#define	DIA_IOCTL_GET_NUM			(4)
#define	DIA_IOCTL_GET_LIST			(5)
#define	DIA_IOCTL_LOG				(6)
#define	DIA_IOCTL_DETECT			(7)
#define	DIA_IOCTL_SPACE				(8)
#define DIA_IOCTL_GET_MEM           (9)
#define DIA_IOCTL_FLAVOUR			(10)
#define	DIA_IOCTL_XLOG_REQ			(11)

/* Error codes */

#define XLOG_ERR_CARD_NUM	(13)
#define XLOG_ERR_DONE		(14)
#define XLOG_ERR_CMD		(15)
#define XLOG_ERR_TIMEOUT	(16)
#define XLOG_ERR_CARD_STATE	(17)
#define XLOG_ERR_UNKNOWN	(18)
#define XLOG_OK 			(0)

/* Adapter states */

#define DIA_UNKNOWN		(0)
#define DIA_RESET		(1)
#define DIA_LOADED		(2)
#define DIA_CONFIGURED	(3)
#define DIA_RUNNING		(4)

/* Stucture for getting card specific information from active cad driver */

typedef struct
{
	int card_type;
	int card_slot;
	int	state;
} dia_card_list_t;

/* use following to select which logging to have active */

#define	DIVAS_LOG_DEBUG		(1 << 0)
#define	DIVAS_LOG_XLOG		(1 << 1)
#define	DIVAS_LOG_IDI		(1 << 2)
#define	DIVAS_LOG_CAPI		(1 << 3)

/* stucture for DIA_IOCTL_LOG to get information from adapter */

typedef struct
{
	int		card_id;
	int		log_types;	/* bit mask of log types: use DIVAS_LOG_XXX */
} dia_log_t;

/* list of cards supported by this driver */

#define	DIA_CARD_TYPE_DIVA_SERVER	(0)	/* Diva Server PRI */
#define	DIA_CARD_TYPE_DIVA_SERVER_B	(1)	/* Diva Server BRI */
#define	DIA_CARD_TYPE_DIVA_SERVER_Q	(2)	/* Diva Server 4-BRI */

/* bus types */

#define	DIA_BUS_TYPE_ISA		(0)
#define	DIA_BUS_TYPE_ISA_PNP	(1)
#define	DIA_BUS_TYPE_PCI		(2)
#define	DIA_BUS_TYPE_MCA		(3)

/* types of memory used (index for memory array below) */

#define DIVAS_RAM_MEMORY 	0
#define DIVAS_REG_MEMORY 	1
#define DIVAS_CFG_MEMORY 	2
#define DIVAS_SHARED_MEMORY 3
#define DIVAS_CTL_MEMORY	4
/*
 * card config information
 * passed as parameter to DIA_IOCTL_INIT ioctl to initialise new card
 */

typedef struct
{
	int		card_id;	/* unique id assigned to this card */
	int		card_type;	/* use DIA_CARD_TYPE_xxx above */
	int		bus_type;	/* use DIA_BUS_TYPE_xxx above */
	int		bus_num;	/* bus number (instance number of bus type) */
	int		func_num;	/* adapter function number (PCI register) */
	int		slot;		/* slot number in bus */
	unsigned char	irq;		/* IRQ number */
    int     reset_base; /* Reset register  for I/O mapped cards */
	int		io_base;	/* I/O base for I/O mapped cards */
	void	*memory[5];	/* memory base addresses for memory mapped cards */
	char	name[9];	/* name of adapter */
	int		serial;		/* serial number */
	unsigned char	int_priority;	/* Interrupt priority */
} dia_card_t;

/*
 * protocol configuration information
 * passed as parameter to DIA_IOCTL_CONFIG ioctl to configure card
 */

typedef struct
{
	int				card_id;			/* to identify particular card */
	unsigned char	tei;
	unsigned char	nt2;
	unsigned char	watchdog;
	unsigned char	permanent;
	unsigned char	x_interface;
	unsigned char	stable_l2;
	unsigned char	no_order_check;
	unsigned char	handset_type;
	unsigned char	sig_flags;
	unsigned char	low_channel;
	unsigned char	prot_version;
	unsigned char	crc4;
	struct
	{
		unsigned char oad[32];
		unsigned char osa[32];
		unsigned char spid[32];
	}terminal[2];
} dia_config_t;

/*
 * code configuration 
 * passed as parameter to DIA_IOCTL_LOAD ioctl
 * one of these ioctl per code file to load
 */

typedef struct
{
	int				card_id;	/* card to load */
	enum
	{
		DIA_CPU_CODE,			/* CPU code */
		DIA_DSP_CODE,			/* DSP code */
		DIA_CONT_CODE,			/* continuation of code */
		DIA_TABLE_CODE,			/* code table */
	        DIA_DLOAD_CNT,           /* number of downloads*/
		DIA_FPGA_CODE
	}				code_type;	/* code for CPU or DSP ? */
	int				length;		/* length of code */
	unsigned char	*code;		/* pointer (in user-space) to code */
} dia_load_t;

/*
 * start configuration 
 * passed as parameter to DIA_IOCTL_START ioctl
 */

typedef struct
{
	int				card_id;	/* card to start */
} dia_start_t;

/* used for retrieving memory from the card */

typedef struct {
	word	card_id;
	dword 	addr;
	byte	data[16 * 8];
} mem_block_t;

/* DIVA Server specific addresses */

#define DIVAS_CPU_START_ADDR    (0x0)
#define	ORG_MAX_PROTOCOL_CODE_SIZE	0x000A0000
#define	ORG_MAX_DSP_CODE_SIZE		(0x000F0000 - ORG_MAX_PROTOCOL_CODE_SIZE)
#define	ORG_DSP_CODE_BASE		(0xBF7F0000 - ORG_MAX_DSP_CODE_SIZE)
#define DIVAS_DSP_START_ADDR    (0xBF7A0000)
#define DIVAS_SHARED_OFFSET     (0x1000)
#define MP_DSP_CODE_BASE           0xa03a0000
#define MQ_PROTCODE_OFFSET  0x100000
#define MQ_SM_OFFSET		0X0f0000

#define	V90D_MAX_PROTOCOL_CODE_SIZE	0x00090000
#define V90D_MAX_DSP_CODE_SIZE		(0x000F0000 - V90D_MAX_PROTOCOL_CODE_SIZE)
#define	V90D_DSP_CODE_BASE		(0xBF7F0000 - V90D_MAX_DSP_CODE_SIZE)

#define MQ_ORG_MAX_PROTOCOL_CODE_SIZE   0x000a0000  /* max 640K Protocol-Code */
#define MQ_ORG_MAX_DSP_CODE_SIZE        0x00050000  /* max 320K DSP-Code */
#define MQ_ORG_DSP_CODE_BASE           (MQ_MAX_DSP_DOWNLOAD_ADDR \
                                      - MQ_ORG_MAX_DSP_CODE_SIZE)
#define MQ_V90D_MAX_PROTOCOL_CODE_SIZE  0x00090000  /* max 576K Protocol-Code */
#define MQ_V90D_MAX_DSP_CODE_SIZE       0x00060000  /* max 384K DSP-Code if V.90D included */
#define	MQ_MAX_DSP_DOWNLOAD_ADDR        0xa03f0000
#define MQ_V90D_DSP_CODE_BASE          (MQ_MAX_DSP_DOWNLOAD_ADDR \
                                      - MQ_V90D_MAX_DSP_CODE_SIZE)


#define ALIGNMENT_MASK_MAESTRA        0xfffffffc

#endif /* DIVAS_H */
