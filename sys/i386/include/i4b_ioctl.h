/*
 * Copyright (c) 1997, 2001 Hellmuth Michaelis. All rights reserved.
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
 *---------------------------------------------------------------------------
 *
 *	i4b_ioctl.h - messages kernel <--> userland
 *	-------------------------------------------
 *
 * $FreeBSD$
 *
 *      last edit-date: [Sun Dec 16 15:09:12 2001]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_IOCTL_H_
#define _I4B_IOCTL_H_

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#ifndef _MACHINE_TYPES_H_
#include <machine/types.h>
#endif /* _MACHINE_TYPES_H_ */
#endif /* __FreeBSD__ */

/*---------------------------------------------------------------------------*
 *	version and release number for isdn4bsd package
 *---------------------------------------------------------------------------*/
#define	VERSION		1		/* version number	*/
#define	REL		1		/* release number	*/
#define STEP		1		/* release step		*/

/*---------------------------------------------------------------------------*
 * date/time format in i4b log messages
 * ------------------------------------
 * Being year 2000 clean is not easy with the current state of the
 * ANSI C library standard and it's implementation for some locales.
 * You might like to use the "%c" format of "strftime" sometimes,
 * but this breaks Y2K in some locales. Also the old standard logfile
 * format "%d.%m.%y %H:%M:%S" is non compliant.
 * NetBSD's current toolset warns about this problems, and we compile
 * with -Werror, so this problems need to be resolved.
 *---------------------------------------------------------------------------*/
#define I4B_TIME_FORMAT	"%d.%m.%Y %H:%M:%S"

/*---------------------------------------------------------------------------*
 *	max number of controllers in system
 *---------------------------------------------------------------------------*/
#define	MAX_CONTROLLERS	8		/* max number of controllers	*/

/*---------------------------------------------------------------------------*
 *	ISDN D-channel protocols 
 *---------------------------------------------------------------------------*/
#define PROTOCOL_DSS1	0		/* default, Euro-ISDN/DSS1 */
#define PROTOCOL_D64S	1		/* 64k leased line, no protocol */

/*---------------------------------------------------------------------------*
 *	controller types
 *---------------------------------------------------------------------------*/
#define CTRL_INVALID	(-1)		/* invalid, error		*/
#define CTRL_UNKNOWN	0		/* unknown controller type	*/
#define CTRL_PASSIVE	1		/* passive ISDN controller cards*/
#define CTRL_DAIC	2		/* Diehl active controller cards*/
#define CTRL_TINADD	3		/* Stollmann Tina-dd active card*/
#define CTRL_AVMB1	4		/* AVM B1 active card		*/
#define CTRL_CAPI       5               /* cards seen via the CAPI layer*/
#define	CTRL_NUMTYPES	6		/* number of controller types	*/

/*---------------------------------------------------------------------------*
 *	CTRL_PASSIVE: driver types
 *---------------------------------------------------------------------------*/
#define MAXL1UNITS      8		/* max number of units	*/

#define L1DRVR_ISIC     0		/* isic - driver	*/
#define L1DRVR_IWIC     1		/* iwic - driver	*/
#define L1DRVR_IFPI     2		/* ifpi - driver	*/
#define L1DRVR_IHFC     3		/* ihfc - driver	*/
#define L1DRVR_IFPNP    4		/* ifpnp - driver	*/
#define L1DRVR_ICCHP	5		/* icchp - driver	*/
#define L1DRVR_ITJC     6		/* itjc - driver	*/
#define L1DRVR_IFPI2    7		/* ifpi2 - driver        */

/* MAXL1DRVR MUST be updated when more passive drivers are added !!! */
#define MAXL1DRVR       (L1DRVR_IFPI2 + 1)

/*---------------------------------------------------------------------------*
 *	card types for CTRL_PASSIVE 
 *---------------------------------------------------------------------------*/
