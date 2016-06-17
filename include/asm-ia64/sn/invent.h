/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_INVENT_H
#define _ASM_IA64_SN_INVENT_H

#include <linux/types.h>
#include <asm/sn/sgi.h>
/*
 * sys/sn/invent.h --  Kernel Hardware Inventory
 *
 * As the system boots, a list of recognized devices is assembled.
 * This list can then be accessed through syssgi() by user-level programs
 * so that they can learn about available peripherals and the system's
 * hardware configuration.
 *
 * The data is organized into a linked list of structures that are composed
 * of an inventory item class and a class-specific type.  Each instance may
 * also specify a 32-bit "state" which might be size, readiness, or
 * anything else that's relevant.
 *
 */

#define major_t int
#define minor_t int
#define app32_ptr_t unsigned long
#define graph_vertex_place_t long
#define GRAPH_VERTEX_NONE ((vertex_hdl_t)-1)
#define GRAPH_EDGE_PLACE_NONE ((graph_edge_place_t)0)
#define GRAPH_INFO_PLACE_NONE ((graph_info_place_t)0)
#define GRAPH_VERTEX_PLACE_NONE ((graph_vertex_place_t)0)


typedef struct inventory_s {
	struct	inventory_s *inv_next;	/* next inventory record in list */
	int	inv_class;		/* class of object */
	int	inv_type;		/* class sub-type of object */
	major_t	inv_controller;		/* object major identifier */
	minor_t	inv_unit;		/* object minor identifier */
	int	inv_state;		/* information specific to object or
					   class */
} inventory_t;

typedef struct cpu_inv_s {
	int	cpuflavor;	/* differentiate processor */
	int	cpufq;		/* cpu frequency */
	int	sdsize;		/* secondary data cache size */
	int	sdfreq;		/* speed of the secondary cache */
} cpu_inv_t;


typedef struct diag_inv_s{
         char name[80];
         int  diagval;
         int  physid;
         int  virtid;
} diag_inv_t;


typedef struct router_inv_s{
  char portmap[80];             /* String indicating which ports int/ext */
  char type[40];                /* String name: e.g. "star", "meta", etc. */
  int  freq;                    /* From hub */
  int  rev;                     /* From hub */
} router_inv_t;


/*
 * NOTE: This file is a central registry for inventory IDs for each
 *       class of inventory object.  It is important to keep the central copy
 *       of this file up-to-date with the work going on in various engineering
 *       projects.  When making changes to this file in an engineering project
 *       tree, please make those changes separately from any others and then
 *       merge the changes to this file into the main line trees in order to
 *       prevent other engineering projects from conflicting with your ID
 *       allocations.
 */


/* Inventory Classes */
/* when adding a new class, add also to classes[] in hinv.c */
#define INV_PROCESSOR	1
#define INV_DISK	2
#define INV_MEMORY	3
#define INV_SERIAL	4
#define INV_PARALLEL	5
#define INV_TAPE	6
#define INV_GRAPHICS	7
#define INV_NETWORK	8
#define INV_SCSI	9	/* SCSI devices other than disk and tape */
#define INV_AUDIO	10
#define	INV_IOBD	11
#define	INV_VIDEO	12
#define	INV_BUS		13
#define	INV_MISC	14	/* miscellaneous: a catchall */
/*** add post-5.2 classes here for backward compatibility ***/
#define	INV_COMPRESSION	15
#define	INV_VSCSI	16	/* SCSI devices on jag other than disk and tape */
#define	INV_DISPLAY     17
#define	INV_UNC_SCSILUN	18	/* Unconnected SCSI lun */
#define	INV_PCI		19	/* PCI Bus */
#define	INV_PCI_NO_DRV	20	/* PCI Bus without any driver */
#define	INV_PROM	21	/* Different proms in the system */
#define INV_IEEE1394	22	/* IEEE1394 devices */
#define INV_RPS		23      /* redundant power source */
#define INV_TPU		24	/* Tensor Processing Unit */
#define INV_FCNODE	25	/* Helper class for SCSI classes, not in classes[] */
#define INV_USB		26	/* Universal Serial Bus */
#define INV_1394NODE    27      /* helper class for 1394/SPB2 classes, not in classes[] */

/* types for class processor */
#define INV_CPUBOARD	1
#define INV_CPUCHIP	2
#define INV_FPUCHIP	3
#define INV_CCSYNC	4	/* CC Rev 2+ sync join counter */

/* states for cpu and fpu chips are revision numbers */

