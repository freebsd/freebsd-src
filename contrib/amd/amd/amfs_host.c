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
 * $Id: amfs_host.c,v 1.4.2.6 2002/12/27 22:44:30 ezk Exp $
 *
 */

/*
 * NFS host file system.
 * Mounts all exported filesystems from a given host.
 * This has now degenerated into a mess but will not
 * be rewritten.  Amd 6 will support the abstractions
 * needed to make this work correctly.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

static char *amfs_host_match(am_opts *fo);
static int amfs_host_fmount(mntfs *mf);
static int amfs_host_fumount(mntfs *mf);
static int amfs_host_init(mntfs *mf);
static void amfs_host_umounted(am_node *mp);

/*
 * Ops structure
 */
am_ops amfs_host_ops =
{
  "host",
  amfs_host_match,
  amfs_host_init,
  amfs_auto_fmount,
  amfs_host_fmount,
  amfs_auto_fumount,
  amfs_host_fumount,
  amfs_error_lookuppn,
  amfs_error_readdir,
  0,				/* amfs_host_readlink */
  0,				/* amfs_host_mounted */
  amfs_host_umounted,
  find_nfs_srvr,
  FS_MKMNT | FS_BACKGROUND | FS_AMQINFO
};


/*
 * Determine the mount point:
 *
 * The next change we put in to better handle PCs.  This is a bit
 * disgusting, so you'd better sit down.  We change the make_mntpt function
 * to look for exported file systems without a leading '/'.  If they don't
 * have a leading '/', we add one.  If the export is 'a:' through 'z:'
 * (without a leading slash), we change it to 'a%' (or b% or z%).  This
 * allows the entire PC disk to be mounted.
 */
static void
make_mntpt(char *mntpt, const exports ex, const mntfs *mf)
{
  if (ex->ex_dir[0] == '/') {
    if (ex->ex_dir[1] == 0)
      strcpy(mntpt, (mf)->mf_mount);
    else
      sprintf(mntpt, "%s%s", mf->mf_mount, ex->ex_dir);
  } else if (ex->ex_dir[0] >= 'a' &&
	     ex->ex_dir[0] <= 'z' &&
	     ex->ex_dir[1] == ':' &&
	     ex->ex_dir[2] == '/' &&
	     ex->ex_dir[3] == 0)
    sprintf(mntpt, "%s/%c%%", mf->mf_mount, ex->ex_dir[0]);
  else
    sprintf(mntpt, "%s/%s", mf->mf_mount, ex->ex_dir);
}


/*
 * Execute needs the same as NFS plus a helper command
 */
static char *
amfs_host_match(am_opts *fo)
{
  extern am_ops nfs_ops;

  /*
   * Make sure rfs is specified to keep nfs_match happy...
   */
  if (!fo->opt_rfs)
    fo->opt_rfs = "/";

  return (*nfs_ops.fs_match) (fo);
}


static int
amfs_host_init(mntfs *mf)
{
  fserver *fs;
  u_short port;

  if (strchr(mf->mf_info, ':') == 0)
    return ENOENT;

  /*
   * This is primarily to schedule a wakeup so that as soon
   * as our fileserver is ready, we can continue setting up
   * the host filesystem.  If we don't do this, the standard
   * amfs_auto code will set up a fileserver structure, but it will
   * have to wait for another nfs request from the client to come
   * in before finishing.  Our way is faster since we don't have
   * to wait for the client to resend its request (which could
   * take a second or two).
   */
  /*
   * First, we find the fileserver for this mntfs and then call
   * nfs_srvr_port with our mntfs passed as the wait channel.
   * nfs_srvr_port will check some things and then schedule
   * it so that when the fileserver is ready, a wakeup is done
   * on this mntfs.   amfs_auto_cont() is already sleeping on this mntfs
   * so as soon as that wakeup happens amfs_auto_cont() is called and
   * this mount is retried.
   */
  if ((fs = mf->mf_server))
    /*
     * We don't really care if there's an error returned.
     * Since this is just to help speed things along, the
     * error will get handled properly elsewhere.
     */
    (void) nfs_srvr_port(fs, &port, (voidp) mf);

  return 0;
}


