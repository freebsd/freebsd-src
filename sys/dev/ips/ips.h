/*-
 * Copyright (c) 2002 Adaptec Inc.
 * All rights reserved.
 *
 * Written by: David Jeffery
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/time.h>

#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

/*
 *   IPS CONSTANTS
 */
#define IPS_VENDOR_ID                   0x1014
#define IPS_MORPHEUS_DEVICE_ID          0x01BD
#define IPS_COPPERHEAD_DEVICE_ID        0x002E
#define IPS_CSL				0xff
#define IPS_POCL			0x30

/* amounts of memory to allocate for certain commands */
#define IPS_ADAPTER_INFO_LEN		(sizeof(ips_adapter_info_t))
#define IPS_DRIVE_INFO_LEN		(sizeof(ips_drive_info_t)) 
#define IPS_COMMAND_LEN			24
#define IPS_MAX_SG_LEN			(sizeof(ips_sg_element_t) * IPS_MAX_SG_ELEMENTS)
#define IPS_NVRAM_PAGE_SIZE		128
/* various flags */
#define IPS_NOWAIT_FLAG			1

/* states for the card to be in */
#define IPS_DEV_OPEN			0x01
#define IPS_TIMEOUT			0x02 /* command time out, need reset */
#define IPS_OFFLINE			0x04 /* can't reset card/card failure */

/* max number of commands set to something low for now */
#define IPS_MAX_CMD_NUM			128	
#define IPS_MAX_NUM_DRIVES		8
#define IPS_MAX_SG_ELEMENTS		32
#define IPS_MAX_IOBUF_SIZE		(64 * 1024)
#define IPS_BLKSIZE			512

/* logical drive states */

#define IPS_LD_OFFLINE               	0x02
#define IPS_LD_OKAY                  	0x03
#define IPS_LD_DEGRADED			0x04
#define IPS_LD_FREE                  	0x00
#define IPS_LD_SYS                   	0x06
#define IPS_LD_CRS                   	0x24

/* register offsets */
#define MORPHEUS_REG_OMR0               0x0018 /* Outbound Msg. Reg. 0 */
#define MORPHEUS_REG_OMR1               0x001C /* Outbound Msg. Reg. 1 */
#define MORPHEUS_REG_IDR		0x0020 /* Inbound Doorbell Reg. */
#define MORPHEUS_REG_IISR               0x0024 /* Inbound IRQ Status Reg. */
#define MORPHEUS_REG_IIMR               0x0028 /* Inbound IRQ Mask Reg. */
#define MORPHEUS_REG_OISR               0x0030 /* Outbound IRQ Status Reg. */
#define MORPHEUS_REG_OIMR               0x0034 /* Outbound IRQ Status Reg. */
#define MORPHEUS_REG_IQPR               0x0040 /* Inbound Queue Port Reg. */
#define MORPHEUS_REG_OQPR               0x0044 /* Outbound Queue Port Reg. */

#define COPPER_REG_SCPR			0x05	/* Subsystem Ctrl. Port Reg. */
#define COPPER_REG_ISPR			0x06	/* IRQ Status Port Reg. */
#define COPPER_REG_CBSP			0x07	/* ? Reg. */
#define COPPER_REG_HISR			0x08	/* Host IRQ Status Reg.    */
#define COPPER_REG_CCSAR		0x10	/* Cmd. Channel Sys Addr Reg.*/
#define COPPER_REG_CCCR			0x14	/* Cmd. Channel Ctrl. Reg. */
#define COPPER_REG_SQHR                	0x20    /* Status Queue Head Reg.  */
#define COPPER_REG_SQTR                	0x24    /* Status Queue Tail Reg.  */
#define COPPER_REG_SQER                	0x28    /* Status Queue End Reg.   */
#define COPPER_REG_SQSR                	0x2C    /* Status Queue Start Reg. */

/* bit definitions */
#define MORPHEUS_BIT_POST1              0x01
#define MORPHEUS_BIT_POST2              0x02
#define MORPHEUS_BIT_CMD_IRQ		0x08

#define COPPER_CMD_START		0x101A
#define COPPER_SEM_BIT			0x08
#define COPPER_EI_BIT			0x80
#define COPPER_EBM_BIT			0x02
#define COPPER_RESET_BIT		0x80
#define COPPER_GHI_BIT			0x04
#define COPPER_SCE_BIT			0x01
#define COPPER_OP_BIT			0x01
#define COPPER_ILE_BIT			0x10