/* cpuboard states */
#define INV_IP20BOARD   10
#define INV_IP19BOARD   11
#define INV_IP22BOARD   12
#define INV_IP21BOARD	13
#define INV_IP26BOARD	14
#define INV_IP25BOARD	15
#define INV_IP30BOARD	16
#define INV_IP28BOARD	17
#define INV_IP32BOARD	18
#define INV_IP27BOARD	19
#define INV_IPMHSIMBOARD 20
#define INV_IP33BOARD	21
#define INV_IP35BOARD	22

/* types for class INV_IOBD */
#define INV_EVIO	2	/* EVEREST I/O board */
#define INV_O200IO	3	/* Origin 200 base I/O */

/* IO board types for origin2000  for class INV_IOBD*/

#define INV_O2000BASEIO	0x21	
#define INV_O2000MSCSI	0x22	
#define INV_O2000MENET	0x23
#define INV_O2000HIPPI	0x24
#define INV_O2000GFX	0x25	
#define INV_O2000HAROLD 0x26
#define INV_O2000VME	0x27
#define INV_O2000MIO	0x28
#define INV_O2000FC	0x29
#define INV_O2000LINC	0x2a

#define INV_PCIADAP	4
/* states for class INV_IOBD type INV_EVERESTIO -- value of eb_type field */
#define INV_IO4_REV1	0x21	

/* types for class disk */
/* NB: types MUST be unique within a class.
   Please check this if adding new types. */

#define INV_SCSICONTROL	1
#define INV_SCSIDRIVE	2
#define INV_SCSIFLOPPY	5	/* also cdroms, optical disks, etc. */
#define INV_JAGUAR	16	/* Interphase Jaguar */
#define INV_VSCSIDRIVE	17	/* Disk connected to Jaguar */
#define INV_GIO_SCSICONTROL 18	/* optional GIO SCSI controller */
#define INV_SCSIRAID	19	/* SCSI attached RAID */
#define INV_XLVGEN      20	/* Generic XLV disk device */
#define INV_PCCARD	21	/* PC-card (PCMCIA) devices */
#define INV_PCI_SCSICONTROL	22   /* optional PCI SCSI controller */

/* states for INV_SCSICONTROL disk type; indicate which chip rev;
 * for 93A and B, unit field has microcode rev. */
#define INV_WD93	0	/* WD 33C93  */
#define INV_WD93A	1	/* WD 33C93A */
#define INV_WD93B	2	/* WD 33C93B */
#define INV_WD95A	3	/* WD 33C95A */
#define INV_SCIP95	4       /* SCIP with a WD 33C95A */
#define INV_ADP7880	5	/* Adaptec 7880 (single channel) */
#define INV_QL_REV1     6       /* qlogic 1040  */
#define INV_QL_REV2     7       /* qlogic 1040A */
#define INV_QL_REV2_4   8       /* qlogic 1040A rev 4 */
#define INV_QL_REV3     9       /* qlogic 1040B */
#define INV_FCADP	10	/* Adaptec Emerald Fibrechannel */
#define INV_QL_REV4     11      /* qlogic 1040B rev 2 */
#define INV_QL		12	/* Unknown QL version */	
#define INV_QL_1240     13      /* qlogic 1240 */
#define INV_QL_1080     14      /* qlogic 1080 */
#define INV_QL_1280     15      /* qlogic 1280 */
#define INV_QL_10160    16      /* qlogic 10160 */
#define INV_QL_12160    17      /* qlogic 12160 */
#define INV_QL_2100	18	/* qLogic 2100 Fibrechannel */
#define INV_QL_2200	19	/* qLogic 2200 Fibrechannel */
#define INV_PR_HIO_D	20	/* Prisa HIO Dual channel */
#define INV_PR_PCI64_D	21	/* Prisa PCI-64 Dual channel */
#define INV_QL_2200A	22	/* qLogic 2200A Fibrechannel */
#define INV_SBP2        23      /* SBP2 protocol over OHCI on 1394 */
#define INV_QL_2300	24	/* qLogic 2300 Fibrechannel */


/* states for INV_SCSIDRIVE type of class disk */
#define INV_RAID5_LUN	0x100
#define INV_PRIMARY	0x200	/* primary path */
#define INV_ALTERNATE	0x400	/* alternate path */
#define INV_FAILED	0x800	/* path has failed */
#define INV_XVMVOL	0x1000	/* disk is managed by XVM */

/* states for INV_SCSIFLOPPY type of class disk */
#define INV_TEAC_FLOPPY 1       /* TEAC 3 1/2 inch floppy drive */
#define INV_INSITE_FLOPPY 2     /* INSITE, IOMEGA  Io20S, SyQuest floppy drives */

