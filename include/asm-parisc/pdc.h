#ifndef _PARISC_PDC_H
#define _PARISC_PDC_H

/*
 *	PDC return values ...
 *	All PDC calls return a subset of these errors. 
 */

#define PDC_WARN		  3	/* Call completed with a warning */
#define PDC_REQ_ERR_1		  2	/* See above			 */
#define PDC_REQ_ERR_0		  1	/* Call would generate a requestor error */
#define PDC_OK			  0	/* Call completed successfully	*/
#define PDC_BAD_PROC		 -1	/* Called non-existent procedure*/
#define PDC_BAD_OPTION		 -2	/* Called with non-existent option */
#define PDC_ERROR		 -3	/* Call could not complete without an error */
#define PDC_NE_MOD		 -5	/* Module not found		*/
#define PDC_NE_CELL_MOD		 -7	/* Cell module not found	*/
#define PDC_INVALID_ARG		-10	/* Called with an invalid argument */
#define PDC_BUS_POW_WARN	-12	/* Call could not complete in allowed power budget */
#define PDC_NOT_NARROW		-17	/* Narrow mode not supported	*/


/*
 *	PDC entry points...
 */

#define PDC_POW_FAIL	1		/* perform a power-fail		*/
#define PDC_POW_FAIL_PREPARE	0	/* prepare for powerfail	*/

#define PDC_CHASSIS	2		/* PDC-chassis functions	*/
#define PDC_CHASSIS_DISP	0	/* update chassis display	*/
#define PDC_CHASSIS_WARN	1	/* return chassis warnings	*/
#define PDC_CHASSIS_DISPWARN	2	/* update&return chassis status */
#define PDC_RETURN_CHASSIS_INFO 128	/* HVERSION dependent: return chassis LED/LCD info  */

#define PDC_PIM         3               /* Get PIM data                 */
#define PDC_PIM_HPMC            0       /* Transfer HPMC data           */
#define PDC_PIM_RETURN_SIZE     1       /* Get Max buffer needed for PIM*/
#define PDC_PIM_LPMC            2       /* Transfer HPMC data           */
#define PDC_PIM_SOFT_BOOT       3       /* Transfer Soft Boot data      */
#define PDC_PIM_TOC             4       /* Transfer TOC data            */

#define PDC_MODEL	4		/* PDC model information call	*/
#define PDC_MODEL_INFO		0	/* returns information 		*/
#define PDC_MODEL_BOOTID	1	/* set the BOOT_ID		*/
#define PDC_MODEL_VERSIONS	2	/* returns cpu-internal versions*/
#define PDC_MODEL_SYSMODEL	3	/* return system model info	*/
#define PDC_MODEL_ENSPEC	4	/* enable specific option	*/
#define PDC_MODEL_DISPEC	5	/* disable specific option	*/
#define PDC_MODEL_CPU_ID	6	/* returns cpu-id (only newer machines!) */
#define PDC_MODEL_CAPABILITIES	7	/* returns OS32/OS64-flags	*/
#define PDC_MODEL_GET_BOOT__OP	8	/* returns boot test options	*/
#define PDC_MODEL_SET_BOOT__OP	9	/* set boot test options	*/

#define PA89_INSTRUCTION_SET	0x4	/* capatibilies returned	*/
#define PA90_INSTRUCTION_SET	0x8

#define PDC_CACHE	5		/* return/set cache (& TLB) info*/
#define PDC_CACHE_INFO		0	/* returns information 		*/
#define PDC_CACHE_SET_COH	1	/* set coherence state		*/
#define PDC_CACHE_RET_SPID	2	/* returns space-ID bits	*/

#define PDC_HPA		6		/* return HPA of processor	*/
#define PDC_HPA_PROCESSOR	0
#define PDC_HPA_MODULES		1

#define PDC_COPROC	7		/* Co-Processor (usually FP unit(s)) */
#define PDC_COPROC_CFG		0	/* Co-Processor Cfg (FP unit(s) enabled?) */

#define PDC_IODC	8		/* talk to IODC			*/
#define PDC_IODC_READ		0	/* read IODC entry point	*/
/*      PDC_IODC_RI_			 * INDEX parameter of PDC_IODC_READ */
#define PDC_IODC_RI_DATA_BYTES	0	/* IODC Data Bytes		*/
/*				1, 2	   obsolete - HVERSION dependent*/
#define PDC_IODC_RI_INIT	3	/* Initialize module		*/
#define PDC_IODC_RI_IO		4	/* Module input/output		*/
#define PDC_IODC_RI_SPA		5	/* Module input/output		*/
#define PDC_IODC_RI_CONFIG	6	/* Module input/output		*/
/*				7	  obsolete - HVERSION dependent */
#define PDC_IODC_RI_TEST	8	/* Module input/output		*/
#define PDC_IODC_RI_TLB		9	/* Module input/output		*/
#define PDC_IODC_NINIT		2	/* non-destructive init		*/
#define PDC_IODC_DINIT		3	/* destructive init		*/
#define PDC_IODC_MEMERR		4	/* check for memory errors	*/
#define PDC_IODC_INDEX_DATA	0	/* get first 16 bytes from mod IODC */
#define PDC_IODC_BUS_ERROR	-4	/* bus error return value	*/
#define PDC_IODC_INVALID_INDEX	-5	/* invalid index return value	*/
#define PDC_IODC_COUNT		-6	/* count is too small		*/

#define PDC_TOD		9		/* time-of-day clock (TOD)	*/
#define PDC_TOD_READ		0	/* read TOD			*/
#define PDC_TOD_WRITE		1	/* write TOD			*/
#define PDC_TOD_ITIMER		2	/* calibrate Interval Timer (CR16) */

#define PDC_STABLE	10		/* stable storage (sprockets)	*/
#define PDC_STABLE_READ		0
#define PDC_STABLE_WRITE	1
#define PDC_STABLE_RETURN_SIZE	2
#define PDC_STABLE_VERIFY_CONTENTS 3
#define PDC_STABLE_INITIALIZE	4

#define PDC_NVOLATILE	11		/* often not implemented	*/

#define PDC_ADD_VALID	12		/* Memory validation PDC call	*/
#define PDC_ADD_VALID_VERIFY	0	/* Make PDC_ADD_VALID verify region */

#define PDC_INSTR	15		/* get instr to invoke PDCE_CHECK() */

