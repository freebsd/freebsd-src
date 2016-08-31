/*
 * Copyright (c) 1999-2003 Ion Badulescu
 * Copyright (c) 1997-2014 Erez Zadok
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 * File: am-utils/conf/autofs/autofs_solaris_v1.c
 *
 */

/*
 * Automounter filesystem
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

#ifdef HAVE_FS_AUTOFS

/*
 * MACROS:
 */
#ifndef AUTOFS_NULL
# define AUTOFS_NULL	NULLPROC
#endif /* not AUTOFS_NULL */

/*
 * STRUCTURES:
 */

/*
 * VARIABLES:
 */

/* forward declarations */
# ifndef HAVE_XDR_MNTREQUEST
bool_t xdr_mntrequest(XDR *xdrs, mntrequest *objp);
# endif /* not HAVE_XDR_MNTREQUEST */
# ifndef HAVE_XDR_MNTRES
bool_t xdr_mntres(XDR *xdrs, mntres *objp);
# endif /* not HAVE_XDR_MNTRES */
# ifndef HAVE_XDR_UMNTREQUEST
bool_t xdr_umntrequest(XDR *xdrs, umntrequest *objp);
# endif /* not HAVE_XDR_UMNTREQUEST */
# ifndef HAVE_XDR_UMNTRES
bool_t xdr_umntres(XDR *xdrs, umntres *objp);
# endif /* not HAVE_XDR_UMNTRES */
static int autofs_mount_1_req(struct mntrequest *mr, struct mntres *result, struct authunix_parms *cred, SVCXPRT *transp);
static int autofs_unmount_1_req(struct umntrequest *ur, struct umntres *result, struct authunix_parms *cred, SVCXPRT *transp);

/****************************************************************************
 *** VARIABLES                                                            ***
 ****************************************************************************/

/****************************************************************************
 *** FUNCTIONS                                                            ***
 ****************************************************************************/

/*
 * AUTOFS XDR FUNCTIONS:
 */

#ifndef HAVE_XDR_MNTREQUEST
bool_t
xdr_mntrequest(XDR *xdrs, mntrequest *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_mntrequest:");

  if (!xdr_string(xdrs, &objp->name, A_MAXNAME))
    return (FALSE);

  if (!xdr_string(xdrs, &objp->map, A_MAXNAME))
    return (FALSE);

  if (!xdr_string(xdrs, &objp->opts, A_MAXOPTS))
    return (FALSE);

  if (!xdr_string(xdrs, &objp->path, A_MAXPATH))
    return (FALSE);

  return (TRUE);
}
#endif /* not HAVE_XDR_MNTREQUEST */


#ifndef HAVE_XDR_MNTRES
bool_t
xdr_mntres(XDR *xdrs, mntres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_mntres:");

  if (!xdr_int(xdrs, &objp->status))
    return (FALSE);

  return (TRUE);
}
# endif /* not HAVE_XDR_MNTRES */


#ifndef HAVE_XDR_UMNTREQUEST
bool_t
xdr_umntrequest(XDR *xdrs, umntrequest *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_umntrequest:");

  if (!xdr_int(xdrs, (int *) &objp->isdirect))
    return (FALSE);

  if (!xdr_u_int(xdrs, (u_int *) &objp->devid))
    return (FALSE);

#ifdef HAVE_UMNTREQUEST_RDEVID
  if (!xdr_u_long(xdrs, &objp->rdevid))
    return (FALSE);
#endif /* HAVE_UMNTREQUEST_RDEVID */

  if (!xdr_pointer(xdrs, (char **) &objp->next, sizeof(umntrequest), (XDRPROC_T_TYPE) xdr_umntrequest))
    return (FALSE);

  return (TRUE);
}
#endif /* not HAVE_XDR_UMNTREQUEST */


#ifndef HAVE_XDR_UMNTRES
bool_t
xdr_umntres(XDR *xdrs, umntres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_mntres:");

  if (!xdr_int(xdrs, &objp->status))
    return (FALSE);

  return (TRUE);
}
#endif /* not HAVE_XDR_UMNTRES */


/*
 * AUTOFS RPC methods
 */