/* END OF CLASS DISK TYPES */

/* types for class memory */
/* NB. the states for class memory are sizes in bytes */
#define INV_MAIN	1
#define INV_DCACHE	3
#define INV_ICACHE	4
#define INV_WBUFFER	5
#define INV_SDCACHE	6
#define INV_SICACHE	7
#define INV_SIDCACHE	8
#define INV_MAIN_MB	9
#define INV_HUBSPC      10      /* HUBSPC */
#define INV_TIDCACHE	11

/* types for class serial */
#define INV_CDSIO	1	/* Central Data serial board */
#define INV_T3270	2	/* T3270 emulation */
#define INV_GSE		3	/* SpectraGraphics Gerbil coax cable */
#define INV_SI		4	/* SNA SDLC controller */
#define	INV_M333X25 	6	/* X.25 controller */
#define INV_CDSIO_E	7	/* Central Data serial board on E space */
#define INV_ONBOARD	8	/* Serial ports per CPU board */
#define INV_EPC_SERIAL	9	/* EVEREST I/O EPC serial port */
#define INV_ICA		10	/* IRIS (IBM) Channel Adapter card */
#define INV_VSC		11	/* SBE VME Synch Comm board */
#define INV_ISC		12	/* SBE ISA Synch Comm board */
#define INV_GSC		13	/* SGI GIO Synch Comm board */
#define INV_ASO_SERIAL	14	/* serial portion of SGI ASO board */
#define INV_PSC		15	/* SBE PCI Synch Comm board */
#define INV_IOC3_DMA	16	/* DMA mode IOC3 serial */
#define INV_IOC3_PIO	17	/* PIO mode IOC3 serial */
#define INV_INVISIBLE	18	/* invisible inventory entry for kernel use */
#define INV_ISA_DMA	19	/* DMA mode ISA serial -- O2 */

/* types for class parallel */
#define INV_GPIB	2	/* National Instrument GPIB board */
#define INV_GPIB_E	3	/* National Instrument GPIB board on E space*/
#define INV_EPC_PLP	4	/* EVEREST I/O EPC Parallel Port */
#define INV_ONBOARD_PLP	5	/* Integral parallel port,
				      state = 0 -> output only
				      state = 1 -> bi-directional */
#define INV_EPP_ECP_PLP	6	/* Integral EPP/ECP parallel port */
#define INV_EPP_PFD	7	/* External EPP parallel peripheral */

/* types for class tape */
#define INV_SCSIQIC	1	/* Any SCSI tape, not just QIC{24,150}... */
#define INV_VSCSITAPE	4	/* SCSI tape connected to Jaguar */

/* sub types for type INV_SCSIQIC and INV_VSCSITAPE (in state) */
#define TPUNKNOWN	0	/* type not known */
#define TPQIC24		1	/* QIC24 1/4" cartridge */
#define TPDAT		2	/* 4mm Digital Audio Tape cartridge */
#define TPQIC150	3	/* QIC150 1/4" cartridge */
#define TP9TRACK	4	/* 9 track reel */
#define TP8MM_8200	5	/* 8 mm video tape cartridge */
#define TP8MM_8500	6	/* 8 mm video tape cartridge */
#define TPQIC1000	7	/* QIC1000 1/4" cartridge */
#define TPQIC1350	8	/* QIC1350 1/4" cartridge */
#define TP3480		9	/* 3480 compatible cartridge */
#define TPDLT		10	/* DEC Digital Linear Tape cartridge */
#define TPD2		11	/* D2 tape cartridge */
#define TPDLTSTACKER	12	/* DEC Digital Linear Tape stacker */
#define TPNTP		13	/* IBM Magstar 3590 Tape Device cartridge */
#define TPNTPSTACKER	14	/* IBM Magstar 3590 Tape Device stacker */
#define TPSTK9490       15      /* StorageTeK 9490 */
#define TPSTKSD3        16      /* StorageTeK SD3 */
#define TPGY10	        17      /* Sony GY-10  */
#define TP8MM_8900	18	/* 8 mm (AME) tape cartridge */
#define TPMGSTRMP       19      /* IBM Magster MP 3570 cartridge */
#define TPMGSTRMPSTCKR  20      /* IBM Magstar MP stacker */
#define TPSTK4791       21      /* StorageTek 4791 */
#define TPSTK4781       22      /* StorageTek 4781 */
#define TPFUJDIANA1     23      /* Fujitsu Diana-1 (M1016/M1017) */
#define TPFUJDIANA2     24      /* Fujitsu Diana-2 (M2483) */
#define TPFUJDIANA3     25      /* Fujitsu Diana-3 (M2488) */
#define TP8MM_AIT	26	/* Sony AIT format tape */
#define TPTD3600        27      /* Philips TD3600  */
#define TPTD3600STCKR   28      /* Philips TD3600  stacker */
#define TPNCTP          29      /* Philips NCTP */
#define TPGY2120        30      /* Sony GY-2120 (replaces GY-10)  */
#define TPOVL490E       31      /* Overland Data L490E (3490E compatible) */
#define TPSTK9840       32      /* StorageTeK 9840 (aka Eagle) */

