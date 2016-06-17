/*
 * Defines for comxhw.c
 *
 * Original authors:  Arpad Bakay <bakay.arpad@synergon.hu>,
 *                    Peter Bajan <bajan.peter@synergon.hu>,
 * Previous maintainer: Tivadar Szemethy <tiv@itc.hu>
 * Current maintainer: Gergely Madarasz <gorgo@itc.hu>
 *
 * Copyright (C) 1995-1999 ITConsult-Pro Co. <info@itc.hu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */

#define	LOCOMX_IO_EXTENT	8
#define COMX_IO_EXTENT		4
#define	HICOMX_IO_EXTENT	16

#define COMX_MAX_TX_SIZE	1600
#define COMX_MAX_RX_SIZE	2048

#define COMX_JAIL_OFFSET	0xffff
#define COMX_JAIL_VALUE		0xfe
#define	COMX_MEMORY_SIZE	65536
#define HICOMX_MEMORY_SIZE	16384
#define COMX_MEM_MIN		0xa0000
#define COMX_MEM_MAX		0xf0000

#define	COMX_DEFAULT_IO		0x360
#define	COMX_DEFAULT_IRQ	10
#define	COMX_DEFAULT_MEMADDR	0xd0000
#define	HICOMX_DEFAULT_IO	0x320
#define	HICOMX_DEFAULT_IRQ	10
#define	HICOMX_DEFAULT_MEMADDR	0xd0000
#define	LOCOMX_DEFAULT_IO	0x368
#define	LOCOMX_DEFAULT_IRQ	7

#define MAX_CHANNELNO		2

#define	COMX_CHANNEL_OFFSET	0x2000

#define COMX_ENABLE_BOARD_IT    0x40
#define COMX_BOARD_RESET       	0x20
#define COMX_ENABLE_BOARD_MEM   0x10
#define COMX_DISABLE_BOARD_MEM  0
#define COMX_DISABLE_ALL	0x00

#define HICOMX_DISABLE_ALL	0x00
#define HICOMX_ENABLE_BOARD_MEM	0x02
#define HICOMX_DISABLE_BOARD_MEM 0x0
#define HICOMX_BOARD_RESET	0x01
#define HICOMX_PRG_MEM		4
#define HICOMX_DATA_MEM		0
#define HICOMX_ID_BYTE		0x55

#define CMX_ID_BYTE		0x31
#define COMX_CLOCK_CONST	8000

#define	LINKUP_READY		3

#define	OFF_FW_L1_ID	0x01e	 /* ID bytes */
#define OFF_FW_L2_ID	0x1006
#define	FW_L1_ID_1	0xab
#define FW_L1_ID_2_COMX		0xc0
#define FW_L1_ID_2_HICOMX	0xc1
#define	FW_L2_ID_1	0xab

#define OFF_A_L2_CMD     0x130   /* command register for L2 */
#define OFF_A_L2_CMDPAR  0x131   /* command parameter byte */
#define OFF_A_L1_STATB   0x122   /* stat. block for L1 */
#define OFF_A_L1_ABOREC  0x122   /* receive ABORT counter */
#define OFF_A_L1_OVERRUN 0x123   /* receive overrun counter */
#define OFF_A_L1_CRCREC  0x124   /* CRC error counter */
#define OFF_A_L1_BUFFOVR 0x125   /* buffer overrun counter */
#define OFF_A_L1_PBUFOVR 0x126   /* priority buffer overrun counter */
#define OFF_A_L1_MODSTAT 0x127   /* current state of modem ctrl lines */
#define OFF_A_L1_STATE   0x127   /* end of stat. block for L1 */
#define OFF_A_L1_TXPC    0x128   /* Tx counter for the PC */
#define OFF_A_L1_TXZ80   0x129   /* Tx counter for the Z80 */
#define OFF_A_L1_RXPC    0x12a   /* Rx counter for the PC */
#define OFF_A_L1_RXZ80   0x12b   /* Rx counter for the Z80 */
#define OFF_A_L1_REPENA  0x12c   /* IT rep disable */
#define OFF_A_L1_CHNR    0x12d   /* L1 channel logical number */
#define OFF_A_L1_CLKINI  0x12e   /* Timer Const */
#define OFF_A_L2_LINKUP	 0x132	 /* Linkup byte */
#define OFF_A_L2_DAV	 0x134   /* Rx DAV */
#define OFF_A_L2_RxBUFP  0x136	 /* Rx buff relative to membase */
#define OFF_A_L2_TxEMPTY 0x138   /* Tx Empty */
#define OFF_A_L2_TxBUFP  0x13a   /* Tx Buf */
#define OFF_A_L2_NBUFFS	 0x144	 /* Number of buffers to fetch */

#define OFF_A_L2_SABMREC 0x164	 /* LAPB no. of SABMs received */
#define OFF_A_L2_SABMSENT 0x165	 /* LAPB no. of SABMs sent */
#define OFF_A_L2_REJREC  0x166	 /* LAPB no. of REJs received */
#define OFF_A_L2_REJSENT 0x167	 /* LAPB no. of REJs sent */
#define OFF_A_L2_FRMRREC 0x168	 /* LAPB no. of FRMRs received */
#define OFF_A_L2_FRMRSENT 0x169	 /* LAPB no. of FRMRs sent */
#define OFF_A_L2_PROTERR 0x16A	 /* LAPB no. of protocol errors rec'd */
#define OFF_A_L2_LONGREC 0x16B	 /* LAPB no. of long frames */
#define OFF_A_L2_INVNR   0x16C	 /* LAPB no. of invalid N(R)s rec'd */
#define OFF_A_L2_UNDEFFR 0x16D	 /* LAPB no. of invalid frames */

#define	OFF_A_L2_T1	0x174	 /* T1 timer */
#define	OFF_A_L2_ADDR	0x176	 /* DCE = 1, DTE = 3 */

#define	COMX_CMD_INIT	1
#define COMX_CMD_EXIT	2
#define COMX_CMD_OPEN	16
#define COMX_CMD_CLOSE	17

