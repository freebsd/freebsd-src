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
 * $Id: ipcp.h,v 1.6 1997/02/22 16:10:22 peter Exp $
 *
 *	TODO:
 */

#ifndef _IPCP_H_
#define _IPCP_H_

#define	IPCP_MAXCODE	CODE_CODEREJ

#define	TY_IPADDRS	1
#define	TY_COMPPROTO	2
#define	TY_IPADDR	3

/* MS PPP NameServer and NetBIOS NameServer stuff */

#ifndef NOMSEXT
#define TY_PRIMARY_DNS		129
#define TY_PRIMARY_NBNS		130
#define TY_SECONDARY_DNS	131
#define TY_SECONDARY_NBNS	132
#endif

struct ipcpstate {
  struct  in_addr his_ipaddr;	/* IP address he is willing to use */
  u_long  his_compproto;

  struct  in_addr want_ipaddr;	/* IP address I'm willing to use */
  u_long  want_compproto;

  u_long  his_reject;		/* Request codes rejected by peer */
  u_long  my_reject;		/* Request codes I have rejected */
  int	  heis1172;		/* True if he is speaking rfc1172 */
};

struct compreq {
  u_short proto;
  u_char  slots;
  u_char  compcid;
};

struct in_range {
  struct in_addr ipaddr;
  struct in_addr mask;
  int    width;
};

extern struct ipcpstate IpcpInfo;
extern struct in_range DefMyAddress;
extern struct in_range DefHisAddress;
extern struct in_range DefTriggerAddress;

#ifndef NOMSEXT
extern struct in_addr ns_entries[2];
extern struct in_addr nbns_entries[2];
#endif

extern void IpcpInit(void);
extern void IpcpDefAddress(void);
#endif