/* Diagnostics inventory */
#define INV_CPUDIAGVAL  70


/*
 *  GFX invent is a subset of gfxinfo
 */

/* types for class graphics */
#define INV_GR1BOARD	1	/* GR1 (Eclipse) graphics */
#define INV_GR1BP	2	/* OBSOLETE - use INV_GR1BIT24 instead */
#define INV_GR1ZBUFFER	3	/* OBSOLETE - use INV_GR1ZBUF24 instead */
#define INV_GRODEV	4	/* Clover1 graphics */
#define INV_GMDEV	5	/* GT graphics */
#define INV_CG2		6	/* CG2 composite video/genlock board */
#define INV_VMUXBOARD	7	/* VMUX video mux board */
#define	INV_VGX		8	/* VGX (PowerVision) graphics */
#define	INV_VGXT	9	/* VGXT (PowerVision) graphics with IMP5s. */
#define	INV_LIGHT	10	/* LIGHT graphics */
#define INV_GR2		11	/* EXPRESS graphics */
#define INV_RE		12	/* RealityEngine graphics */
#define INV_VTX		13	/* RealityEngine graphics - VTX variant */
#define INV_NEWPORT	14	/* Newport graphics */
#define INV_MGRAS	15	/* Mardigras graphics */
#define INV_IR		16	/* InfiniteReality graphics */
#define INV_CRIME	17	/* Moosehead on board CRIME graphics */
#define INV_IR2		18	/* InfiniteReality2 graphics */
#define INV_IR2LITE	19	/* Reality graphics */
#define INV_IR2E	20	/* InfiniteReality2e graphics */
#define INV_ODSY        21      /* Odyssey graphics */
#define INV_IR3		22	/* InfiniteReality3 graphics */

/* states for graphics class GR1 */
#define INV_GR1REMASK	0x7	/* RE version */
#define INV_GR1REUNK	0x0	/* RE version unknown */
#define INV_GR1RE1	0x1	/* RE1 */
#define INV_GR1RE2	0x2	/* RE2 */
#define INV_GR1BUSMASK	0x38	/* GR1 bus architecture */
#define INV_GR1PB	0x00	/* Eclipse private bus */
#define INV_GR1PBVME	0x08	/* VGR2 board VME and private bus interfaces */
#define INV_GR1TURBO	0x40	/* has turbo option */
#define INV_GR1BIT24  	0x80    /* has bitplane option */
#define INV_GR1ZBUF24 	0x100   /* has z-buffer option */
#define INV_GR1SMALLMON 0x200   /* using 14" monitor */
#define INV_GR1SMALLMAP 0x400   /* has 256 entry color map */
#define INV_GR1AUX4 	0x800   /* has AUX/WID plane option */

/* states for graphics class GR2 */
		/* bitmasks */
#define INV_GR2_Z	0x1	/* has z-buffer option */
#define INV_GR2_24	0x2	/* has bitplane option */
#define INV_GR2_4GE     0x4     /* has 4 GEs */
#define INV_GR2_1GE	0x8	/* has 1 GEs */
#define INV_GR2_2GE	0x10	/* has 2 GEs */
#define INV_GR2_8GE	0x20	/* has 8 GEs */
#define INV_GR2_GR3	0x40	/* board GR3 */
#define INV_GR2_GU1	0x80	/* board GU1 */
#define INV_GR2_INDY    0x100   /* board GR3 on Indy*/
#define INV_GR2_GR5	0x200	/* board GR3 with 4 GEs, hinv prints GR5-XZ */

		/* supported configurations */
