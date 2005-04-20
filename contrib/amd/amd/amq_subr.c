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
 * $Id: amq_subr.c,v 1.6.2.6 2004/01/19 00:25:55 ezk Exp $
 * $FreeBSD$
 *
 */
/*
 * Auxiliary routines for amq tool
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/* forward definitions */
bool_t xdr_amq_mount_tree_node(XDR *xdrs, amq_mount_tree *objp);
bool_t xdr_amq_mount_subtree(XDR *xdrs, amq_mount_tree *objp);


voidp
amqproc_null_1_svc(voidp argp, struct svc_req *rqstp)
{
  static char res;

  return (voidp) &res;
}


/*
 * Return a sub-tree of mounts
 */
amq_mount_tree_p *
amqproc_mnttree_1_svc(voidp argp, struct svc_req *rqstp)
{
  static am_node *mp;

  mp = find_ap(*(char **) argp);
  return (amq_mount_tree_p *) ((void *)&mp);
}


/*
 * Unmount a single node
 */
voidp
amqproc_umnt_1_svc(voidp argp, struct svc_req *rqstp)
{
  static char res;
  am_node *mp = find_ap(*(char **) argp);

  if (mp)
    forcibly_timeout_mp(mp);

  return (voidp) &res;
}


/*
 * Return global statistics
 */
amq_mount_stats *
amqproc_stats_1_svc(voidp argp, struct svc_req *rqstp)
{
  return (amq_mount_stats *) ((void *)&amd_stats);
}


/*
 * Return the entire tree of mount nodes
 */
amq_mount_tree_list *
amqproc_export_1_svc(voidp argp, struct svc_req *rqstp)
{
  static amq_mount_tree_list aml;

  aml.amq_mount_tree_list_val = (amq_mount_tree_p *) &exported_ap[0];
  aml.amq_mount_tree_list_len = 1;	/* XXX */

  return &aml;
}


int *
amqproc_setopt_1_svc(voidp argp, struct svc_req *rqstp)
{
  static int rc;
  amq_setopt *opt = (amq_setopt *) argp;

  rc = 0;

  switch (opt->as_opt) {

  case AMOPT_DEBUG:
#ifdef DEBUG
    if (debug_option(opt->as_str))
#endif /* DEBUG */
      rc = EINVAL;
    break;

  case AMOPT_LOGFILE:
    if (gopt.logfile && opt->as_str
	&& STREQ(gopt.logfile, opt->as_str)) {
      if (switch_to_logfile(opt->as_str, orig_umask))
	rc = EINVAL;
    } else {
      rc = EACCES;
    }
    break;

  case AMOPT_XLOG:
    if (switch_option(opt->as_str))
      rc = EINVAL;
    break;

  case AMOPT_FLUSHMAPC:
    if (amd_state == Run) {
      plog(XLOG_INFO, "amq says flush cache");
      do_mapc_reload = 0;
      flush_nfs_fhandle_cache((fserver *) 0);
      flush_srvr_nfs_cache();
    }
    break;
  }

  return &rc;
}


amq_mount_info_list *
amqproc_getmntfs_1_svc(voidp argp, struct svc_req *rqstp)
{
  return (amq_mount_info_list *) ((void *)&mfhead); /* XXX */
}


amq_string *
amqproc_getvers_1_svc(voidp argp, struct svc_req *rqstp)
{
  static amq_string res;

  res = get_version_string();
  return &res;
}


/* get PID of remote amd */
int *
amqproc_getpid_1_svc(voidp argp, struct svc_req *rqstp)
{
  static int res;

  res = getpid();
  return &res;
}


/*
 * XDR routines.
 */


bool_t
xdr_amq_setopt(XDR *xdrs, amq_setopt *objp)
{
  if (!xdr_enum(xdrs, (enum_t *) & objp->as_opt)) {
    return (FALSE);
  }
  if (!xdr_string(xdrs, &objp->as_str, AMQ_STRLEN)) {
    return (FALSE);
  }
  return (TRUE);
}


/*
 * More XDR routines  - Should be used for OUTPUT ONLY.
 */
bool_t
xdr_amq_mount_tree_node(XDR *xdrs, amq_mount_tree *objp)
{
  am_node *mp = (am_node *) objp;
  long mtime;

  if (!xdr_amq_string(xdrs, &mp->am_mnt->mf_info)) {
    return (FALSE);
  }
  if (!xdr_amq_string(xdrs, &mp->am_path)) {
    return (FALSE);
  }
  if (!xdr_amq_string(xdrs, mp->am_link ? &mp->am_link : &mp->am_mnt->mf_mount)) {
    return (FALSE);
  }
  if (!xdr_amq_string(xdrs, &mp->am_mnt->mf_ops->fs_type)) {
    return (FALSE);
  }
  mtime = mp->am_stats.s_mtime;
  if (!xdr_long(xdrs, &mtime)) {
    return (FALSE);
  }
  if (!xdr_u_short(xdrs, &mp->am_stats.s_uid)) {
    return (FALSE);
  }
  if (!xdr_int(xdrs, &mp->am_stats.s_getattr)) {
    return (FALSE);
  }
  if (!xdr_int(xdrs, &mp->am_stats.s_lookup)) {
    return (FALSE);
  }
  if (!xdr_int(xdrs, &mp->am_stats.s_readdir)) {
    return (FALSE);
  }
  if (!xdr_int(xdrs, &mp->am_stats.s_readlink)) {
    return (FALSE);
  }
  if (!xdr_int(xdrs, &mp->am_stats.s_statfs)) {
    return (FALSE);
  }
  return (TRUE);
}


