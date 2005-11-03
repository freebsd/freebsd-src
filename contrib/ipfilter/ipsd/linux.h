/*	$FreeBSD: src/contrib/ipfilter/ipsd/linux.h,v 1.2 2005/04/25 18:20:10 darrenr Exp $	*/

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
