/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: vars.h,v 1.41 1998/01/20 22:47:48 brian Exp $
 *
 *	TODO:
 */

struct confdesc {
  const char *name;
  int myside, hisside;
};

#define	CONF_NONE	-1
#define	CONF_DISABLE	0
#define	CONF_ENABLE	1

#define	CONF_DENY	0
#define	CONF_ACCEPT	1

#define	ConfAcfcomp	0
#define	ConfChap	1
#define	ConfDeflate	2
#define	ConfLqr		3
#define	ConfPap		4
#define	ConfPppdDeflate	5
#define	ConfPred1	6
#define	ConfProtocomp	7
#define	ConfVjcomp	8

#define ConfMSExt	9
#define ConfPasswdAuth	10
#define	ConfProxy	11
#define ConfThroughput	12
#define ConfUtmp	13
#define	MAXCONFS	14

#define	Enabled(x)	(pppConfs[x].myside & CONF_ENABLE)
#define	Acceptable(x)	(pppConfs[x].hisside & CONF_ACCEPT)

extern struct confdesc pppConfs[MAXCONFS + 1];

struct pppvars {
  u_short var_mru;		/* Initial MRU value */
  u_short pref_mtu;		/* Preferred MTU value */
  int var_accmap;		/* Initial ACCMAP value */
  int modem_speed;		/* Current modem speed */
  int modem_parity;		/* Parity setting */
  int modem_ctsrts;		/* Use CTS/RTS on modem port? (boolean) */
  int idle_timeout;		/* Idle timeout value */
  int lqr_timeout;		/* LQR timeout value */
  int retry_timeout;		/* Retry timeout value */
  int reconnect_timer;		/* Timeout before reconnect on carrier loss */
  int reconnect_tries;		/* Attempt reconnect on carrier loss */
  int redial_timeout;		/* Redial timeout value */
  int redial_next_timeout;	/* Redial next timeout value */
  int dial_tries;		/* Dial attempts before giving up, 0 == inf */
  int loopback;			/* Turn around packets addressed to me */
  char modem_devlist[LINE_LEN];	/* Comma-separated list of devices */
  char modem_dev[40];		/* Name of device / host:port */
  const char *base_modem_dev;	/* Pointer to base of modem_dev */
  int open_mode;		/* Delay before first LCP REQ (-1 = passive) */
#define LOCAL_AUTH	0x01
#define LOCAL_NO_AUTH	0x02
#define LOCAL_DENY	0x03
  u_char lauth;			/* Local Authorized status */
  FILE *termfp;			/* The terminal */
#define DIALUP_REQ	0x01
#define DIALUP_DONE	0x02
  char dial_script[SCRIPT_LEN];	/* Dial script */
  char login_script[SCRIPT_LEN]; /* Login script */
  char auth_key[50];		/* PAP/CHAP key */
  char auth_name[50];		/* PAP/CHAP system name */
  char local_auth_key[50];		/* Local auth passwd */
  int have_local_auth_key;		/* Local auth passwd specified ? */
#ifdef HAVE_DES
  int use_MSChap;		/* Use MSCHAP encryption */
#endif
  char phone_numbers[200];	/* Telephone Numbers */
  char phone_copy[200];		/* copy for strsep() */
  char *next_phone;		/* Next phone from the list */
  char *alt_phone;		/* Next phone from the list */
  char shostname[MAXHOSTNAMELEN]; /* Local short Host Name */
  char hangup_script[SCRIPT_LEN]; /* Hangup script before modem is closed */
  struct aliasHandlers handler;	/* Alias function pointers */
};

