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
 * $Id: os.h,v 1.2 1995/02/26 12:17:49 amurai Exp $
 *
 *	TODO:
 */

#ifndef _OS_H_
#define	_OS_H_
#include "cdefs.h"

int OsSetIpaddress __P((struct in_addr myaddr, struct in_addr hisaddr, struct in_addr netmask));
int OsInterfaceDown __P((int));
void OsSetInterfaceParams __P((int type, int mtu, int speed));
int OpenTunnel __P((int *));
void OsCloseLink __P((int flag));
void OsLinkup __P((void)), OsLinkdown __P((void));
void OsSetRoute __P((int, struct in_addr, struct in_addr, struct in_addr));
void DeleteIfRoutes __P((int));
void OsAddInOctets __P((int cnt));
void OsAddOutOctets __P((int cnt));
#endif
