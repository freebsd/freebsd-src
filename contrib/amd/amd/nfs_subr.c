/*
 * Copyright (c) 1997-2003 Erez Zadok
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
 * $Id: nfs_subr.c,v 1.6.2.5 2002/12/27 22:44:39 ezk Exp $
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/*
 * Convert from UN*X to NFS error code.
 * Some systems like linux define their own (see
 * conf/mount/mount_linux.h).
 */
#ifndef nfs_error
# define nfs_error(e) ((nfsstat)(e))
#endif /* nfs_error */

/* forward declarations */
static void count_map_entries(const am_node *mp, u_int *out_blocks, u_int *out_bfree, u_int *out_bavail);


static char *
do_readlink(am_node *mp, int *error_return, nfsattrstat **attrpp)
{
  char *ln;

  /*
   * If there is a readlink method, then use
   * that, otherwise if a link exists use
   * that, otherwise use the mount point.
   */
  if (mp->am_mnt->mf_ops->readlink) {
    int retry = 0;
    mp = (*mp->am_mnt->mf_ops->readlink) (mp, &retry);
    if (mp == 0) {
      *error_return = retry;
      return 0;
    }
    /* reschedule_timeout_mp(); */
  }

  if (mp->am_link) {
    ln = mp->am_link;
  } else {
    ln = mp->am_mnt->mf_mount;
  }
  if (attrpp)
    *attrpp = &mp->am_attr;

  return ln;
}


voidp
nfsproc_null_2_svc(voidp argp, struct svc_req *rqstp)
{
  static char res;

  return (voidp) &res;
}


nfsattrstat *
nfsproc_getattr_2_svc(am_nfs_fh *argp, struct svc_req *rqstp)
{
  static nfsattrstat res;
  am_node *mp;
  int retry;
  time_t now = clocktime();

#ifdef DEBUG
  amuDebug(D_TRACE)
    plog(XLOG_DEBUG, "getattr:");
#endif /* DEBUG */

  mp = fh_to_mp2(argp, &retry);
  if (mp == 0) {

#ifdef DEBUG
    amuDebug(D_TRACE)
      plog(XLOG_DEBUG, "\tretry=%d", retry);
#endif /* DEBUG */

    if (retry < 0) {
      amd_stats.d_drops++;
      return 0;
    }
    res.ns_status = nfs_error(retry);
  } else {
    nfsattrstat *attrp = &mp->am_attr;

#ifdef DEBUG
    amuDebug(D_TRACE)
      plog(XLOG_DEBUG, "\tstat(%s), size = %d, mtime=%ld",
	   mp->am_path,
	   (int) attrp->ns_u.ns_attr_u.na_size,
	   (long) attrp->ns_u.ns_attr_u.na_mtime.nt_seconds);
#endif /* DEBUG */

    /* Delay unmount of what was looked up */
    if (mp->am_timeo_w < 4 * gopt.am_timeo_w)
      mp->am_timeo_w += gopt.am_timeo_w;
    mp->am_ttl = now + mp->am_timeo_w;

    mp->am_stats.s_getattr++;
    return attrp;
  }

  return &res;
}


nfsattrstat *
nfsproc_setattr_2_svc(nfssattrargs *argp, struct svc_req *rqstp)
{
  static nfsattrstat res;

  if (!fh_to_mp(&argp->sag_fhandle))
    res.ns_status = nfs_error(ESTALE);
  else
    res.ns_status = nfs_error(EROFS);

  return &res;
}


voidp
nfsproc_root_2_svc(voidp argp, struct svc_req *rqstp)
{
  static char res;

  return (voidp) &res;
}