#define CARD_TYPEP_INVAL	(-1)	/* invalid, error		*/
#define CARD_TYPEP_UNK		0	/* unknown			*/
#define CARD_TYPEP_8		1	/* Teles, S0/8 			*/
#define CARD_TYPEP_16		2	/* Teles, S0/16			*/
#define CARD_TYPEP_16_3		3	/* Teles, S0/16.3		*/
#define CARD_TYPEP_AVMA1	4	/* AVM A1 or AVM Fritz!Card	*/
#define CARD_TYPEP_163P		5	/* Teles, S0/16.3 PnP		*/
#define CARD_TYPEP_CS0P		6	/* Creatix, S0 PnP		*/
#define CARD_TYPEP_USRTA	7	/* US Robotics ISDN TA internal	*/
#define CARD_TYPEP_DRNNGO	8	/* Dr. Neuhaus Niccy GO@	*/
#define CARD_TYPEP_SWS		9	/* Sedlbauer Win Speed		*/
#define CARD_TYPEP_DYNALINK	10	/* Dynalink IS64PH		*/
#define CARD_TYPEP_BLMASTER	11	/* ISDN Blaster / ISDN Master	*/
#define	CARD_TYPEP_PCFRITZ	12	/* AVM PCMCIA Fritz!Card	*/
#define CARD_TYPEP_ELSAQS1ISA	13	/* ELSA QuickStep 1000pro ISA	*/
#define CARD_TYPEP_ELSAQS1PCI	14	/* ELSA QuickStep 1000pro PCI	*/
#define CARD_TYPEP_SIEMENSITALK	15	/* Siemens I-Talk		*/
#define	CARD_TYPEP_ELSAMLIMC	16	/* ELSA MicroLink ISDN/MC	*/
#define	CARD_TYPEP_ELSAMLMCALL	17	/* ELSA MicroLink MCall		*/
#define	CARD_TYPEP_ITKIX1	18	/* ITK ix1 micro 		*/
#define CARD_TYPEP_AVMA1PCI	19	/* AVM FRITZ!CARD PCI		*/
#define CARD_TYPEP_PCC16	20	/* ELSA PCC-16			*/
#define CARD_TYPEP_AVM_PNP	21	/* AVM FRITZ!CARD PnP		*/
#define CARD_TYPEP_SIE_ISURF2 	22	/* Siemens I-Surf 2 PnP		*/
#define CARD_TYPEP_ASUSCOMIPAC	23	/* Asuscom ISDNlink 128 K PnP	*/
#define CARD_TYPEP_WINB6692	24	/* Winbond W6692 based		*/
#define CARD_TYPEP_16_3C	25	/* Teles S0/16.3c PnP (HFC-S/SP	*/
#define CARD_TYPEP_ACERP10	26	/* Acer ISDN P10 (HFC-S)	*/
#define CARD_TYPEP_TELEINT_NO_1	27	/* TELEINT ISDN SPEED No. 1 (HFC-1) */
#define CARD_TYPEP_CCD_HFCS_PCI	28	/* Cologne Chip HFC-S PCI based	*/
#define	CARD_TYPEP_NETJET_S	29	/* Traverse NetJet-S (Tiger300) */
#define	CARD_TYPEP_DIVA_ISA	30	/* Eicon DIVA ISA PnP 2.0 or 2.02 */
#define CARD_TYPEP_COMPAQ_M610	31	/* Compaq Microcom 610 		*/
#define CARD_TYPEP_AVMA1PCI_V2	32      /* AVM FRITZ!CARD PCI Ver. 2    */
/*
 * in case you add support for more cards, please update:
 *
 *	isdnd:		controller.c, name_of_controller()
 *
 * and adjust CARD_TYPEP_MAX below.
 */

#define CARD_TYPEP_MAX		32	/* max type */

/*---------------------------------------------------------------------------*
 *	card types for CTRL_DAIC
 *---------------------------------------------------------------------------*/
#define CARD_TYPEA_DAIC_UNK	0
#define	CARD_TYPEA_DAIC_S	1
#define	CARD_TYPEA_DAIC_SX	2
#define	CARD_TYPEA_DAIC_SCOM	3
#define	CARD_TYPEA_DAIC_QUAD	4

/*---------------------------------------------------------------------------*
 *	card types for CTRL_CAPI
 *---------------------------------------------------------------------------*/