static int
do_mount(am_nfs_handle_t *fhp, char *dir, char *fs_name, char *opts, mntfs *mf)
{
  struct stat stb;

#ifdef DEBUG
  dlog("amfs_host: mounting fs %s on %s\n", fs_name, dir);
#endif /* DEBUG */

  (void) mkdirs(dir, 0555);
  if (stat(dir, &stb) < 0 || (stb.st_mode & S_IFMT) != S_IFDIR) {
    plog(XLOG_ERROR, "No mount point for %s - skipping", dir);
    return ENOENT;
  }

  return mount_nfs_fh(fhp, dir, fs_name, opts, mf);
}


static int
sortfun(const voidp x, const voidp y)
{
  exports *a = (exports *) x;
  exports *b = (exports *) y;

  return strcmp((*a)->ex_dir, (*b)->ex_dir);
}


/*
 * Get filehandle
 */
static int
fetch_fhandle(CLIENT *client, char *dir, am_nfs_handle_t *fhp, u_long nfs_version)
{
  struct timeval tv;
  enum clnt_stat clnt_stat;

  /*
   * Pick a number, any number...
   */
  tv.tv_sec = 20;
  tv.tv_usec = 0;

#ifdef DEBUG
  dlog("Fetching fhandle for %s", dir);
#endif /* DEBUG */

  /*
   * Call the mount daemon on the remote host to
   * get the filehandle.  Use NFS version specific call.
   */

  plog(XLOG_INFO, "fetch_fhandle: NFS version %d", (int) nfs_version);
#ifdef HAVE_FS_NFS3
  if (nfs_version == NFS_VERSION3) {
    memset((char *) &fhp->v3, 0, sizeof(fhp->v3));
    clnt_stat = clnt_call(client,
			  MOUNTPROC_MNT,
			  (XDRPROC_T_TYPE) xdr_dirpath,
			  (SVC_IN_ARG_TYPE) &dir,
			  (XDRPROC_T_TYPE) xdr_mountres3,
			  (SVC_IN_ARG_TYPE) &fhp->v3,
			  tv);
    if (clnt_stat != RPC_SUCCESS) {
      plog(XLOG_ERROR, "mountd rpc failed: %s", clnt_sperrno(clnt_stat));
      return EIO;
    }
    /* Check the status of the filehandle */
    if ((errno = fhp->v3.fhs_status)) {
#ifdef DEBUG
      dlog("fhandle fetch for mount version 3 failed: %m");
#endif /* DEBUG */
      return errno;
    }
  } else {			/* not NFS_VERSION3 mount */
#endif /* HAVE_FS_NFS3 */
    clnt_stat = clnt_call(client,
			  MOUNTPROC_MNT,
			  (XDRPROC_T_TYPE) xdr_dirpath,
			  (SVC_IN_ARG_TYPE) &dir,
			  (XDRPROC_T_TYPE) xdr_fhstatus,
			  (SVC_IN_ARG_TYPE) &fhp->v2,
			  tv);
    if (clnt_stat != RPC_SUCCESS) {
      plog(XLOG_ERROR, "mountd rpc failed: %s", clnt_sperrno(clnt_stat));
      return EIO;
    }
    /* Check status of filehandle */
    if (fhp->v2.fhs_status) {
      errno = fhp->v2.fhs_status;
#ifdef DEBUG
      dlog("fhandle fetch for mount version 1 failed: %m");
#endif /* DEBUG */
      return errno;
    }
#ifdef HAVE_FS_NFS3
  } /* end of "if (nfs_version == NFS_VERSION3)" statement */
#endif /* HAVE_FS_NFS3 */

  /* all is well */
  return 0;
}


/*
 * Scan mount table to see if something already mounted
 */
static int
already_mounted(mntlist *mlist, char *dir)
{
  mntlist *ml;

  for (ml = mlist; ml; ml = ml->mnext)
    if (STREQ(ml->mnt->mnt_dir, dir))
      return 1;
  return 0;
}