#define INV_GR2_XS	0x0     /* GR2-XS */
#define INV_GR2_XSZ	0x1     /* GR2-XS with z-buffer */
#define INV_GR2_XS24	0x2     /* GR2-XS24 */
#define INV_GR2_XS24Z	0x3     /* GR2-XS24 with z-buffer */
#define INV_GR2_XSM	0x4     /* GR2-XSM */
#define INV_GR2_ELAN	0x7	/* GR2-Elan */
#define	INV_GR2_XZ	0x13	/* GR2-XZ */
#define	INV_GR3_XSM	0x44	/* GR3-XSM */
#define	INV_GR3_ELAN	0x47	/* GR3-Elan */
#define	INV_GU1_EXTREME	0xa3	/* GU1-Extreme */

/* States for graphics class NEWPORT */
#define	INV_NEWPORT_XL	0x01	/* Indigo2 XL model */
#define INV_NEWPORT_24	0x02	/* board has 24 bitplanes */
#define INV_NEWTON      0x04    /* Triton SUBGR tagging */

/* States for graphics class MGRAS */
#define INV_MGRAS_ARCHS 0xff000000      /* architectures */
#define INV_MGRAS_HQ3   0x00000000   /*impact*/
#define INV_MGRAS_HQ4	0x01000000   /*gamera*/
#define INV_MGRAS_MOT   0x02000000   /*mothra*/
#define INV_MGRAS_GES	0x00ff0000	/* number of GEs */
#define INV_MGRAS_1GE	0x00010000
#define INV_MGRAS_2GE	0x00020000
#define INV_MGRAS_RES	0x0000ff00	/* number of REs */
#define INV_MGRAS_1RE	0x00000100
#define INV_MGRAS_2RE	0x00000200
#define INV_MGRAS_TRS	0x000000ff	/* number of TRAMs */
#define INV_MGRAS_0TR	0x00000000
#define INV_MGRAS_1TR	0x00000001
#define INV_MGRAS_2TR	0x00000002

/* States for graphics class CRIME */
#define INV_CRM_BASE    0x01            /* Moosehead basic model */

/* States for graphics class ODSY */
#define INV_ODSY_ARCHS      0xff000000 /* architectures */
#define INV_ODSY_REVA_ARCH  0x01000000 /* Buzz Rev A */
#define INV_ODSY_REVB_ARCH  0x02000000 /* Buzz Rev B */
#define INV_ODSY_MEMCFG     0x00ff0000 /* memory configs */
#define INV_ODSY_MEMCFG_32  0x00010000 /* 32MB memory */
#define INV_ODSY_MEMCFG_64  0x00020000 /* 64MB memory */
#define INV_ODSY_MEMCFG_128 0x00030000 /* 128MB memory */
#define INV_ODSY_MEMCFG_256 0x00040000 /* 256MB memory */
#define INV_ODSY_MEMCFG_512 0x00050000 /* 512MB memory */


/* types for class network */
#define INV_NET_ETHER		0	/* 10Mb Ethernet */
#define INV_NET_HYPER		1	/* HyperNet */
#define	INV_NET_CRAYIOS		2	/* Cray Input/Ouput Subsystem */
#define	INV_NET_FDDI		3	/* FDDI */
#define INV_NET_TOKEN		4	/* 16/4 Token Ring */
#define INV_NET_HIPPI		5	/* HIPPI */
#define INV_NET_ATM		6	/* ATM */
#define INV_NET_ISDN_BRI	7	/* ISDN */
#define INV_NET_ISDN_PRI	8	/* PRI ISDN */
#define INV_NET_HIPPIS		9	/* HIPPI-Serial */
#define	INV_NET_GSN		10	/* GSN (aka HIPPI-6400) */
#define INV_NET_MYRINET		11	/* Myricom PCI network */