#define CARD_TYPEC_CAPI_UNK	0
#define	CARD_TYPEC_AVM_T1_PCI	1
#define CARD_TYPEC_AVM_B1_PCI	2
#define CARD_TYPEC_AVM_B1_ISA	3

/*---------------------------------------------------------------------------*
 *	max length of some strings
 *---------------------------------------------------------------------------*/
#define TELNO_MAX	41  /* max length of a telephone number (+ '\0')  */
#define DISPLAY_MAX	91  /* max length of display information (+ '\0') */
#define DATETIME_MAX	21  /* max length of datetime information (+ '\0')*/
#define KEYPAD_MAX	35  /* max length of a keypad string (+ '\0')     */

/*---------------------------------------------------------------------------*
 *	in case the src or dst telephone number is empty
 *---------------------------------------------------------------------------*/
#define TELNO_EMPTY	"NotAvailable"	

/*---------------------------------------------------------------------------*
 *	B channel parameters
 *---------------------------------------------------------------------------*/
#define BCH_MAX_DATALEN	2048	/* max length of a B channel frame */

/*---------------------------------------------------------------------------*
 * userland driver types
 * ---------------------
 * a "driver" is defined here as a piece of software interfacing an 
 * ISDN B channel with a userland service, such as IP, Telephony etc.
 *---------------------------------------------------------------------------*/
#define BDRV_RBCH	0       /* raw b-channel interface driver       */
#define BDRV_TEL	1       /* telephone (speech) interface driver  */
#define BDRV_IPR	2       /* IP over raw HDLC interface driver    */
#define BDRV_ISPPP	3       /* sync Kernel PPP interface driver     */
#define BDRV_IBC	4       /* BSD/OS point to point driver		*/
#define BDRV_ING	5       /* NetGraph ing driver			*/

/*---------------------------------------------------------------------------*
 * B channel protocol
 *---------------------------------------------------------------------------*/
#define BPROT_NONE	0	/* no protocol at all, raw data		*/
#define BPROT_RHDLC	1	/* raw HDLC: flag, data, crc, flag	*/

/*---------------------------------------------------------------------------*
 * causes data type
 *---------------------------------------------------------------------------*/
typedef	unsigned int cause_t;		/* 32 bit unsigned int	*/

/*---------------------------------------------------------------------------*
 * call descriptor id (cdid) definitions
 *---------------------------------------------------------------------------*/
#define CDID_UNUSED	0	/* cdid is invalid and unused		*/
#define CDID_MAX	99999	/* highest valid cdid, wraparound to 1	*/

/*---------------------------------------------------------------------------*
 *	The shorthold algorithm to use
 *---------------------------------------------------------------------------*/
#define SHA_FIXU	0    /* timeout algorithm for fix unit charging */
#define SHA_VARU	1    /* timeout algorithm for variable unit charging */

/*---------------------------------------------------------------------------*
 *	The shorthold data struct
 *---------------------------------------------------------------------------*/
typedef struct {
	int	shorthold_algorithm;	/* shorthold algorithm to use	*/
	int	unitlen_time;		/* length of a charging unit	*/
	int	idle_time;		/* time without activity on b ch*/
	int	earlyhup_time;		/* safety area at end of unit	*/
} msg_shorthold_t;


/****************************************************************************

	outgoing call:
	--------------

		userland		kernel
		--------		------

		CDID_REQ ----------------->

	            <------------------ cdid
	
		CONNECT_REQ -------------->

		    <------------------ PROCEEDING_IND (if connect req ok)

	            <------------------ CONNECT_ACTIVE_IND (if connection ok)

		or

	            <------------------ DISCONNECT_IND (if connection failed)
	            
		

	incoming call:
	--------------

		userland		kernel
		--------		------

	            <------------------ CONNECT_IND

		CONNECT_RESP ------------->

	            <------------------ CONNECT_ACTIVE_IND (if accepted)



	active disconnect:
	------------------

		userland		kernel
		--------		------

		DISCONNECT_REQ ------------>

	            <------------------ DISCONNECT_IND
	            

	passive disconnect:
	-------------------

		userland		kernel
		--------		------

	            <------------------ DISCONNECT_IND
	            

****************************************************************************/