#define PDC_PROC	16		/* (sprockets)			*/

#define PDC_CONFIG	16		/* (sprockets)			*/
#define PDC_CONFIG_DECONFIG	0
#define PDC_CONFIG_DRECONFIG	1
#define PDC_CONFIG_DRETURN_CONFIG 2

#define PDC_BLOCK_TLB	18		/* manage hardware block-TLB	*/
#define PDC_BTLB_INFO		0	/* returns parameter 		*/
#define PDC_BTLB_INSERT		1	/* insert BTLB entry		*/
#define PDC_BTLB_PURGE		2	/* purge BTLB entries 		*/
#define PDC_BTLB_PURGE_ALL	3	/* purge all BTLB entries 	*/

#define PDC_TLB		19		/* manage hardware TLB miss handling */
#define PDC_TLB_INFO		0	/* returns parameter 		*/
#define PDC_TLB_SETUP		1	/* set up miss handling 	*/

#define PDC_MEM		20		/* Manage memory		*/
#define PDC_MEM_MEMINFO		0
#define PDC_MEM_ADD_PAGE	1
#define PDC_MEM_CLEAR_PDT	2
#define PDC_MEM_READ_PDT	3
#define PDC_MEM_RESET_CLEAR	4
#define PDC_MEM_GOODMEM		5
#define PDC_MEM_TABLE		128	/* Non contig mem map (sprockets) */
#define PDC_MEM_RETURN_ADDRESS_TABLE	PDC_MEM_TABLE
#define PDC_MEM_GET_MEMORY_SYSTEM_TABLES_SIZE	131
#define PDC_MEM_GET_MEMORY_SYSTEM_TABLES	132
#define PDC_MEM_GET_PHYSICAL_LOCATION_FROM_MEMORY_ADDRESS 133

#define PDC_MEM_RET_SBE_REPLACED	5	/* PDC_MEM return values */
#define PDC_MEM_RET_DUPLICATE_ENTRY	4
#define PDC_MEM_RET_BUF_SIZE_SMALL	1
#define PDC_MEM_RET_PDT_FULL		-11
#define PDC_MEM_RET_INVALID_PHYSICAL_LOCATION ~0ULL

#ifndef __ASSEMBLY__
typedef struct {
    unsigned long long	baseAddr;
    unsigned int	pages;
    unsigned int	reserved;
} MemAddrTable_t;
#endif


#define PDC_PSW		21		/* Get/Set default System Mask  */
#define PDC_PSW_MASK		0	/* Return mask                  */
#define PDC_PSW_GET_DEFAULTS	1	/* Return defaults              */
#define PDC_PSW_SET_DEFAULTS	2	/* Set default                  */
#define PDC_PSW_ENDIAN_BIT	1	/* set for big endian           */
#define PDC_PSW_WIDE_BIT	2	/* set for wide mode            */ 

#define PDC_SYSTEM_MAP	22		/* find system modules		*/
#define PDC_FIND_MODULE 	0
#define PDC_FIND_ADDRESS	1
#define PDC_TRANSLATE_PATH	2

#define PDC_SOFT_POWER	23		/* soft power switch		*/
#define PDC_SOFT_POWER_INFO	0	/* return info about the soft power switch */
#define PDC_SOFT_POWER_ENABLE	1	/* enable/disable soft power switch */


/* HVERSION dependent */

/* The PDC_MEM_MAP calls */
#define PDC_MEM_MAP	128		/* on s700: return page info	*/
#define PDC_MEM_MAP_HPA		0	/* returns hpa of a module	*/

#define PDC_EEPROM	129		/* EEPROM access		*/
#define PDC_EEPROM_READ_WORD	0
#define PDC_EEPROM_WRITE_WORD	1
#define PDC_EEPROM_READ_BYTE	2
#define PDC_EEPROM_WRITE_BYTE	3
#define PDC_EEPROM_EEPROM_PASSWORD -1000

#define PDC_NVM		130		/* NVM (non-volatile memory) access */
#define PDC_NVM_READ_WORD	0
#define PDC_NVM_WRITE_WORD	1
#define PDC_NVM_READ_BYTE	2
#define PDC_NVM_WRITE_BYTE	3

#define PDC_SEED_ERROR	132		/* (sprockets)			*/

#define PDC_IO		135		/* log error info, reset IO system */
#define PDC_IO_READ_AND_CLEAR_ERRORS	0
#define PDC_IO_READ_AND_LOG_ERRORS	1
#define PDC_IO_SUSPEND_USB		2
/* sets bits 6&7 (little endian) of the HcControl Register */
#define PDC_IO_USB_SUSPEND	0xC000000000000000
#define PDC_IO_EEPROM_IO_ERR_TABLE_FULL	-5	/* return value */
#define PDC_IO_NO_SUSPEND		-6	/* return value */

#define PDC_BROADCAST_RESET 136		/* reset all processors		*/
#define PDC_DO_RESET		0	/* option: perform a broadcast reset */
#define PDC_DO_FIRM_TEST_RESET	1	/* Do broadcast reset with bitmap */
#define PDC_BR_RECONFIGURATION	2	/* reset w/reconfiguration	*/
#define PDC_FIRM_TEST_MAGIC	0xab9ec36fUL    /* for this reboot only	*/

#define PDC_LAN_STATION_ID 138		/* Hversion dependent mechanism for */
#define PDC_LAN_STATION_ID_READ	0	/* getting the lan station address  */

#define	PDC_LAN_STATION_ID_SIZE	6

#define PDC_CHECK_RANGES 139		/* (sprockets)			*/

#define PDC_NV_SECTIONS	141		/* (sprockets)			*/

#define PDC_PERFORMANCE	142		/* performance monitoring	*/

#define PDC_SYSTEM_INFO	143		/* system information		*/
#define PDC_SYSINFO_RETURN_INFO_SIZE	0
#define PDC_SYSINFO_RRETURN_SYS_INFO	1
#define PDC_SYSINFO_RRETURN_ERRORS	2
#define PDC_SYSINFO_RRETURN_WARNINGS	3
#define PDC_SYSINFO_RETURN_REVISIONS	4
#define PDC_SYSINFO_RRETURN_DIAGNOSE	5
#define PDC_SYSINFO_RRETURN_HV_DIAGNOSE	1005

