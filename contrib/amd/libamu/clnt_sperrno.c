/*
 * Copyright (c) 1997-1998 Erez Zadok
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
 *    must display the following acknowledgement:
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
 * $Id: clnt_sperrno.c,v 5.2.2.1 1992/02/09 15:08:40 jsp beta $
 *
 */

/*
 * Early RPC seems to be missing these..
 * Extracted from the RPC 3.9 sources as indicated
 */

/* @(#)clnt_perror.c    1.1 87/11/04 3.9 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>


struct rpc_errtab {
  enum clnt_stat status;
  char *message;
};

static struct rpc_errtab rpc_errlist[] =
{
  {RPC_SUCCESS,
   "RPC: Success"},
  {RPC_CANTENCODEARGS,
   "RPC: Can't encode arguments"},
  {RPC_CANTDECODERES,
   "RPC: Can't decode result"},
  {RPC_CANTSEND,
   "RPC: Unable to send"},
  {RPC_CANTRECV,
   "RPC: Unable to receive"},
  {RPC_TIMEDOUT,
   "RPC: Timed out"},
  {RPC_VERSMISMATCH,
   "RPC: Incompatible versions of RPC"},
  {RPC_AUTHERROR,
   "RPC: Authentication error"},
  {RPC_PROGUNAVAIL,
   "RPC: Program unavailable"},
  {RPC_PROGVERSMISMATCH,
   "RPC: Program/version mismatch"},
  {RPC_PROCUNAVAIL,
   "RPC: Procedure unavailable"},
  {RPC_CANTDECODEARGS,
   "RPC: Server can't decode arguments"},
  {RPC_SYSTEMERROR,
   "RPC: Remote system error"},
  {RPC_UNKNOWNHOST,
   "RPC: Unknown host"},
/*      { RPC_UNKNOWNPROTO,
 * "RPC: Unknown protocol" }, */
  {RPC_PMAPFAILURE,
   "RPC: Port mapper failure"},
  {RPC_PROGNOTREGISTERED,
   "RPC: Program not registered"},
  {RPC_FAILED,
   "RPC: Failed (unspecified error)"}
};


/*
 * This interface for use by clntrpc
 */
char *
clnt_sperrno(enum clnt_stat stat)
{
  int i;

  for (i = 0; i < sizeof(rpc_errlist) / sizeof(struct rpc_errtab); i++) {
    if (rpc_errlist[i].status == stat) {
      return (rpc_errlist[i].message);
    }
  }
  return ("RPC: (unknown error code)");
}
