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
 * $Id: vars.h,v 1.42.2.12 1998/03/09 19:26:41 brian Exp $
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
  int lqr_timeout;		/* LQR timeout value */
  int retry_timeout;		/* Retry timeout value */
  int loopback;			/* Turn around packets addressed to me */
  int open_mode;		/* Delay before first LCP REQ (-1 = passive) */
#define LOCAL_AUTH	0x01
#define LOCAL_NO_AUTH	0x02
#define LOCAL_DENY	0x03
#define LOCAL_CX	0x04	/* OR'd value - require a context */
#define LOCAL_CX_OPT	0x08	/* OR'd value - optional context */
  u_char lauth;			/* Local Authorized status */

  /* The rest are just default initialized in vars.c */
#define DIALUP_REQ	0x01
#define DIALUP_DONE	0x02
  char auth_key[50];		/* PAP/CHAP key */
  char auth_name[50];		/* PAP/CHAP system name */
  char local_auth_key[50];		/* Local auth passwd */
  int have_local_auth_key;		/* Local auth passwd specified ? */
#ifdef HAVE_DES
  int use_MSChap;		/* Use MSCHAP encryption */
#endif
  char shostname[MAXHOSTNAMELEN]; /* Local short Host Name */
  struct aliasHandlers handler;	/* Alias function pointers */
};

#define VarAccmap		pppVars.var_accmap
#define VarMRU			pppVars.var_mru
#define VarPrefMTU		pppVars.pref_mtu
#define	VarOpenMode		pppVars.open_mode
#define	VarLocalAuth		pppVars.lauth
#define	VarLqrTimeout		pppVars.lqr_timeout
#define	VarRetryTimeout		pppVars.retry_timeout
#define	VarAuthKey		pppVars.auth_key
#define	VarAuthName		pppVars.auth_name
#define	VarLocalAuthKey		pppVars.local_auth_key
#define	VarHaveLocalAuthKey	pppVars.have_local_auth_key
#ifdef HAVE_DES
#define	VarMSChap		pppVars.use_MSChap
#endif
#define	VarShortHost		pppVars.shostname
#define VarLoopback		pppVars.loopback

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

extern struct pppvars pppVars;
extern char VarVersion[];
extern char VarLocalVersion[];

extern int ipKeepAlive;

extern int EnableCommand(struct cmdargs const *);
extern int DisableCommand(struct cmdargs const *);
extern int AcceptCommand(struct cmdargs const *);
extern int DenyCommand(struct cmdargs const *);
extern int LocalAuthCommand(struct cmdargs const *);
extern int DisplayCommand(struct cmdargs const *);