/*===========================================================================*
 *===========================================================================*
 *	"read" messages from kernel -> userland
 *===========================================================================* 
 *===========================================================================*/

 
/*---------------------------------------------------------------------------*
 *	message header, included in every message
 *---------------------------------------------------------------------------*/
typedef struct {
	char		type;		/* message identifier		*/
#define MSG_CONNECT_IND		'a'
#define MSG_CONNECT_ACTIVE_IND	'b'
#define MSG_DISCONNECT_IND	'c'
#define MSG_DIALOUT_IND		'd'
#define MSG_IDLE_TIMEOUT_IND	'e'
#define MSG_ACCT_IND		'f'
#define MSG_CHARGING_IND	'g'
#define MSG_PROCEEDING_IND	'h'
#define MSG_ALERT_IND		'i'
#define MSG_DRVRDISC_REQ	'j'
#define MSG_L12STAT_IND		'k'
#define MSG_TEIASG_IND		'l'
#define MSG_PDEACT_IND		'm'
#define	MSG_NEGCOMP_IND		'n'
#define	MSG_IFSTATE_CHANGED_IND	'o'
#define MSG_DIALOUTNUMBER_IND	'p'
#define MSG_PACKET_IND		'q'
#define MSG_KEYPAD_IND		'r'
	int		cdid;		/* call descriptor id		*/
} msg_hdr_t;

/*---------------------------------------------------------------------------*
 *	connect indication
 *		indicates incoming connection
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header		*/
	int		controller;	/* controller number		*/
	int		channel;	/* channel number		*/
#define  CHAN_B1  0		/* this _must_ be 0, HSCX B1 is also 0	*/
#define  CHAN_B2  1		/* this _must_ be 1, HSCX B2 is also 1	*/
#define  CHAN_ANY (-1)		/* outgoing, not possible for incoming	*/
#define  CHAN_NO  (-2)		/* call waiting (CW) for incoming	*/
	int		bprot;	/* b channel protocot, see BPROT_XXX	*/
	char		dst_telno[TELNO_MAX];	/* destination telno	*/
	char		src_telno[TELNO_MAX];	/* source telno		*/
	int		scr_ind;/* screening indicator			*/
#define  SCR_NONE     0		/* no screening indicator transmitted	*/
#define  SCR_USR_NOSC 1		/* screening user provided, not screened*/
#define  SCR_USR_PASS 2		/* screening user provided, verified & passed */
#define  SCR_USR_FAIL 3		/* screening user provided, verified & failed */
#define  SCR_NET      4		/* screening network provided		*/
	int		prs_ind;/* presentation indicator		*/
#define  PRS_NONE     0		/* no presentation indicator transmitted*/
#define  PRS_ALLOWED  1		/* presentation allowed			*/
#define  PRS_RESTRICT 2		/* presentation restricted		*/
#define  PRS_NNINTERW 3		/* number not available due to interworking */
#define  PRS_RESERVED 4		/* reserved				*/
	char		display[DISPLAY_MAX];	/* content of display IE*/
} msg_connect_ind_t;

/*---------------------------------------------------------------------------*
 *	connect active indication
 *		indicates active connection
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header		   */
	int		controller;	/* controller number actually used */
	int		channel;	/* channel number actually used    */
	char		datetime[DATETIME_MAX];	/* content of date/time IE */
} msg_connect_active_ind_t;

/*---------------------------------------------------------------------------*
 *	disconnect indication
 *		indicates a disconnect
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
	cause_t		cause;		/* cause code		*/
} msg_disconnect_ind_t;

/*---------------------------------------------------------------------------*
 *	negotiation complete
 *		indicates an interface is completely up & running
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
} msg_negcomplete_ind_t;

/*---------------------------------------------------------------------------*
 *	interface changes internal state
 *		indicates an interface has somehow switched its FSM
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
	int state;			/* new interface state */
} msg_ifstatechg_ind_t;