static int
autofs_mount_1_req(struct mntrequest *m,
		   struct mntres *res,
		   struct authunix_parms *cred,
		   SVCXPRT *transp)
{
  int err = 0;
  int isdirect = 0;
  am_node *mp, *ap;
  mntfs *mf;

  dlog("MOUNT REQUEST: name=%s map=%s opts=%s path=%s",
       m->name, m->map, m->opts, m->path);

  /* find the effective uid/gid from RPC request */
  xsnprintf(opt_uid, sizeof(uid_str), "%d", (int) cred->aup_uid);
  xsnprintf(opt_gid, sizeof(gid_str), "%d", (int) cred->aup_gid);

  mp = find_ap(m->path);
  if (!mp) {
    plog(XLOG_ERROR, "map %s not found", m->path);
    err = ENOENT;
    goto out;
  }

  mf = mp->am_al->al_mnt;
  isdirect = (mf->mf_fsflags & FS_DIRECT) ? 1 : 0;
  ap = mf->mf_ops->lookup_child(mp, m->name + isdirect, &err, VLOOK_CREATE);
  if (ap && err < 0)
    ap = mf->mf_ops->mount_child(ap, &err);
  if (ap == NULL) {
    if (err < 0) {
      /* we're working on it */
      amd_stats.d_drops++;
      return 1;
    }
    err = ENOENT;
    goto out;
  }

out:
  if (err) {
    if (isdirect) {
      /* direct mount */
      plog(XLOG_ERROR, "mount of %s failed", m->path);
    } else {
      /* indirect mount */
      plog(XLOG_ERROR, "mount of %s/%s failed", m->path, m->name);
    }
  }

  dlog("MOUNT REPLY: status=%d (%s)", err, strerror(err));

  res->status = err;
  return 0;
}


static int
autofs_unmount_1_req(struct umntrequest *ul,
		     struct umntres *res,
		     struct authunix_parms *cred,
		     SVCXPRT *transp)
{
  int mapno, err;
  am_node *mp = NULL;

  dlog("UNMOUNT REQUEST: dev=%lx rdev=%lx %s",
       (u_long) ul->devid,
       (u_long) ul->rdevid,
       ul->isdirect ? "direct" : "indirect");

  /* by default, and if not found, succeed */
  res->status = 0;

  for (mapno = 0; ; mapno++) {
    mp = get_exported_ap(mapno);
    if (!mp)
      break;
    if (mp->am_dev == ul->devid &&
	(ul->rdevid == 0 || mp->am_rdev == ul->rdevid))
      break;
  }

  if (mp) {
    /* save RPC context */
    if (!mp->am_transp && transp) {
      mp->am_transp = (SVCXPRT *) xmalloc(sizeof(SVCXPRT));
      *(mp->am_transp) = *transp;
    }

    mapno = mp->am_mapno;
    err = unmount_mp(mp);

    if (err)
      /* backgrounded, don't reply yet */
      return 1;

    if (get_exported_ap(mapno))
      /* unmounting failed, tell the kernel */
      res->status = 1;
  }

  dlog("UNMOUNT REPLY: status=%d", res->status);
  return 0;
}


/****************************************************************************/
/* autofs program dispatcher */
static void
autofs_program_1(struct svc_req *rqstp, SVCXPRT *transp)
{
  union {
    mntrequest autofs_mount_1_arg;
    umntrequest autofs_umount_1_arg;
  } argument;
  union {
    mntres mount_res;
    umntres umount_res;
  } result;
  int ret;

  bool_t (*xdr_argument)();
  bool_t (*xdr_result)();
  int (*local)();

  current_transp = transp;

  switch (rqstp->rq_proc) {

  case AUTOFS_NULL:
    svc_sendreply(transp,
		  (XDRPROC_T_TYPE) xdr_void,
		  (SVC_IN_ARG_TYPE) NULL);
    return;

  case AUTOFS_MOUNT:
    xdr_argument = xdr_mntrequest;
    xdr_result = xdr_mntres;
    local = autofs_mount_1_req;
    break;

  case AUTOFS_UNMOUNT:
    xdr_argument = xdr_umntrequest;
    xdr_result = xdr_umntres;
    local = autofs_unmount_1_req;
    break;

  default:
    svcerr_noproc(transp);
    return;
  }

  memset((char *) &argument, 0, sizeof(argument));
  if (!svc_getargs(transp,
		   (XDRPROC_T_TYPE) xdr_argument,
		   (SVC_IN_ARG_TYPE) &argument)) {
    plog(XLOG_ERROR,
	 "AUTOFS xdr decode failed for %d %d %d",
	 (int) rqstp->rq_prog, (int) rqstp->rq_vers, (int) rqstp->rq_proc);
    svcerr_decode(transp);
    return;
  }

  memset((char *)&result, 0, sizeof(result));
  ret = (*local) (&argument, &result, rqstp, transp);

  current_transp = NULL;

  /* send reply only if the RPC method returned 0 */
  if (!ret) {
    if (!svc_sendreply(transp,
		       (XDRPROC_T_TYPE) xdr_result,
		       (SVC_IN_ARG_TYPE) &result)) {
      svcerr_systemerr(transp);
    }
  }

  if (!svc_freeargs(transp,
		    (XDRPROC_T_TYPE) xdr_argument,
		    (SVC_IN_ARG_TYPE) &argument)) {
    plog(XLOG_FATAL, "unable to free rpc arguments in autofs_program_1");
  }
}