/* controllers for network types, unique within class network */
#define INV_ETHER_EC	0	/* IP6 integral controller */
#define INV_ETHER_ENP	1	/* CMC board */
#define INV_ETHER_ET	2	/* IP5 integral controller */
#define INV_HYPER_HY	3	/* HyperNet controller */
#define	INV_CRAYIOS_CFEI3 4	/* Cray Front End Interface, v3 */
#define	INV_FDDI_IMF	5	/* Interphase/Martin 3211 FDDI */
#define INV_ETHER_EGL	6	/* Interphase V/4207 Eagle */
#define INV_ETHER_FXP	7	/* CMC C/130 FXP */
#define INV_FDDI_IPG	8	/* Interphase/SGI 4211 Peregrine FDDI */
#define INV_TOKEN_FV	9	/* Formation fv1600 Token-Ring board */
#define INV_FDDI_XPI	10	/* XPI GIO bus FDDI */
#define INV_TOKEN_GTR	11	/* GTR GIO bus TokenRing */
#define INV_ETHER_GIO	12	/* IP12/20 optional GIO ethernet controller */
#define INV_ETHER_EE	13	/* Everest IO4 EPC SEEQ/EDLC */
#define INV_HIO_HIPPI	14	/* HIO HIPPI for Challenge/Onyx */
#define INV_ATM_GIO64	15	/* ATM OC-3c Mez card */
#define INV_ETHER_EP	16	/* 8-port E-Plex Ethernet */
#define INV_ISDN_SM	17	/* Siemens PEB 2085 */
#define INV_TOKEN_MTR	18	/* EISA TokenRing */
#define INV_ETHER_EF	19	/* IOC3 Fast Ethernet */
#define INV_ISDN_48XP	20	/* Xircom PRI-48XP */
#define INV_FDDI_RNS	21	/* Rockwell Network Systems FDDI */
#define INV_HIPPIS_XTK	22	/* Xtalk HIPPI-Serial */
#define INV_ATM_QUADOC3	23	/* Xtalk Quad OC-3c ATM interface */
#define INV_TOKEN_MTRPCI 24     /* PCI TokenRing */
#define INV_ETHER_ECF	25	/* PCI Fast Ethernet */
#define INV_GFE		26	/* GIO Fast Ethernet */
#define INV_VFE		27	/* VME Fast Ethernet */
#define	INV_ETHER_GE	28	/* Gigabit Ethernet */
#define	INV_ETHER_EFP	INV_ETHER_EF	/* unused (same as IOC3 Fast Ethernet) */
#define INV_GSN_XTK1	29	/* single xtalk version of GSN */
#define INV_GSN_XTK2	30	/* dual xtalk version of GSN */
#define INV_FORE_HE	31	/* FORE HE ATM Card */
#define INV_FORE_PCA	32	/* FORE PCA ATM Card */
#define INV_FORE_VMA    33      /* FORE VMA ATM Card */
#define INV_FORE_ESA    34      /* FORE ESA ATM Card */
#define INV_FORE_GIA    35      /* FORE GIA ATM Card */

/* Types for class INV_SCSI and INV_VSCSI; The type code is the same as
 * the device type code returned by the Inquiry command, iff the Inquiry
 * command defines a type code for the device in question.  If it doesn't,
 * values over 31 will be used for the device type.
 * Note: the lun is encoded in bits 8-15 of the state.  The
 * state field low 3 bits contains the information from the inquiry
 * cmd that indicates ANSI SCSI 1,2, etc. compliance, and bit 7
 * contains the inquiry info that indicates whether the media is
 * removable.
 */
#define INV_PRINTER	2	/* SCSI printer */
#define INV_CPU		3	/* SCSI CPU device */
#define INV_WORM	4	/* write-once-read-many (e.g. optical disks) */
#define INV_CDROM	5	/* CD-ROM  */
#define INV_SCANNER	6	/* scanners */
#define INV_OPTICAL	7	/* optical disks (read-write) */
#define INV_CHANGER	8	/* jukebox's for CDROMS, for example */
#define INV_COMM	9	/* Communications device */
#define INV_STARCTLR	12	/* Storage Array Controller */
#define INV_RAIDCTLR	32	/* RAID ctlr actually gives type 0 */

/* bit definitions for state field for class INV_SCSI */
#define INV_REMOVE	0x80	/* has removable media */
#define INV_SCSI_MASK	7	/* to which ANSI SCSI standard device conforms*/

/* types for class INV_AUDIO */

#define INV_AUDIO_HDSP		0	/* Indigo DSP system */
#define INV_AUDIO_VIGRA110	1	/* ViGRA 110 audio board */
#define INV_AUDIO_VIGRA210	2	/* ViGRA 210 audio board */
#define INV_AUDIO_A2		3	/* HAL2 / Audio Module for Indigo 2 */
#define INV_AUDIO_A3		4	/* Moosehead (IP32) AD1843 codec */
#define INV_AUDIO_RAD		5	/* RAD PCI chip */

/* types for class INV_VIDEO */

#define	INV_VIDEO_LIGHT		0
#define	INV_VIDEO_VS2		1	/* MultiChannel Option */
#define	INV_VIDEO_EXPRESS	2	/* kaleidecope video */
#define	INV_VIDEO_VINO		3
#define	INV_VIDEO_VO2		4	/* Sirius Video */
#define	INV_VIDEO_INDY		5	/* Indy Video - kal vid on Newport
					  gfx on Indy */
