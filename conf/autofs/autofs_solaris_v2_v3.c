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
 * File: am-utils/conf/autofs/autofs_solaris_v2_v3.c
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

/*
 * MACROS:
 */
#ifndef AUTOFS_NULL
# define AUTOFS_NULL	NULLPROC
#endif /* not AUTOFS_NULL */

/*
 * STRUCTURES:
 */

struct amd_rddirres {
  enum autofs_res rd_status;
  u_long rd_bufsize;
  nfsdirlist rd_dl;
};
typedef struct amd_rddirres amd_rddirres;

/*
 * VARIABLES:
 */

SVCXPRT *autofs_xprt = NULL;

/* forward declarations */
bool_t xdr_umntrequest(XDR *xdrs, umntrequest *objp);
bool_t xdr_umntres(XDR *xdrs, umntres *objp);
bool_t xdr_autofs_lookupargs(XDR *xdrs, autofs_lookupargs *objp);
bool_t xdr_autofs_mountres(XDR *xdrs, autofs_mountres *objp);
bool_t xdr_autofs_lookupres(XDR *xdrs, autofs_lookupres *objp);
bool_t xdr_autofs_rddirargs(XDR *xdrs, autofs_rddirargs *objp);
static bool_t xdr_amd_rddirres(XDR *xdrs, amd_rddirres *objp);

/*
 * These exist only in the AutoFS V2 protocol.
 */
#ifdef AUTOFS_POSTUNMOUNT
bool_t xdr_postumntreq(XDR *xdrs, postumntreq *objp);
bool_t xdr_postumntres(XDR *xdrs, postumntres *objp);
bool_t xdr_postmountreq(XDR *xdrs, postmountreq *objp);
bool_t xdr_postmountres(XDR *xdrs, postmountres *objp);
#endif /* AUTOFS_POSTUMOUNT */

/*
 * AUTOFS XDR FUNCTIONS:
 */