int
autofs_get_fh(am_node *mp)
{
  autofs_fh_t *fh;
  char buf[MAXHOSTNAMELEN];
  mntfs *mf = mp->am_al->al_mnt;
  struct utsname utsname;

  plog(XLOG_DEBUG, "autofs_get_fh for %s", mp->am_path);
  fh = ALLOC(autofs_fh_t);
  memset((voidp) fh, 0, sizeof(autofs_fh_t)); /* Paranoid */

  /*
   * SET MOUNT ARGS
   */
  if (uname(&utsname) < 0) {
    xstrlcpy(buf, "localhost.autofs", sizeof(buf));
  } else {
    xstrlcpy(buf, utsname.nodename, sizeof(buf));
    xstrlcat(buf, ".autofs", sizeof(buf));
  }
#ifdef HAVE_AUTOFS_ARGS_T_ADDR
  fh->addr.buf = xstrdup(buf);
  fh->addr.len = fh->addr.maxlen = strlen(buf);
#endif /* HAVE_AUTOFS_ARGS_T_ADDR */

  fh->direct = (mf->mf_fsflags & FS_DIRECT) ? 1 : 0;
  fh->rpc_to = 1;		/* XXX: arbitrary */
  fh->mount_to = mp->am_timeo;
  fh->path = mp->am_path;
  fh->opts = "";		/* XXX: arbitrary */
  fh->map = mp->am_path;	/* this is what we get back in readdir */

  mp->am_autofs_fh = fh;
  return 0;
}


void
autofs_mounted(am_node *mp)
{
  /* We don't want any timeouts on autofs nodes */
  mp->am_autofs_ttl = NEVER;
}


void
autofs_release_fh(am_node *mp)
{
  autofs_fh_t *fh = mp->am_autofs_fh;
#ifdef HAVE_AUTOFS_ARGS_T_ADDR
  XFREE(fh->addr.buf);
#endif /* HAVE_AUTOFS_ARGS_T_ADDR */
  XFREE(fh);
  mp->am_autofs_fh = NULL;
}


void
autofs_get_mp(am_node *mp)
{
  /* nothing to do */
}


void
autofs_release_mp(am_node *mp)
{
  /* nothing to do */
}


void
autofs_add_fdset(fd_set *readfds)
{
  /* nothing to do */
}


int
autofs_handle_fdset(fd_set *readfds, int nsel)
{
  /* nothing to do */
  return nsel;
}


/*
 * Create the autofs service for amd
 */
int
create_autofs_service(void)
{
  dlog("creating autofs service listener");
  return register_autofs_service(AUTOFS_CONFTYPE, autofs_program_1);
}


int
destroy_autofs_service(void)
{
  dlog("destroying autofs service listener");
  return unregister_autofs_service(AUTOFS_CONFTYPE);
}


int
autofs_mount_fs(am_node *mp, mntfs *mf)
{
  int err = 0;
  char *target, *target2 = NULL;
  char *space_hack = autofs_strdup_space_hack(mp->am_path);
  struct stat buf;

  if (mf->mf_flags & MFF_ON_AUTOFS) {
    if ((err = mkdir(space_hack, 0555)))
      goto out;
  }

  /*
   * For sublinks, we could end up here with an already mounted f/s.
   * Don't do anything in that case.
   */
  if (!(mf->mf_flags & MFF_MOUNTED))
    err = mf->mf_ops->mount_fs(mp, mf);

  if (err) {
    if (mf->mf_flags & MFF_ON_AUTOFS)
      rmdir(space_hack);
    errno = err;
    goto out;
  }

  /*
   * Autofs v1 doesn't support symlinks,
   * so we ignore the CFM_AUTOFS_USE_LOFS flag
   */
  if (mf->mf_flags & MFF_ON_AUTOFS)
    /* Nothing to do */
    goto out;

  if (mp->am_link)
    target = mp->am_link;
  else
    target = mf->mf_mount;

  if (target[0] != '/')
    target2 = str3cat(NULL, mp->am_parent->am_path, "/", target);
  else
    target2 = xstrdup(target);

  plog(XLOG_INFO, "autofs: converting from link to lofs (%s -> %s)", mp->am_path, target2);
  /*
   * we need to stat() the destination, because the bind mount does not
   * follow symlinks and/or allow for non-existent destinations.
   *
   * WARNING: we will deadlock if this function is called from the master
   * amd process and it happens to trigger another auto mount. Therefore,
   * this function should be called only from a child amd process, or
   * at the very least it should not be called from the parent unless we
   * know for sure that it won't cause a recursive mount. We refuse to
   * cause the recursive mount anyway if called from the parent amd.
   */
  if (!foreground) {
    if ((err = stat(target2, &buf)))
      goto out;
  }
  if ((err = lstat(target2, &buf)))
    goto out;

  if ((err = mkdir(space_hack, 0555)))
    goto out;

  if ((err = mount_lofs(mp->am_path, target2, mf->mf_mopts, 1))) {
    errno = err;
    goto out;
  }

 out:
  XFREE(space_hack);
  if (target2)
    XFREE(target2);

  if (err)
    return errno;
  return 0;
}