#define	INV_VIDEO_MVP		6	/* Moosehead Video Ports */
#define	INV_VIDEO_INDY_601	7	/* Indy Video 601 */
#define	INV_VIDEO_PMUX		8	/* PALMUX video w/ PGR gfx */
#define	INV_VIDEO_MGRAS		9	/* Galileo 1.5 video */
#define	INV_VIDEO_DIVO		10	/* DIVO video */
#define	INV_VIDEO_RACER		11	/* SpeedRacer Pro Video */
#define	INV_VIDEO_EVO		12	/* EVO Personal Video */
#define INV_VIDEO_XTHD		13	/* XIO XT-HDTV video */
#define INV_VIDEO_XTDIGVID      14      /* XIO XT-HDDIGVID video */

/* states for video class INV_VIDEO_EXPRESS */

#define INV_GALILEO_REV		0xF
#define INV_GALILEO_JUNIOR	0x10
#define INV_GALILEO_INDY_CAM	0x20
#define INV_GALILEO_DBOB	0x40
#define INV_GALILEO_ELANTEC	0x80

/* states for video class VINO */

#define INV_VINO_REV		0xF
#define INV_VINO_INDY_CAM	0x10
#define INV_VINO_INDY_NOSW	0x20	/* nebulous - means s/w not installed */

/* states for video class MVP */

#define INV_MVP_REV(x)		(((x)&0x0000000f))
#define INV_MVP_REV_SW(x)	(((x)&0x000000f0)>>4)
#define INV_MVP_AV_BOARD(x)	(((x)&0x00000f00)>>8)
#define	INV_MVP_AV_REV(x)	(((x)&0x0000f000)>>12)
#define	INV_MVP_CAMERA(x)	(((x)&0x000f0000)>>16)
#define	INV_MVP_CAM_REV(x)	(((x)&0x00f00000)>>20)
#define INV_MVP_SDIINF(x)       (((x)&0x0f000000)>>24)
#define INV_MVP_SDI_REV(x)      (((x)&0xf0000000)>>28)

/* types for class INV_BUS */

#define INV_BUS_VME	0
#define INV_BUS_EISA	1
#define INV_BUS_GIO	2
#define INV_BUS_BT3_PCI	3

/* types for class INV_MISC */
#define INV_MISC_EPC_EINT	0	/* EPC external interrupts */
#define INV_MISC_PCKM		1	/* pc keyboard or mouse */
#define INV_MISC_IOC3_EINT	2	/* IOC3 external interrupts */
#define INV_MISC_OTHER		3	/* non-specific type */

/*
 * The four components below do not actually have inventory information
 * associated with the vertex. These symbols are used by grio at the 
 * moment to figure out the device type from the vertex. If these get
 * inventory structures in the future, either the type values must
 * remain the same or grio code needs to change.
 */

#define INV_XBOW        	3	/* cross bow */
#define INV_HUB         	4	/* hub */
#define INV_PCI_BRIDGE  	5	/* pci bridge */
#define INV_ROUTER		6	/* router */

/*  types for class INV_PROM */
#define INV_IO6PROM	0
#define INV_IP27PROM	1
#define INV_IP35PROM	2

/* types for class INV_COMPRESSION */

#define	INV_COSMO		0
#define	INV_INDYCOMP		1
#define	INV_IMPACTCOMP		2	/* cosmo2, aka impact compression */
#define	INV_VICE		3 	/* Video imaging & compression engine */

/* types for class INV_DISPLAY */
#define INV_PRESENTER_BOARD	0       /* Indy Presenter adapter board */
#define INV_PRESENTER_PANEL	1       /* Indy Presenter board and panel */
#define INV_ICO_BOARD		2	/* IMPACT channel option board */
#define INV_DCD_BOARD		3	/* O2 dual channel option board */
#define INV_7of9_BOARD          4       /* 7of9 flatpanel adapter board */
#define INV_7of9_PANEL          5       /* 7of9 flatpanel board and panel */

/* types for class INV_IEEE1394 */
#define INV_OHCI	0	/* Ohci IEEE1394 pci card */

/* state for class INV_IEEE1394 & type INV_OHCI */
#define INV_IEEE1394_STATE_TI_REV_1 0

/* O2 DVLink 1.1 controller static info */
#define INV_IEEE1394_CTLR_O2_DVLINK_11 0x8009104c

/* types for class INV_TPU */
#define	INV_TPU_EXT		0	/* External XIO Tensor Processing Unit */
#define	INV_TPU_XIO		1	/* Internal XIO Tensor Processing Unit */

/*
 * USB Types.  The upper 8 bits contain general usb device class and are used to
 * qualify the lower 8 bits which contain device type within a usb class.
 * Use USB_INV_DEVCLASS and USB_INV_DEVTYPE to to decode an i_type, and
 * USB_INV_TYPE to set it.
 */