nfsdiropres *
nfsproc_lookup_2_svc(nfsdiropargs *argp, struct svc_req *rqstp)
{
  static nfsdiropres res;
  am_node *mp;
  int retry;
  uid_t uid;
  gid_t gid;

#ifdef DEBUG
  amuDebug(D_TRACE)
    plog(XLOG_DEBUG, "lookup:");
#endif /* DEBUG */

  /* finally, find the effective uid/gid from RPC request */
  if (getcreds(rqstp, &uid, &gid, nfsxprt) < 0)
    plog(XLOG_ERROR, "cannot get uid/gid from RPC credentials");
  sprintf(opt_uid, "%d", (int) uid);
  sprintf(opt_gid, "%d", (int) gid);

  mp = fh_to_mp2(&argp->da_fhandle, &retry);
  if (mp == 0) {
    if (retry < 0) {
      amd_stats.d_drops++;
      return 0;
    }
    res.dr_status = nfs_error(retry);
  } else {
    int error;
    am_node *ap;
#ifdef DEBUG
    amuDebug(D_TRACE)
      plog(XLOG_DEBUG, "\tlookuppn(%s, %s)", mp->am_path, argp->da_name);
#endif /* DEBUG */
    ap = (*mp->am_mnt->mf_ops->lookuppn) (mp, argp->da_name, &error, VLOOK_CREATE);
    if (ap == 0) {
      if (error < 0) {
	amd_stats.d_drops++;
	return 0;
      }
      res.dr_status = nfs_error(error);
    } else {
      /*
       * XXX: EXPERIMENTAL! Delay unmount of what was looked up.  This
       * should reduce the chance for race condition between unmounting an
       * entry synchronously, and re-mounting it asynchronously.
       */
      if (ap->am_ttl < mp->am_ttl)
 	ap->am_ttl = mp->am_ttl;
      mp_to_fh(ap, &res.dr_u.dr_drok_u.drok_fhandle);
      res.dr_u.dr_drok_u.drok_attributes = ap->am_fattr;
      res.dr_status = NFS_OK;
    }
    mp->am_stats.s_lookup++;
    /* reschedule_timeout_mp(); */
  }

  return &res;
}


void
quick_reply(am_node *mp, int error)
{
  SVCXPRT *transp = mp->am_transp;
  nfsdiropres res;
  xdrproc_t xdr_result = (xdrproc_t) xdr_diropres;

  /*
   * If there's a transp structure then we can reply to the client's
   * nfs lookup request.
   */
  if (transp) {
    if (error == 0) {
      /*
       * Construct a valid reply to a lookup request.  Same
       * code as in nfsproc_lookup_2_svc() above.
       */
      mp_to_fh(mp, &res.dr_u.dr_drok_u.drok_fhandle);
      res.dr_u.dr_drok_u.drok_attributes = mp->am_fattr;
      res.dr_status = NFS_OK;
    } else
      /*
       * Return the error that was passed to us.
       */
      res.dr_status = nfs_error(error);

    /*
     * Send off our reply
     */
    if (!svc_sendreply(transp, (XDRPROC_T_TYPE) xdr_result, (SVC_IN_ARG_TYPE) & res))
      svcerr_systemerr(transp);

    /*
     * Free up transp.  It's only used for one reply.
     */
    XFREE(transp);
    mp->am_transp = NULL;
#ifdef DEBUG
    dlog("Quick reply sent for %s", mp->am_mnt->mf_mount);
#endif /* DEBUG */
  }
}


nfsreadlinkres *
nfsproc_readlink_2_svc(am_nfs_fh *argp, struct svc_req *rqstp)
{
  static nfsreadlinkres res;
  am_node *mp;
  int retry;

#ifdef DEBUG
  amuDebug(D_TRACE)
    plog(XLOG_DEBUG, "readlink:");
#endif /* DEBUG */

  mp = fh_to_mp2(argp, &retry);
  if (mp == 0) {
  readlink_retry:
    if (retry < 0) {
      amd_stats.d_drops++;
      return 0;
    }
    res.rlr_status = nfs_error(retry);
  } else {
    char *ln = do_readlink(mp, &retry, (nfsattrstat **) 0);
    if (ln == 0)
      goto readlink_retry;
    res.rlr_status = NFS_OK;
#ifdef DEBUG
    amuDebug(D_TRACE)
      if (ln)
	plog(XLOG_DEBUG, "\treadlink(%s) = %s", mp->am_path, ln);
#endif /* DEBUG */
    res.rlr_u.rlr_data_u = ln;
    mp->am_stats.s_readlink++;
  }

  return &res;
}


nfsreadres *
nfsproc_read_2_svc(nfsreadargs *argp, struct svc_req *rqstp)
{
  static nfsreadres res;

  memset((char *) &res, 0, sizeof(res));
  res.rr_status = nfs_error(EACCES);

  return &res;
}


