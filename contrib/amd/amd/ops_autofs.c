/*
 * Copyright (c) 1997-1999 Erez Zadok
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
 * $Id: ops_autofs.c,v 1.4 1999/01/13 23:31:00 ezk Exp $
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
 * KLUDGE: wrap whole file in HAVE_FS_AUTOFS, because
 * not all systems with an automounter file system are supported
 * by am-utils yet...
 */

#ifdef HAVE_FS_AUTOFS

/*
 * MACROS:
 */
#ifndef AUTOFS_NULL
# define AUTOFS_NULL	((u_long)0)
#endif /* not AUTOFS_NULL */

/*
 * VARIABLES:
 */

/* forward declarations */
static int mount_autofs(char *dir, char *opts);
static int autofs_mount_1_svc(struct mntrequest *mr, struct mntres *result, struct authunix_parms *cred);
static int autofs_unmount_1_svc(struct umntrequest *ur, struct umntres *result, struct authunix_parms *cred);

/* external declarations */
extern bool_t xdr_mntrequest(XDR *, mntrequest *);
extern bool_t xdr_mntres(XDR *, mntres *);
extern bool_t xdr_umntrequest(XDR *, umntrequest *);
extern bool_t xdr_umntres(XDR *, umntres *);

/*
 * STRUCTURES:
 */

/* Sun's kernel-based automounter-supporting file system */
am_ops autofs_ops =
{
  "autofs",
  amfs_auto_match,
  0,				/* amfs_auto_init */
  autofs_mount,
  0,
  autofs_umount,
  0,
  amfs_auto_lookuppn,
  amfs_auto_readdir,		/* browsable version of readdir() */
  0,				/* autofs_readlink */
  autofs_mounted,
  0,				/* autofs_umounted */
  find_amfs_auto_srvr,
  FS_MKMNT | FS_NOTIMEOUT | FS_BACKGROUND | FS_AMQINFO | FS_DIRECTORY
};


/****************************************************************************
 *** FUNCTIONS                                                            ***
 ****************************************************************************/

/*
 * Mount the top-level using autofs
 */
int
autofs_mount(am_node *mp)
{
  mntfs *mf = mp->am_mnt;
  struct stat stb;
  char opts[256], preopts[256];
  int error;
  char *mnttype;

  /*
   * Mounting the automounter.
   * Make sure the mount directory exists, construct
   * the mount options and call the mount_autofs routine.
   */

  if (stat(mp->am_path, &stb) < 0) {
    return errno;
  } else if ((stb.st_mode & S_IFMT) != S_IFDIR) {
    plog(XLOG_WARNING, "%s is not a directory", mp->am_path);
    return ENOTDIR;
  }
  if (mf->mf_ops == &autofs_ops)
    mnttype = "indirect";
  else if (mf->mf_ops == &amfs_direct_ops)
    mnttype = "direct";
#ifdef HAVE_AM_FS_UNION
  else if (mf->mf_ops == &amfs_union_ops)
    mnttype = "union";
#endif /* HAVE_AM_FS_UNION */
  else
    mnttype = "auto";

  /*
   * Construct some mount options:
   *
   * Tack on magic map=<mapname> option in mtab to emulate
   * SunOS automounter behavior.
   */
  preopts[0] = '\0';
#ifdef MNTTAB_OPT_INTR
  strcat(preopts, MNTTAB_OPT_INTR);
  strcat(preopts, ",");
#endif /* MNTTAB_OPT_INTR */
#ifdef MNTTAB_OPT_IGNORE
  strcat(preopts, MNTTAB_OPT_IGNORE);
  strcat(preopts, ",");
#endif /* MNTTAB_OPT_IGNORE */
  sprintf(opts, "%s%s,%s=%d,%s=%d,%s=%d,%s,map=%s",
	  preopts,
	  MNTTAB_OPT_RW,
	  MNTTAB_OPT_PORT, nfs_port,
	  MNTTAB_OPT_TIMEO, gopt.amfs_auto_timeo,
	  MNTTAB_OPT_RETRANS, gopt.amfs_auto_retrans,
	  mnttype, mf->mf_info);

  /* now do the mount */
  error = mount_autofs(mf->mf_mount, opts);
  if (error) {
    errno = error;
    plog(XLOG_FATAL, "mount_autofs: %m");
    return error;
  }
  return 0;
}


void
autofs_mounted(mntfs *mf)
{
  amfs_auto_mkcacheref(mf);
}


/*
 * Unmount a top-level automount node
 */
