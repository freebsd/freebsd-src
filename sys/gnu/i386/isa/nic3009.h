/* @(#)$Id: nic3009.h,v 1.1 1995/02/14 15:00:16 jkh Exp $
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
 * $Log: nic3009.h,v $
 * Revision 1.1  1995/02/14 15:00:16  jkh
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
 * This file defines the NICCY 3009 Interface. Copyright Dr. Neuhaus GmbH,
 * Hamburg and Dietmar Friede
 *
 */

#define NO_MORE_DATA            0x00	/* der Message folgen keine Daten    */
#define MORE_DATA               0x01	/* der Message folgen weitere Daten  */

#define	DPR_LEN			0x800	/* 2 kBytes gross            */
#define	DPR_VAR_AREA_LEN	0x100	/* davon fuer allg. Variablen */

#define	DPR_MBX_LEN		(DPR_LEN-DPR_VAR_AREA_LEN)/2	/* 0x380 lang */
#define	DPR_MBX_FLAG_LEN	2	/* zwei Bytes MBX-Zustand... */
#define	DPR_MSG_HDR_LEN		10	/* Msg-Laenge ohne Datafield */
#define	DATAFIELD_LEN		(DPR_MBX_LEN-DPR_MBX_FLAG_LEN-DPR_MSG_HDR_LEN)
#define MAX_B3_LEN              (2048+2)	/* Daten und Network-Header  */

#pragma pack (1)
typedef struct
{
	u_char          msg_flag;	/* Signalisierung NICCY / PC */
	u_char          progress;	/* NICCY-interne Verwendung ! */
	u_char          type;
	u_char          subtype;
	u_short         number;
	u_char          more_data;
	u_char          reserved;
	u_short         data_len;
	u_short         plci;
	u_char          data[DATAFIELD_LEN];
}               mbx_type;

typedef struct
{
	mbx_type        up_mbx;	/* Offset 0x000-0x37F */
	mbx_type        dn_mbx;	/* Offset 0x380-0x6FF */
	u_char          card_number;	/* Offset 0x700      */
	u_char          card_state;	/* Offset 0x701      */
	u_short         mainloop_cnt;	/* Offset 0x702-0x703 */
	u_char          watchdog_cnt;	/* Offset 0x704      */
	u_char          hw_config;	/* Offset 0x705      */
	u_char          int_flg_pc;	/* Offset 0x706      */
	u_char          int_flg_nic;	/* Offset 0x707      */
	u_char          api_area[64];	/* Offset 0x708-0x747 */
	u_char          api_active;	/* Offset 0x748      */
	u_char          tei;	/* Offset 0x749      */
	u_char          state_b1;	/* Offset 0x74A      */
	u_char          state_b2;	/* Offset 0x74B      */
	u_char          si_b1;	/* Offset 0x74C      */
	u_char          si_b2;	/* Offset 0x74D      */
	u_short         calls_in;	/* Offset 0x74E-0x74F */
	u_short         calls_out;	/* Offset 0x750-0x751 */
	u_char          ram_config;	/* Offset 0x752      */
	u_char          spv_request_flag;	/* Offset 0x753      */
	u_char          dcp_state_b1;	/* Offset 0x754      */
	u_char          dcp_state_b2;	/* Offset 0x755      */
	u_char          dc_protocol;	/* Offset 0x756      */
	u_char          poll_flag;	/* Offset 0x757      */
	u_char          debug[DPR_LEN - 0x758 - 4];	/* Offset 0x758-0x7FB */
	u_short         signal_niccy_to_pc;	/* Offset 0x7FC-0x7FD */
	u_short         signal_pc_to_niccy;	/* Offset 0x7FE-0x7FF */
}               dpr_type;
#pragma pack ()