#define PDC_RDR		144		/* (sprockets)			*/
#define PDC_RDR_READ_BUFFER	0
#define PDC_RDR_READ_SINGLE	1
#define PDC_RDR_WRITE_SINGLE	2

#define PDC_INTRIGUE	145 		/* (sprockets)			*/
#define PDC_INTRIGUE_WRITE_BUFFER 	 0
#define PDC_INTRIGUE_GET_SCRATCH_BUFSIZE 1
#define PDC_INTRIGUE_START_CPU_COUNTERS	 2
#define PDC_INTRIGUE_STOP_CPU_COUNTERS	 3

#define PDC_STI		146 		/* STI access			*/
/* same as PDC_PCI_XXX values (see below) */

/* Legacy PDC definitions for same stuff */
#define PDC_PCI_INDEX	147
#define PDC_PCI_INTERFACE_INFO		0
#define PDC_PCI_SLOT_INFO		1
#define PDC_PCI_INFLIGHT_BYTES		2
#define PDC_PCI_READ_CONFIG		3
#define PDC_PCI_WRITE_CONFIG		4
#define PDC_PCI_READ_PCI_IO		5
#define PDC_PCI_WRITE_PCI_IO		6
#define PDC_PCI_READ_CONFIG_DELAY	7
#define PDC_PCI_UPDATE_CONFIG_DELAY	8
#define PDC_PCI_PCI_PATH_TO_PCI_HPA	9
#define PDC_PCI_PCI_HPA_TO_PCI_PATH	10
#define PDC_PCI_PCI_PATH_TO_PCI_BUS	11
#define PDC_PCI_PCI_RESERVED		12
#define PDC_PCI_PCI_INT_ROUTE_SIZE	13
#define PDC_PCI_GET_INT_TBL_SIZE	PDC_PCI_PCI_INT_ROUTE_SIZE
#define PDC_PCI_PCI_INT_ROUTE		14
#define PDC_PCI_GET_INT_TBL		PDC_PCI_PCI_INT_ROUTE 
#define PDC_PCI_READ_MON_TYPE		15
#define PDC_PCI_WRITE_MON_TYPE		16


/* Get SCSI Interface Card info:  SDTR, SCSI ID, mode (SE vs LVD) */
#define PDC_INITIATOR	163
#define PDC_GET_INITIATOR	0
#define PDC_SET_INITIATOR	1
#define PDC_DELETE_INITIATOR	2
#define PDC_RETURN_TABLE_SIZE	3
#define PDC_RETURN_TABLE	4

#define PDC_LINK	165 		/* (sprockets)			*/
#define PDC_LINK_PCI_ENTRY_POINTS	0  /* list (Arg1) = 0 */
#define PDC_LINK_USB_ENTRY_POINTS	1  /* list (Arg1) = 1 */


/* constants for OS (NVM...) */
#define OS_ID_NONE		0	/* Undefined OS ID	*/
#define OS_ID_HPUX		1	/* HP-UX OS		*/
#define OS_ID_LINUX		OS_ID_HPUX /* just use the same value as hpux */
#define OS_ID_MPEXL		2	/* MPE XL OS		*/
#define OS_ID_OSF		3	/* OSF OS		*/
#define OS_ID_HPRT		4	/* HP-RT OS		*/
#define OS_ID_NOVEL		5	/* NOVELL OS		*/
#define OS_ID_NT		6	/* NT OS		*/


/* constants for PDC_CHASSIS */
#define OSTAT_OFF		0
#define OSTAT_FLT		1 
#define OSTAT_TEST		2
#define OSTAT_INIT		3
#define OSTAT_SHUT		4
#define OSTAT_WARN		5
#define OSTAT_RUN		6
#define OSTAT_ON		7

#ifdef __LP64__
/* PDC PAT CELL */
#define PDC_PAT_CELL	64L		/* Interface for gaining and 
					 * manipulating cell state within PD */
#define PDC_PAT_CELL_GET_NUMBER	   0L	/* Return Cell number		*/
#define PDC_PAT_CELL_GET_INFO      1L	/* Returns info about Cell	*/
#define PDC_PAT_CELL_MODULE        2L	/* Returns info about Module	*/
#define PDC_PAT_CELL_SET_ATTENTION 9L	/* Set Cell Attention indicator	*/
#define PDC_PAT_CELL_NUMBER_TO_LOC 10L	/* Cell Number -> Location	*/
#define PDC_PAT_CELL_WALK_FABRIC   11L	/* Walk the Fabric		*/
#define PDC_PAT_CELL_GET_RDT_SIZE  12L	/* Return Route Distance Table Sizes */
#define PDC_PAT_CELL_GET_RDT       13L	/* Return Route Distance Tables	*/
#define PDC_PAT_CELL_GET_LOCAL_PDH_SZ  14L /* Read Local PDH Buffer Size*/
#define PDC_PAT_CELL_SET_LOCAL_PDH     15L /* Write Local PDH Buffer	*/
#define PDC_PAT_CELL_GET_REMOTE_PDH_SZ 16L /* Return Remote PDH Buffer Size */
#define PDC_PAT_CELL_GET_REMOTE_PDH    17L /* Read Remote PDH Buffer	*/
#define PDC_PAT_CELL_GET_DBG_INFO  128L	/* Return DBG Buffer Info	*/
#define PDC_PAT_CELL_CHANGE_ALIAS  129L	/* Change Non-Equivalent Alias Checking */

/*
** Arg to PDC_PAT_CELL_MODULE memaddr[4]
**
** Addresses on the Merced Bus != all Runway Bus addresses.
** This is intended for programming SBA/LBA chips range registers.
*/
#define IO_VIEW			0UL
#define PA_VIEW			1UL

/* PDC_PAT_CELL_MODULE entity type values */
#define PAT_ENTITY_CA		0	/* central agent	*/
#define PAT_ENTITY_PROC		1	/* processor		*/
#define PAT_ENTITY_MEM		2	/* memory controller	*/
#define PAT_ENTITY_SBA		3	/* system bus adapter	*/
#define PAT_ENTITY_LBA		4	/* local bus adapter	*/
#define PAT_ENTITY_PBC		5	/* processor bus converter */
#define PAT_ENTITY_XBC		6	/* crossbar fabric connect */
#define PAT_ENTITY_RC		7	/* fabric interconnect	*/

