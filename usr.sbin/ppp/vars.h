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
 * $Id:$
 *
 *	TODO:
 */

#ifndef _VARS_H_
#define	_VARS_H_

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
#define	ConfIpAddress	6
#define	MAXCONFS	7

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
  char   modem_dev[20];		/* Name of device */
  int	 open_mode;		/* LCP open mode */
  char   dial_script[200];	/* Dial script */
  char   login_script[200];	/* Login script */
  char   auth_key[50];		/* PAP/CHAP key */
  char	 auth_name[50];		/* PAP/CHAP system name */
  char   phone_number[50];	/* Telephone Number */
};

#define VarAccmap	pppVars.var_accmap
#define VarMRU		pppVars.var_mru
#define	VarDevice	pppVars.modem_dev
#define	VarSpeed	pppVars.modem_speed
#define	VarParity	pppVars.modem_parity
#define	VarOpenMode	pppVars.open_mode
#define	VarDialScript	pppVars.dial_script
#define	VarLoginScript	pppVars.login_script
#define VarIdleTimeout  pppVars.idle_timeout
#define	VarLqrTimeout	pppVars.lqr_timeout
#define	VarAuthKey	pppVars.auth_key
#define	VarAuthName	pppVars.auth_name
#define	VarPhone	pppVars.phone_number

extern struct pppvars pppVars;

int ipInOctets, ipOutOctets;
int ipConnectSecs, ipIdleSecs;
#endif
