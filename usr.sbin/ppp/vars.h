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
 * $Id: vars.h,v 1.3 1995/03/11 15:18:55 amurai Exp $
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
#define	MAXCONFS	8

#define	Enabled(x)	(pppConfs[x].myside & CONF_ENABLE)
#define	Acceptable(x)	(pppConfs[x].hisside & CONF_ACCEPT)

extern struct confdesc pppConfs[MAXCONFS+1];

struct pppvars {
  u_long var_mru;		/* Initial MRU value */
  int    var_accmap;		/* Initial ACCMAP value */
  int    modem_speed;		/* Current modem speed */
  int    modem_parity;		/* Parity setting */
  int    idle_timeout;		/* Idle timeout value */
  int	 lqr_timeout;		/* LQR timeout value */
  int    retry_timeout;		/* Retry timeout value */
  int    redial_timeout;	/* Redial timeout value */
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
  char   phone_number[50];	/* Telephone Number */
  char   shostname[MAXHOSTNAMELEN];/* Local short Host Name */
};

#define VarAccmap	pppVars.var_accmap
#define VarMRU		pppVars.var_mru
#define	VarDevice	pppVars.modem_dev
#define	VarSpeed	pppVars.modem_speed
#define	VarParity	pppVars.modem_parity
#define	VarOpenMode	pppVars.open_mode
#define	VarLocalAuth	pppVars.lauth
#define	VarDialScript	pppVars.dial_script
#define	VarLoginScript	pppVars.login_script
#define VarIdleTimeout  pppVars.idle_timeout
#define	VarLqrTimeout	pppVars.lqr_timeout
#define	VarRetryTimeout	pppVars.retry_timeout
#define	VarAuthKey	pppVars.auth_key
#define	VarAuthName	pppVars.auth_name
#define	VarPhone	pppVars.phone_number
#define	VarShortHost	pppVars.shostname
#define VarRedialTimeout pppVars.redial_timeout
#define VarDialTries	pppVars.dial_tries

#define	DEV_IS_SYNC	(VarSpeed == 0)

extern struct pppvars pppVars;

int ipInOctets, ipOutOctets, ipKeepAlive;
int ipConnectSecs, ipIdleSecs;
#endif
