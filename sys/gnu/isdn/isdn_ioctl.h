static char     _isdn_ioctl_id[] = "@(#)$Id: isdn_ioctl.h,v 1.1 1995/02/14 15:00:35 jkh Exp $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.1 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: isdn_ioctl.h,v $
 * Revision 1.1  1995/02/14  15:00:35  jkh
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


#pragma pack (1)
typedef struct
{
	u_char protokoll;
	u_char length;
	u_short data_length;
	u_char link_addr_a;
	u_char link_addr_b;
	u_char modulo_mode;
	u_char window_size;
	u_char xid;
} dlpd_t;

typedef struct
{
	u_char protokoll;
	u_char length;
	u_short lic, hic, ltc, htc, loc, hoc;
	u_char modulo_mode;
}ncpd_t;

typedef struct
{
	u_char length;
	u_short lic, hic, ltc, htc, loc, hoc;
	u_char modulo_mode;
}ncpi_t;

typedef struct
{
	u_char stat;
	u_char length;
	u_char no[124];
} telno_t;

#pragma pack ()

typedef struct
{
	short appl;
	dlpd_t dlpd;
	ncpd_t ncpd;
	u_long timeout;
	u_char prot;
	int	(*PassUp)();	 /* pass data from isdn interface upstream to appl. */
	int	(*PassUpInfo)(); /* pass info from isdn interface upstream to appl. */
	int	(*PassDown)();	 /* get data from application */
	int	(*Connect)();	 /* Connect Indikation */
	int	(*DisConn)();	 /* Disconnect Indikation */
	short drivno;		 /* Number of the high level Driver */
	char ctrl;
	char typ;
	short state;
	short listen_state;
	u_long send_err;
} isdn_appl_t;

typedef struct
{
	char ctrl;
	char islisten;
	short unit;
	short appl;
	int	(*connect)();
	int	(*listen)();
	int	(*accept)();
	int	(*disconnect)();
	int	(*output)();
	int	(*state)();
	short	o_len;
	char	*o_buf;
	time_t		lastact;
	u_long send_err;
	u_long rcv_err;
} isdn_ctrl_t;

typedef struct
{
	short appl;
	dlpd_t dlpd;
	ncpd_t ncpd;
	u_long timeout;
	u_char prot;
} isdn_param;

typedef struct
{
	short appl;
	short ctrl;
	u_char b_channel;
	u_long inf_mask;
	u_char out_serv;
	u_char out_serv_add;
	u_char src_subadr;
	u_char spv;
	telno_t telno;
} dial_t;

typedef struct
{
	short appl;
	short ctrl;
	u_long inf_mask;
	u_short subadr_mask;
	u_short si_mask;
} listen_t;

#define ISBUSY(x)	(((x) & 0x80) == 0)
#define ISFREE(x)	(((x) & 0x80) == 0x80)
#define TELNO_VALID	1
#define	TELNO_PROMISC	2

#define N_ISDN_CTRL	2

#define	 ISDN_DIAL		_IOWR('I',1,dial_t)
#define	 ISDN_LISTEN		_IOWR('I',2,listen_t)
#define	 ISDN_ACCEPT		_IOWR('I',3,int)
#define	 ISDN_HANGUP		_IOWR('I',4,int)
#define	 ISDN_SET_PARAM		_IOWR('I',8,isdn_param)
#define	 ISDN_GET_PARAM		_IOWR('I',9,isdn_param)

#ifdef KERNEL

/* XXX should be elsewhere. */

/* From isdn.c. */
void	isdn_accept_con_ind __P((int an, int cn, char serv, char serv_add,
				 char subadr, char nl, char *num));
void	isdn_conn_ind __P((int an, int cn, int dial));
int	isdn_ctrl_attach __P((int n));
void	isdn_disconn_ind __P((int an));
void	isdn_disconnect __P((int an, int rea));
void	isdn_info __P((int an, int typ, int len, char *data));
int	isdn_input __P((int an, int len, char *buf, int dir));
int	isdn_msg __P((int an));
int	isdn_output __P((int an));

/* From if_ii.c. */
int	iiattach __P((int ap));

/* From iispy.c. */
int	ispyattach __P((int ap));

/* From iitel.c. */
int	itelattach __P((int ap));

/* From iitty.c. */
int	ityattach __P((int ap));

#endif /* KERNEL */