/* PDC_PAT_CELL_MODULE address range type values */
#define PAT_PBNUM		0	/* PCI Bus Number	*/
#define PAT_LMMIO		1	/* < 4G MMIO Space	*/
#define PAT_GMMIO		2	/* > 4G MMIO Space	*/
#define PAT_NPIOP		3	/* Non Postable I/O Port Space */
#define PAT_PIOP		4	/* Postable I/O Port Space */
#define PAT_AHPA		5	/* Additional HPA Space	*/
#define PAT_UFO			6	/* HPA Space (UFO for Mariposa) */
#define PAT_GNIP		7	/* GNI Reserved Space	*/


/* PDC PAT CHASSIS LOG */
#define PDC_PAT_CHASSIS_LOG	65L	/* Platform logging & forward
					 ** progress functions	*/
#define PDC_PAT_CHASSIS_WRITE_LOG	0L /* Write Log Entry	*/
#define PDC_PAT_CHASSIS_READ_LOG	1L /* Read  Log Entry	*/


/* PDC PAT CPU  */
#define PDC_PAT_CPU		67L	/* Interface to CPU configuration
					 * within the protection domain */
#define PDC_PAT_CPU_INFO		0L /* Return CPU config info	*/
#define PDC_PAT_CPU_DELETE		1L /* Delete CPU		*/
#define PDC_PAT_CPU_ADD			2L /* Add    CPU		*/
#define PDC_PAT_CPU_GET_NUMBER		3L /* Return CPU Number		*/
#define PDC_PAT_CPU_GET_HPA		4L /* Return CPU HPA		*/
#define PDC_PAT_CPU_STOP            	5L /* Stop   CPU		*/
#define PDC_PAT_CPU_RENDEZVOUS      	6L /* Rendezvous CPU		*/
#define PDC_PAT_CPU_GET_CLOCK_INFO  	7L /* Return CPU Clock info	*/
#define PDC_PAT_CPU_GET_RENDEZVOUS_STATE 8L /* Return Rendezvous State	*/
#define PDC_PAT_CPU_PLUNGE_FABRIC	128L /* Plunge Fabric		*/
#define PDC_PAT_CPU_UPDATE_CACHE_CLEANSING 129L /* Manipulate Cache 
                                                 * Cleansing Mode	*/

/*  PDC PAT EVENT */
#define PDC_PAT_EVENT		68L	/* Interface to Platform Events */
#define PDC_PAT_EVENT_GET_CAPS		0L /* Get Capabilities		*/
#define PDC_PAT_EVENT_SET_MODE		1L /* Set Notification Mode	*/
#define PDC_PAT_EVENT_SCAN		2L /* Scan Event		*/
#define PDC_PAT_EVENT_HANDLE		3L /* Handle Event		*/
#define PDC_PAT_EVENT_GET_NB_CALL	4L /* Get Non-Blocking call Args*/

/*  PDC PAT HPMC */
#define PDC_PAT_HPMC		70L	/* Cause processor to go into spin
					 ** loop, and wait for wake up from
					 ** Monarch Processor		*/
#define PDC_PAT_HPMC_RENDEZ_CPU		0L /* go into spin loop		*/
#define PDC_PAT_HPMC_SET_PARAMS		1L /* Allows OS to specify intr which PDC 
                                        * will use to interrupt OS during machine
                                        * check rendezvous		*/

/* parameters for PDC_PAT_HPMC_SET_PARAMS */
#define HPMC_SET_PARAMS_INTR		1L /* Rendezvous Interrupt	*/
#define HPMC_SET_PARAMS_WAKE		2L /* Wake up processor		*/

/*  PDC PAT IO */
#define PDC_PAT_IO		71L	/* On-line services for I/O modules */
#define PDC_PAT_IO_GET_SLOT_STATUS	 5L /* Get Slot Status Info	*/
#define PDC_PAT_IO_GET_LOC_FROM_HARDWARE 6L /* Get Physical Location from */
                                            /* Hardware Path		*/
#define PDC_PAT_IO_GET_HARDWARE_FROM_LOC 7L /* Get Hardware Path from 
                                             * Physical Location	*/
#define PDC_PAT_IO_GET_PCI_CONFIG_FROM_HW 11L /* Get PCI Configuration
                                               * Address from Hardware Path */
#define PDC_PAT_IO_GET_HW_FROM_PCI_CONFIG 12L /* Get Hardware Path 
                                               * from PCI Configuration Address */
#define PDC_PAT_IO_READ_HOST_BRIDGE_INFO  13L /* Read Host Bridge State Info */
#define PDC_PAT_IO_CLEAR_HOST_BRIDGE_INFO 14L /* Clear Host Bridge State Info*/
#define PDC_PAT_IO_GET_PCI_ROUTING_TABLE_SIZE 15L /* Get PCI INT Routing Table 
                                                   * Size		*/
#define PDC_PAT_IO_GET_PCI_ROUTING_TABLE  16L /* Get PCI INT Routing Table */
#define PDC_PAT_IO_GET_HINT_TABLE_SIZE    17L /* Get Hint Table Size	*/
#define PDC_PAT_IO_GET_HINT_TABLE	18L /* Get Hint Table		*/
#define PDC_PAT_IO_PCI_CONFIG_READ	19L /* PCI Config Read		*/
#define PDC_PAT_IO_PCI_CONFIG_WRITE	20L /* PCI Config Write		*/
#define PDC_PAT_IO_GET_NUM_IO_SLOTS	21L /* Get Number of I/O Bay Slots in 
                                       		  * Cabinet		*/
#define PDC_PAT_IO_GET_LOC_IO_SLOTS	22L /* Get Physical Location of I/O */
                                   	    /* Bay Slots in Cabinet	*/
#define PDC_PAT_IO_BAY_STATUS_INFO	28L /* Get I/O Bay Slot Status Info */
#define PDC_PAT_IO_GET_PROC_VIEW	29L /* Get Processor view of IO address */
#define PDC_PAT_IO_PROG_SBA_DIR_RANGE	30L /* Program directed range	*/

