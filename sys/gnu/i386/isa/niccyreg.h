/* @(#)$Id: niccyreg.h,v 1.1 1995/02/14 15:00:19 jkh Exp $
 *******************************************************************************
 *  II - Version 0.1 $Revision: 1.1 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: niccyreg.h,v $
 * Revision 1.1  1995/02/14 15:00:19  jkh
 * An ISDN driver that supports the EDSS1 and the 1TR6 ISDN interfaces.
 * EDSS1 is the "Euro-ISDN", 1TR6 is the soon obsolete german ISDN Interface.
 * Obtained from: Dietmar Friede <dfriede@drnhh.neuhaus.de> and
 * 	Juergen Krause <jkr@saarlink.de>
 *
 * This is only one part - the rest to follow in a couple of hours.
 * This part is a benign import, since it doesn't affect anything else.
 *
 *
 ******************************************************************************/

/*
 * This file defines the NICCY 3008 Interface.
 * Copyright Dr. Neuhaus GmbH, Hamburg and Dietmar Friede
 *
*/

#define MBX_MU 0
#define MBX_MD 1
#define MBX_DU 2
#define MBX_DD 3
#define MBX_B1U 4
#define MBX_B1D 5
#define MBX_B2U 6
#define MBX_B2D 7

#define MBX_xU 0x55
#define MBX_xD 0xAA

/* -------------------------------------------------------------------- */

#define MU_INIT_CNF 0x00
#define MU_INIT_IND 0x01
#define MU_RESET_CNF 0x02
#define MU_HANDSET_IND 0x03
#define MU_DNL_MOD_CNF 0x04
/* reserved: 0x05 */
#define MU_DNL_MOD_IND 0x06
#define MU_DISC_MOD_CNF 0x07
#define MU_LIST_MOD_CNF 0x08
#define MU_LIST_MOD_DATA 0x09
/* reserved: 0x0A to 0x0B */
#define MU_HW_CONFIG_CNF 0x0C
#define MU_HW_ID_CNF 0x0D
#define MU_SET_CLOCK_CNF 0x0E
#define MU_GET_CLOCK_CNF 0x0F
#define MU_ACT_IA_CNF 0x10
#define MU_ACT_IA_IND 0x11
#define MU_DEACT_IA_CNF 0x12
#define MU_DEACT_IA_IND 0x13
#define MU_POLL_CNF 0x14
#define MU_POLL_IND 0x15
/* reserved: 0x16 to 0x1D */
#define MU_MANUFACT_CNF 0x1E
#define MU_MANUFACT_IND 0x1F

/*---------------------------------------------------------------------------*/

#define MD_INIT_REQ 0x20
#define MD_INIT_RSP 0x21
#define MD_RESET_REQ 0x22
#define MD_HANDSET_RSP 0x23
#define MD_DNL_MOD_REQ 0x24
#define MD_DNL_MOD_DATA 0x25
#define MD_DNL_MOD_RSP 0x26
#define MD_DISC_MOD_REQ 0x27
#define MD_LIST_MOD_REQ 0x28
/* reserved: 0x29 to 0x2B */
#define MD_HW_CONFIG_REQ 0x2C
#define MD_HW_ID_REQ 0x2D
#define MD_SET_CLOCK_REQ 0x2E
#define MD_GET_CLOCK_REQ 0x2F
#define MD_ACT_IA_REQ 0x30
#define MD_ACT_IA_RSP 0x31
#define MD_DEACT_IA_REQ 0x32
#define MD_DEACT_IA_RSP 0x33
#define MD_POLL_REQ 0x34
#define MD_POLL_RSP 0x35
#define MD_STATE_IND 0x37
#define MD_MANUFACT_REQ 0x3E
#define MD_MANUFACT_RSP 0x3F

/*---------------------------------------------------------------------------*/

#define DU_CONN_CNF 0x40
#define DU_CONN_IND 0x41
#define DU_CONN_ACT_IND 0x42
#define DU_DISC_CNF 0x43
#define DU_DISC_IND 0x44
#define DU_DATA_CNF 0x45
#define DU_DATA_IND 0x46
#define DU_LISTEN_CNF 0x47
#define DU_GET_PAR_CNF 0x48
#define DU_INFO_CNF 0x49
#define DU_INFO_IND 0x4A
#define DU_CONN_INFO_CNF 0x4B
#define DU_REL_PLCI_CNF 0x4C
/* reserved: 0x4C to 0x5E */
#define DU_STR_NOT_COMP 0x5F

/*---------------------------------------------------------------------------*/

#define DD_CONN_REQ 0x60
#define DD_CONN_RSP 0x61
#define DD_CONN_ACT_RSP 0x62
#define DD_DISC_REQ 0x63
#define DD_DISC_RSP 0x64
#define DD_DATA_REQ 0x65
#define DD_DATA_RSP 0x66
#define DD_LISTEN_REQ 0x67
#define DD_GET_PAR_REQ 0x68
#define DD_INFO_REQ 0x69
#define DD_INFO_RSP 0x6A
#define DD_CONN_INFO_REQ 0x6B
#define DD_REL_PLCI_REQ 0x6C

/*---------------------------------------------------------------------------*/

#define BD_SEL_PROT_REQ 0xA0
#define BD_LIST_B3_REQ 0xA1
#define BD_CONN_B3_REQ 0xA2
#define BD_CONN_B3_RSP 0xA3
#define BD_C_B3_ACT_RSP 0xA4
#define BD_DISC_B3_REQ 0xA5
#define BD_DISC_B3_RSP 0xA6
#define BD_GET_P_B3_REQ 0xA7
#define BD_DATA_B3_REQ 0xA8
#define BD_DATA_B3_RSP 0xA9
#define BD_RESET_B3_REQ 0xAA
#define BD_RESET_B3_RSP 0xAB

/*---------------------------------------------------------------------------*/


#define	NICCY_DEBUG	_IOWR('N',1,dbg_type)
#define	NICCY_RESET	_IOWR('N',2,int)
#define	NICCY_LOAD	_IOWR('N',3,struct head)
#define	NICCY_SET_CLOCK _IOWR('N',4,time_str_t)
#define	NICCY_SPY	_IOWR('N',5,int)

struct head
{
	u_long          len;
	u_long          sig;
	char            nam[8];
	char            ver[5];
	u_char          typ;
	u_short		status;
	u_long		d_len;
	u_char		*data;
};

typedef char time_str_t[14];
typedef u_char dbg_type[10000];