voidp
nfsproc_writecache_2_svc(voidp argp, struct svc_req *rqstp)
{
  static char res;

  return (voidp) &res;
}


nfsattrstat *
nfsproc_write_2_svc(nfswriteargs *argp, struct svc_req *rqstp)
{
  static nfsattrstat res;

  if (!fh_to_mp(&argp->wra_fhandle))
    res.ns_status = nfs_error(ESTALE);
  else
    res.ns_status = nfs_error(EROFS);

  return &res;
}


nfsdiropres *
nfsproc_create_2_svc(nfscreateargs *argp, struct svc_req *rqstp)
{
  static nfsdiropres res;

  if (!fh_to_mp(&argp->ca_where.da_fhandle))
    res.dr_status = nfs_error(ESTALE);
  else
    res.dr_status = nfs_error(EROFS);

  return &res;
}


static nfsstat *
unlink_or_rmdir(nfsdiropargs *argp, struct svc_req *rqstp, int unlinkp)
{
  static nfsstat res;
  int retry;

  am_node *mp = fh_to_mp3(&argp->da_fhandle, &retry, VLOOK_DELETE);
  if (mp == 0) {
    if (retry < 0) {
      amd_stats.d_drops++;
      return 0;
    }
    res = nfs_error(retry);
    goto out;
  }

  if (mp->am_fattr.na_type != NFDIR) {
    res = nfs_error(ENOTDIR);
    goto out;
  }

#ifdef DEBUG
  amuDebug(D_TRACE)
    plog(XLOG_DEBUG, "\tremove(%s, %s)", mp->am_path, argp->da_name);
#endif /* DEBUG */

  mp = (*mp->am_mnt->mf_ops->lookuppn) (mp, argp->da_name, &retry, VLOOK_DELETE);
  if (mp == 0) {
    /*
     * Ignore retries...
     */
    if (retry < 0)
      retry = 0;
    /*
     * Usual NFS workaround...
     */
    else if (retry == ENOENT)
      retry = 0;
    res = nfs_error(retry);
  } else {
    forcibly_timeout_mp(mp);
    res = NFS_OK;
  }

out:
  return &res;
}


nfsstat *
nfsproc_remove_2_svc(nfsdiropargs *argp, struct svc_req *rqstp)
{
  return unlink_or_rmdir(argp, rqstp, TRUE);
}


nfsstat *
nfsproc_rename_2_svc(nfsrenameargs *argp, struct svc_req *rqstp)
{
  static nfsstat res;

  if (!fh_to_mp(&argp->rna_from.da_fhandle) || !fh_to_mp(&argp->rna_to.da_fhandle))
    res = nfs_error(ESTALE);
  /*
   * If the kernel is doing clever things with referenced files
   * then let it pretend...
   */
  else if (NSTREQ(argp->rna_to.da_name, ".nfs", 4))
    res = NFS_OK;
  /*
   * otherwise a failure
   */
  else
    res = nfs_error(EROFS);

  return &res;
}


nfsstat *
nfsproc_link_2_svc(nfslinkargs *argp, struct svc_req *rqstp)
{
  static nfsstat res;

  if (!fh_to_mp(&argp->la_fhandle) || !fh_to_mp(&argp->la_to.da_fhandle))
    res = nfs_error(ESTALE);
  else
    res = nfs_error(EROFS);

  return &res;
}


nfsstat *
nfsproc_symlink_2_svc(nfssymlinkargs *argp, struct svc_req *rqstp)
{
  static nfsstat res;

  if (!fh_to_mp(&argp->sla_from.da_fhandle))
    res = nfs_error(ESTALE);
  else
    res = nfs_error(EROFS);

  return &res;
}


nfsdiropres *
nfsproc_mkdir_2_svc(nfscreateargs *argp, struct svc_req *rqstp)
{
  static nfsdiropres res;

  if (!fh_to_mp(&argp->ca_where.da_fhandle))
    res.dr_status = nfs_error(ESTALE);
  else
    res.dr_status = nfs_error(EROFS);

  return &res;
}