/* PDC PAT MEM */
#define PDC_PAT_MEM		72L  /* Manage memory page deallocation */
#define PDC_PAT_MEM_PD_INFO     	0L /* Return PDT info for PD	*/
#define PDC_PAT_MEM_PD_CLEAR    	1L /* Clear PDT for PD		*/
#define PDC_PAT_MEM_PD_READ     	2L /* Read PDT entries for PD	*/
#define PDC_PAT_MEM_PD_RESET    	3L /* Reset clear bit for PD	*/
#define PDC_PAT_MEM_CELL_INFO   	5L /* Return PDT info For Cell	*/
#define PDC_PAT_MEM_CELL_CLEAR  	6L /* Clear PDT For Cell	*/
#define PDC_PAT_MEM_CELL_READ   	7L /* Read PDT entries For Cell	*/
#define PDC_PAT_MEM_CELL_RESET  	8L /* Reset clear bit For Cell	*/
#define PDC_PAT_MEM_SETGM	  	9L /* Set Golden Memory value	*/
#define PDC_PAT_MEM_ADD_PAGE    	10L /* ADDs a page to the cell	*/
#define PDC_PAT_MEM_ADDRESS     	11L /* Get Physical Location From*/
					    /* Memory Address		*/
#define PDC_PAT_MEM_GET_TXT_SIZE   	12L /* Get Formatted Text Size	*/
#define PDC_PAT_MEM_GET_PD_TXT     	13L /* Get PD Formatted Text	*/
#define PDC_PAT_MEM_GET_CELL_TXT   	14L /* Get Cell Formatted Text	*/
#define PDC_PAT_MEM_RD_STATE_INFO  	15L /* Read Mem Module State Info*/
#define PDC_PAT_MEM_CLR_STATE_INFO 	16L /*Clear Mem Module State Info*/
#define PDC_PAT_MEM_CLEAN_RANGE    	128L /*Clean Mem in specific range*/
#define PDC_PAT_MEM_GET_TBL_SIZE   	131L /* Get Memory Table Size	*/
#define PDC_PAT_MEM_GET_TBL        	132L /* Get Memory Table	*/

/* PDC PAT NVOLATILE */
#define PDC_PAT_NVOLATILE	73L	   /* Access Non-Volatile Memory*/
#define PDC_PAT_NVOLATILE_READ		0L /* Read Non-Volatile Memory	*/
#define PDC_PAT_NVOLATILE_WRITE		1L /* Write Non-Volatile Memory	*/
#define PDC_PAT_NVOLATILE_GET_SIZE	2L /* Return size of NVM	*/
#define PDC_PAT_NVOLATILE_VERIFY	3L /* Verify contents of NVM	*/
#define PDC_PAT_NVOLATILE_INIT		4L /* Initialize NVM		*/

/* PDC PAT PD */
#define PDC_PAT_PD		74L	    /* Protection Domain Info	*/
#define PDC_PAT_PD_GET_ADDR_MAP		0L  /* Get Address Map		*/

/* PDC_PAT_PD_GET_ADDR_MAP entry types */
#define PAT_MEMORY_DESCRIPTOR		1

/* PDC_PAT_PD_GET_ADDR_MAP memory types */
#define PAT_MEMTYPE_MEMORY		0
#define PAT_MEMTYPE_FIRMWARE		4

/* PDC_PAT_PD_GET_ADDR_MAP memory usage */
#define PAT_MEMUSE_GENERAL		0
#define PAT_MEMUSE_GI			128
#define PAT_MEMUSE_GNI			129
#endif /* __LP64__ */

#ifndef __ASSEMBLY__

#include <linux/types.h>

extern int pdc_type;

/* Values for pdc_type */
#define PDC_TYPE_ILLEGAL	-1
#define PDC_TYPE_PAT		 0 /* 64-bit PAT-PDC */
#define PDC_TYPE_SYSTEM_MAP	 1 /* 32-bit, but supports PDC_SYSTEM_MAP */
#define PDC_TYPE_SNAKE		 2 /* Doesn't support SYSTEM_MAP */

#define is_pdc_pat()	(pdc_type == PDC_TYPE_PAT)

struct pdc_chassis_info {       /* for PDC_CHASSIS_INFO */
	unsigned long actcnt;   /* actual number of bytes returned */
	unsigned long maxcnt;   /* maximum number of bytes that could be returned */
};

struct pdc_coproc_cfg {         /* for PDC_COPROC_CFG */
        unsigned long ccr_functional;
        unsigned long ccr_present;
        unsigned long revision;
        unsigned long model;
};

struct pdc_model {		/* for PDC_MODEL */
	unsigned long hversion;
	unsigned long sversion;
	unsigned long hw_id;
	unsigned long boot_id;
	unsigned long sw_id;
	unsigned long sw_cap;
	unsigned long arch_rev;
	unsigned long pot_key;
	unsigned long curr_key;
};

/* Values for PDC_MODEL_CAPABILITES non-equivalent virtual aliasing support */

#define PDC_MODEL_IOPDIR_FDC            (1 << 2)        /* see sba_iommu.c */
#define PDC_MODEL_NVA_MASK		(3 << 4)
#define PDC_MODEL_NVA_SUPPORTED		(0 << 4)
#define PDC_MODEL_NVA_SLOW		(1 << 4)
#define PDC_MODEL_NVA_UNSUPPORTED	(3 << 4)

struct pdc_cache_cf {		/* for PDC_CACHE  (I/D-caches) */
    unsigned long
#ifdef __LP64__
		cc_padW:32,
#endif
		cc_alias:4,	/* alias boundaries for virtual addresses   */
		cc_block: 4,	/* to determine most efficient stride */
		cc_line	: 3,	/* maximum amount written back as a result of store (multiple of 16 bytes) */
		cc_pad0 : 2,	/* reserved */
		cc_wt	: 1,	/* 0 = WT-Dcache, 1 = WB-Dcache */
		cc_sh	: 2,	/* 0 = separate I/D-cache, else shared I/D-cache */
		cc_cst  : 3,	/* 0 = incoherent D-cache, 1=coherent D-cache */
		cc_pad1 : 5,	/* reserved */
		cc_assoc: 8;	/* associativity of I/D-cache */
};

struct pdc_tlb_cf {		/* for PDC_CACHE (I/D-TLB's) */
    unsigned long tc_pad0:12,	/* reserved */
#ifdef __LP64__
		tc_padW:32,
#endif
		tc_sh	: 2,	/* 0 = separate I/D-TLB, else shared I/D-TLB */
		tc_hv   : 1,	/* HV */
		tc_page : 1,	/* 0 = 2K page-size-machine, 1 = 4k page size */
		tc_cst  : 3,	/* 0 = incoherent operations, else coherent operations */
		tc_aid  : 5,	/* ITLB: width of access ids of processor (encoded!) */
		tc_pad1 : 8;	/* ITLB: width of space-registers (encoded) */
};