#define USB_INV_DEVCLASS(invtype)	((invtype) >> 8)
#define USB_INV_DEVTYPE(invtype)	((invtype) & 0xf)
#define USB_INV_TYPE(usbclass, usbtype)	(((usbclass) << 8) | (usbtype))

/*
 * USB device classes.  These classes might not match the classes as defined
 * by the usb spec, but where possible we will try.
 */

#define USB_INV_CLASS_RH	0x00	/* root hub (ie. controller) */
#define USB_INV_CLASS_HID	0x03	/* human interface device */
#define USB_INV_CLASS_HUB	0x09	/* hub device */

/*
 * USB device types within a class.  These will not match USB device types,
 * as the usb is not consistent on how specific types are defined (sometimes
 * they are found in the interface subclass, sometimes (as in HID devices) they
 * are found within data generated by the device (hid report descriptors for
 * example).
 */

/*
 * RH types
 */

#define USB_INV_RH_OHCI		0x01	/* ohci root hub */

/*
 * HID types
 */

#define USB_INV_HID_KEYBOARD	0x01	/* kbd (HID class) */
#define USB_INV_HID_MOUSE	0x02	/* mouse (HID class) */

/*
 * HUB types - none yet
 */

typedef struct invent_generic_s {
	unsigned short	ig_module;
	unsigned short	ig_slot;
	unsigned char	ig_flag;
	int	ig_invclass;
} invent_generic_t;

#define INVENT_ENABLED	0x1

typedef struct invent_membnkinfo {
	unsigned short	imb_size;	/* bank size in MB */
	unsigned short	imb_attr;	/* Mem attributes */
	unsigned int	imb_flag;	/* bank flags */
} invent_membnkinfo_t;


typedef struct invent_meminfo {
	invent_generic_t 	im_gen;
	unsigned short	im_size;	/* memory size     */
	unsigned short	im_banks;	/* number of banks */
	/*
	 * declare an array with one element. Each platform is expected to
	 * allocate the size required based on the number of banks and set
	 * the im_banks correctly for this array traversal.
	 */
	invent_membnkinfo_t im_bank_info[1]; 
} invent_meminfo_t;

#define INV_MEM_PREMIUM	 0x01

typedef struct invent_cpuinfo {
	invent_generic_t ic_gen;
	cpu_inv_t     ic_cpu_info;
	unsigned short	ic_cpuid;
	unsigned short	ic_slice;
	unsigned short  ic_cpumode;

} invent_cpuinfo_t;

typedef struct invent_rpsinfo {
	invent_generic_t ir_gen;
	int 		 ir_xbox;	/* is RPS connected to an xbox */
} invent_rpsinfo_t;

typedef struct invent_miscinfo {
	invent_generic_t im_gen;
	int       	 im_rev;
	int		 im_version;
	int	         im_type;
	uint64_t	 im_speed;
} invent_miscinfo_t;


typedef struct invent_routerinfo{
         invent_generic_t im_gen;
         router_inv_t     rip;
} invent_routerinfo_t;



#ifdef __KERNEL__

typedef struct invplace_s {
	vertex_hdl_t		invplace_vhdl;		/* current vertex */
	vertex_hdl_t		invplace_vplace;	/* place in vertex list */
	inventory_t		*invplace_inv;		/* place in inv list on vertex */
} invplace_t; /* Magic cookie placeholder in inventory list */

extern invplace_t invplace_none;
#define INVPLACE_NONE invplace_none

extern void	    add_to_inventory(int, int, int, int, int);
extern void	    replace_in_inventory(inventory_t *, int, int, int, int, int);
extern void         start_scan_inventory(invplace_t *);
extern inventory_t  *get_next_inventory(invplace_t *);
extern void         end_scan_inventory(invplace_t *);
extern inventory_t  *find_inventory(inventory_t *, int, int, int, int, int);
extern int	    scaninvent(int (*)(inventory_t *, void *), void *);
extern int	    get_sizeof_inventory(int);

extern void device_inventory_add(	vertex_hdl_t device, 
					int class, 
					int type, 
					major_t ctlr, 
					minor_t unit, 
					int state);


extern inventory_t *device_inventory_get_next(	vertex_hdl_t device,
						invplace_t *);

extern void device_controller_num_set(	vertex_hdl_t,
					int);
extern int device_controller_num_get(	vertex_hdl_t);
#endif /* __KERNEL__ */
#endif /* _ASM_IA64_SN_INVENT_H */