/*
 * Mount the export tree from a host
 */
static int
amfs_host_fmount(mntfs *mf)
{
  struct timeval tv2;
  CLIENT *client;
  enum clnt_stat clnt_stat;
  int n_export;
  int j, k;
  exports exlist = 0, ex;
  exports *ep = 0;
  am_nfs_handle_t *fp = 0;
  char *host;
  int error = 0;
  struct sockaddr_in sin;
  int sock = RPC_ANYSOCK;
  int ok = FALSE;
  mntlist *mlist;
  char fs_name[MAXPATHLEN], *rfs_dir;
  char mntpt[MAXPATHLEN];
  struct timeval tv;
  u_long mnt_version;

  /*
   * Read the mount list
   */
  mlist = read_mtab(mf->mf_mount, mnttab_file_name);

#ifdef MOUNT_TABLE_ON_FILE
  /*
   * Unlock the mount list
   */
  unlock_mntlist();
#endif /* MOUNT_TABLE_ON_FILE */

  /*
   * Take a copy of the server hostname, address, and nfs version
   * to mount version conversion.
   */
  host = mf->mf_server->fs_host;
  sin = *mf->mf_server->fs_ip;
  plog(XLOG_INFO, "amfs_host_fmount: NFS version %d", (int) mf->mf_server->fs_version);
#ifdef HAVE_FS_NFS3
  if (mf->mf_server->fs_version == NFS_VERSION3)
    mnt_version = MOUNTVERS3;
  else
#endif /* HAVE_FS_NFS3 */
    mnt_version = MOUNTVERS;

  /*
   * The original 10 second per try timeout is WAY too large, especially
   * if we're only waiting 10 or 20 seconds max for the response.
   * That would mean we'd try only once in 10 seconds, and we could
   * lose the transmit or receive packet, and never try again.
   * A 2-second per try timeout here is much more reasonable.
   * 09/28/92 Mike Mitchell, mcm@unx.sas.com
   */
  tv.tv_sec = 2;
  tv.tv_usec = 0;

  /*
   * Create a client attached to mountd
   */
  client = get_mount_client(host, &sin, &tv, &sock, mnt_version);
  if (client == NULL) {
#ifdef HAVE_CLNT_SPCREATEERROR
    plog(XLOG_ERROR, "get_mount_client failed for %s: %s",
	 host, clnt_spcreateerror(""));
#else /* not HAVE_CLNT_SPCREATEERROR */
    plog(XLOG_ERROR, "get_mount_client failed for %s", host);
#endif /* not HAVE_CLNT_SPCREATEERROR */
    error = EIO;
    goto out;
  }
  if (!nfs_auth) {
    error = make_nfs_auth();
    if (error)
      goto out;
  }
  client->cl_auth = nfs_auth;

#ifdef DEBUG
  dlog("Fetching export list from %s", host);
#endif /* DEBUG */

  /*
   * Fetch the export list
   */
  tv2.tv_sec = 10;
  tv2.tv_usec = 0;
  clnt_stat = clnt_call(client,
			MOUNTPROC_EXPORT,
			(XDRPROC_T_TYPE) xdr_void,
			0,
			(XDRPROC_T_TYPE) xdr_exports,
			(SVC_IN_ARG_TYPE) & exlist,
			tv2);
  if (clnt_stat != RPC_SUCCESS) {
    const char *msg = clnt_sperrno(clnt_stat);
    plog(XLOG_ERROR, "host_fmount rpc failed: %s", msg);
    /* clnt_perror(client, "rpc"); */
    error = EIO;
    goto out;
  }

  /*
   * Figure out how many exports were returned
   */
  for (n_export = 0, ex = exlist; ex; ex = ex->ex_next) {
    n_export++;
  }

  /*
   * Allocate an array of pointers into the list
   * so that they can be sorted.  If the filesystem
   * is already mounted then ignore it.
   */
  ep = (exports *) xmalloc(n_export * sizeof(exports));
  for (j = 0, ex = exlist; ex; ex = ex->ex_next) {
    make_mntpt(mntpt, ex, mf);
    if (already_mounted(mlist, mntpt))
      /* we have at least one mounted f/s, so don't fail the mount */
      ok = TRUE;
    else
      ep[j++] = ex;
  }
  n_export = j;

  /*
   * Sort into order.
   * This way the mounts are done in order down the tree,
   * instead of any random order returned by the mount
   * daemon (the protocol doesn't specify...).
   */
  qsort(ep, n_export, sizeof(exports), sortfun);

  /*
   * Allocate an array of filehandles
   */
  fp = (am_nfs_handle_t *) xmalloc(n_export * sizeof(am_nfs_handle_t));

  /*
   * Try to obtain filehandles for each directory.
   * If a fetch fails then just zero out the array
   * reference but discard the error.
   */
  for (j = k = 0; j < n_export; j++) {
    /* Check and avoid a duplicated export entry */
    if (j > k && ep[k] && STREQ(ep[j]->ex_dir, ep[k]->ex_dir)) {
#ifdef DEBUG
      dlog("avoiding dup fhandle requested for %s", ep[j]->ex_dir);
#endif /* DEBUG */
      ep[j] = 0;
    } else {
      k = j;
      error = fetch_fhandle(client, ep[j]->ex_dir, &fp[j],
			    mf->mf_server->fs_version);
      if (error)
	ep[j] = 0;
    }
  }

  /*
   * Mount each filesystem for which we have a filehandle.
   * If any of the mounts succeed then mark "ok" and return
   * error code 0 at the end.  If they all fail then return
   * the last error code.
   */
  strncpy(fs_name, mf->mf_info, sizeof(fs_name));
  if ((rfs_dir = strchr(fs_name, ':')) == (char *) 0) {
    plog(XLOG_FATAL, "amfs_host_fmount: mf_info has no colon");
    error = EINVAL;
    goto out;
  }
  ++rfs_dir;
  for (j = 0; j < n_export; j++) {
    ex = ep[j];
    if (ex) {
      strcpy(rfs_dir, ex->ex_dir);
      make_mntpt(mntpt, ex, mf);
      if (do_mount(&fp[j], mntpt, fs_name, mf->mf_mopts, mf) == 0)
	ok = TRUE;
    }
  }

  /*
   * Clean up and exit
   */
out:
  discard_mntlist(mlist);
  if (ep)
    XFREE(ep);
  if (fp)
    XFREE(fp);
  if (sock != RPC_ANYSOCK)
    (void) amu_close(sock);
  if (client)
    clnt_destroy(client);
  if (exlist)
    xdr_pri_free((XDRPROC_T_TYPE) xdr_exports, (caddr_t) &exlist);
  if (ok)
    return 0;
  return error;
}