struct pdc_cache_info {		/* main-PDC_CACHE-structure (caches & TLB's) */
	/* I-cache */
	unsigned long	ic_size;	/* size in bytes */
	struct pdc_cache_cf ic_conf;	/* configuration */
	unsigned long	ic_base;	/* base-addr */
	unsigned long	ic_stride;
	unsigned long	ic_count;
	unsigned long	ic_loop;
	/* D-cache */
	unsigned long	dc_size;	/* size in bytes */
	struct pdc_cache_cf dc_conf;	/* configuration */
	unsigned long	dc_base;	/* base-addr */
	unsigned long	dc_stride;
	unsigned long	dc_count;
	unsigned long	dc_loop;
	/* Instruction-TLB */
	unsigned long	it_size;	/* number of entries in I-TLB */
	struct pdc_tlb_cf it_conf;	/* I-TLB-configuration */
	unsigned long	it_sp_base;
	unsigned long	it_sp_stride;
	unsigned long	it_sp_count;
	unsigned long	it_off_base;
	unsigned long	it_off_stride;
	unsigned long	it_off_count;
	unsigned long	it_loop;
	/* data-TLB */
	unsigned long	dt_size;	/* number of entries in D-TLB */
	struct pdc_tlb_cf dt_conf;	/* D-TLB-configuration */
	unsigned long	dt_sp_base;
	unsigned long	dt_sp_stride;
	unsigned long	dt_sp_count;
	unsigned long	dt_off_base;
	unsigned long	dt_off_stride;
	unsigned long	dt_off_count;
	unsigned long	dt_loop;
};

#if 0
/* If you start using the next struct, you'll have to adjust it to
 * work with 64-bit firmware I think -PB
 */
struct pdc_iodc {     /* PDC_IODC */
	unsigned char   hversion_model;
	unsigned char 	hversion;
	unsigned char 	spa;
	unsigned char 	type;
	unsigned int	sversion_rev:4;
	unsigned int	sversion_model:19;
	unsigned int	sversion_opt:8;
	unsigned char	rev;
	unsigned char	dep;
	unsigned char	features;
	unsigned char	pad1;
	unsigned int	checksum:16;
	unsigned int	length:16;
	unsigned int    pad[15];
} __attribute__((aligned(8))) ;
#endif

#ifndef CONFIG_PA20
/* no BLTBs in pa2.0 processors */
struct pdc_btlb_info_range {
	__u8 res00;
	__u8 num_i;
	__u8 num_d;
	__u8 num_comb;
};

struct pdc_btlb_info {	/* PDC_BLOCK_TLB, return of PDC_BTLB_INFO */
	unsigned int min_size;	/* minimum size of BTLB in pages */
	unsigned int max_size;	/* maximum size of BTLB in pages */
	struct pdc_btlb_info_range fixed_range_info;
	struct pdc_btlb_info_range variable_range_info;
};

#endif /* !CONFIG_PA20 */

#ifdef __LP64__
struct pdc_memory_table_raddr { /* PDC_MEM/PDC_MEM_TABLE (return info) */
	unsigned long entries_returned;
	unsigned long entries_total;
};

struct pdc_memory_table {       /* PDC_MEM/PDC_MEM_TABLE (arguments) */
	unsigned long paddr;
	unsigned int  pages;
	unsigned int  reserved;
};
#endif /* __LP64__ */

struct pdc_system_map_mod_info { /* PDC_SYSTEM_MAP/FIND_MODULE */
	unsigned long mod_addr;
	unsigned long mod_pgs;
	unsigned long add_addrs;
};

struct pdc_system_map_addr_info { /* PDC_SYSTEM_MAP/FIND_ADDRESS */
	unsigned long mod_addr;
	unsigned long mod_pgs;
};

struct hardware_path {
	char  flags;	/* see bit definitions below */
	char  bc[6];	/* Bus Converter routing info to a specific */
			/* I/O adaptor (< 0 means none, > 63 resvd) */
	char  mod;	/* fixed field of specified module */
};

/*
 * Device path specifications used by PDC.
 */
struct pdc_module_path {
	struct hardware_path path;
	unsigned int layers[6]; /* device-specific info (ctlr #, unit # ...) */
};

#ifndef CONFIG_PA20
/* Only used on some pre-PA2.0 boxes */
struct pdc_memory_map {		/* PDC_MEMORY_MAP */
	unsigned long hpa;	/* mod's register set address */
	unsigned long more_pgs;	/* number of additional I/O pgs */
};
#endif

struct pdc_tod {
	unsigned long tod_sec; 
	unsigned long tod_usec;
};

#ifdef __LP64__
struct pdc_pat_cell_num {
	unsigned long cell_num;
	unsigned long cell_loc;
};

struct pdc_pat_cpu_num {
	unsigned long cpu_num;
	unsigned long cpu_loc;
};

struct pdc_pat_pd_addr_map_entry {
	unsigned char entry_type;       /* 1 = Memory Descriptor Entry Type */
	unsigned char reserve1[5];
	unsigned char memory_type;
	unsigned char memory_usage;
	unsigned long paddr;
	unsigned int  pages;            /* Length in 4K pages */
	unsigned int  reserve2;
	unsigned long cell_map;
};

/* FIXME: mod[508] should really be a union of the various mod components */
struct pdc_pat_cell_mod_maddr_block {	/* PDC_PAT_CELL_MODULE */
	unsigned long cba;              /* function 0 configuration space address */
	unsigned long mod_info;         /* module information */
	unsigned long mod_location;     /* physical location of the module */
	struct hardware_path mod_path;	/* hardware path */
	unsigned long mod[508];		/* PAT cell module components */
};

typedef struct pdc_pat_cell_mod_maddr_block pdc_pat_cell_mod_maddr_block_t;
#endif /* __LP64__ */

/* architected results from PDC_PIM/transfer hpmc on a PA1.1 machine */

struct pdc_hpmc_pim_11 { /* PDC_PIM */
	__u32 gr[32];
	__u32 cr[32];
	__u32 sr[8];
	__u32 iasq_back;
	__u32 iaoq_back;
	__u32 check_type;
	__u32 cpu_state;
	__u32 rsvd1;
	__u32 cache_check;
	__u32 tlb_check;
	__u32 bus_check;
	__u32 assists_check;
	__u32 rsvd2;
	__u32 assist_state;
	__u32 responder_addr;
	__u32 requestor_addr;
	__u32 path_info;
	__u64 fr[32];
};

