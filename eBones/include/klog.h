/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * This file defines the types of log messages logged by klog.  Each
 * type of message may be selectively turned on or off.
 *
 *	from: klog.h,v 4.7 89/01/24 17:55:07 jon Exp $
 *	$FreeBSD$
 */

#ifndef KLOG_DEFS
#define KLOG_DEFS

#define KRBLOG 		"/var/log/kerberos.log"  /* master server  */
#define KRBSLAVELOG	"/var/log/kerberos_slave.log"  /* master server  */
#define	NLOGTYPE	100	/* Maximum number of log msg types  */

#define L_NET_ERR	  1	/* Error in network code	    */
#define L_NET_INFO	  2	/* Info on network activity	    */
#define L_KRB_PERR	  3	/* Kerberos protocol errors	    */
#define L_KRB_PINFO	  4	/* Kerberos protocol info	    */
#define L_INI_REQ	  5	/* Request for initial ticket	    */
#define L_NTGT_INTK       6	/* Initial request not for TGT	    */
#define L_DEATH_REQ       7	/* Request for server death	    */
#define L_TKT_REQ	  8	/* All ticket requests using a tgt  */
#define L_ERR_SEXP	  9	/* Service expired		    */
#define L_ERR_MKV	 10	/* Master key version incorrect     */
#define L_ERR_NKY	 11	/* User's key is null		    */
#define L_ERR_NUN	 12	/* Principal not unique		    */
#define L_ERR_UNK	 13	/* Principal Unknown		    */
#define L_ALL_REQ	 14	/* All requests			    */
#define L_APPL_REQ	 15	/* Application requests (using tgt) */
#define L_KRB_PWARN      16	/* Protocol warning messages	    */

char   *klog();

#endif /* KLOG_DEFS */