int
autofs_umount_fs(am_node *mp, mntfs *mf)
{
  int err = 0;
  char *space_hack = autofs_strdup_space_hack(mp->am_path);

  /*
   * Autofs v1 doesn't support symlinks,
   * so we ignore the CFM_AUTOFS_USE_LOFS flag
   */
  if (!(mf->mf_flags & MFF_ON_AUTOFS)) {
    err = UMOUNT_FS(mp->am_path, mnttab_file_name, 1);
    if (err)
      goto out;
    rmdir(space_hack);
  }

  /*
   * Multiple sublinks could reference this f/s.
   * Don't actually unmount it unless we're holding the last reference.
   */
  if (mf->mf_refc == 1) {
    if ((err = mf->mf_ops->umount_fs(mp, mf)))
      goto out;

    if (mf->mf_flags & MFF_ON_AUTOFS)
      rmdir(space_hack);
  }

 out:
  XFREE(space_hack);
  return err;
}


int
autofs_umount_succeeded(am_node *mp)
{
  umntres res;
  SVCXPRT *transp = mp->am_transp;

  if (transp) {
    res.status = 0;

    if (!svc_sendreply(transp,
		       (XDRPROC_T_TYPE) xdr_umntres,
		       (SVC_IN_ARG_TYPE) &res))
      svcerr_systemerr(transp);

    dlog("Quick reply sent for %s", mp->am_al->al_mnt->mf_mount);
    XFREE(transp);
    mp->am_transp = NULL;
  }

  plog(XLOG_INFO, "autofs: unmounting %s succeeded", mp->am_path);
  return 0;
}


int
autofs_umount_failed(am_node *mp)
{
  umntres res;
  SVCXPRT *transp = mp->am_transp;

  if (transp) {
    res.status = 1;

    if (!svc_sendreply(transp,
		       (XDRPROC_T_TYPE) xdr_umntres,
		       (SVC_IN_ARG_TYPE) &res))
      svcerr_systemerr(transp);

    dlog("Quick reply sent for %s", mp->am_al->al_mnt->mf_mount);
    XFREE(transp);
    mp->am_transp = NULL;
  }

  plog(XLOG_INFO, "autofs: unmounting %s failed", mp->am_path);
  return 0;
}


void
autofs_mount_succeeded(am_node *mp)
{
  SVCXPRT *transp = mp->am_transp;
  struct stat stb;
  char *space_hack;

  if (transp) {
    /* this was a mount request */
    mntres res;
    res.status = 0;

    if (!svc_sendreply(transp,
		       (XDRPROC_T_TYPE) xdr_mntres,
		       (SVC_IN_ARG_TYPE) &res))
      svcerr_systemerr(transp);

    dlog("Quick reply sent for %s", mp->am_al->al_mnt->mf_mount);
    XFREE(transp);
    mp->am_transp = NULL;
  }

  space_hack = autofs_strdup_space_hack(mp->am_path);
  if (!lstat(space_hack, &stb)) {
    mp->am_dev = stb.st_dev;
    mp->am_rdev = stb.st_rdev;
  }
  XFREE(space_hack);
  /* don't expire the entries -- the kernel will do it for us */
  mp->am_flags |= AMF_NOTIMEOUT;

  plog(XLOG_INFO, "autofs: mounting %s succeeded", mp->am_path);
}


void
autofs_mount_failed(am_node *mp)
{
  SVCXPRT *transp = mp->am_transp;

  if (transp) {
    /* this was a mount request */
    mntres res;
    res.status = ENOENT;

    if (!svc_sendreply(transp,
		       (XDRPROC_T_TYPE) xdr_mntres,
		       (SVC_IN_ARG_TYPE) &res))
      svcerr_systemerr(transp);

    dlog("Quick reply sent for %s", mp->am_al->al_mnt->mf_mount);
    XFREE(transp);
    mp->am_transp = NULL;
  }

  plog(XLOG_INFO, "autofs: mounting %s failed", mp->am_path);
}


void
autofs_get_opts(char *opts, size_t l, autofs_fh_t *fh)
{
  xsnprintf(opts, l, "%sdirect",
	    fh->direct ? "" : "in");
}


int
autofs_compute_mount_flags(mntent_t *mntp)
{
  /* Must use overlay mounts */
  return MNT2_GEN_OPT_OVERLAY;
}


void autofs_timeout_mp(am_node *mp)
{
  /* We don't want any timeouts on autofs nodes */
  mp->am_autofs_ttl = NEVER;
}
#endif /* HAVE_FS_AUTOFS */