#define VarAccmap		pppVars.var_accmap
#define VarMRU			pppVars.var_mru
#define VarPrefMTU		pppVars.pref_mtu
#define	VarDevice		pppVars.modem_dev
#define	VarDeviceList		pppVars.modem_devlist
#define	VarBaseDevice		pppVars.base_modem_dev
#define	VarSpeed		pppVars.modem_speed
#define	VarParity		pppVars.modem_parity
#define	VarCtsRts		pppVars.modem_ctsrts
#define	VarOpenMode		pppVars.open_mode
#define	VarLocalAuth		pppVars.lauth
#define	VarDialScript		pppVars.dial_script
#define VarHangupScript 	pppVars.hangup_script
#define	VarLoginScript		pppVars.login_script
#define VarIdleTimeout  	pppVars.idle_timeout
#define	VarLqrTimeout		pppVars.lqr_timeout
#define	VarRetryTimeout		pppVars.retry_timeout
#define	VarAuthKey		pppVars.auth_key
#define	VarAuthName		pppVars.auth_name
#define	VarLocalAuthKey		pppVars.local_auth_key
#define	VarHaveLocalAuthKey	pppVars.have_local_auth_key
#ifdef HAVE_DES
#define	VarMSChap		pppVars.use_MSChap
#endif
#define VarPhoneList    	pppVars.phone_numbers
#define VarPhoneCopy    	pppVars.phone_copy
#define VarNextPhone    	pppVars.next_phone
#define VarAltPhone     	pppVars.alt_phone
#define	VarShortHost		pppVars.shostname
#define VarReconnectTimer	pppVars.reconnect_timer
#define VarReconnectTries	pppVars.reconnect_tries
#define VarRedialTimeout	pppVars.redial_timeout
#define VarRedialNextTimeout	pppVars.redial_next_timeout
#define VarDialTries		pppVars.dial_tries
#define VarLoopback		pppVars.loopback
#define VarTerm			pppVars.termfp

#define VarAliasHandlers	   pppVars.handler
#define VarPacketAliasGetFragment  (*pppVars.handler.PacketAliasGetFragment)
#define VarPacketAliasGetFragment  (*pppVars.handler.PacketAliasGetFragment)
#define VarPacketAliasInit	   (*pppVars.handler.PacketAliasInit)
#define VarPacketAliasIn	   (*pppVars.handler.PacketAliasIn)
#define VarPacketAliasOut	   (*pppVars.handler.PacketAliasOut)
#define VarPacketAliasRedirectAddr (*pppVars.handler.PacketAliasRedirectAddr)
#define VarPacketAliasRedirectPort (*pppVars.handler.PacketAliasRedirectPort)
#define VarPacketAliasSaveFragment (*pppVars.handler.PacketAliasSaveFragment)
#define VarPacketAliasSetAddress   (*pppVars.handler.PacketAliasSetAddress)
#define VarPacketAliasSetMode	   (*pppVars.handler.PacketAliasSetMode)
#define VarPacketAliasFragmentIn   (*pppVars.handler.PacketAliasFragmentIn)

#define	DEV_IS_SYNC (VarSpeed == 0)

extern struct pppvars pppVars;
extern char VarVersion[];
extern char VarLocalVersion[];

extern int Utmp;		/* Are we in /etc/utmp ? */
extern int ipKeepAlive;
extern int reconnectState;
extern int reconnectCount;

#define RECON_TRUE (1)
#define RECON_FALSE (2)
#define RECON_UNKNOWN (3)
#define RECON_ENVOKED (4)
#define reconnect(x)                          \
  do                                          \
    if (reconnectState == RECON_UNKNOWN) {    \
      reconnectState = x;                     \
      if (x == RECON_FALSE)                   \
        reconnectCount = 0;                   \
    }                                         \
  while(0)


/*
 * This is the logic behind the reconnect variables:
 * We have four reconnect "states".  We start off not requiring anything
 * from the reconnect code (reconnectState == RECON_UNKNOWN).  If the
 * line is brought down (via LcpClose() or LcpDown()), we have to decide
 * whether to set to RECON_TRUE or RECON_FALSE.  It's only here that we
 * know the correct action.  Once we've decided, we don't want that
 * decision to be overridden (hence the above reconnect() macro) - If we
 * call LcpClose, the ModemTimeout() still gets to "notice" that the line
 * is down.  When it "notice"s, it should only set RECON_TRUE if a decision
 * hasn't already been made.
 *
 * In main.c, when we notice we have RECON_TRUE, we must only action
 * it once.  The fourth "state" is where we're bringing the line up,
 * but if we call LcpClose for any reason (failed PAP/CHAP etc) we
 * don't want to set to RECON_{TRUE,FALSE}.
 *
 * If we get a connection or give up dialing, we go back to RECON_UNKNOWN.
 * If we get give up dialing or reconnecting or if we chose to down the
 * connection, we set reconnectCount back to zero.
 *
 */

extern int EnableCommand(struct cmdargs const *);
extern int DisableCommand(struct cmdargs const *);
extern int AcceptCommand(struct cmdargs const *);
extern int DenyCommand(struct cmdargs const *);
extern int LocalAuthCommand(struct cmdargs const *);
extern int DisplayCommand(struct cmdargs const *);
