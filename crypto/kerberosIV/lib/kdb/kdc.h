/*
 * $Id: kdc.h,v 1.8 1997/04/01 03:59:05 assar Exp $
 *
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology. 
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>. 
 *
 * Include file for the Kerberos Key Distribution Center. 
 */

#ifndef KDC_DEFS
#define KDC_DEFS

/* Don't depend on this! */
#ifndef MKEYFILE
#if 0
#define MKEYFILE	"/var/kerberos/master-key"
#else
#define MKEYFILE	"/.k"
#endif
#endif
#ifndef K_LOGFIL
#define K_LOGFIL	"/var/log/kpropd.log"
#endif

#define ONE_MINUTE	60
#define FIVE_MINUTES	(5 * ONE_MINUTE)
#define ONE_HOUR	(60 * ONE_MINUTE)
#define ONE_DAY		(24 * ONE_HOUR)
#define THREE_DAYS	(3 * ONE_DAY)

#endif /* KDC_DEFS */

