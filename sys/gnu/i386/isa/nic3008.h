/* @(#)$Id: nic3008.h,v 1.1 1995/02/14 15:00:12 jkh Exp $
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
 * $Log: nic3008.h,v $
 * Revision 1.1  1995/02/14 15:00:12  jkh
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

#pragma pack (1)

typedef struct {
	u_short type;		/* Message Subtype/Type */
	u_char source;
	u_char dest;
	u_short number;		/* laufende Nachrichten-Nummer */
	u_short timeoutval;	/* Wert fÅr Timeout */
	u_char priority;	/* Nachrichten-PrioritÑt */
	u_char more_data;	/* Nachricht vollstÑndig? */
	u_short data_len;		/* Datenmenge */
	u_char buf_valid;	/* im aux. buf.? */
	u_char reserved[1];
	u_short add_info;		/* Maske */
	u_char data[0x30];/* Datenfeld */
} mbx_type;

/* ------------------------------------------------------------------------ */

typedef struct {
/* Offset 0x0000 ---------------------------------------------------------- */
	u_char msg_flg[8];	/* Messages in MBX i */
	u_char card_number;	/* Kartennummer of Applikation */
	u_char card_state;	/* Kartenstatus */
	u_short mainloop_cnt;	/* NICCY's M'loop,68000-Notation*/
	u_char watchdog_cnt;	/* Applikation "lebt"? */
	u_char hw_config;	/* Steckmodule? */
	u_char jmp_config;	/* Karten-Jumper? */
	u_char ram_config;	/* Karten-Speicher? */
/* Offset 0x0010 -----------------------------------------------------------*/
	char niccy_ver[0x0E];	/* "NICCY V x.yyy\c" */
	u_char int_flg_pc;	/* Will PC Interrupts? */
	u_char int_flg_nic;	/* Will NICCY Interrupts? */
/* Offset 0x0020 -----------------------------------------------------------*/
	u_short buf_ptr[8];	/* Pointer to aux. buf. ... */
/* Offset 0x0030 -----------------------------------------------------------*/
	u_short buf_len[8];	/* Size of aux. buf. ... */
/* Offset 0x0040 -----------------------------------------------------------*/
		/* 0x40 Bytes fÅr die */
		/* frei verfÅgbar */
	u_char old_flg[8];	/* Messages in MBX i */
	u_char irq_level;	/* welcher IRQ (als Bitmaske */
	u_char res[7];		/* FREI */
/* Offset 0x0050 -----------------------------------------------------------*/
	u_char api_area_int_nr;	/*SW-Int des API wenn API_ACTIVE*/
	u_char api_area_PLCI[2];	/* PLCI wÑhrend ApiManufacturer */
	u_char capi_version[6];	/* Versionsnummer der CAPI */
	u_char api_area[0x27];	/* FREI */
/* Offset 0x0080 -----------------------------------------------------------*/
	u_char api_active;	/* Flag ob CAPI aktiv ist */
	u_char ext_hw_config;	/* Bit 0: UART 16550 */
		/* Bit 1..7: reserved */
	u_char dpr_hw_id[0x0E];	/* Hardware ID */
/* Offset 0x0090 -----------------------------------------------------------*/
	u_char	 dpr_listen_req;/* Anzahl Listen Request's */
	u_char dpr_state_b1;	/* state B1 channel */
		/* 0x00 : channel ist frei */
		/* 0x01 : Verbindungsaufb. Req */
		/* 0x02 : Verbindungsaufb. Act */
		/* 0x03 : Verbindung besteht */
		/* 0x04 : eintreffender Ruf */
		/* 0x05 : Verbindung angenommen */
		/* 0x06 : Verbindungsabb. Req */
		/* 0x07 : Verbindungsabb. laeuft*/
		/* 0x08 : Verbindung getrennt */
	u_char dpr_state_b2; 	/* state B2 channel (siehe oben)*/
	u_char dpr_state_ic1;	/* state of Intercomm-Channel */
	u_char dpr_state_ic2;	/* ----------- " -------------- */
	u_char state_res[0x04];
	u_char dpr_si_b1; 	/* Service Indicator auf B1 */
	u_char dpr_si_b2; 	/* Service Indicator auf B2 */
	u_char dpr_state_res_0[0x05];
/* Offset 0x00A0 -----------------------------------------------------------*/
	u_char dpr_state_hscx;	/* state of HSCX */
	u_char dpr_state_itac; 	/* state of ITAC */
	u_char dpr_state_arcofi;/* state of ARCOFI */
	u_char dpr_state_modem;	/* state of Aufsteckmodem */
	u_char dpr_state_com; 	/* state of COM */
	u_char dpr_state_res[0x0B];
/* Offset 0x00B0 -----------------------------------------------------------*/
	u_char dpr_state_ia_tel;/* state of internal Appl. */
	u_char dpr_state_ia_com;/* state of internal Appl. */
	u_char dpr_state_ia_mod;/* state of internal Appl. */
	u_char dpr_state_res_1[0x0D];
/* Offset 0x00C0 -----------------------------------------------------------*/
	u_char dpr_state_dcp[0x10];/* state of D-channel Prot */
/* Offset 0x00D0 -----------------------------------------------------------*/
	u_char reserved[0x130];
/* Offset 0x0200 -----------------------------------------------------------*/
	mbx_type dpr_mbx[8];	/* the mailboxes ... */
} dpr_type;

#pragma pack ()
