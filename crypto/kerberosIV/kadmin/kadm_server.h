/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

/* $Id: kadm_server.h,v 1.10 1997/05/11 04:08:26 assar Exp $ */

/*
 * Definitions for Kerberos administration server & client
 */

#ifndef KADM_SERVER_DEFS
#define KADM_SERVER_DEFS

/*
 * kadm_server.h
 * Header file for the fourth attempt at an admin server
 * Doug Church, December 28, 1989, MIT Project Athena
 *    ps. Yes that means this code belongs to athena etc...
 *        as part of our ongoing attempt to copyright all greek names
 */

typedef struct {
  struct sockaddr_in admin_addr;
  struct sockaddr_in recv_addr;
  int recv_addr_len;
  int admin_fd;			/* our link to clients */
  char sname[ANAME_SZ];
  char sinst[INST_SZ];
  char krbrlm[REALM_SZ];
  des_cblock master_key;
  des_cblock session_key;
  des_key_schedule master_key_schedule;
  long master_key_version;
} Kadm_Server;

/* the default syslog file */
#ifndef KADM_SYSLOG
#define KADM_SYSLOG  "/var/log/admin_server.syslog"
#endif /* KADM_SYSLOG */

#ifndef DEFAULT_ACL_DIR
#define DEFAULT_ACL_DIR	"/var/kerberos"
#endif /* DEFAULT_ACL_DIR */
#define	ADD_ACL_FILE	"/admin_acl.add"
#define	GET_ACL_FILE	"/admin_acl.get"
#define	MOD_ACL_FILE	"/admin_acl.mod"
#define	DEL_ACL_FILE	"/admin_acl.del"

#endif /* KADM_SERVER_DEFS */
