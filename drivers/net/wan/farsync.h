/*
 *      FarSync X21 driver for Linux
 *
 *      Actually sync driver for X.21, V.35 and V.24 on FarSync T-series cards
 *
 *      Copyright (C) 2001 FarSite Communications Ltd.
 *      www.farsite.co.uk
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Author: R.J.Dunlop      <bob.dunlop@farsite.co.uk>
 *
 *      For the most part this file only contains structures and information
 *      that is visible to applications outside the driver. Shared memory
 *      layout etc is internal to the driver and described within farsync.c.
 *      Overlap exists in that the values used for some fields within the
 *      ioctl interface extend into the cards firmware interface so values in
 *      this file may not be changed arbitrarily.
 */

/*      What's in a name
 *
 *      The project name for this driver is Oscar. The driver is intended to be
 *      used with the FarSite T-Series cards (T2P & T4P) running in the high
 *      speed frame shifter mode. This is sometimes referred to as X.21 mode
 *      which is a complete misnomer as the card continues to support V.24 and
 *      V.35 as well as X.21.
 *
 *      A short common prefix is useful for routines within the driver to avoid
 *      conflict with other similar drivers and I chosen to use "fst_" for this
 *      purpose (FarSite T-series).
 */
#define FST_NAME                "fst"           /* In debug/info etc */
#define FST_DEV_NAME            "farsync"       /* For misc interfaces */


/*      User version number
 *
 *      This version number is incremented with each official release of the
 *      package and is a simplified number for normal user reference.
 *      Individual files are tracked by the version control system and may
 *      have individual versions (or IDs) that move much faster than the
 *      the release version as individual updates are tracked.
 */
#define FST_USER_VERSION        "0.09"


/*      Ioctl call command values
 *
 *      The first three private ioctls are used by the sync-PPP module,
 *      allowing a little room for expansion we start our numbering at 10.
 */
#define FSTWRITE        (SIOCDEVPRIVATE+10)
#define FSTCPURESET     (SIOCDEVPRIVATE+11)
#define FSTCPURELEASE   (SIOCDEVPRIVATE+12)
#define FSTGETCONF      (SIOCDEVPRIVATE+13)
#define FSTSETCONF      (SIOCDEVPRIVATE+14)


/*      FSTWRITE
 *
 *      Used to write a block of data (firmware etc) before the card is running
 */
struct fstioc_write {
        unsigned int  size;
        unsigned int  offset;
        unsigned char data[0];
};


/*      FSTCPURESET and FSTCPURELEASE
 *
 *      These take no additional data.
 *      FSTCPURESET forces the cards CPU into a reset state and holds it there.
 *      FSTCPURELEASE releases the CPU from this reset state allowing it to run,
 *      the reset vector should be setup before this ioctl is run.
 */

/*      FSTGETCONF and FSTSETCONF
 *
 *      Get and set a card/ports configuration.
 *      In order to allow selective setting of items and for the kernel to
 *      indicate a partial status response the first field "valid" is a bitmask
 *      indicating which other fields in the structure are valid.
 *      Many of the field names in this structure match those used in the
 *      firmware shared memory configuration interface and come originally from
 *      the NT header file Smc.h
 *
 *      When used with FSTGETCONF this structure should be zeroed before use.
 *      This is to allow for possible future expansion when some of the fields
 *      might be used to indicate a different (expanded) structure.
 */
struct fstioc_info {
        unsigned int   valid;           /* Bits of structure that are valid */
        unsigned int   nports;          /* Number of serial ports */
        unsigned int   type;            /* Type index of card */
        unsigned int   state;           /* State of card */
        unsigned int   index;           /* Index of port ioctl was issued on */
        unsigned int   smcFirmwareVersion;
        unsigned short lineInterface;   /* Physical interface type */
        unsigned char  proto;           /* Line protocol */
        unsigned char  internalClock;   /* 1 => internal clock, 0 => external */
        unsigned int   lineSpeed;       /* Speed in bps */
        unsigned int   v24IpSts;        /* V.24 control input status */
        unsigned int   v24OpSts;        /* V.24 control output status */
        unsigned short clockStatus;     /* lsb: 0=> present, 1=> absent */
        unsigned short cableStatus;     /* lsb: 0=> present, 1=> absent */
        unsigned short cardMode;        /* lsb: LED id mode */
        unsigned short debug;           /* Debug flags */
};

/* "valid" bitmask */
#define FSTVAL_NONE     0x00000000      /* Nothing valid (firmware not running).
                                         * Slight misnomer. In fact nports,
                                         * type, state and index will be set
                                         * based on hardware detected.
                                         */