/* status defines */
#define IPS_POST1_OK                    0x8000
#define IPS_POST2_OK                    0x000f

/* command op codes */
#define IPS_READ_CMD			0x02
#define IPS_WRITE_CMD			0x03
#define IPS_ADAPTER_INFO_CMD		0x05
#define IPS_CACHE_FLUSH_CMD		0x0A
#define IPS_REBUILD_STATUS_CMD		0x0C
#define IPS_ERROR_TABLE_CMD		0x17
#define IPS_DRIVE_INFO_CMD		0x19
#define IPS_SUBSYS_PARAM_CMD		0x40
#define IPS_CONFIG_SYNC_CMD		0x58
#define IPS_SG_READ_CMD			0x82
#define IPS_SG_WRITE_CMD		0x83
#define IPS_RW_NVRAM_CMD		0xBC
#define IPS_FFDC_CMD			0xD7

/* error information returned by the adapter */
#define IPS_MIN_ERROR			0x02
#define IPS_ERROR_STATUS		0x13000200 /* ahh, magic numbers */

#define IPS_OS_FREEBSD			8
#define IPS_VERSION_MAJOR		"0.90"
#define IPS_VERSION_MINOR		".10"

/* Adapter Types */
#define IPS_ADAPTER_COPPERHEAD		0x01
#define IPS_ADAPTER_COPPERHEAD2		0x02
#define IPS_ADAPTER_COPPERHEADOB1	0x03
#define IPS_ADAPTER_COPPERHEADOB2	0x04
#define IPS_ADAPTER_CLARINET		0x05
#define IPS_ADAPTER_CLARINETLITE	0x06
#define IPS_ADAPTER_TROMBONE		0x07
#define IPS_ADAPTER_MORPHEUS		0x08
#define IPS_ADAPTER_MORPHEUSLITE	0x09
#define IPS_ADAPTER_NEO			0x0A
#define IPS_ADAPTER_NEOLITE		0x0B
#define IPS_ADAPTER_SARASOTA2		0x0C
#define IPS_ADAPTER_SARASOTA1		0x0D
#define IPS_ADAPTER_MARCO		0x0E
#define IPS_ADAPTER_SEBRING		0x0F
#define IPS_ADAPTER_MAX_T		IPS_ADAPTER_SEBRING

/* values for ffdc_settime (from gmtime) */
#define IPS_SECSPERMIN      60
#define IPS_MINSPERHOUR     60
#define IPS_HOURSPERDAY     24
#define IPS_DAYSPERWEEK     7
#define IPS_DAYSPERNYEAR    365
#define IPS_DAYSPERLYEAR    366
#define IPS_SECSPERHOUR     (IPS_SECSPERMIN * IPS_MINSPERHOUR)
#define IPS_SECSPERDAY      ((long) IPS_SECSPERHOUR * IPS_HOURSPERDAY)
#define IPS_MONSPERYEAR     12
#define IPS_EPOCH_YEAR      1970
#define IPS_LEAPS_THRU_END_OF(y)    ((y) / 4 - (y) / 100 + (y) / 400)
#define ips_isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

/*
 *  IPS MACROS
 */

#define ips_read_1(sc,offset)		bus_space_read_1(sc->bustag, sc->bushandle, offset)
#define ips_read_2(sc,offset) 		bus_space_read_2(sc->bustag, sc->bushandle, offset)
#define ips_read_4(sc,offset)		bus_space_read_4(sc->bustag, sc->bushandle, offset)

#define ips_write_1(sc,offset,value)	bus_space_write_1(sc->bustag, sc->bushandle, offset, value)
#define ips_write_2(sc,offset,value) 	bus_space_write_2(sc->bustag, sc->bushandle, offset, value)
#define ips_write_4(sc,offset,value)	bus_space_write_4(sc->bustag, sc->bushandle, offset, value)

/* this is ugly.  It zeros the end elements in an ips_command_t struct starting with the status element */
#define clear_ips_command(command)	bzero(&((command)->status), (unsigned long)(&(command)[1])-(unsigned long)&((command)->status))

#define ips_read_request(iobuf)		((iobuf)->bio_cmd == BIO_READ)

#define COMMAND_ERROR(status)		(((status)->fields.basic_status & 0x0f) >= IPS_MIN_ERROR)

#ifndef IPS_DEBUG
#define DEVICE_PRINTF(x...)
#define PRINTF(x...)
#else
#define DEVICE_PRINTF(level,x...)	if(IPS_DEBUG >= level)device_printf(x)
#define PRINTF(level,x...)		if(IPS_DEBUG >= level)printf(x)
#endif
/*
 *   IPS STRUCTS
 */

