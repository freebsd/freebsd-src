/*
 * $Id: klog.h,v 1.5 1997/05/11 11:05:28 assar Exp $
 *
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * This file defines the types of log messages logged by klog.  Each
 * type of message may be selectively turned on or off. 
 */

#ifndef KLOG_DEFS
#define KLOG_DEFS

#ifndef KRBLOG
#define KRBLOG 		"/var/log/kerberos.log"  /* master server  */
#endif
#ifndef KRBSLAVELOG
#define KRBSLAVELOG	"/var/log/kerberos_slave.log"  /* slave server  */
#endif
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

char * klog __P((int type, const char *format, ...))
#ifdef __GNUC__
__attribute__ ((format (printf, 2, 3)))
#endif
;

#endif /* KLOG_DEFS */