nfsstat *
nfsproc_rmdir_2_svc(nfsdiropargs *argp, struct svc_req *rqstp)
{
  return unlink_or_rmdir(argp, rqstp, FALSE);
}


nfsreaddirres *
nfsproc_readdir_2_svc(nfsreaddirargs *argp, struct svc_req *rqstp)
{
  static nfsreaddirres res;
  static nfsentry e_res[MAX_READDIR_ENTRIES];
  am_node *mp;
  int retry;

#ifdef DEBUG
  amuDebug(D_TRACE)
    plog(XLOG_DEBUG, "readdir:");
#endif /* DEBUG */

  mp = fh_to_mp2(&argp->rda_fhandle, &retry);
  if (mp == 0) {
    if (retry < 0) {
      amd_stats.d_drops++;
      return 0;
    }
    res.rdr_status = nfs_error(retry);
  } else {
#ifdef DEBUG
    amuDebug(D_TRACE)
      plog(XLOG_DEBUG, "\treaddir(%s)", mp->am_path);
#endif /* DEBUG */
    res.rdr_status = nfs_error((*mp->am_mnt->mf_ops->readdir)
			   (mp, argp->rda_cookie,
			    &res.rdr_u.rdr_reply_u, e_res, argp->rda_count));
    mp->am_stats.s_readdir++;
  }

  return &res;
}


nfsstatfsres *
nfsproc_statfs_2_svc(am_nfs_fh *argp, struct svc_req *rqstp)
{
  static nfsstatfsres res;
  am_node *mp;
  int retry;
  mntent_t mnt;

#ifdef DEBUG
  amuDebug(D_TRACE)
    plog(XLOG_DEBUG, "statfs:");
#endif /* DEBUG */

  mp = fh_to_mp2(argp, &retry);
  if (mp == 0) {
    if (retry < 0) {
      amd_stats.d_drops++;
      return 0;
    }
    res.sfr_status = nfs_error(retry);
  } else {
    nfsstatfsokres *fp;
#ifdef DEBUG
    amuDebug(D_TRACE)
      plog(XLOG_DEBUG, "\tstat_fs(%s)", mp->am_path);
#endif /* DEBUG */

    /*
     * just return faked up file system information
     */
    fp = &res.sfr_u.sfr_reply_u;

    fp->sfrok_tsize = 1024;
    fp->sfrok_bsize = 1024;

    /* check if map is browsable and show_statfs_entries=yes  */
    if ((gopt.flags & CFM_SHOW_STATFS_ENTRIES) &&
	mp->am_mnt && mp->am_mnt->mf_mopts) {
      mnt.mnt_opts = mp->am_mnt->mf_mopts;
      if (hasmntopt(&mnt, "browsable")) {
	count_map_entries(mp,
			  &fp->sfrok_blocks,
			  &fp->sfrok_bfree,
			  &fp->sfrok_bavail);
      }
    } else {
      fp->sfrok_blocks = 0; /* set to 1 if you don't want empty automounts */
      fp->sfrok_bfree = 0;
      fp->sfrok_bavail = 0;
    }

    res.sfr_status = NFS_OK;
    mp->am_stats.s_statfs++;
  }

  return &res;
}


/*
 * count how many total entries there are in a map, and how many
 * of them are in use.
 */
static void
count_map_entries(const am_node *mp, u_int *out_blocks, u_int *out_bfree, u_int *out_bavail)
{
  u_int blocks, bfree, bavail, i;
  mntfs *mf;
  mnt_map *mmp;
  kv *k;

  blocks = bfree = bavail = 0;
  if (!mp)
    goto out;
  mf = mp->am_mnt;
  if (!mf)
    goto out;
  mmp = (mnt_map *) mf->mf_private;
  if (!mmp)
    goto out;

  /* iterate over keys */
  for (i = 0; i < NKVHASH; i++) {
    for (k = mmp->kvhash[i]; k ; k = k->next) {
      if (!k->key)
	continue;
      blocks++;
      /*
       * XXX: Need to count how many are actively in use and recompute
       * bfree and bavail based on it.
       */
    }
  }

out:
  *out_blocks = blocks;
  *out_bfree = bfree;
  *out_bavail = bavail;
}