struct ips_softc;

typedef struct{
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	drivenum;
	u_int8_t	reserve2;
	u_int32_t	lba;
	u_int32_t	buffaddr;
	u_int32_t	reserve3;
} __attribute__ ((packed)) ips_generic_cmd;

typedef struct{
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	drivenum;
	u_int8_t	segnum;
	u_int32_t	lba;
	u_int32_t	buffaddr;
	u_int16_t	length;
	u_int16_t	reserve1;
} __attribute__ ((packed)) ips_io_cmd;

typedef struct{
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	pagenum;
	u_int8_t	rw;
	u_int32_t	reserve1;
	u_int32_t	buffaddr;
	u_int32_t	reserve3;
} __attribute__ ((packed)) ips_rw_nvram_cmd;

typedef struct{
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	drivenum;
	u_int8_t	reserve1;
	u_int32_t	reserve2;
	u_int32_t	buffaddr;
	u_int32_t	reserve3;
} __attribute__ ((packed)) ips_drive_cmd;

typedef struct{
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	reserve1;
	u_int8_t	commandtype;
	u_int32_t	reserve2;
	u_int32_t	buffaddr;
	u_int32_t	reserve3;
} __attribute__((packed)) ips_adapter_info_cmd;

typedef struct{
	u_int8_t	command;
	u_int8_t	id;
	u_int8_t	reset_count;
	u_int8_t	reset_type;
	u_int8_t	second;
	u_int8_t	minute;
	u_int8_t	hour;
	u_int8_t	day;
	u_int8_t	reserve1[4];
	u_int8_t	month;
	u_int8_t	yearH;
	u_int8_t	yearL;
	u_int8_t	reserve2;
} __attribute__((packed)) ips_adapter_ffdc_cmd;

typedef union{
	ips_generic_cmd		generic_cmd;
	ips_drive_cmd 		drive_cmd;
	ips_adapter_info_cmd 	adapter_info_cmd;
} ips_cmd_buff_t;

typedef struct {
   u_int32_t  signature;
   u_int8_t   reserved;
   u_int8_t   adapter_slot;
   u_int16_t  adapter_type;
   u_int8_t   bios_high[4];
   u_int8_t   bios_low[4];
   u_int16_t  reserve2;
   u_int8_t   reserve3;
   u_int8_t   operating_system;
   u_int8_t   driver_high[4];
   u_int8_t   driver_low[4];
   u_int8_t   reserve4[100];
}__attribute__((packed)) ips_nvram_page5;

typedef struct{
	u_int32_t	addr;
	u_int32_t	len;
} ips_sg_element_t;

typedef struct{
	u_int8_t	drivenum;
	u_int8_t	merge_id;
	u_int8_t	raid_lvl;
	u_int8_t	state;
	u_int32_t	sector_count;
} __attribute__((packed)) ips_drive_t;

typedef struct{
	u_int8_t	drivecount;
	u_int8_t	reserve1;
	u_int16_t	reserve2;
	ips_drive_t drives[IPS_MAX_NUM_DRIVES];
}__attribute__((packed)) ips_drive_info_t;

typedef struct{
	u_int8_t	drivecount;
	u_int8_t	miscflags;
	u_int8_t	SLTflags;
	u_int8_t	BSTflags;
	u_int8_t	pwr_chg_count;
	u_int8_t	wrong_addr_count;
	u_int8_t	unident_count;
	u_int8_t	nvram_dev_chg_count;
	u_int8_t	codeblock_version[8];
	u_int8_t	bootblock_version[8];
	u_int32_t	drive_sector_count[IPS_MAX_NUM_DRIVES];
	u_int8_t	max_concurrent_cmds;
	u_int8_t	max_phys_devices;
	u_int16_t	flash_prog_count;
	u_int8_t	defunct_disks;
	u_int8_t	rebuildflags;
	u_int8_t	offline_drivecount;
	u_int8_t	critical_drivecount;
	u_int16_t	config_update_count;
	u_int8_t	blockedflags;
	u_int8_t	psdn_error;
	u_int16_t	addr_dead_disk[4*16];/* ugly, max # channels * max # scsi devices per channel */
}__attribute__((packed)) ips_adapter_info_t;