/*---------------------------------------------------------------------------*
 *	initiate a call to a remote site
 *		i.e. the IP driver got a packet and wants a connection
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
	int		driver;		/* driver type		*/
	int		driver_unit;	/* driver unit number	*/
} msg_dialout_ind_t;

/*---------------------------------------------------------------------------*
 *	dial a number
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
	int		driver;		/* driver type		*/
	int		driver_unit;	/* driver unit number	*/
	int		cmdlen;		/* length of string	*/
	char		cmd[TELNO_MAX];	/* the number to dial	*/	
} msg_dialoutnumber_ind_t;

/*---------------------------------------------------------------------------*
 *	send keypad string
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
	int		driver;		/* driver type		*/
	int		driver_unit;	/* driver unit number	*/
	int		cmdlen;		/* length of string	*/
	char		cmd[KEYPAD_MAX];/* keypad string	*/	
} msg_keypad_ind_t;

/*---------------------------------------------------------------------------*
 *	idle timeout disconnect sent indication
 *		kernel has sent disconnect request because of b-ch idle
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
} msg_idle_timeout_ind_t;

/*---------------------------------------------------------------------------*
 *	accounting information from userland interface driver to daemon
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header		*/
	int		accttype;	/* accounting type		*/
#define  ACCT_DURING 0
#define  ACCT_FINAL  1
	int		driver;		/* driver type			*/
	int		driver_unit;	/* driver unit number		*/
	int		ioutbytes;	/* ISDN # of bytes sent		*/
	int		iinbytes;	/* ISDN # of bytes received	*/
	int		outbps;		/* bytes per sec out		*/
	int		inbps;		/* bytes per sec in		*/
	int		outbytes;	/* driver # of bytes sent	*/
	int		inbytes;	/* driver # of bytes received	*/
} msg_accounting_ind_t;

/*---------------------------------------------------------------------------*
 *	charging information from isdn driver to daemon
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header		*/
	int		units;		/* number of units		*/
	int		units_type;	/* type of units info		*/
#define  CHARGE_INVALID	0	/* invalid, unknown */
#define  CHARGE_AOCD	1	/* advice of charge during call */
#define  CHARGE_AOCE	2	/* advice of charge at end of call */
#define  CHARGE_CALC	3	/* locally calculated from rates information */
} msg_charging_ind_t;

/*---------------------------------------------------------------------------*
 *	call proceeding indication
 *		indicates outgoing SETUP has been acknowleged
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header		   */
	int		controller;	/* controller number actually used */
	int		channel;	/* channel number actually used    */
} msg_proceeding_ind_t;

/*---------------------------------------------------------------------------*
 *	alert indication
 *		indicates remote user side "rings"
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header		   */
} msg_alert_ind_t;

/*---------------------------------------------------------------------------*
 *	driver requests to disconnect line
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
	int		driver;		/* driver type		*/
	int		driver_unit;	/* driver unit number	*/
} msg_drvrdisc_req_t;

/*---------------------------------------------------------------------------*
 *	connect packet logging
 *---------------------------------------------------------------------------*/

typedef struct {
	msg_hdr_t	header;		/* common header	*/
	int		driver;		/* driver type		*/
	int		driver_unit;	/* driver unit number	*/
	int		direction;	/* 0=in 1=out		*/
#define DIRECTION_IN	0		/* sending packet to remote	*/
#define DIRECTION_OUT	1		/* received packet from remote	*/
#define MAX_PACKET_LOG	40		/* space for IP and TCP header	*/
	u_int8_t	pktdata[MAX_PACKET_LOG];
} msg_packet_ind_t;

/*---------------------------------------------------------------------------*
 *	state of layer 1/2
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
	int		controller;	/* controller unit 	*/
	int		layer;		/* layer number (1/2)	*/
#define LAYER_ONE	1
#define LAYER_TWO	2
	int		state;		/* state info		*/
#define LAYER_IDLE	0
#define LAYER_ACTIVE	1
} msg_l12stat_ind_t;

/*---------------------------------------------------------------------------*
 *	TEI assignment messages
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
	int		controller;	/* controller unit 	*/
	int		tei;		/* TEI or -1 if invalid */
} msg_teiasg_ind_t;