#define FSTVAL_OMODEM   0x0000001F      /* First 5 bits correspond to the
                                         * output status bits defined for
                                         * v24OpSts
                                         */
#define FSTVAL_SPEED    0x00000020      /* internalClock, lineSpeed, clockStatus
                                         */
#define FSTVAL_CABLE    0x00000040      /* lineInterface, cableStatus */
#define FSTVAL_IMODEM   0x00000080      /* v24IpSts */
#define FSTVAL_CARD     0x00000100      /* nports, type, state, index,
                                         * smcFirmwareVersion
                                         */
#define FSTVAL_PROTO    0x00000200      /* proto */
#define FSTVAL_MODE     0x00000400      /* cardMode */
#define FSTVAL_DEBUG    0x80000000      /* debug */
#define FSTVAL_ALL      0x000007FF      /* Note: does not include DEBUG flag */

/* "type" */
#define FST_TYPE_NONE   0               /* Probably should never happen */
#define FST_TYPE_T2P    1               /* T2P X21 2 port card */
#define FST_TYPE_T4P    2               /* T4P X21 4 port card */

/* "state" */
#define FST_UNINIT      0               /* Raw uninitialised state following
                                         * system startup */
#define FST_RESET       1               /* Processor held in reset state */
#define FST_DOWNLOAD    2               /* Card being downloaded */
#define FST_STARTING    3               /* Released following download */
#define FST_RUNNING     4               /* Processor running */
#define FST_BADVERSION  5               /* Bad shared memory version detected */
#define FST_HALTED      6               /* Processor flagged a halt */
#define FST_IFAILED     7               /* Firmware issued initialisation failed
                                         * interrupt
                                         */
/* "lineInterface" */
#define V24             1
#define X21             2
#define V35             3

/* "proto" */
#define FST_HDLC        1               /* Cisco compatible HDLC */
#define FST_PPP         2               /* Sync PPP */
#define FST_MONITOR     3               /* Monitor only (raw packet reception) */
#define FST_RAW         4               /* Two way raw packets */
#define FST_GEN_HDLC    5               /* Using "Generic HDLC" module */

/* "internalClock" */
#define INTCLK          1
#define EXTCLK          0

/* "v24IpSts" bitmask */
#define IPSTS_CTS       0x00000001      /* Clear To Send (Indicate for X.21) */
#define IPSTS_INDICATE  IPSTS_CTS
#define IPSTS_DSR       0x00000002      /* Data Set Ready (T2P Port A) */
#define IPSTS_DCD       0x00000004      /* Data Carrier Detect */
#define IPSTS_RI        0x00000008      /* Ring Indicator (T2P Port A) */
#define IPSTS_TMI       0x00000010      /* Test Mode Indicator (Not Supported)*/

/* "v24OpSts" bitmask */
#define OPSTS_RTS       0x00000001      /* Request To Send (Control for X.21) */
#define OPSTS_CONTROL   OPSTS_RTS
#define OPSTS_DTR       0x00000002      /* Data Terminal Ready */
#define OPSTS_DSRS      0x00000004      /* Data Signalling Rate Select (Not
                                         * Supported) */
#define OPSTS_SS        0x00000008      /* Select Standby (Not Supported) */
#define OPSTS_LL        0x00000010      /* Maintenance Test (Not Supported) */

/* "cardMode" bitmask */
#define CARD_MODE_IDENTIFY      0x0001


/*      Debug support
 *
 *      These should only be enabled for development kernels, production code
 *      should define FST_DEBUG=0 in order to exclude the code.
 *      Setting FST_DEBUG=1 will include all the debug code but in a disabled
 *      state, use the FSTSETCONF ioctl to enable specific debug actions, or
 *      FST_DEBUG can be set to prime the debug selection.
 */
#define FST_DEBUG       0x0000
#if FST_DEBUG

extern int fst_debug_mask;              /* Bit mask of actions to debug, bits
                                         * listed below. Note: Bit 0 is used
                                         * to trigger the inclusion of this
                                         * code, without enabling any actions.
                                         */
#define DBG_INIT        0x0002          /* Card detection and initialisation */
#define DBG_OPEN        0x0004          /* Open and close sequences */
#define DBG_PCI         0x0008          /* PCI config operations */
#define DBG_IOCTL       0x0010          /* Ioctls and other config */
#define DBG_INTR        0x0020          /* Interrupt routines (be careful) */
#define DBG_TX          0x0040          /* Packet transmission */
#define DBG_RX          0x0080          /* Packet reception */
#define DBG_CMD         0x0100          /* Port command issuing */

#define DBG_ASS         0xFFFF          /* Assert like statements. Code that
                                         * should never be reached, if you see
                                         * one of these then I've been an ass
                                         */
#endif  /* FST_DEBUG */