/*
 * Return true if pref is a directory prefix of dir.
 *
 * XXX TODO:
 * Does not work if pref is "/".
 */
static int
directory_prefix(char *pref, char *dir)
{
  int len = strlen(pref);

  if (!NSTREQ(pref, dir, len))
    return FALSE;
  if (dir[len] == '/' || dir[len] == '\0')
    return TRUE;
  return FALSE;
}


/*
 * Unmount a mount tree
 */
static int
amfs_host_fumount(mntfs *mf)
{
  mntlist *ml, *mprev;
  int xerror = 0;

  /*
   * Read the mount list
   */
  mntlist *mlist = read_mtab(mf->mf_mount, mnttab_file_name);

#ifdef MOUNT_TABLE_ON_FILE
  /*
   * Unlock the mount list
   */
  unlock_mntlist();
#endif /* MOUNT_TABLE_ON_FILE */

  /*
   * Reverse list...
   */
  ml = mlist;
  mprev = 0;
  while (ml) {
    mntlist *ml2 = ml->mnext;
    ml->mnext = mprev;
    mprev = ml;
    ml = ml2;
  }
  mlist = mprev;

  /*
   * Unmount all filesystems...
   */
  for (ml = mlist; ml && !xerror; ml = ml->mnext) {
    char *dir = ml->mnt->mnt_dir;
    if (directory_prefix(mf->mf_mount, dir)) {
      int error;
#ifdef DEBUG
      dlog("amfs_host: unmounts %s", dir);
#endif /* DEBUG */
      /*
       * Unmount "dir"
       */
      error = UMOUNT_FS(dir, mnttab_file_name);
      /*
       * Keep track of errors
       */
      if (error) {
	if (!xerror)
	  xerror = error;
	if (error != EBUSY) {
	  errno = error;
	  plog(XLOG_ERROR, "Tree unmount of %s failed: %m", ml->mnt->mnt_dir);
	}
      } else {
	(void) rmdirs(dir);
      }
    }
  }

  /*
   * Throw away mount list
   */
  discard_mntlist(mlist);

  /*
   * Try to remount, except when we are shutting down.
   */
  if (xerror && amd_state != Finishing) {
    xerror = amfs_host_fmount(mf);
    if (!xerror) {
      /*
       * Don't log this - it's usually too verbose
       plog(XLOG_INFO, "Remounted host %s", mf->mf_info);
       */
      xerror = EBUSY;
    }
  }
  return xerror;
}


