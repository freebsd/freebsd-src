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
 * $Id: vars.h,v 1.42.2.19 1998/04/03 19:24:49 brian Exp $
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
#define ConfIdCheck	14
#define ConfLoopback	15
#define	NCONFS		16

#define	Enabled(x)	(pppConfs[x].myside & CONF_ENABLE)
#define	Acceptable(x)	(pppConfs[x].hisside & CONF_ACCEPT)

extern struct confdesc pppConfs[NCONFS];

struct pppvars {
  /* The rest are just default initialized in vars.c */
  int use_MSChap;		/* Use MSCHAP encryption */
  struct aliasHandlers handler;	/* Alias function pointers */
};

#define	VarMSChap		pppVars.use_MSChap

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

extern int EnableCommand(struct cmdargs const *);
extern int DisableCommand(struct cmdargs const *);
extern int AcceptCommand(struct cmdargs const *);
extern int DenyCommand(struct cmdargs const *);
extern int DisplayCommand(struct cmdargs const *);