int
autofs_umount(am_node *mp)
{
  int error;
  struct stat stb;

  /*
   * The lstat is needed if this mount is type=direct.  When that happens,
   * the kernel cache gets confused between the underlying type (dir) and
   * the mounted type (link) and so needs to be re-synced before the
   * unmount.  This is all because the unmount system call follows links and
   * so can't actually unmount a link (stupid!).  It was noted that doing an
   * ls -ld of the mount point to see why things were not working actually
   * fixed the problem - so simulate an ls -ld here.
   */
  if (lstat(mp->am_path, &stb) < 0) {
#ifdef DEBUG
    dlog("lstat(%s): %m", mp->am_path);
#endif /* DEBUG */
  }
  error = UMOUNT_FS(mp->am_path, mnttab_file_name);
  if (error == EBUSY && mp->am_flags & AMF_AUTOFS) {
    plog(XLOG_WARNING, "autofs_unmount of %s busy (autofs). exit", mp->am_path);
    error = 0;			/* fake unmount was ok */
  }
  return error;
}


/*
 * Mount an automounter directory.
 * The automounter is connected into the system
 * as a user-level NFS server.  mount_autofs constructs
 * the necessary NFS parameters to be given to the
 * kernel so that it will talk back to us.
 */
static int
mount_autofs(char *dir, char *opts)
{
  char fs_hostname[MAXHOSTNAMELEN + MAXPATHLEN + 1];
  char *map_opt, buf[MAXHOSTNAMELEN];
  int retry, error, flags;
  struct utsname utsname;
  mntent_t mnt;
  autofs_args_t autofs_args;
  MTYPE_TYPE type = MOUNT_TYPE_AUTOFS;

  memset((voidp) &autofs_args, 0, sizeof(autofs_args)); /* Paranoid */

  memset((voidp) &mnt, 0, sizeof(mnt));
  mnt.mnt_dir = dir;
  mnt.mnt_fsname = pid_fsname;
  mnt.mnt_opts = opts;
  mnt.mnt_type = type;

  retry = hasmntval(&mnt, "retry");
  if (retry <= 0)
    retry = 2;			/* XXX */

  /*
   * SET MOUNT ARGS
   */
  if (uname(&utsname) < 0) {
    strcpy(buf, "localhost.autofs");
  } else {
    strcpy(buf, utsname.nodename);
    strcat(buf, ".autofs");
  }
#ifdef HAVE_FIELD_AUTOFS_ARGS_T_ADDR
  autofs_args.addr.buf = buf;
  autofs_args.addr.len = strlen(autofs_args.addr.buf);
  autofs_args.addr.maxlen = autofs_args.addr.len;
#endif /* HAVE_FIELD_AUTOFS_ARGS_T_ADDR */

  autofs_args.path = dir;
  autofs_args.opts = opts;

  map_opt = hasmntopt(&mnt, "map");
  if (map_opt) {
    map_opt += sizeof("map="); /* skip the "map=" */
    if (map_opt == NULL) {
      plog(XLOG_WARNING, "map= has a null map name. reset to amd.unknown");
      map_opt = "amd.unknown";
    }
  }
  autofs_args.map = map_opt;

  /* XXX: these I set arbitrarily... */
  autofs_args.mount_to = 300;
  autofs_args.rpc_to = 60;
  autofs_args.direct = 0;

  /*
   * Make a ``hostname'' string for the kernel
   */
  sprintf(fs_hostname, "pid%ld@%s:%s",
	  (long) (foreground ? am_mypid : getppid()),
	  am_get_hostname(), dir);

  /*
   * Most kernels have a name length restriction.
   */
  if (strlen(fs_hostname) >= MAXHOSTNAMELEN)
    strcpy(fs_hostname + MAXHOSTNAMELEN - 3, "..");

  /*
   * Finally we can compute the mount flags set above.
   */
  flags = compute_mount_flags(&mnt);

  /*
   * This is it!  Here we try to mount amd on its mount points.
   */
  error = mount_fs(&mnt, flags, (caddr_t) &autofs_args, retry, type, 0, NULL, mnttab_file_name);
  return error;
}