/*
 * Tell mountd we're done.
 * This is not quite right, because we may still
 * have other filesystems mounted, but the existing
 * mountd protocol is badly broken anyway.
 */
static void
amfs_host_umounted(am_node *mp)
{
  mntfs *mf = mp->am_mnt;
  char *host;
  CLIENT *client;
  enum clnt_stat clnt_stat;
  struct sockaddr_in sin;
  int sock = RPC_ANYSOCK;
  struct timeval tv;
  u_long mnt_version;

  if (mf->mf_error || mf->mf_refc > 1 || !mf->mf_server)
    return;

  /*
   * Take a copy of the server hostname, address, and NFS version
   * to mount version conversion.
   */
  host = mf->mf_server->fs_host;
  sin = *mf->mf_server->fs_ip;
  plog(XLOG_INFO, "amfs_host_umounted: NFS version %d", (int) mf->mf_server->fs_version);
#ifdef HAVE_FS_NFS3
  if (mf->mf_server->fs_version == NFS_VERSION3)
    mnt_version = MOUNTVERS3;
  else
#endif /* HAVE_FS_NFS3 */
    mnt_version = MOUNTVERS;

  /*
   * Create a client attached to mountd
   */
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  client = get_mount_client(host, &sin, &tv, &sock, mnt_version);
  if (client == NULL) {
#ifdef HAVE_CLNT_SPCREATEERROR
    plog(XLOG_ERROR, "get_mount_client failed for %s: %s",
	 host, clnt_spcreateerror(""));
#else /* not HAVE_CLNT_SPCREATEERROR */
    plog(XLOG_ERROR, "get_mount_client failed for %s", host);
#endif /* not HAVE_CLNT_SPCREATEERROR */
    goto out;
  }

  if (!nfs_auth) {
    if (make_nfs_auth())
      goto out;
  }
  client->cl_auth = nfs_auth;

#ifdef DEBUG
  dlog("Unmounting all from %s", host);
#endif /* DEBUG */

  clnt_stat = clnt_call(client,
			MOUNTPROC_UMNTALL,
			(XDRPROC_T_TYPE) xdr_void,
			0,
			(XDRPROC_T_TYPE) xdr_void,
			0,
			tv);
  if (clnt_stat != RPC_SUCCESS && clnt_stat != RPC_SYSTEMERROR) {
    /* RPC_SYSTEMERROR seems to be returned for no good reason ... */
    const char *msg = clnt_sperrno(clnt_stat);
    plog(XLOG_ERROR, "unmount all from %s rpc failed: %s", host, msg);
    goto out;
  }

out:
  if (sock != RPC_ANYSOCK)
    (void) amu_close(sock);
  if (client)
    clnt_destroy(client);
}
