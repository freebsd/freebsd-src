/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * This file contains configuration information for the Kerberos library
 * which is machine specific; currently, this file contains
 * configuration information for the vax, the "ibm032" (RT), and the
 * "PC8086" (IBM PC).
 *
 * Note:  cross-compiled targets must appear BEFORE their corresponding
 * cross-compiler host.  Otherwise, both will be defined when running
 * the native compiler on the programs that construct cross-compiled
 * sources.
 *
 *	from: krb_conf.h,v 4.0 89/01/23 09:59:27 jtkohl Exp $
 *	$FreeBSD$
 */

#ifndef KRB_CONF_DEFS
#define KRB_CONF_DEFS

/* Byte ordering */
extern int krbONE;
#define		HOST_BYTE_ORDER	(* (char *) &krbONE)
#define		MSB_FIRST		0	/* 68000, IBM RT/PC */
#define		LSB_FIRST		1	/* Vax, PC8086 */

#endif KRB_CONF_DEFS