/*
 * architected results from PDC_PIM/transfer hpmc on a PA2.0 machine
 *
 * Note that PDC_PIM doesn't care whether or not wide mode was enabled
 * so the results are different on  PA1.1 vs. PA2.0 when in narrow mode.
 *
 * Note also that there are unarchitected results available, which
 * are hversion dependent. Do a "ser pim 0 hpmc" after rebooting, since
 * the firmware is probably the best way of printing hversion dependent
 * data.
 */

struct pdc_hpmc_pim_20 { /* PDC_PIM */
	__u64 gr[32];
	__u64 cr[32];
	__u64 sr[8];
	__u64 iasq_back;
	__u64 iaoq_back;
	__u32 check_type;
	__u32 cpu_state;
	__u32 cache_check;
	__u32 tlb_check;
	__u32 bus_check;
	__u32 assists_check;
	__u32 assist_state;
	__u32 path_info;
	__u64 responder_addr;
	__u64 requestor_addr;
	__u64 fr[32];
};

#endif /* __ASSEMBLY__ */

/* flags of the device_path (see below) */
#define	PF_AUTOBOOT	0x80
#define	PF_AUTOSEARCH	0x40
#define	PF_TIMER	0x0F

#ifndef __ASSEMBLY__

struct device_path {		/* page 1-69 */
	unsigned char flags;	/* flags see above! */
	unsigned char bc[6];	/* bus converter routing info */
	unsigned char mod;
	unsigned int  layers[6];/* device-specific layer-info */
} __attribute__((aligned(8))) ;

struct pz_device {
	struct	device_path dp;	/* see above */
	/* struct	iomod *hpa; */
	unsigned int hpa;	/* HPA base address */
	/* char	*spa; */
	unsigned int spa;	/* SPA base address */
	/* int	(*iodc_io)(struct iomod*, ...); */
	unsigned int iodc_io;	/* device entry point */
	short	pad;		/* reserved */
	unsigned short cl_class;/* see below */
} __attribute__((aligned(8))) ;

#endif /* __ASSEMBLY__ */

/* cl_class
 * page 3-33 of IO-Firmware ARS
 * IODC ENTRY_INIT(Search first) RET[1]
 */
#define	CL_NULL		0	/* invalid */
#define	CL_RANDOM	1	/* random access (as disk) */
#define	CL_SEQU		2	/* sequential access (as tape) */
#define	CL_DUPLEX	7	/* full-duplex point-to-point (RS-232, Net) */
#define	CL_KEYBD	8	/* half-duplex console (HIL Keyboard) */
#define	CL_DISPL	9	/* half-duplex console (display) */
#define	CL_FC		10	/* FiberChannel access media */

#if 0
/* FIXME: DEVCLASS_* duplicates CL_* (above).  Delete DEVCLASS_*? */
#define DEVCLASS_RANDOM		1
#define DEVCLASS_SEQU		2
#define DEVCLASS_DUPLEX		7
#define DEVCLASS_KEYBD		8
#define DEVCLASS_DISP		9
#endif

/* IODC ENTRY_INIT() */
#define ENTRY_INIT_SRCH_FRST	2
#define ENTRY_INIT_SRCH_NEXT	3
#define ENTRY_INIT_MOD_DEV	4
#define ENTRY_INIT_DEV		5
#define ENTRY_INIT_MOD		6
#define ENTRY_INIT_MSG		9

/* IODC ENTRY_IO() */
#define ENTRY_IO_BOOTIN		0
#define ENTRY_IO_BOOTOUT	1
#define ENTRY_IO_CIN		2
#define ENTRY_IO_COUT		3
#define ENTRY_IO_CLOSE		4
#define ENTRY_IO_GETMSG		9
#define ENTRY_IO_BBLOCK_IN	16
#define ENTRY_IO_BBLOCK_OUT	17

/* IODC ENTRY_SPA() */

/* IODC ENTRY_CONFIG() */

/* IODC ENTRY_TEST() */

/* IODC ENTRY_TLB() */


/* DEFINITION OF THE ZERO-PAGE (PAG0) */
/* based on work by Jason Eckhardt (jason@equator.com) */

#ifndef __ASSEMBLY__

#define PAGE0   ((struct zeropage *)__PAGE_OFFSET)

struct zeropage {
	/* [0x000] initialize vectors (VEC) */
	unsigned int	vec_special;		/* must be zero */
	/* int	(*vec_pow_fail)(void);*/
	unsigned int	vec_pow_fail; /* power failure handler */
	/* int	(*vec_toc)(void); */
	unsigned int	vec_toc;
	unsigned int	vec_toclen;
	/* int	(*vec_rendz)(void); */
	unsigned int vec_rendz;
	int	vec_pow_fail_flen;
	int	vec_pad[10];		
	
	/* [0x040] reserved processor dependent */
	int	pad0[112];

	/* [0x200] reserved */
	int	pad1[84];

	/* [0x350] memory configuration (MC) */
	int	memc_cont;		/* contiguous mem size (bytes) */
	int	memc_phsize;		/* physical memory size */
	int	memc_adsize;		/* additional mem size, bytes of SPA space used by PDC */
	unsigned int mem_pdc_hi;	/* used for 64-bit */

	/* [0x360] various parameters for the boot-CPU */
	/* unsigned int *mem_booterr[8]; */
	unsigned int mem_booterr[8];	/* ptr to boot errors */
	unsigned int mem_free;		/* first location, where OS can be loaded */
	/* struct iomod *mem_hpa; */
	unsigned int mem_hpa;		/* HPA of the boot-CPU */
	/* int (*mem_pdc)(int, ...); */
	unsigned int mem_pdc;		/* PDC entry point */
	unsigned int mem_10msec;	/* number of clock ticks in 10msec */

	/* [0x390] initial memory module (IMM) */
	/* struct iomod *imm_hpa; */
	unsigned int imm_hpa;		/* HPA of the IMM */
	int	imm_soft_boot;		/* 0 = was hard boot, 1 = was soft boot */
	unsigned int	imm_spa_size;		/* SPA size of the IMM in bytes */
	unsigned int	imm_max_mem;		/* bytes of mem in IMM */

