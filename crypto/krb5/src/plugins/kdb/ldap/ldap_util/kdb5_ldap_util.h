/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/ldap_util/kdb5_ldap_util.h */
/* Copyright (c) 2004-2005, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "kdb_ldap.h"
#include "kdb5_ldap_realm.h"
#include "kdb5_ldap_services.h"
#include "kdb5_ldap_policy.h"

#define MAIN_HELP            -1
#define CREATE_REALM          1
#define MODIFY_REALM          2
#define VIEW_REALM            3
#define DESTROY_REALM         4
#define LIST_REALM            5

#define STASH_SRV_PW          17

#define CREATE_POLICY         11
#define MODIFY_POLICY         12
#define VIEW_POLICY           13
#define DESTROY_POLICY        14
#define LIST_POLICY           15

extern char *progname;

extern int exit_status;
extern krb5_context util_context;

extern void usage(void);
extern void db_usage(int);

#define ARG_VAL (--argc > 0 ? (koptarg = *(++argv)) : (char *)(db_usage(MAIN_HELP), NULL))

/* Following are the bitmaps that indicate which of the options among -D, -w, -h, -p & -t
 * were specified on the command line.
 */
#define CMD_LDAP_D      0x1     /* set if -D option is specified */
#define CMD_LDAP_W      0x2     /* set if -w option is specified */
#define CMD_LDAP_H      0x4     /* set if -h option is specified */
#define CMD_LDAP_P      0x8     /* set if -p option is specified */

#define MAX_PASSWD_LEN          1024
#define MAX_PASSWD_PROMPT_LEN   276     /* max_dn_size(=256) + strlen("Password for \" \"")=20 */