/*---------------------------------------------------------------------------*
 *	persistent deactivation state of stack
 *---------------------------------------------------------------------------*/
typedef struct {
	msg_hdr_t	header;		/* common header	*/
	int		controller;	/* controller unit 	*/
	int		numactive;	/* number of active connections */
} msg_pdeact_ind_t;


/*===========================================================================*
 *===========================================================================*
 *	"ioctl" messages from userland -> kernel
 *===========================================================================* 
 *===========================================================================*/

/*---------------------------------------------------------------------------*
 *	request a unique cdid (to setup an outgoing call)
 *---------------------------------------------------------------------------*/
typedef struct {
	int		cdid;			/* call descriptor id	*/
} msg_cdid_req_t;
 
#define	I4B_CDID_REQ		_IOWR('4', 0, int)

/*---------------------------------------------------------------------------*
 *	connect request
 *		requests an outgoing connection
 *---------------------------------------------------------------------------*/
typedef struct {
	int		cdid;		/* call descriptor id		     */
	int		controller;	/* controller to use		     */
	int		channel;	/* channel to use		     */
	int		txdelay;	/* tx delay after connect	     */
	int		bprot;		/* b channel protocol		     */
	int		driver;		/* driver to route b channel data to */
	int		driver_unit;	/*      unit number for above driver */
	msg_shorthold_t	shorthold_data;	/* the shorthold data		     */
	int		unitlen_method;	/* how to calculate the unitlength   */
#define  ULEN_METHOD_STATIC  0	/* use unitlen_time value (see above) */
#define  ULEN_METHOD_DYNAMIC 1	/* use AOCD */	
	char		dst_telno[TELNO_MAX];	/* destination telephone no  */
	char		src_telno[TELNO_MAX];	/* source telephone number   */
	char		keypad[KEYPAD_MAX];	/* keypad string 	     */	
} msg_connect_req_t;

#define	I4B_CONNECT_REQ	_IOW('4', 1, msg_connect_req_t)

/*---------------------------------------------------------------------------*
 *	connect response
 *		this is the answer to an incoming connect indication
 *---------------------------------------------------------------------------*/
typedef struct {
	int	cdid;		/* call descriptor id			*/
	int	response;	/* what to do with incoming call	*/
#define  SETUP_RESP_DNTCRE 0	/* dont care, call is not for me	*/
#define  SETUP_RESP_REJECT 1	/* reject call				*/
#define  SETUP_RESP_ACCEPT 2	/* accept call				*/
	cause_t	cause;		/* cause for case SETUP_RESP_REJECT	*/
		/* the following are only used for SETUP_RESP_ACCEPT !! */
	int	txdelay;	/* tx delay after connect		*/
	int	bprot;		/* B chan protocol			*/
	int	driver;		/* driver to route b channel data to	*/
	int	driver_unit;	/*      unit number for above driver	*/
	int	max_idle_time;	/* max time without activity on b ch	*/	
} msg_connect_resp_t;
	
#define	I4B_CONNECT_RESP	_IOW('4', 2, msg_connect_resp_t)

/*---------------------------------------------------------------------------*
 *	disconnect request
 *		active disconnect request
 *---------------------------------------------------------------------------*/
typedef struct {
	int	cdid;		/* call descriptor id			*/
	cause_t	cause;		/* protocol independent cause		*/
} msg_discon_req_t;
	
#define	I4B_DISCONNECT_REQ	_IOW('4', 3, msg_discon_req_t)

/*---------------------------------------------------------------------------*
 *	controller info request
 *---------------------------------------------------------------------------*/
typedef struct {
	int	controller;	/* controller number			*/
	int	ncontroller;	/* number of controllers in system	*/
	int	ctrl_type;	/* controller type passive/active	*/
	int	card_type;	/* brand / version			*/
	int	tei;		/* tei controller probably has		*/
        int     nbch;           /* number of b channels provided        */
} msg_ctrl_info_req_t;
	
#define	I4B_CTRL_INFO_REQ	_IOWR('4', 4, msg_ctrl_info_req_t)