bool_t
xdr_autofs_stat(XDR *xdrs, autofs_stat *objp)
{
  if (!xdr_enum(xdrs, (enum_t *)objp))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_autofs_action(XDR *xdrs, autofs_action *objp)
{
  if (!xdr_enum(xdrs, (enum_t *)objp))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_linka(XDR *xdrs, linka *objp)
{
  if (!xdr_string(xdrs, &objp->dir, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->link, AUTOFS_MAXPATHLEN))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_autofs_netbuf(XDR *xdrs, struct netbuf *objp)
{
  bool_t dummy;

  if (!xdr_u_long(xdrs, (u_long *) &objp->maxlen))
    return (FALSE);
  dummy = xdr_bytes(xdrs, (char **)&(objp->buf),
		    (u_int *)&(objp->len), objp->maxlen);
  return (dummy);
}


bool_t
xdr_autofs_args(XDR *xdrs, autofs_args *objp)
{
  if (!xdr_autofs_netbuf(xdrs, &objp->addr))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->path, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->opts, AUTOFS_MAXOPTSLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->map, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->subdir, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->key, AUTOFS_MAXCOMPONENTLEN))
    return (FALSE);
  if (!xdr_int(xdrs, &objp->mount_to))
    return (FALSE);
  if (!xdr_int(xdrs, &objp->rpc_to))
    return (FALSE);
  if (!xdr_int(xdrs, &objp->direct))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_mounta(XDR *xdrs, struct mounta *objp)
{
  if (!xdr_string(xdrs, &objp->spec, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->dir, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_int(xdrs, &objp->flags))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->fstype, AUTOFS_MAXCOMPONENTLEN))
    return (FALSE);
  if (!xdr_pointer(xdrs, (char **)&objp->dataptr, sizeof(autofs_args),
		   (XDRPROC_T_TYPE) xdr_autofs_args))
    return (FALSE);
  if (!xdr_int(xdrs, &objp->datalen))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_action_list_entry(XDR *xdrs, action_list_entry *objp)
{
  if (!xdr_autofs_action(xdrs, &objp->action))
    return (FALSE);
  switch (objp->action) {
  case AUTOFS_MOUNT_RQ:
    if (!xdr_mounta(xdrs, &objp->action_list_entry_u.mounta))
      return (FALSE);
    break;
  case AUTOFS_LINK_RQ:
    if (!xdr_linka(xdrs, &objp->action_list_entry_u.linka))
      return (FALSE);
    break;
  default:
    break;
  }
  return (TRUE);
}


bool_t
xdr_action_list(XDR *xdrs, action_list *objp)
{
  if (!xdr_action_list_entry(xdrs, &objp->action))
    return (FALSE);
  if (!xdr_pointer(xdrs, (char **)&objp->next, sizeof(action_list),
		   (XDRPROC_T_TYPE) xdr_action_list))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_umntrequest(XDR *xdrs, umntrequest *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_umntrequest:");

  if (!xdr_bool_t(xdrs, &objp->isdirect))
    return (FALSE);
#ifdef HAVE_STRUCT_UMNTREQUEST_DEVID
  if (!xdr_dev_t(xdrs, &objp->devid))
    return (FALSE);
  if (!xdr_dev_t(xdrs, &objp->rdevid))
    return (FALSE);
#else  /* not HAVE_STRUCT_UMNTREQUEST_DEVID */
  if (!xdr_string(xdrs, &objp->mntresource, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->mntpnt, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->fstype, AUTOFS_MAXCOMPONENTLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->mntopts, AUTOFS_MAXOPTSLEN))
    return (FALSE);
#endif /* not HAVE_STRUCT_UMNTREQUEST_DEVID */
  if (!xdr_pointer(xdrs, (char **) &objp->next, sizeof(umntrequest),
		   (XDRPROC_T_TYPE) xdr_umntrequest))
    return (FALSE);

  return (TRUE);
}


bool_t
xdr_umntres(XDR *xdrs, umntres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_mntres:");

  if (!xdr_int(xdrs, &objp->status))
    return (FALSE);
  return (TRUE);
}


/*
 * These exist only in the AutoFS V2 protocol.
 */
#ifdef AUTOFS_POSTUNMOUNT
bool_t
xdr_postumntreq(XDR *xdrs, postumntreq *objp)
{
  if (!xdr_dev_t(xdrs, &objp->devid))
    return (FALSE);
  if (!xdr_dev_t(xdrs, &objp->rdevid))
    return (FALSE);
  if (!xdr_pointer(xdrs, (char **)&objp->next,
		   sizeof(struct postumntreq),
		   (XDRPROC_T_TYPE) xdr_postumntreq))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_postumntres(XDR *xdrs, postumntres *objp)
{
  if (!xdr_int(xdrs, &objp->status))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_postmountreq(XDR *xdrs, postmountreq *objp)
{
  if (!xdr_string(xdrs, &objp->special, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->mountp, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->fstype, AUTOFS_MAXCOMPONENTLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->mntopts, AUTOFS_MAXOPTSLEN))
    return (FALSE);
  if (!xdr_dev_t(xdrs, &objp->devid))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_postmountres(XDR *xdrs, postmountres *objp)
{
  if (!xdr_int(xdrs, &objp->status))
    return (FALSE);
  return (TRUE);
}
#endif /* AUTOFS_POSTUNMOUNT */


bool_t
xdr_autofs_res(XDR *xdrs, autofs_res *objp)
{
  if (!xdr_enum(xdrs, (enum_t *)objp))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_autofs_lookupargs(XDR *xdrs, autofs_lookupargs *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_autofs_lookupargs:");

  if (!xdr_string(xdrs, &objp->map, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->path, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->name, AUTOFS_MAXCOMPONENTLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->subdir, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_string(xdrs, &objp->opts, AUTOFS_MAXOPTSLEN))
    return (FALSE);
  if (!xdr_bool_t(xdrs, &objp->isdirect))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_mount_result_type(XDR *xdrs, mount_result_type *objp)
{
  if (!xdr_autofs_stat(xdrs, &objp->status))
    return (FALSE);
  switch (objp->status) {
  case AUTOFS_ACTION:
    if (!xdr_pointer(xdrs,
		     (char **)&objp->mount_result_type_u.list,
		     sizeof(action_list), (XDRPROC_T_TYPE) xdr_action_list))
      return (FALSE);
    break;
  case AUTOFS_DONE:
    if (!xdr_int(xdrs, &objp->mount_result_type_u.error))
      return (FALSE);
    break;
  }
  return (TRUE);
}


bool_t
xdr_autofs_mountres(XDR *xdrs, autofs_mountres *objp)
{
  if (amuDebug(D_XDRTRACE))
    plog(XLOG_DEBUG, "xdr_mntres:");

  if (!xdr_mount_result_type(xdrs, &objp->mr_type))
    return (FALSE);
  if (!xdr_int(xdrs, &objp->mr_verbose))
    return (FALSE);

  return (TRUE);
}


bool_t
xdr_lookup_result_type(XDR *xdrs, lookup_result_type *objp)
{
  if (!xdr_autofs_action(xdrs, &objp->action))
    return (FALSE);
  switch (objp->action) {
  case AUTOFS_LINK_RQ:
    if (!xdr_linka(xdrs, &objp->lookup_result_type_u.lt_linka))
      return (FALSE);
    break;
  default:
    break;
  }
  return (TRUE);
}


bool_t
xdr_autofs_lookupres(XDR *xdrs, autofs_lookupres *objp)
{
  if (!xdr_autofs_res(xdrs, &objp->lu_res))
    return (FALSE);
  if (!xdr_lookup_result_type(xdrs, &objp->lu_type))
    return (FALSE);
  if (!xdr_int(xdrs, &objp->lu_verbose))
    return (FALSE);
  return (TRUE);
}


bool_t
xdr_autofs_rddirargs(XDR *xdrs, autofs_rddirargs *objp)
{
  if (!xdr_string(xdrs, &objp->rda_map, AUTOFS_MAXPATHLEN))
    return (FALSE);
  if (!xdr_u_int(xdrs, (u_int *) &objp->rda_offset))
    return (FALSE);
  if (!xdr_u_int(xdrs, (u_int *) &objp->rda_count))
    return (FALSE);
  return (TRUE);
}


/*
 * ENCODE ONLY
 *
 * Solaris automountd uses struct autofsrddir to pass the results.
 * We use the traditional nfsreaddirres and do the conversion ourselves.
 */
static bool_t
xdr_amd_putrddirres(XDR *xdrs, nfsdirlist *dp, ulong reqsize)
{
  nfsentry *ep;
  char *name;
  u_int namlen;
  bool_t true = TRUE;
  bool_t false = FALSE;
  int entrysz;
  int tofit;
  int bufsize;
  u_long ino, off;

  bufsize = 1 * BYTES_PER_XDR_UNIT;
  for (ep = dp->dl_entries; ep; ep = ep->ne_nextentry) {
    name = ep->ne_name;
    namlen = strlen(name);
    ino = (u_long) ep->ne_fileid;
    off = (u_long) ep->ne_cookie + AUTOFS_DAEMONCOOKIE;
    entrysz = (1 + 1 + 1 + 1) * BYTES_PER_XDR_UNIT +
      roundup(namlen, BYTES_PER_XDR_UNIT);
    tofit = entrysz + 2 * BYTES_PER_XDR_UNIT;
    if (bufsize + tofit > reqsize) {
      dp->dl_eof = FALSE;
      break;
    }
    if (!xdr_bool(xdrs, &true) ||
	!xdr_u_long(xdrs, &ino) ||
	!xdr_bytes(xdrs, &name, &namlen, AUTOFS_MAXPATHLEN) ||
	!xdr_u_long(xdrs, &off)) {
      return (FALSE);
    }
    bufsize += entrysz;
  }
  if (!xdr_bool(xdrs, &false)) {
    return (FALSE);
  }
  if (!xdr_bool(xdrs, &dp->dl_eof)) {
    return (FALSE);
  }
  return (TRUE);
}


static bool_t
xdr_amd_rddirres(XDR *xdrs, amd_rddirres *objp)
{
  if (!xdr_enum(xdrs, (enum_t *)&objp->rd_status))
    return (FALSE);
  if (objp->rd_status != AUTOFS_OK)
    return (TRUE);
  return (xdr_amd_putrddirres(xdrs, &objp->rd_dl, objp->rd_bufsize));
}


/*
 * AUTOFS RPC methods
 */

static int
autofs_lookup_2_req(autofs_lookupargs *m,
		    autofs_lookupres *res,
		    struct authunix_parms *cred,
		    SVCXPRT *transp)
{
  int err;
  am_node *mp, *new_mp;
  mntfs *mf;

  dlog("LOOKUP REQUEST: name=%s[%s] map=%s opts=%s path=%s direct=%d",
       m->name, m->subdir, m->map, m->opts,
       m->path, m->isdirect);

  /* find the effective uid/gid from RPC request */
  xsnprintf(opt_uid, sizeof(uid_str), "%d", (int) cred->aup_uid);
  xsnprintf(opt_gid, sizeof(gid_str), "%d", (int) cred->aup_gid);

  mp = find_ap(m->path);
  if (!mp) {
    plog(XLOG_ERROR, "map %s not found", m->path);
    err = AUTOFS_NOENT;
    goto out;
  }

  mf = mp->am_al->al_mnt;
  new_mp = mf->mf_ops->lookup_child(mp, m->name, &err, VLOOK_LOOKUP);
  if (!new_mp) {
    err = AUTOFS_NOENT;
    goto out;
  }

  if (err == 0) {
    plog(XLOG_ERROR, "autofs requests to mount an already mounted node???");
  } else {
    free_map(new_mp);
  }
  err = AUTOFS_OK;
  res->lu_type.action = AUTOFS_NONE;

 out:
  res->lu_res = err;
  res->lu_verbose = 1;

  dlog("LOOKUP REPLY: status=%d", res->lu_res);
  return 0;
}


static void
autofs_lookup_2_free(autofs_lookupres *res)
{
  struct linka link;

  if ((res->lu_res == AUTOFS_OK) &&
      (res->lu_type.action == AUTOFS_LINK_RQ)) {
    /*
     * Free link information
     */
    link = res->lu_type.lookup_result_type_u.lt_linka;
    if (link.dir)
      XFREE(link.dir);
    if (link.link)
      XFREE(link.link);
  }
}


static int
autofs_mount_2_req(autofs_lookupargs *m,
		   autofs_mountres *res,
		   struct authunix_parms *cred,
		   SVCXPRT *transp)
{
  int err = AUTOFS_OK;
  am_node *mp, *new_mp;
  mntfs *mf;

  dlog("MOUNT REQUEST: name=%s[%s] map=%s opts=%s path=%s direct=%d",
       m->name, m->subdir, m->map, m->opts,
       m->path, m->isdirect);

  /* find the effective uid/gid from RPC request */
  xsnprintf(opt_uid, sizeof(uid_str), "%d", (int) cred->aup_uid);
  xsnprintf(opt_gid, sizeof(gid_str), "%d", (int) cred->aup_gid);

  mp = find_ap(m->path);
  if (!mp) {
    plog(XLOG_ERROR, "map %s not found", m->path);
    res->mr_type.status = AUTOFS_DONE;
    res->mr_type.mount_result_type_u.error = AUTOFS_NOENT;
    goto out;
  }

  mf = mp->am_al->al_mnt;
  new_mp = mf->mf_ops->lookup_child(mp, m->name + m->isdirect, &err, VLOOK_CREATE);
  if (new_mp && err < 0) {
    /* new_mp->am_transp = transp; */
    new_mp = mf->mf_ops->mount_child(new_mp, &err);
  }
  if (new_mp == NULL) {
    if (err < 0) {
      /* we're working on it */
      amd_stats.d_drops++;
      return 1;
    }
    res->mr_type.status = AUTOFS_DONE;
    res->mr_type.mount_result_type_u.error = AUTOFS_NOENT;
    goto out;
  }

  if (gopt.flags & CFM_AUTOFS_USE_LOFS ||
      new_mp->am_al->al_mnt->mf_flags & MFF_ON_AUTOFS) {
    res->mr_type.status = AUTOFS_DONE;
    res->mr_type.mount_result_type_u.error = AUTOFS_OK;
  } else {
    struct action_list *list = malloc(sizeof(struct action_list));
    char *target;
    if (new_mp->am_link)
      target = new_mp->am_link;
    else
      target = new_mp->am_al->al_mnt->mf_mount;
    list->action.action = AUTOFS_LINK_RQ;
    list->action.action_list_entry_u.linka.dir = xstrdup(new_mp->am_name);
    list->action.action_list_entry_u.linka.link = xstrdup(target);
    list->next = NULL;
    res->mr_type.status = AUTOFS_ACTION;
    res->mr_type.mount_result_type_u.list = list;
  }

out:
  res->mr_verbose = 1;

  switch (res->mr_type.status) {
  case AUTOFS_ACTION:
    dlog("MOUNT REPLY: status=%d, AUTOFS_ACTION", err);
    break;
  case AUTOFS_DONE:
    dlog("MOUNT REPLY: status=%d, AUTOFS_DONE", err);
    break;
  default:
    dlog("MOUNT REPLY: status=%d, UNKNOWN(%d)", err, res->mr_type.status);
  }

  if (err) {
    if (m->isdirect) {
      /* direct mount */
      plog(XLOG_ERROR, "mount of %s failed", m->path);
    } else {
      /* indirect mount */
      plog(XLOG_ERROR, "mount of %s/%s failed", m->path, m->name);
    }
  }
  return 0;
}


static void
autofs_mount_2_free(struct autofs_mountres *res)
{
  if (res->mr_type.status == AUTOFS_ACTION &&
      res->mr_type.mount_result_type_u.list != NULL) {
    autofs_action action;
    dlog("freeing action list");
    action = res->mr_type.mount_result_type_u.list->action.action;
    if (action == AUTOFS_LINK_RQ) {
      /*
       * Free link information
       */
      struct linka *link;
      link = &(res->mr_type.mount_result_type_u.list->action.action_list_entry_u.linka);
      if (link->dir)
	XFREE(link->dir);
      if (link->link)
	XFREE(link->link);
    } else if (action == AUTOFS_MOUNT_RQ) {
      struct mounta *mnt;
      mnt = &(res->mr_type.mount_result_type_u.list->action.action_list_entry_u.mounta);
      if (mnt->spec)
	XFREE(mnt->spec);
      if (mnt->dir)
	XFREE(mnt->dir);
      if (mnt->fstype)
	XFREE(mnt->fstype);
      if (mnt->dataptr)
	XFREE(mnt->dataptr);
#ifdef HAVE_MOUNTA_OPTPTR
      if (mnt->optptr)
	XFREE(mnt->optptr);
#endif /* HAVE_MOUNTA_OPTPTR */
    }
    XFREE(res->mr_type.mount_result_type_u.list);
  }
}


static int
autofs_unmount_2_req(umntrequest *ul,
		     umntres *res,
		     struct authunix_parms *cred,
		     SVCXPRT *transp)
{
  int mapno, err;
  am_node *mp = NULL;

#ifdef HAVE_STRUCT_UMNTREQUEST_DEVID
  dlog("UNMOUNT REQUEST: dev=%lx rdev=%lx %s",
       (u_long) ul->devid,
       (u_long) ul->rdevid,
       ul->isdirect ? "direct" : "indirect");
#else  /* not HAVE_STRUCT_UMNTREQUEST_DEVID */
  dlog("UNMOUNT REQUEST: mntresource='%s' mntpnt='%s' fstype='%s' mntopts='%s' %s",
       ul->mntresource,
       ul->mntpnt,
       ul->fstype,
       ul->mntopts,
       ul->isdirect ? "direct" : "indirect");
#endif /* not HAVE_STRUCT_UMNTREQUEST_DEVID */

  /* by default, and if not found, succeed */
  res->status = 0;

#ifdef HAVE_STRUCT_UMNTREQUEST_DEVID
  for (mp = get_first_exported_ap(&mapno);
       mp;
       mp = get_next_exported_ap(&mapno)) {
    if (mp->am_dev == ul->devid &&
	mp->am_rdev == ul->rdevid)
      break;
  }
#else  /* not HAVE_STRUCT_UMNTREQUEST_DEVID */
  mp = find_ap(ul->mntpnt);
#endif /* not HAVE_STRUCT_UMNTREQUEST_DEVID */

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


/*
 * These exist only in the AutoFS V2 protocol.
 */
#ifdef AUTOFS_POSTUNMOUNT
/* XXX not implemented */
static int
autofs_postunmount_2_req(postumntreq *req,
			 postumntres *res,
			 struct authunix_parms *cred,
			 SVCXPRT *transp)
{
  postumntreq *ul = req;

  dlog("POSTUNMOUNT REQUEST: dev=%lx rdev=%lx",
       (u_long) ul->devid,
       (u_long) ul->rdevid);

  /* succeed unconditionally */
  res->status = 0;

  dlog("POSTUNMOUNT REPLY: status=%d", res->status);
  return 0;
}


/* XXX not implemented */
static int
autofs_postmount_2_req(postmountreq *req,
		       postmountres *res,
		       struct authunix_parms *cred,
		       SVCXPRT *transp)
{
  dlog("POSTMOUNT REQUEST: %s\tdev=%lx\tspecial=%s %s",
       req->mountp, (u_long) req->devid, req->special, req->mntopts);

  /* succeed unconditionally */
  res->status = 0;

  dlog("POSTMOUNT REPLY: status=%d", res->status);
  return 0;
}
#endif /* AUTOFS_POSTUNMOUNT */


static int
autofs_readdir_2_req(struct autofs_rddirargs *req,
		     struct amd_rddirres *res,
		     struct authunix_parms *cred,
		     SVCXPRT *transp)
{
  am_node *mp;
  int err;
  static nfsentry e_res[MAX_READDIR_ENTRIES];

  dlog("READDIR REQUEST: %s @ %d",
       req->rda_map, (int) req->rda_offset);

  mp = find_ap(req->rda_map);
  if (!mp) {
    plog(XLOG_ERROR, "map %s not found", req->rda_map);
    res->rd_status = AUTOFS_NOENT;
    goto out;
  }

  mp->am_stats.s_readdir++;
  req->rda_offset -= AUTOFS_DAEMONCOOKIE;
  err = mp->am_al->al_mnt->mf_ops->readdir(mp, (char *)&req->rda_offset,
				    &res->rd_dl, e_res, req->rda_count);
  if (err) {
    res->rd_status = AUTOFS_ECOMM;
    goto out;
  }

  res->rd_status = AUTOFS_OK;
  res->rd_bufsize = req->rda_count;

out:
  dlog("READDIR REPLY: status=%d", res->rd_status);
  return 0;
}


/****************************************************************************/
/* autofs program dispatcher */
static void
autofs_program_2(struct svc_req *rqstp, SVCXPRT *transp)
{
  union {
    autofs_lookupargs autofs_mount_2_arg;
    autofs_lookupargs autofs_lookup_2_arg;
    umntrequest autofs_umount_2_arg;
    autofs_rddirargs autofs_readdir_2_arg;
#ifdef AUTOFS_POSTUNMOUNT
    postmountreq autofs_postmount_2_arg;
    postumntreq autofs_postumnt_2_arg;
#endif /* AUTOFS_POSTUNMOUNT */
  } argument;

  union {
    autofs_mountres mount_res;
    autofs_lookupres lookup_res;
    umntres umount_res;
    amd_rddirres readdir_res;
#ifdef AUTOFS_POSTUNMOUNT
    postumntres postumnt_res;
    postmountres postmnt_res;
#endif /* AUTOFS_POSTUNMOUNT */
  } result;
  int ret;

  bool_t (*xdr_argument)();
  bool_t (*xdr_result)();
  int (*local)();
  void (*local_free)() = NULL;

  current_transp = transp;

  switch (rqstp->rq_proc) {

  case AUTOFS_NULL:
    svc_sendreply(transp,
		  (XDRPROC_T_TYPE) xdr_void,
		  (SVC_IN_ARG_TYPE) NULL);
    return;

  case AUTOFS_LOOKUP:
    xdr_argument = xdr_autofs_lookupargs;
    xdr_result = xdr_autofs_lookupres;
    local = autofs_lookup_2_req;
    local_free = autofs_lookup_2_free;
    break;

  case AUTOFS_MOUNT:
    xdr_argument = xdr_autofs_lookupargs;
    xdr_result = xdr_autofs_mountres;
    local = autofs_mount_2_req;
    local_free = autofs_mount_2_free;
    break;

  case AUTOFS_UNMOUNT:
    xdr_argument = xdr_umntrequest;
    xdr_result = xdr_umntres;
    local = autofs_unmount_2_req;
    break;

/*
 * These exist only in the AutoFS V2 protocol.
 */
#ifdef AUTOFS_POSTUNMOUNT
  case AUTOFS_POSTUNMOUNT:
    xdr_argument = xdr_postumntreq;
    xdr_result = xdr_postumntres;
    local = autofs_postunmount_2_req;
    break;

  case AUTOFS_POSTMOUNT:
    xdr_argument = xdr_postmountreq;
    xdr_result = xdr_postmountres;
    local = autofs_postmount_2_req;
    break;
#endif /* AUTOFS_POSTUNMOUNT */

  case AUTOFS_READDIR:
    xdr_argument = xdr_autofs_rddirargs;
    xdr_result = xdr_amd_rddirres;
    local = autofs_readdir_2_req;
    break;

  default:
    svcerr_noproc(transp);
    return;
  }

  memset((char *) &argument, 0, sizeof(argument));
  if (!svc_getargs(transp,
		   (XDRPROC_T_TYPE) xdr_argument,
		   (SVC_IN_ARG_TYPE) &argument)) {
    plog(XLOG_ERROR, "AUTOFS xdr decode failed for %d %d %d",
	 (int) rqstp->rq_prog, (int) rqstp->rq_vers, (int) rqstp->rq_proc);
    svcerr_decode(transp);
    return;
  }

  memset((char *)&result, 0, sizeof(result));
  ret = (*local) (&argument, &result, rqstp->rq_clntcred, transp);

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
    plog(XLOG_FATAL, "unable to free rpc arguments in autofs_program_2");
  }

  if (local_free)
    (*local_free)(&result);
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

  fh->direct = ((mf->mf_ops->autofs_fs_flags & FS_DIRECT) == FS_DIRECT);
  fh->rpc_to = 1;		/* XXX: arbitrary */
  fh->mount_to = mp->am_timeo;
  fh->path = mp->am_path;
  fh->opts = "";		/* XXX: arbitrary */
  fh->map = mp->am_path;	/* this is what we get back in readdir */
  fh->subdir = "";
  if (fh->direct)
    fh->key = mp->am_name;
  else
    fh->key = "";

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
  return register_autofs_service(AUTOFS_CONFTYPE, autofs_program_2);
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
  struct stat buf;

  /*
   * For sublinks, we could end up here with an already mounted f/s.
   * Don't do anything in that case.
   */
  if (!(mf->mf_flags & MFF_MOUNTED))
    err = mf->mf_ops->mount_fs(mp, mf);

  if (err || mf->mf_flags & MFF_ON_AUTOFS)
    /* Nothing else to do */
    return err;

  if (!(gopt.flags & CFM_AUTOFS_USE_LOFS))
    /* Symlinks will be requested in autofs_mount_succeeded */
    return 0;

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
   * we fall back to symlinks if there are problems.
   *
   * we need to temporarily change pgrp, otherwise our stat() won't
   * trigger whatever cascading mounts are needed.
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

  if ((err = mount_lofs(mp->am_path, target2, mf->mf_mopts, 1))) {
    errno = err;
    goto out;
  }

 out:
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
  if (!(mf->mf_flags & MFF_ON_AUTOFS) &&
      gopt.flags & CFM_AUTOFS_USE_LOFS) {
    err = UMOUNT_FS(mp->am_path, mnttab_file_name, 1);
    if (err)
      return err;
  }

  /*
   * Multiple sublinks could reference this f/s.
   * Don't actually unmount it unless we're holding the last reference.
   */
  if (mf->mf_refc == 1)
    err = mf->mf_ops->umount_fs(mp, mf);
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

  /*
   * Store dev and rdev -- but not for symlinks
   */
  if (gopt.flags & CFM_AUTOFS_USE_LOFS ||
      mp->am_al->al_mnt->mf_flags & MFF_ON_AUTOFS) {
    if (!lstat(mp->am_path, &stb)) {
      mp->am_dev = stb.st_dev;
      mp->am_rdev = stb.st_rdev;
    }
    /* don't expire the entries -- the kernel will do it for us */
    mp->am_flags |= AMF_NOTIMEOUT;
  }

  if (transp) {
    autofs_mountres res;
    res.mr_type.status = AUTOFS_DONE;
    res.mr_type.mount_result_type_u.error = AUTOFS_OK;
    res.mr_verbose = 1;

    if (!svc_sendreply(transp,
		       (XDRPROC_T_TYPE) xdr_autofs_mountres,
		       (SVC_IN_ARG_TYPE) &res))
      svcerr_systemerr(transp);

    dlog("Quick reply sent for %s", mp->am_al->al_mnt->mf_mount);
    XFREE(transp);
    mp->am_transp = NULL;
  }

  plog(XLOG_INFO, "autofs: mounting %s succeeded", mp->am_path);
}


void
autofs_mount_failed(am_node *mp)
{
  SVCXPRT *transp = mp->am_transp;

  if (transp) {
    autofs_mountres res;
    res.mr_type.status = AUTOFS_DONE;
    res.mr_type.mount_result_type_u.error = AUTOFS_NOENT;
    res.mr_verbose = 1;

    if (!svc_sendreply(transp,
		       (XDRPROC_T_TYPE) xdr_autofs_mountres,
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
