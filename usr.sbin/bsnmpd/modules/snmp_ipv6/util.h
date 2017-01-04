/*
 *
 *        Copyright 1989, 1991, 1992 by Carnegie Mellon University
 *
 *  		  Derivative Work - 1996, 1998-2000
 * Copyright 1996, 1998-2000 The Regents of the University of California
 *
 *  			 All Rights Reserved
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of CMU and The Regents of
 * the University of California not be used in advertising or publicity
 * pertaining to distribution of the software without specific written
 * permission.
 *
 * CMU AND THE REGENTS OF THE UNIVERSITY OF CALIFORNIA DISCLAIM ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL CMU OR
 * THE REGENTS OF THE UNIVERSITY OF CALIFORNIA BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* See util.c for comments about licensing */

#ifndef __SNMP_IPV6__UTIL_H__
#define __SNMP_IPV6__UTIL_H__

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#include "ipv6.h"

#include <ifaddrs.h>
#include <net/if_dl.h>

int if_getifmibdata(int idx, struct ifmibdata *result);

#endif