bool_t
xdr_amq_mount_subtree(XDR *xdrs, amq_mount_tree *objp)
{
  am_node *mp = (am_node *) objp;

  if (!xdr_amq_mount_tree_node(xdrs, objp)) {
    return (FALSE);
  }
  if (!xdr_pointer(xdrs, (char **) &mp->am_osib, sizeof(amq_mount_tree), (XDRPROC_T_TYPE) xdr_amq_mount_subtree)) {
    return (FALSE);
  }
  if (!xdr_pointer(xdrs, (char **) &mp->am_child, sizeof(amq_mount_tree), (XDRPROC_T_TYPE) xdr_amq_mount_subtree)) {
    return (FALSE);
  }
  return (TRUE);
}


bool_t
xdr_amq_mount_tree(XDR *xdrs, amq_mount_tree *objp)
{
  am_node *mp = (am_node *) objp;
  am_node *mnil = 0;

  if (!xdr_amq_mount_tree_node(xdrs, objp)) {
    return (FALSE);
  }
  if (!xdr_pointer(xdrs, (char **) ((void *)&mnil), sizeof(amq_mount_tree), (XDRPROC_T_TYPE) xdr_amq_mount_subtree)) {
    return (FALSE);
  }
  if (!xdr_pointer(xdrs, (char **) &mp->am_child, sizeof(amq_mount_tree), (XDRPROC_T_TYPE) xdr_amq_mount_subtree)) {
    return (FALSE);
  }
  return (TRUE);
}


bool_t
xdr_amq_mount_tree_p(XDR *xdrs, amq_mount_tree_p *objp)
{
  if (!xdr_pointer(xdrs, (char **) objp, sizeof(amq_mount_tree), (XDRPROC_T_TYPE) xdr_amq_mount_tree)) {
    return (FALSE);
  }
  return (TRUE);
}


bool_t
xdr_amq_mount_stats(XDR *xdrs, amq_mount_stats *objp)
{
  if (!xdr_int(xdrs, &objp->as_drops)) {
    return (FALSE);
  }
  if (!xdr_int(xdrs, &objp->as_stale)) {
    return (FALSE);
  }
  if (!xdr_int(xdrs, &objp->as_mok)) {
    return (FALSE);
  }
  if (!xdr_int(xdrs, &objp->as_merr)) {
    return (FALSE);
  }
  if (!xdr_int(xdrs, &objp->as_uerr)) {
    return (FALSE);
  }
  return (TRUE);
}



bool_t
xdr_amq_mount_tree_list(XDR *xdrs, amq_mount_tree_list *objp)
{
  if (!xdr_array(xdrs,
		 (char **) &objp->amq_mount_tree_list_val,
		 (u_int *) &objp->amq_mount_tree_list_len,
		 ~0,
		 sizeof(amq_mount_tree_p),
		 (XDRPROC_T_TYPE) xdr_amq_mount_tree_p)) {
    return (FALSE);
  }
  return (TRUE);
}



/*
 * Compute length of list
 */
bool_t
xdr_amq_mount_info_qelem(XDR *xdrs, qelem *qhead)
{
  mntfs *mf;
  u_int len = 0;

  for (mf = AM_LAST(mntfs, qhead); mf != HEAD(mntfs, qhead); mf = PREV(mntfs, mf)) {
    if (!(mf->mf_ops->fs_flags & FS_AMQINFO))
      continue;
    len++;
  }
  xdr_u_int(xdrs, &len);

  /*
   * Send individual data items
   */
  for (mf = AM_LAST(mntfs, qhead); mf != HEAD(mntfs, qhead); mf = PREV(mntfs, mf)) {
    int up;
    if (!(mf->mf_ops->fs_flags & FS_AMQINFO))
      continue;

    if (!xdr_amq_string(xdrs, &mf->mf_ops->fs_type)) {
      return (FALSE);
    }
    if (!xdr_amq_string(xdrs, &mf->mf_mount)) {
      return (FALSE);
    }
    if (!xdr_amq_string(xdrs, &mf->mf_info)) {
      return (FALSE);
    }
    if (!xdr_amq_string(xdrs, &mf->mf_server->fs_host)) {
      return (FALSE);
    }
    if (!xdr_int(xdrs, &mf->mf_error)) {
      return (FALSE);
    }
    if (!xdr_int(xdrs, &mf->mf_refc)) {
      return (FALSE);
    }
    if (mf->mf_server->fs_flags & FSF_ERROR)
      up = 0;
    else
      switch (mf->mf_server->fs_flags & (FSF_DOWN | FSF_VALID)) {
      case FSF_DOWN | FSF_VALID:
	up = 0;
	break;
      case FSF_VALID:
	up = 1;
	break;
      default:
	up = -1;
	break;
      }
    if (!xdr_int(xdrs, &up)) {
      return (FALSE);
    }
  }
  return (TRUE);
}


bool_t
xdr_pri_free(XDRPROC_T_TYPE xdr_args, caddr_t args_ptr)
{
  XDR xdr;

  xdr.x_op = XDR_FREE;
  return ((*xdr_args) (&xdr, (caddr_t *) args_ptr));
}
