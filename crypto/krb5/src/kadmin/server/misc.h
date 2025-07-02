/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1994 OpenVision Technologies, Inc., All Rights Reserved
 *
 */

#ifndef _MISC_H
#define _MISC_H 1

#include "net-server.h"         /* for krb5_fulladdr */

int
setup_gss_names(struct svc_req *, gss_buffer_desc *,
                gss_buffer_desc *);

kadm5_ret_t
schpw_util_wrapper(void *server_handle, krb5_principal client,
                   krb5_principal target, krb5_boolean initial_flag,
                   char *new_pw, char **ret_pw,
                   char *msg_ret, unsigned int msg_len);

kadm5_ret_t check_min_life(void *server_handle, krb5_principal principal,
                           char *msg_ret, unsigned int msg_len);

void kadm_1(struct svc_req *, SVCXPRT *);
void krb5_iprop_prog_1(struct svc_req *, SVCXPRT *);

void trunc_name(size_t *len, char **dots);

int
gss_to_krb5_name_1(struct svc_req *rqstp, krb5_context ctx, gss_name_t gss_name,
                   krb5_principal *princ, gss_buffer_t gss_str);


void reset_db(void);

void log_badauth(OM_uint32 major, OM_uint32 minor, SVCXPRT *xprt, char *data);

const char *client_addr(SVCXPRT *xprt);

/* network.c */
#include "net-server.h"


void
krb5_iprop_prog_1(struct svc_req *rqstp, SVCXPRT *transp);

kadm5_ret_t
kiprop_get_adm_host_srv_name(krb5_context,
                             const char *,
                             char **);


#endif /* _MISC_H */