	/* [0x3A0] boot console, display device and keyboard */
	struct pz_device mem_cons;	/* description of console device */
	struct pz_device mem_boot;	/* description of boot device */
	struct pz_device mem_kbd;	/* description of keyboard device */

	/* [0x430] reserved */
	int	pad430[116];

	/* [0x600] processor dependent */
	__u32	pad600[1];
	__u32	proc_sti;		/* pointer to STI ROM */
	__u32	pad608[126];
};

#endif /* __ASSEMBLY__ */

/* Page Zero constant offsets used by the HPMC handler */

#define BOOT_CONSOLE_HPA_OFFSET  0x3c0
#define BOOT_CONSOLE_SPA_OFFSET  0x3c4
#define BOOT_CONSOLE_PATH_OFFSET 0x3a8

#ifndef __ASSEMBLY__
void pdc_console_init(void);	/* in pdc_console.c */
void pdc_console_restart(void);

void setup_pdc(void);		/* in inventory.c */

/* wrapper-functions from pdc.c */

int pdc_add_valid(unsigned long address);
int pdc_chassis_info(struct pdc_chassis_info *chassis_info, void *led_info, unsigned long len);
int pdc_chassis_disp(unsigned long disp);
int pdc_coproc_cfg(struct pdc_coproc_cfg *pdc_coproc_info);
int pdc_iodc_read(unsigned long *actcnt, unsigned long hpa, unsigned int index,
		  void *iodc_data, unsigned int iodc_data_size);
int pdc_system_map_find_mods(struct pdc_system_map_mod_info *pdc_mod_info,
			     struct pdc_module_path *mod_path, long mod_index);
int pdc_system_map_find_addrs(struct pdc_system_map_addr_info *pdc_addr_info, 
			      long mod_index, long addr_index);
int pdc_model_info(struct pdc_model *model);
int pdc_model_sysmodel(char *name);
int pdc_model_cpuid(unsigned long *cpu_id);
int pdc_model_versions(unsigned long *versions, int id);
int pdc_model_capabilities(unsigned long *capabilities);
int pdc_cache_info(struct pdc_cache_info *cache);
#ifndef CONFIG_PA20
int pdc_btlb_info(struct pdc_btlb_info *btlb);
int pdc_mem_map_hpa(struct pdc_memory_map *r_addr, struct pdc_module_path *mod_path);
#endif /* !CONFIG_PA20 */
int pdc_lan_station_id(char *lan_addr, unsigned long net_hpa);

int pdc_pci_irt_size(unsigned long *num_entries, unsigned long hpa);
int pdc_pci_irt(unsigned long num_entries, unsigned long hpa, void *tbl);

int pdc_get_initiator(struct hardware_path *hwpath, unsigned char *scsi_id, unsigned long *period, char *width, char *mode);
int pdc_tod_read(struct pdc_tod *tod);
int pdc_tod_set(unsigned long sec, unsigned long usec);

#ifdef __LP64__
int pdc_mem_mem_table(struct pdc_memory_table_raddr *r_addr,
		struct pdc_memory_table *tbl, unsigned long entries);
#endif

int pdc_do_firm_test_reset(unsigned long ftc_bitmap);
int pdc_do_reset(void);
int pdc_soft_power_info(unsigned long *power_reg);
int pdc_soft_power_button(int sw_control);
void pdc_suspend_usb(void);
int pdc_iodc_getc(void);
void pdc_iodc_putc(unsigned char c);
void pdc_iodc_outc(unsigned char c);

void pdc_emergency_unlock(void);
int pdc_sti_call(unsigned long func, unsigned long flags,
                 unsigned long inptr, unsigned long outputr,
                 unsigned long glob_cfg);

#ifdef __LP64__
int pdc_pat_chassis_send_log(unsigned long status, unsigned long data);

int pdc_pat_cell_get_number(struct pdc_pat_cell_num *cell_info);
int pdc_pat_cell_module(unsigned long *actcnt, unsigned long ploc, unsigned long mod,
			unsigned long view_type, void *mem_addr);
int pdc_pat_cpu_get_number(struct pdc_pat_cpu_num *cpu_info, void *hpa);
int pdc_pat_get_irt_size(unsigned long *num_entries, unsigned long cell_num);
int pdc_pat_get_irt(void *r_addr, unsigned long cell_num);
int pdc_pat_pd_get_addr_map(unsigned long *actual_len, void *mem_addr, 
			    unsigned long count, unsigned long offset);

/********************************************************************
* PDC_PAT_CELL[Return Cell Module] memaddr[0] conf_base_addr
* ----------------------------------------------------------
* Bit  0 to 51 - conf_base_addr
* Bit 52 to 62 - reserved
* Bit       63 - endianess bit
********************************************************************/
#define PAT_GET_CBA(value) ((value) & 0xfffffffffffff000UL)

/********************************************************************
* PDC_PAT_CELL[Return Cell Module] memaddr[1] mod_info
* ----------------------------------------------------
* Bit  0 to  7 - entity type
*    0 = central agent,            1 = processor,
*    2 = memory controller,        3 = system bus adapter,
*    4 = local bus adapter,        5 = processor bus converter,
*    6 = crossbar fabric connect,  7 = fabric interconnect,
*    8 to 254 reserved,            255 = unknown.
* Bit  8 to 15 - DVI
* Bit 16 to 23 - IOC functions
* Bit 24 to 39 - reserved
* Bit 40 to 63 - mod_pages
*    number of 4K pages a module occupies starting at conf_base_addr
********************************************************************/
#define PAT_GET_ENTITY(value)	(((value) >> 56) & 0xffUL)
#define PAT_GET_DVI(value)	(((value) >> 48) & 0xffUL)
#define PAT_GET_IOC(value)	(((value) >> 40) & 0xffUL)
#define PAT_GET_MOD_PAGES(value)(((value) & 0xffffffUL)

#else /* !__LP64__ */
/* No PAT support for 32-bit kernels...sorry */
#define pdc_pat_get_irt_size(num_entries, cell_numn)	PDC_BAD_PROC
#define pdc_pat_get_irt(r_addr, cell_num)	PDC_BAD_PROC
#endif /* !__LP64__ */

extern void pdc_init(void);

#endif /* __ASSEMBLY__ */

#endif /* _PARISC_PDC_H */