/*---------------------------------------------------------------------------*
 *	dialout response
 *		status report to driver who requested a dialout
 *---------------------------------------------------------------------------*/
typedef struct {
	int		driver;		/* driver to route b channel data to */
	int		driver_unit;	/*      unit number for above driver */
	int		stat;		/* state of dialout request	     */
#define  DSTAT_NONE	0
#define  DSTAT_TFAIL	1		/* transient failure */
#define  DSTAT_PFAIL	2		/* permanent failure */
#define  DSTAT_INONLY	3		/* no outgoing dials allowed */
	cause_t		cause;		/* exact i4b cause */
} msg_dialout_resp_t;

#define	I4B_DIALOUT_RESP	_IOW('4', 5, msg_dialout_resp_t)
	
/*---------------------------------------------------------------------------*
 *	timeout value update
 *---------------------------------------------------------------------------*/
typedef struct {
	int	cdid;		/* call descriptor id			*/
	msg_shorthold_t	shorthold_data;
} msg_timeout_upd_t;
	
#define	I4B_TIMEOUT_UPD		_IOW('4', 6, msg_timeout_upd_t)

/*---------------------------------------------------------------------------*
 *	soft enable/disable
 *---------------------------------------------------------------------------*/
typedef struct {
	int		driver;		/* driver to route b channel data to */
	int		driver_unit;	/*      unit number for above driver */
	int		updown;		/* what to do			     */
#define  SOFT_ENA	0	/* enable interface */
#define  SOFT_DIS	1	/* disable interface */
} msg_updown_ind_t;

#define	I4B_UPDOWN_IND		_IOW('4', 7, msg_updown_ind_t)

/*---------------------------------------------------------------------------*
 *	send alert request
 *---------------------------------------------------------------------------*/
typedef struct {
	int	cdid;		/* call descriptor id			*/
} msg_alert_req_t;
	
#define	I4B_ALERT_REQ		_IOW('4', 8, msg_alert_req_t)

/*---------------------------------------------------------------------------*
 *	request version and release info from kernel part
 *	(msg_vr_req_t is also used by tel & rbch drivers)
 *---------------------------------------------------------------------------*/
typedef struct {
	int	version;	/* version number */
	int	release;	/* release number */
	int	step;		/* release step number */	
} msg_vr_req_t;

#define I4B_VR_REQ              _IOR('4', 9, msg_vr_req_t)

/*---------------------------------------------------------------------------*
 *	set ISDN protocol used by a controller
 *---------------------------------------------------------------------------*/
typedef struct {
	int	controller;	/* controller number		*/
	int	protocol;	/* ISDN D-channel protocol type	*/
} msg_prot_ind_t;

#define I4B_PROT_IND		_IOW('4', 10, msg_prot_ind_t)

/*---------------------------------------------------------------------------*
 *	Protocol download to active cards
 *---------------------------------------------------------------------------*/
struct isdn_dr_prot {
	size_t bytecount;	/* length of code */
	u_int8_t *microcode;	/* pointer to microcode */
};

struct isdn_download_request {
	int controller;		/* controller number */
	int numprotos;		/* number of protocols in 'protocols' */
	struct isdn_dr_prot *protocols;
};

#define	I4B_CTRL_DOWNLOAD	_IOW('4', 100, struct isdn_download_request)

/*---------------------------------------------------------------------------*
 *	Generic diagnostic interface for active cards
 *---------------------------------------------------------------------------*/
struct isdn_diagnostic_request {
	int controller;		/* controller number */
	u_int32_t cmd;		/* diagnostic command to execute */
	size_t in_param_len;	/* length of additional input parameter */
#define I4B_ACTIVE_DIAGNOSTIC_MAXPARAMLEN	65536
	void *in_param;		/* optional input parameter */
	size_t out_param_len;	/* available output space */
	void *out_param;	/* output data goes here */
};

#define	I4B_ACTIVE_DIAGNOSTIC	_IOW('4', 102, struct isdn_diagnostic_request)

#endif /* _I4B_IOCTL_H_ */
