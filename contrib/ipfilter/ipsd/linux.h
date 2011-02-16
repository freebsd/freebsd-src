/*	$FreeBSD: src/contrib/ipfilter/ipsd/linux.h,v 1.2.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $	*/

/*
 * Copyright (C) 1997-1998 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * @(#)linux.h	1.1 8/19/95
 */

#include <linux/config.h>
#ifdef MODULE
#include <linux/module.h>
#include <linux/version.h>
#endif /* MODULE */

#include "ip_compat.h"
