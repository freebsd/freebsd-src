/*
 * Copyright (c) 1997-2004 Erez Zadok
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      %W% (Berkeley) %G%
 *
 * $Id: amq_svc.c,v 1.4.2.5 2004/01/06 03:15:16 ezk Exp $
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/* typedefs */
typedef char *(*amqsvcproc_t)(voidp, struct svc_req *);


void
amq_program_1(struct svc_req *rqstp, SVCXPRT *transp)
{
  union {
    amq_string amqproc_mnttree_1_arg;
    amq_string amqproc_umnt_1_arg;
    amq_setopt amqproc_setopt_1_arg;
  } argument;
  char *result;
  xdrproc_t xdr_argument, xdr_result;
  amqsvcproc_t local;

  switch (rqstp->rq_proc) {

  case AMQPROC_NULL:
    xdr_argument = (xdrproc_t) xdr_void;
    xdr_result = (xdrproc_t) xdr_void;
    local = (amqsvcproc_t) amqproc_null_1_svc;
    break;

  case AMQPROC_MNTTREE:
    xdr_argument = (xdrproc_t) xdr_amq_string;
    xdr_result = (xdrproc_t) xdr_amq_mount_tree_p;
    local = (amqsvcproc_t) amqproc_mnttree_1_svc;
    break;

  case AMQPROC_UMNT:
    xdr_argument = (xdrproc_t) xdr_amq_string;
    xdr_result = (xdrproc_t) xdr_void;
    local = (amqsvcproc_t) amqproc_umnt_1_svc;
    break;

  case AMQPROC_STATS:
    xdr_argument = (xdrproc_t) xdr_void;
    xdr_result = (xdrproc_t) xdr_amq_mount_stats;
    local = (amqsvcproc_t) amqproc_stats_1_svc;
    break;

  case AMQPROC_EXPORT:
    xdr_argument = (xdrproc_t) xdr_void;
    xdr_result = (xdrproc_t) xdr_amq_mount_tree_list;
    local = (amqsvcproc_t) amqproc_export_1_svc;
    break;

  case AMQPROC_SETOPT:
    xdr_argument = (xdrproc_t) xdr_amq_setopt;
    xdr_result = (xdrproc_t) xdr_int;
    local = (amqsvcproc_t) amqproc_setopt_1_svc;
    break;

  case AMQPROC_GETMNTFS:
    xdr_argument = (xdrproc_t) xdr_void;
    xdr_result = (xdrproc_t) xdr_amq_mount_info_qelem;
    local = (amqsvcproc_t) amqproc_getmntfs_1_svc;
    break;

  case AMQPROC_GETVERS:
    xdr_argument = (xdrproc_t) xdr_void;
    xdr_result = (xdrproc_t) xdr_amq_string;
    local = (amqsvcproc_t) amqproc_getvers_1_svc;
    break;

  case AMQPROC_GETPID:
    xdr_argument = (xdrproc_t) xdr_void;
    xdr_result = (xdrproc_t) xdr_int;
    local = (amqsvcproc_t) amqproc_getpid_1_svc;
    break;

  default:
    svcerr_noproc(transp);
    return;
  }

  memset((char *) &argument, 0, sizeof(argument));
  if (!svc_getargs(transp,
		   (XDRPROC_T_TYPE) xdr_argument,
		   (SVC_IN_ARG_TYPE) & argument)) {
    svcerr_decode(transp);
    return;
  }

  result = (*local) (&argument, rqstp);

  if (result != NULL && !svc_sendreply(transp,
				       (XDRPROC_T_TYPE) xdr_result,
				       result)) {
    svcerr_systemerr(transp);
  }

  if (!svc_freeargs(transp,
		    (XDRPROC_T_TYPE) xdr_argument,
		    (SVC_IN_ARG_TYPE) & argument)) {
    plog(XLOG_FATAL, "unable to free rpc arguments in amqprog_1");
    going_down(1);
  }
}