/****************************************************************************/
/* autofs program dispatcher */
void
autofs_program_1(struct svc_req *rqstp, SVCXPRT *transp)
{
  int ret;
  union {
    mntrequest autofs_mount_1_arg;
    umntrequest autofs_umount_1_arg;
  } argument;
  union {
    mntres mount_res;
    umntres umount_res;
  } result;

  bool_t (*xdr_argument)(), (*xdr_result)();
  int (*local)();

  switch (rqstp->rq_proc) {

  case AUTOFS_NULL:
    svc_sendreply(transp,
		  (XDRPROC_T_TYPE) xdr_void,
		  (SVC_IN_ARG_TYPE) NULL);
    return;

  case AUTOFS_MOUNT:
    xdr_argument = xdr_mntrequest;
    xdr_result = xdr_mntres;
    local = (int (*)()) autofs_mount_1_svc;
    break;

  case AUTOFS_UNMOUNT:
    xdr_argument = xdr_umntrequest;
    xdr_result = xdr_umntres;
    local = (int (*)()) autofs_unmount_1_svc;
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

  ret = (*local) (&argument, &result, rqstp);
  if (!svc_sendreply(transp,
		     (XDRPROC_T_TYPE) xdr_result,
		     (SVC_IN_ARG_TYPE) &result)) {
    svcerr_systemerr(transp);
  }

  if (!svc_freeargs(transp,
		    (XDRPROC_T_TYPE) xdr_argument,
		    (SVC_IN_ARG_TYPE) &argument)) {
    plog(XLOG_FATAL, "unable to free rpc arguments in autofs_program_1");
    going_down(1);
  }
}


static int
autofs_mount_1_svc(struct mntrequest *mr, struct mntres *result, struct authunix_parms *cred)
{
  int err = 0;
  am_node *anp, *anp2;

  plog(XLOG_INFO, "XXX: autofs_mount_1_svc: %s:%s:%s:%s",
       mr->map, mr->name, mr->opts, mr->path);

  /* look for map (eg. "/home") */
  anp = find_ap(mr->path);
  if (!anp) {
    plog(XLOG_ERROR, "map %s not found", mr->path);
    err = ENOENT;
    goto out;
  }
  /* turn on autofs in map flags */
  if (!(anp->am_flags & AMF_AUTOFS)) {
    plog(XLOG_INFO, "turning on AMF_AUTOFS for node %s", mr->path);
    anp->am_flags |= AMF_AUTOFS;
  }

  /*
   * Look for (and create if needed) the new node.
   *
   * If an error occurred, return it.  If a -1 was returned, that indicates
   * that a mount is in progress, so sleep a while (while the backgrounded
   * mount is happening), and then signal the autofs to retry the mount.
   *
   * There's something I don't understand.  I was thinking that this code
   * here is the one which will succeed eventually and will send an RPC
   * reply to the kernel, but apparently that happens somewhere else, not
   * here.  It works though, just that I don't know how.  Arg. -Erez.
   * */
  err = 0;
  anp2 = autofs_lookuppn(anp, mr->name, &err, VLOOK_CREATE);
  if (!anp2) {
    if (err == -1) {		/* then tell autofs to retry */
      sleep(1);
      err = EAGAIN;
    }
    goto out;
  }

out:
  result->status = err;
  return err;
}


static int
autofs_unmount_1_svc(struct umntrequest *ur, struct umntres *result, struct authunix_parms *cred)
{
  int err = 0;

#ifdef HAVE_FIELD_UMNTREQUEST_RDEVID
  plog(XLOG_INFO, "XXX: autofs_unmount_1_svc: %d:%lu:%lu:0x%lx",
       ur->isdirect, (unsigned long) ur->devid, (unsigned long) ur->rdevid,
       (unsigned long) ur->next);
#else /* HAVE_FIELD_UMNTREQUEST_RDEVID */
  plog(XLOG_INFO, "XXX: autofs_unmount_1_svc: %d:%lu:0x%lx",
       ur->isdirect, (unsigned long) ur->devid,
       (unsigned long) ur->next);
#endif /* HAVE_FIELD_UMNTREQUEST_RDEVID */

  err = EINVAL;			/* XXX: not implemented yet */
  goto out;

out:
  result->status = err;
  return err;
}


/*
 * Pick a file system to try mounting and
 * do that in the background if necessary
 *
 For each location:
 if it is new -defaults then
 extract and process
 continue;
 fi
 if it is a cut then
 if a location has been tried then
 break;
 fi
 continue;
 fi
 parse mount location
 discard previous mount location if required
 find matching mounted filesystem
 if not applicable then
 this_error = No such file or directory
 continue
 fi
 if the filesystem failed to be mounted then
 this_error = error from filesystem
 elif the filesystem is mounting or unmounting then
 this_error = -1
 elif the fileserver is down then
 this_error = -1
 elif the filesystem is already mounted
 this_error = 0
 break
 fi
 if no error on this mount then
 this_error = initialize mount point
 fi
 if no error on this mount and mount is delayed then
 this_error = -1
 fi
 if this_error < 0 then
 retry = true
 fi
 if no error on this mount then
 make mount point if required
 fi
 if no error on this mount then
 if mount in background then
 run mount in background
 return -1
 else
 this_error = mount in foreground
 fi
 fi
 if an error occurred on this mount then
 update stats
 save error in mount point
 fi
 endfor
 */
static int
autofs_bgmount(struct continuation * cp, int mpe)
{
  mntfs *mf = cp->mp->am_mnt;	/* Current mntfs */
  mntfs *mf_retry = 0;		/* First mntfs which needed retrying */
  int this_error = -1;		/* Per-mount error */
  int hard_error = -1;
  int mp_error = mpe;

  /*
   * Try to mount each location.
   * At the end:
   * hard_error == 0 indicates something was mounted.
   * hard_error > 0 indicates everything failed with a hard error
   * hard_error < 0 indicates nothing could be mounted now
   */
  for (; this_error && *cp->ivec; cp->ivec++) {
    am_ops *p;
    am_node *mp = cp->mp;
    char *link_dir;
    int dont_retry;

    if (hard_error < 0)
      hard_error = this_error;

    this_error = -1;

    if (**cp->ivec == '-') {
      /*
       * Pick up new defaults
       */
      if (cp->auto_opts && *cp->auto_opts)
	cp->def_opts = str3cat(cp->def_opts, cp->auto_opts, ";", *cp->ivec + 1);
      else
	cp->def_opts = strealloc(cp->def_opts, *cp->ivec + 1);
#ifdef DEBUG
      dlog("Setting def_opts to \"%s\"", cp->def_opts);
#endif /* DEBUG */
      continue;
    }
    /*
     * If a mount has been attempted, and we find
     * a cut then don't try any more locations.
     */
    if (STREQ(*cp->ivec, "/") || STREQ(*cp->ivec, "||")) {
      if (cp->tried) {
#ifdef DEBUG
	dlog("Cut: not trying any more locations for %s",
	     mp->am_path);
#endif /* DEBUG */
	break;
      }
      continue;
    }

    /* match the operators */
    p = ops_match(&cp->fs_opts, *cp->ivec, cp->def_opts, mp->am_path, cp->key, mp->am_parent->am_mnt->mf_info);

    /*
     * Find a mounted filesystem for this node.
     */
    mp->am_mnt = mf = realloc_mntfs(mf, p, &cp->fs_opts,
				    cp->fs_opts.opt_fs,
				    cp->fs_opts.fs_mtab,
				    cp->auto_opts,
				    cp->fs_opts.opt_opts,
				    cp->fs_opts.opt_remopts);

    p = mf->mf_ops;
#ifdef DEBUG
    dlog("Got a hit with %s", p->fs_type);
#endif /* DEBUG */

    /*
     * Note whether this is a real mount attempt
     */
    if (p == &amfs_error_ops) {
      plog(XLOG_MAP, "Map entry %s for %s failed to match", *cp->ivec, mp->am_path);
      if (this_error <= 0)
	this_error = ENOENT;
      continue;
    } else {
      if (cp->fs_opts.fs_mtab) {
	plog(XLOG_MAP, "Trying mount of %s on \"%s\" fstype %s",
	     cp->fs_opts.fs_mtab, mp->am_path, p->fs_type);
      }
      cp->tried = TRUE;
    }

    this_error = 0;
    dont_retry = FALSE;

    if (mp->am_link) {
      XFREE(mp->am_link);
      mp->am_link = 0;
    }
    link_dir = mf->mf_fo->opt_sublink;

    if (link_dir && *link_dir) {
      if (*link_dir == '/') {
	mp->am_link = strdup(link_dir);
      } else {
	/*
	 * try getting fs option from continuation, not mountpoint!
	 * Don't try logging the string from mf, since it may be bad!
	 */
	if (cp->fs_opts.opt_fs != mf->mf_fo->opt_fs)
	  plog(XLOG_ERROR, "use %s instead of 0x%lx",
	       cp->fs_opts.opt_fs, (unsigned long) mf->mf_fo->opt_fs);

	mp->am_link = str3cat((char *) 0,
			      cp->fs_opts.opt_fs, "/", link_dir);

	normalize_slash(mp->am_link);
      }
    }

    if (mf->mf_error > 0) {
      this_error = mf->mf_error;
    } else if (mf->mf_flags & (MFF_MOUNTING | MFF_UNMOUNTING)) {
      /*
       * Still mounting - retry later
       */
#ifdef DEBUG
      dlog("Duplicate pending mount fstype %s", p->fs_type);
#endif /* DEBUG */
      this_error = -1;
    } else if (FSRV_ISDOWN(mf->mf_server)) {
      /*
       * Would just mount from the same place
       * as a hung mount - so give up
       */
#ifdef DEBUG
      dlog("%s is already hung - giving up", mf->mf_mount);
#endif /* DEBUG */
      mp_error = EWOULDBLOCK;
      dont_retry = TRUE;
      this_error = -1;
    } else if (mf->mf_flags & MFF_MOUNTED) {
#ifdef DEBUG
      dlog("duplicate mount of \"%s\" ...", mf->mf_info);
#endif /* DEBUG */

      /*
       * Just call mounted()
       */
      am_mounted(mp);

      this_error = 0;
      break;
    }

    /*
     * Will usually need to play around with the mount nodes
     * file attribute structure.  This must be done here.
     * Try and get things initialized, even if the fileserver
     * is not known to be up.  In the common case this will
     * progress things faster.
     */
    if (!this_error) {
      /*
       * Fill in attribute fields.
       */
      if (mf->mf_ops->fs_flags & FS_DIRECTORY)
	mk_fattr(mp, NFDIR);
      else
	mk_fattr(mp, NFLNK);

      mp->am_fattr.na_fileid = mp->am_gen;

      if (p->fs_init)
	this_error = (*p->fs_init) (mf);
    }

    /*
     * Make sure the fileserver is UP before doing any more work
     */
    if (!FSRV_ISUP(mf->mf_server)) {
#ifdef DEBUG
      dlog("waiting for server %s to become available", mf->mf_server->fs_host);
#endif /* DEBUG */
      this_error = -1;
    }

    if (!this_error && mf->mf_fo->opt_delay) {
      /*
       * If there is a delay timer on the mount
       * then don't try to mount if the timer
       * has not expired.
       */
      int i = atoi(mf->mf_fo->opt_delay);
      if (i > 0 && clocktime() < (cp->start + i)) {
#ifdef DEBUG
	dlog("Mount of %s delayed by %lds", mf->mf_mount, i - clocktime() + cp->start);
#endif /* DEBUG */
	this_error = -1;
      }
    }

    if (this_error < 0 && !dont_retry) {
      if (!mf_retry)
	mf_retry = dup_mntfs(mf);
      cp->retry = TRUE;
    }

    if (!this_error) {
      if (p->fs_flags & FS_MBACKGROUND) {
	mf->mf_flags |= MFF_MOUNTING;	/* XXX */
#ifdef DEBUG
	dlog("backgrounding mount of \"%s\"", mf->mf_mount);
#endif /* DEBUG */
	if (cp->callout) {
	  untimeout(cp->callout);
	  cp->callout = 0;
	}
	run_task(try_mount, (voidp) mp, amfs_auto_cont, (voidp) cp);
	mf->mf_flags |= MFF_MKMNT;	/* XXX */
	if (mf_retry)
	  free_mntfs(mf_retry);
	return -1;
      } else {
#ifdef DEBUG
	dlog("foreground mount of \"%s\" ...", mf->mf_info);
#endif /* DEBUG */
	this_error = try_mount((voidp) mp);
	if (this_error < 0) {
	  if (!mf_retry)
	    mf_retry = dup_mntfs(mf);
	  cp->retry = TRUE;
	}
      }
    }

    if (this_error >= 0) {
      if (this_error > 0) {
	amd_stats.d_merr++;
	if (mf != mf_retry) {
	  mf->mf_error = this_error;
	  mf->mf_flags |= MFF_ERROR;
	}
      }

      /*
       * Wakeup anything waiting for this mount
       */
      wakeup((voidp) mf);
    }
  }

  if (this_error && cp->retry) {
    free_mntfs(mf);
    mf = cp->mp->am_mnt = mf_retry;
    /*
     * Not retrying again (so far)
     */
    cp->retry = FALSE;
    cp->tried = FALSE;
    /*
     * Start at the beginning.
     * Rewind the location vector and
     * reset the default options.
     */
    cp->ivec = cp->xivec;
    cp->def_opts = strealloc(cp->def_opts, cp->auto_opts);
    /*
     * Arrange that autofs_bgmount is called
     * after anything else happens.
     */
#ifdef DEBUG
    dlog("Arranging to retry mount of %s", cp->mp->am_path);
#endif /* DEBUG */
    sched_task(amfs_auto_retry, (voidp) cp, (voidp) mf);
    if (cp->callout)
      untimeout(cp->callout);
    cp->callout = timeout(RETRY_INTERVAL, wakeup, (voidp) mf);

    cp->mp->am_ttl = clocktime() + RETRY_INTERVAL;

    /*
     * Not done yet - so don't return anything
     */
    return -1;
  }

  if (hard_error < 0 || this_error == 0)
    hard_error = this_error;

  /*
   * Discard handle on duff filesystem.
   * This should never happen since it
   * should be caught by the case above.
   */
  if (mf_retry) {
    if (hard_error)
      plog(XLOG_ERROR, "discarding a retry mntfs for %s", mf_retry->mf_mount);
    free_mntfs(mf_retry);
  }

  /*
   * If we get here, then either the mount succeeded or
   * there is no more mount information available.
   */
  if (hard_error < 0 && mp_error)
    hard_error = cp->mp->am_error = mp_error;
  if (hard_error > 0) {
    /*
     * Set a small(ish) timeout on an error node if
     * the error was not a time out.
     */
    switch (hard_error) {
    case ETIMEDOUT:
    case EWOULDBLOCK:
      cp->mp->am_timeo = 17;
      break;
    default:
      cp->mp->am_timeo = 5;
      break;
    }
    new_ttl(cp->mp);
  }

  /*
   * Make sure that the error value in the mntfs has a
   * reasonable value.
   */
  if (mf->mf_error < 0) {
    mf->mf_error = hard_error;
    if (hard_error)
      mf->mf_flags |= MFF_ERROR;
  }

  /*
   * In any case we don't need the continuation any more
   */
  free_continuation(cp);

  return hard_error;
}


/*
 * Automount interface to RPC lookup routine
 * Find the corresponding entry and return
 * the file handle for it.
 */
am_node *
autofs_lookuppn(am_node *mp, char *fname, int *error_return, int op)
{
  am_node *ap, *new_mp, *ap_hung;
  char *info;			/* Mount info - where to get the file system */
  char **ivec, **xivec;		/* Split version of info */
  char *auto_opts;		/* Automount options */
  int error = 0;		/* Error so far */
  char path_name[MAXPATHLEN];	/* General path name buffer */
  char apath[MAXPATHLEN];	/* autofs path (added space) */
  char *pfname;			/* Path for database lookup */
  struct continuation *cp;	/* Continuation structure if need to mount */
  int in_progress = 0;		/* # of (un)mount in progress */
  char *dflts;
  mntfs *mf;

#ifdef DEBUG
  dlog("in autofs_lookuppn");
#endif /* DEBUG */

  /*
   * If the server is shutting down
   * then don't return information
   * about the mount point.
   */
  if (amd_state == Finishing) {
#ifdef DEBUG
    if ((mf = mp->am_mnt) == 0 || mf->mf_ops == &amfs_direct_ops) {
      dlog("%s mount ignored - going down", fname);
    } else {
      dlog("%s/%s mount ignored - going down", mp->am_path, fname);
    }
#endif /* DEBUG */
    ereturn(ENOENT);
  }

  /*
   * Handle special case of "." and ".."
   */
  if (fname[0] == '.') {
    if (fname[1] == '\0')
      return mp;		/* "." is the current node */
    if (fname[1] == '.' && fname[2] == '\0') {
      if (mp->am_parent) {
#ifdef DEBUG
	dlog(".. in %s gives %s", mp->am_path, mp->am_parent->am_path);
#endif /* DEBUG */
	return mp->am_parent;	/* ".." is the parent node */
      }
      ereturn(ESTALE);
    }
  }

  /*
   * Check for valid key name.
   * If it is invalid then pretend it doesn't exist.
   */
  if (!valid_key(fname)) {
    plog(XLOG_WARNING, "Key \"%s\" contains a disallowed character", fname);
    ereturn(ENOENT);
  }

  /*
   * Expand key name.
   * fname is now a private copy.
   */
  fname = expand_key(fname);

  for (ap_hung = 0, ap = mp->am_child; ap; ap = ap->am_osib) {
    /*
     * Otherwise search children of this node
     */
    if (FSTREQ(ap->am_name, fname)) {
      mf = ap->am_mnt;
      if (ap->am_error) {
	error = ap->am_error;
	continue;
      }
      /*
       * If the error code is undefined then it must be
       * in progress.
       */
      if (mf->mf_error < 0)
	goto in_progrss;

      /*
       * Check for a hung node
       */
      if (FSRV_ISDOWN(mf->mf_server)) {
#ifdef DEBUG
	dlog("server hung");
#endif /* DEBUG */
	error = ap->am_error;
	ap_hung = ap;
	continue;
      }
      /*
       * If there was a previous error with this node
       * then return that error code.
       */
      if (mf->mf_flags & MFF_ERROR) {
	error = mf->mf_error;
	continue;
      }
      if (!(mf->mf_flags & MFF_MOUNTED) || (mf->mf_flags & MFF_UNMOUNTING)) {
      in_progrss:
	/*
	 * If the fs is not mounted or it is unmounting then there
	 * is a background (un)mount in progress.  In this case
	 * we just drop the RPC request (return nil) and
	 * wait for a retry, by which time the (un)mount may
	 * have completed.
	 */
#ifdef DEBUG
	dlog("ignoring mount of %s in %s -- flags (%x) in progress",
	     fname, mf->mf_mount, mf->mf_flags);
#endif /* DEBUG */
	in_progress++;
	continue;
      }

      /*
       * Otherwise we have a hit: return the current mount point.
       */
#ifdef DEBUG
      dlog("matched %s in %s", fname, ap->am_path);
#endif /* DEBUG */
      XFREE(fname);
      return ap;
    }
  }

  if (in_progress) {
#ifdef DEBUG
    dlog("Waiting while %d mount(s) in progress", in_progress);
#endif /* DEBUG */
    XFREE(fname);
    ereturn(-1);
  }

  /*
   * If an error occurred then return it.
   */
  if (error) {
#ifdef DEBUG
    errno = error;		/* XXX */
    dlog("Returning error: %m");
#endif /* DEBUG */
    XFREE(fname);
    ereturn(error);
  }

  /*
   * If doing a delete then don't create again!
   */
  switch (op) {
  case VLOOK_DELETE:
    ereturn(ENOENT);

  case VLOOK_CREATE:
    break;

  default:
    plog(XLOG_FATAL, "Unknown op to autofs_lookuppn: 0x%x", op);
    ereturn(EINVAL);
  }

  /*
   * If the server is going down then just return,
   * don't try to mount any more file systems
   */
  if ((int) amd_state >= (int) Finishing) {
#ifdef DEBUG
    dlog("not found - server going down anyway");
#endif /* DEBUG */
    XFREE(fname);
    ereturn(ENOENT);
  }

  /*
   * If we get there then this is a reference to an,
   * as yet, unknown name so we need to search the mount
   * map for it.
   */
  if (mp->am_pref) {
    sprintf(path_name, "%s%s", mp->am_pref, fname);
    pfname = path_name;
  } else {
    pfname = fname;
  }

  mf = mp->am_mnt;

#ifdef DEBUG
  dlog("will search map info in %s to find %s", mf->mf_info, pfname);
#endif /* DEBUG */
  /*
   * Consult the oracle for some mount information.
   * info is malloc'ed and belongs to this routine.
   * It ends up being free'd in free_continuation().
   *
   * Note that this may return -1 indicating that information
   * is not yet available.
   */
  error = mapc_search((mnt_map *) mf->mf_private, pfname, &info);
  if (error) {
    if (error > 0)
      plog(XLOG_MAP, "No map entry for %s", pfname);
    else
      plog(XLOG_MAP, "Waiting on map entry for %s", pfname);
    XFREE(fname);
    ereturn(error);
  }
#ifdef DEBUG
  dlog("mount info is %s", info);
#endif /* DEBUG */

  /*
   * Split info into an argument vector.
   * The vector is malloc'ed and belongs to
   * this routine.  It is free'd in free_continuation()
   */
  xivec = ivec = strsplit(info, ' ', '\"');

  /*
   * Default error code...
   */
  if (ap_hung)
    error = EWOULDBLOCK;
  else
    error = ENOENT;

  /*
   * Allocate a new map
   */
  new_mp = exported_ap_alloc();
  if (new_mp == 0) {
    XFREE(xivec);
    XFREE(info);
    XFREE(fname);
    ereturn(ENOSPC);
  }
  if (mf->mf_auto)
    auto_opts = mf->mf_auto;
  else
    auto_opts = "";

  auto_opts = strdup(auto_opts);

#ifdef DEBUG
  dlog("searching for /defaults entry");
#endif /* DEBUG */
  if (mapc_search((mnt_map *) mf->mf_private, "/defaults", &dflts) == 0) {
    char *dfl;
    char **rvec;
#ifdef DEBUG
    dlog("/defaults gave %s", dflts);
#endif /* DEBUG */
    if (*dflts == '-')
      dfl = dflts + 1;
    else
      dfl = dflts;

    /*
     * Chop the defaults up
     */
    rvec = strsplit(dfl, ' ', '\"');

    if (gopt.flags & CFM_ENABLE_DEFAULT_SELECTORS) {
      /*
       * Pick whichever first entry matched the list of selectors.
       * Strip the selectors from the string, and assign to dfl the
       * rest of the string.
       */
      if (rvec) {
	am_opts ap;
	am_ops *pt;
	char **sp = rvec;
	while (*sp) {		/* loop until you find something, if any */
	  memset((char *) &ap, 0, sizeof(am_opts));
	  pt = ops_match(&ap, *sp, "", mp->am_path, "/defaults",
			 mp->am_parent->am_mnt->mf_info);
	  free_opts(&ap);	/* don't leak */
	  if (pt == &amfs_error_ops) {
	    plog(XLOG_MAP, "failed to match defaults for \"%s\"", *sp);
	  } else {
	    dfl = strip_selectors(*sp, "/defaults");
	    plog(XLOG_MAP, "matched default selectors \"%s\"", dfl);
	    break;
	  }
	  ++sp;
	}
      }
    } else {			/* not enable_default_selectors */
      /*
       * Extract first value
       */
      dfl = rvec[0];
    }

    /*
     * If there were any values at all...
     */
    if (dfl) {
      /*
       * Log error if there were other values
       */
      if (!(gopt.flags & CFM_ENABLE_DEFAULT_SELECTORS) && rvec[1]) {
# ifdef DEBUG
	dlog("/defaults chopped into %s", dfl);
# endif /* DEBUG */
	plog(XLOG_USER, "More than a single value for /defaults in %s", mf->mf_info);
      }

      /*
       * Prepend to existing defaults if they exist,
       * otherwise just use these defaults.
       */
      if (*auto_opts && *dfl) {
	char *nopts = (char *) xmalloc(strlen(auto_opts) + strlen(dfl) + 2);
	sprintf(nopts, "%s;%s", dfl, auto_opts);
	XFREE(auto_opts);
	auto_opts = nopts;
      } else if (*dfl) {
	auto_opts = strealloc(auto_opts, dfl);
      }
    }
    XFREE(dflts);
    /*
     * Don't need info vector any more
     */
    XFREE(rvec);
  }

  /*
   * Fill it in
   */
  init_map(new_mp, fname);

  /*
   * Turn on autofs flag if needed.
   */
  if (mp->am_flags & AMF_AUTOFS) {
    new_mp->am_flags |= AMF_AUTOFS;
  }

  /*
   * Put it in the table
   */
  insert_am(new_mp, mp);

  /*
   * Fill in some other fields,
   * path and mount point.
   *
   * bugfix: do not prepend old am_path if direct map
   *         <wls@astro.umd.edu> William Sebok
   */

  strcpy(apath, fname);
  strcat(apath, " ");
  new_mp->am_path = str3cat(new_mp->am_path,
			    mf->mf_ops == &amfs_direct_ops ? "" : mp->am_path,
			    *fname == '/' ? "" : "/",
			    apath);

#ifdef DEBUG
  dlog("setting path to \"%s\"", new_mp->am_path);
#endif /* DEBUG */

  /*
   * Take private copy of pfname
   */
  pfname = strdup(pfname);

  /*
   * Construct a continuation
   */
  cp = ALLOC(struct continuation);
  cp->callout = 0;
  cp->mp = new_mp;
  cp->xivec = xivec;
  cp->ivec = ivec;
  cp->info = info;
  cp->key = pfname;
  cp->auto_opts = auto_opts;
  cp->retry = FALSE;
  cp->tried = FALSE;
  cp->start = clocktime();
  cp->def_opts = strdup(auto_opts);
  memset((voidp) &cp->fs_opts, 0, sizeof(cp->fs_opts));

  /*
   * Try and mount the file system.  If this succeeds immediately (possible
   * for a ufs file system) then return the attributes, otherwise just
   * return an error.
   */
  error = autofs_bgmount(cp, error);
  reschedule_timeout_mp();
  if (!error) {
    XFREE(fname);
    return new_mp;
  }

  /*
   * Code for quick reply.  If nfs_program_2_transp is set, then
   * its the transp that's been passed down from nfs_program_2().
   * If new_mp->am_transp is not already set, set it by copying in
   * nfs_program_2_transp.  Once am_transp is set, quick_reply() can
   * use it to send a reply to the client that requested this mount.
   */
  if (nfs_program_2_transp && !new_mp->am_transp) {
    new_mp->am_transp = (SVCXPRT *) xmalloc(sizeof(SVCXPRT));
    *(new_mp->am_transp) = *nfs_program_2_transp;
  }
  if (error && (new_mp->am_mnt->mf_ops == &amfs_error_ops))
    new_mp->am_error = error;

  assign_error_mntfs(new_mp);

  XFREE(fname);

  ereturn(error);
}
#endif /* HAVE_FS_AUTOFS */
