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
 * $Id: vars.h,v 1.12 1997/04/21 01:02:02 brian Exp $
 *
 *	TODO:
 */

#ifndef _VARS_H_
#define	_VARS_H_

#include <sys/param.h>

struct confdesc {
  char *name;
  int  myside, hisside;
};

#define	CONF_DISABLE	0
#define	CONF_ENABLE	1

#define	CONF_DENY	0
#define	CONF_ACCEPT	1

#define	ConfVjcomp	0
#define	ConfLqr		1
#define	ConfChap	2
#define	ConfPap		3
#define	ConfAcfcomp	4
#define	ConfProtocomp	5
#define	ConfPred1	6
#define	ConfProxy	7
#define ConfMSExt	8
#define ConfPasswdAuth	9
#define	MAXCONFS	10

#define	Enabled(x)	(pppConfs[x].myside & CONF_ENABLE)
#define	Acceptable(x)	(pppConfs[x].hisside & CONF_ACCEPT)

extern struct confdesc pppConfs[MAXCONFS+1];

struct pppvars {
  u_long var_mru;		/* Initial MRU value */
  int    var_accmap;		/* Initial ACCMAP value */
  int    modem_speed;		/* Current modem speed */
  int    modem_parity;		/* Parity setting */
  int    modem_ctsrts;		/* Use CTS/RTS on modem port? (boolean) */
  int    idle_timeout;		/* Idle timeout value */
  int	 lqr_timeout;		/* LQR timeout value */
  int    retry_timeout;		/* Retry timeout value */
  int    reconnect_timer;	/* Timeout before reconnect on carrier loss */
  int    reconnect_tries;	/* Attempt reconnect on carrier loss */
  int    redial_timeout;	/* Redial timeout value */
  int    redial_next_timeout;	/* Redial next timeout value */
  int    dial_tries;		/* Dial attempts before giving up, 0 == forever */
  char   modem_dev[20];		/* Name of device */
  int	 open_mode;		/* LCP open mode */
  #define LOCAL_AUTH	0x01
  #define LOCAL_NO_AUTH	0x02
  u_char lauth;			/* Local Authorized status */
  #define DIALUP_REQ	0x01
  #define DIALUP_DONE	0x02
  char   dial_script[200];	/* Dial script */
  char   login_script[200];	/* Login script */
  char   auth_key[50];		/* PAP/CHAP key */
  char	 auth_name[50];		/* PAP/CHAP system name */
  char   phone_numbers[200];    /* Telephone Numbers */
  char   phone_copy[200];       /* copy for strsep() */
  char   *next_phone;           /* Next phone from the list */
  char   shostname[MAXHOSTNAMELEN];/* Local short Host Name */
};

#define VarAccmap	pppVars.var_accmap
#define VarMRU		pppVars.var_mru
#define	VarDevice	pppVars.modem_dev
#define	VarSpeed	pppVars.modem_speed
#define	VarParity	pppVars.modem_parity
#define	VarCtsRts	pppVars.modem_ctsrts
#define	VarOpenMode	pppVars.open_mode
#define	VarLocalAuth	pppVars.lauth
#define	VarDialScript	pppVars.dial_script
#define	VarLoginScript	pppVars.login_script
#define VarIdleTimeout  pppVars.idle_timeout
#define	VarLqrTimeout	pppVars.lqr_timeout
#define	VarRetryTimeout	pppVars.retry_timeout
#define	VarAuthKey	pppVars.auth_key
#define	VarAuthName	pppVars.auth_name
#define VarPhoneList    pppVars.phone_numbers
#define VarPhoneCopy    pppVars.phone_copy
#define VarNextPhone    pppVars.next_phone
#define	VarShortHost	pppVars.shostname
#define VarReconnectTimer pppVars.reconnect_timer
#define VarReconnectTries pppVars.reconnect_tries
#define VarRedialTimeout pppVars.redial_timeout
#define VarRedialNextTimeout pppVars.redial_next_timeout
#define VarDialTries	pppVars.dial_tries

#define	DEV_IS_SYNC	(VarSpeed == 0)

extern struct pppvars pppVars;

int ipInOctets, ipOutOctets, ipKeepAlive;
int ipConnectSecs, ipIdleSecs;
int lostCarrier;
#endif