typedef struct {
	u_int32_t 	status[IPS_MAX_CMD_NUM];
	u_int32_t 	base_phys_addr;
	int 		nextstatus;
	bus_dma_tag_t	dmatag;
	bus_dmamap_t	dmamap;
} ips_copper_queue_t;

typedef union {
   struct {
      u_int8_t  reserved;
      u_int8_t  command_id;
      u_int8_t  basic_status;
      u_int8_t  extended_status;
   } fields;
   volatile u_int32_t    value;
} ips_cmd_status_t;

/* used to keep track of current commands to the card */
typedef struct ips_command{
	u_int8_t		command_number;
	u_int8_t 		id;
	u_int8_t		timeout;
	struct ips_softc *	sc;
	bus_dmamap_t		command_dmamap;
	void *			command_buffer;
	u_int32_t		command_phys_addr;/*WARNING! must be changed if 64bit addressing ever used*/	
	ips_cmd_status_t	status;
	SLIST_ENTRY(ips_command)	next;
	bus_dma_tag_t		data_dmatag;
	bus_dmamap_t		data_dmamap;
	void *			data_buffer;
	void * 			arg;
	void			(* callback)(struct ips_command *command);
}ips_command_t;

typedef struct ips_wait_list{
	STAILQ_ENTRY(ips_wait_list) next;
	void 			*data;
	int			(* callback)(ips_command_t *command);
}ips_wait_list_t;

typedef struct ips_softc{
        struct resource *       iores;
        struct resource *       irqres;
        struct intr_config_hook ips_ich;
        int                     configured;
        int                     state;
        int                     iotype;
        int                     rid;
        int                     irqrid;
        void *                  irqcookie;
        bus_space_tag_t	        bustag;
	bus_space_handle_t      bushandle;
	bus_dma_tag_t	        adapter_dmatag;
	bus_dma_tag_t		command_dmatag;
	bus_dma_tag_t		sg_dmatag;
        device_t                dev;
        dev_t                   device_file;
	struct callout_handle	timer;
	u_int16_t		adapter_type;
	ips_adapter_info_t	adapter_info;
	device_t		diskdev[IPS_MAX_NUM_DRIVES];
	ips_drive_t		drives[IPS_MAX_NUM_DRIVES];
	u_int8_t		drivecount;
	u_int16_t		ffdc_resetcount;
	struct timeval		ffdc_resettime;
	u_int8_t		next_drive;
	u_int8_t		max_cmds;
	volatile u_int8_t	used_commands;
	ips_command_t		commandarray[IPS_MAX_CMD_NUM];
	SLIST_HEAD(command_list, ips_command) free_cmd_list;
	STAILQ_HEAD(command_wait_list,ips_wait_list)  cmd_wait_list;
	int			(* ips_adapter_reinit)(struct ips_softc *sc, 
						       int force);
        void                    (* ips_adapter_intr)(void *sc);
	void			(* ips_issue_cmd)(ips_command_t *command);
	ips_copper_queue_t *	copper_queue;
	struct mtx		cmd_mtx;
}ips_softc_t;

/* function defines from ips_ioctl.c */
extern int ips_ioctl_request(ips_softc_t *sc, u_long ioctl_cmd, caddr_t addr, 
				int32_t flags);
/* function defines from ips_disk.c */
extern void ipsd_finish(struct bio *iobuf);

/* function defines from ips_commands.c */
extern int ips_flush_cache(ips_softc_t *sc);
extern void ips_start_io_request(ips_softc_t *sc, struct bio *iobuf);
extern int ips_get_drive_info(ips_softc_t *sc);
extern int ips_get_adapter_info(ips_softc_t *sc);
extern int ips_ffdc_reset(ips_softc_t *sc);
extern int ips_update_nvram(ips_softc_t *sc); 
extern int ips_clear_adapter(ips_softc_t *sc);

/* function defines from ips.c */
extern int ips_get_free_cmd(ips_softc_t *sc, int (*callback)(ips_command_t *), 
				void *data, unsigned long flags);
extern void ips_insert_free_cmd(ips_softc_t *sc, ips_command_t *command);
extern int ips_adapter_init(ips_softc_t *sc);
extern int ips_morpheus_reinit(ips_softc_t *sc, int force);
extern int ips_adapter_free(ips_softc_t *sc);
extern void ips_morpheus_intr(void *sc);
extern void ips_issue_morpheus_cmd(ips_command_t *command);
extern int ips_copperhead_reinit(ips_softc_t *sc, int force);
extern void ips_copperhead_intr(void *sc);
extern void ips_issue_copperhead_cmd(ips_command_t *command);

