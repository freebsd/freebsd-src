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
 * $Id: amfs_auto.c,v 1.9.2.12 2004/01/06 03:15:16 ezk Exp $
 *
 */

/*
 * Automount file system
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/****************************************************************************
 *** MACROS                                                               ***
 ****************************************************************************/
#define	IN_PROGRESS(cp) ((cp)->mp->am_mnt->mf_flags & MFF_MOUNTING)

#define DOT_DOT_COOKIE	(u_int) 1

/****************************************************************************
 *** STRUCTURES                                                           ***
 ****************************************************************************/


/****************************************************************************
 *** FORWARD DEFINITIONS                                                  ***
 ****************************************************************************/
static int amfs_auto_bgmount(struct continuation *cp, int mpe);
static int amfs_auto_mount(am_node *mp);
static int amfs_auto_readdir_browsable(am_node *mp, nfscookie cookie, nfsdirlist *dp, nfsentry *ep, int count, int fully_browsable);
static void amfs_auto_umounted(am_node *mp);


/****************************************************************************
 *** OPS STRUCTURES                                                       ***
 ****************************************************************************/
am_ops amfs_auto_ops =
{
  "auto",
  amfs_auto_match,
  0,				/* amfs_auto_init */
  amfs_auto_mount,
  0,
  amfs_auto_umount,
  0,
  amfs_auto_lookuppn,
  amfs_auto_readdir,
  0,				/* amfs_auto_readlink */
  0,				/* amfs_auto_mounted */
  amfs_auto_umounted,
  find_amfs_auto_srvr,
  FS_AMQINFO | FS_DIRECTORY
};


/****************************************************************************
 *** FUNCTIONS                                                             ***
 ****************************************************************************/
/*
 * AMFS_AUTO needs nothing in particular.
 */
char *
amfs_auto_match(am_opts *fo)
{
  char *p = fo->opt_rfs;

  if (!fo->opt_rfs) {
    plog(XLOG_USER, "auto: no mount point named (rfs:=)");
    return 0;
  }
  if (!fo->opt_fs) {
    plog(XLOG_USER, "auto: no map named (fs:=)");
    return 0;
  }

  /*
   * Swap round fs:= and rfs:= options
   * ... historical (jsp)
   */
  fo->opt_rfs = fo->opt_fs;
  fo->opt_fs = p;

  /*
   * mtab entry turns out to be the name of the mount map
   */
  return strdup(fo->opt_rfs ? fo->opt_rfs : ".");
}




/*
 * Build a new map cache for this node, or re-use
 * an existing cache for the same map.
 */
void
amfs_auto_mkcacheref(mntfs *mf)
{
  char *cache;

  if (mf->mf_fo && mf->mf_fo->opt_cache)
    cache = mf->mf_fo->opt_cache;
  else
    cache = "none";
  mf->mf_private = (voidp) mapc_find(mf->mf_info, cache,
				     mf->mf_fo->opt_maptype);
  mf->mf_prfree = mapc_free;
}


/*
 * Mount a sub-mount
 */
static int
amfs_auto_mount(am_node *mp)
{
  mntfs *mf = mp->am_mnt;

  /*
   * Pseudo-directories are used to provide some structure
   * to the automounted directories instead
   * of putting them all in the top-level automount directory.
   *
   * Here, just increment the parent's link count.
   */
  mp->am_parent->am_fattr.na_nlink++;

  /*
   * Info field of . means use parent's info field.
   * Historical - not documented.
   */
  if (mf->mf_info[0] == '.' && mf->mf_info[1] == '\0')
    mf->mf_info = strealloc(mf->mf_info, mp->am_parent->am_mnt->mf_info);

  /*
   * Compute prefix:
   *
   * If there is an option prefix then use that else
   * If the parent had a prefix then use that with name
   *      of this node appended else
   * Use the name of this node.
   *
   * That means if you want no prefix you must say so
   * in the map.
   */
  if (mf->mf_fo->opt_pref) {
    /* allow pref:=null to set a real null prefix */
    if (STREQ(mf->mf_fo->opt_pref, "null")) {
      mp->am_pref = strdup("");
    } else {
      /*
       * the prefix specified as an option
       */
      mp->am_pref = strdup(mf->mf_fo->opt_pref);
    }
  } else {
    /*
     * else the parent's prefix
     * followed by the name
     * followed by /
     */
    char *ppref = mp->am_parent->am_pref;
    if (ppref == 0)
      ppref = "";
    mp->am_pref = str3cat((char *) 0, ppref, mp->am_name, "/");
  }

  /*
   * Attach a map cache
   */
  amfs_auto_mkcacheref(mf);

  return 0;
}




/*
 * Unmount an automount sub-node
 */
int
amfs_auto_umount(am_node *mp)
{
  return 0;
}


/*
 * Unmount an automount node
 */
static void
amfs_auto_umounted(am_node *mp)
{
  /*
   * If this is a pseudo-directory then just adjust the link count
   * in the parent, otherwise call the generic unmount routine
   */
  if (mp->am_parent && mp->am_parent->am_parent)
    --mp->am_parent->am_fattr.na_nlink;
}


/*
 * Discard an old continuation
 */
void
free_continuation(struct continuation *cp)
{
  if (cp->callout)
    untimeout(cp->callout);
  XFREE(cp->key);
  XFREE(cp->xivec);
  XFREE(cp->info);
  XFREE(cp->auto_opts);
  XFREE(cp->def_opts);
  free_opts(&cp->fs_opts);
  XFREE(cp);
}


/*
 * Discard the underlying mount point and replace
 * with a reference to an error filesystem.
 */
void
assign_error_mntfs(am_node *mp)
{
  if (mp->am_error > 0) {
    /*
     * Save the old error code
     */
    int error = mp->am_error;
    if (error <= 0)
      error = mp->am_mnt->mf_error;
    /*
     * Discard the old filesystem
     */
    free_mntfs(mp->am_mnt);
    /*
     * Allocate a new error reference
     */
    mp->am_mnt = new_mntfs();
    /*
     * Put back the error code
     */
    mp->am_mnt->mf_error = error;
    mp->am_mnt->mf_flags |= MFF_ERROR;
    /*
     * Zero the error in the mount point
     */
    mp->am_error = 0;
  }
}


/*
 * The continuation function.  This is called by
 * the task notifier when a background mount attempt
 * completes.
 */
void
amfs_auto_cont(int rc, int term, voidp closure)
{
  struct continuation *cp = (struct continuation *) closure;
  mntfs *mf = cp->mp->am_mnt;

  /*
   * Definitely not trying to mount at the moment
   */
  mf->mf_flags &= ~MFF_MOUNTING;

  /*
   * While we are mounting - try to avoid race conditions
   */
  new_ttl(cp->mp);

  /*
   * Wakeup anything waiting for this mount
   */
  wakeup((voidp) mf);

  /*
   * Check for termination signal or exit status...
   */
  if (rc || term) {
    am_node *xmp;

    if (term) {
      /*
       * Not sure what to do for an error code.
       */
      mf->mf_error = EIO;	/* XXX ? */
      mf->mf_flags |= MFF_ERROR;
      plog(XLOG_ERROR, "mount for %s got signal %d", cp->mp->am_path, term);
    } else {
      /*
       * Check for exit status...
       */
      mf->mf_error = rc;
      mf->mf_flags |= MFF_ERROR;
      errno = rc;		/* XXX */
      if (!STREQ(cp->mp->am_mnt->mf_ops->fs_type, "linkx"))
	plog(XLOG_ERROR, "%s: mount (amfs_auto_cont): %m", cp->mp->am_path);
    }

    /*
     * If we get here then that attempt didn't work, so
     * move the info vector pointer along by one and
     * call the background mount routine again
     */
    amd_stats.d_merr++;
    cp->ivec++;
    xmp = cp->mp;
    (void) amfs_auto_bgmount(cp, 0);
    assign_error_mntfs(xmp);
  } else {
    /*
     * The mount worked.
     */
    am_mounted(cp->mp);
    free_continuation(cp);
  }

  reschedule_timeout_mp();
}


/*
 * Retry a mount
 */
void
amfs_auto_retry(int rc, int term, voidp closure)
{
  struct continuation *cp = (struct continuation *) closure;
  int error = 0;

#ifdef DEBUG
  dlog("Commencing retry for mount of %s", cp->mp->am_path);
#endif /* DEBUG */

  new_ttl(cp->mp);

  if ((cp->start + ALLOWED_MOUNT_TIME) < clocktime()) {
    /*
     * The entire mount has timed out.  Set the error code and skip past all
     * the info vectors so that amfs_auto_bgmount will not have any more
     * ways to try the mount, so causing an error.
     */
    plog(XLOG_INFO, "mount of \"%s\" has timed out", cp->mp->am_path);
    error = ETIMEDOUT;
    while (*cp->ivec)
      cp->ivec++;
    /* explicitly forbid further retries after timeout */
    cp->retry = FALSE;
  }
  if (error || !IN_PROGRESS(cp)) {
    (void) amfs_auto_bgmount(cp, error);
  }
  reschedule_timeout_mp();
}


/*
 * Try to mount a file system.  Can be called
 * directly or in a sub-process by run_task.
 */
int
try_mount(voidp mvp)
{
  int error = 0;
  am_node *mp = (am_node *) mvp;
  mntfs *mf = mp->am_mnt;

  /*
   * If the directory is not yet made and it needs to be made, then make it!
   * This may be run in a background process in which case the flag setting
   * won't be noticed later - but it is set anyway just after run_task is
   * called.  It should probably go away totally...
   */
  if (!(mf->mf_flags & MFF_MKMNT) && mf->mf_ops->fs_flags & FS_MKMNT) {
    error = mkdirs(mf->mf_mount, 0555);
    if (!error)
      mf->mf_flags |= MFF_MKMNT;
  }

  /*
   * Mount it!
   */
  error = mount_node(mp);

#ifdef DEBUG
  if (error > 0) {
    errno = error;
    dlog("amfs_auto: call to mount_node(%s) failed: %m", mp->am_path);
  }
#endif /* DEBUG */

  return error;
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
amfs_auto_bgmount(struct continuation *cp, int mpe)
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
      plog(XLOG_MAP, "Map entry %s for %s did not match", *cp->ivec, mp->am_path);
      if (this_error <= 0)
	this_error = ENOENT;
      continue;
    } else {
      if (cp->fs_opts.fs_mtab) {
	plog(XLOG_MAP, "Trying mount of %s on %s fstype %s",
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
	 * Try getting fs option from continuation, not mountpoint!
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
	dlog("Mount of %s delayed by %lds", mf->mf_mount, (long) (i - clocktime() + cp->start));
#endif /* DEBUG */
	this_error = -1;
      }
    }

    if (this_error < 0 && !dont_retry) {
      if (!mf_retry)
	mf_retry = dup_mntfs(mf);
      cp->retry = TRUE;
#ifdef DEBUG
      dlog("will retry ...\n");
#endif /* DEBUG */
      break;
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

	/* actually run the task, backgrounding as necessary */
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
#ifdef DEBUG
    dlog("(skipping rewind)\n");
#endif /* DEBUG */
    /*
     * Arrange that amfs_auto_bgmount is called
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
amfs_auto_lookuppn(am_node *mp, char *fname, int *error_return, int op)
{
  am_node *ap, *new_mp, *ap_hung;
  char *info;			/* Mount info - where to get the file system */
  char **ivec, **xivec;		/* Split version of info */
  char *auto_opts;		/* Automount options */
  int error = 0;		/* Error so far */
  char path_name[MAXPATHLEN];	/* General path name buffer */
  char *pfname;			/* Path for database lookup */
  struct continuation *cp;	/* Continuation structure if need to mount */
  int in_progress = 0;		/* # of (un)mount in progress */
  char *dflts;
  mntfs *mf;

#ifdef DEBUG
  dlog("in amfs_auto_lookuppn");
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
    plog(XLOG_FATAL, "Unknown op to amfs_auto_lookuppn: 0x%x", op);
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

    if (gopt.flags & CFM_SELECTORS_IN_DEFAULTS) {
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
	  /*
	   * This next routine cause many spurious "expansion of ... is"
	   * messages, which are ignored, b/c all we need out of this
	   * routine is to match selectors.  These spurious messages may
	   * be wrong, esp. if they try to expand ${key} b/c it will
	   * get expanded to "/defaults"
	   */
	  pt = ops_match(&ap, *sp, "", mp->am_path, "/defaults",
			 mp->am_parent->am_mnt->mf_info);
	  free_opts(&ap);	/* don't leak */
	  if (pt == &amfs_error_ops) {
	    plog(XLOG_MAP, "did not match defaults for \"%s\"", *sp);
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
      if (!(gopt.flags & CFM_SELECTORS_IN_DEFAULTS) && rvec[1]) {
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
  new_mp->am_path = str3cat(new_mp->am_path,
			    mf->mf_ops == &amfs_direct_ops ? "" : mp->am_path,
			    *fname == '/' ? "" : "/", fname);

#ifdef DEBUG
  dlog("setting path to %s", new_mp->am_path);
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
  error = amfs_auto_bgmount(cp, error);
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


/*
 * Locate next node in sibling list which is mounted
 * and is not an error node.
 */
am_node *
next_nonerror_node(am_node *xp)
{
  mntfs *mf;

  /*
   * Bug report (7/12/89) from Rein Tollevik <rein@ifi.uio.no>
   * Fixes a race condition when mounting direct automounts.
   * Also fixes a problem when doing a readdir on a directory
   * containing hung automounts.
   */
  while (xp &&
	 (!(mf = xp->am_mnt) ||	/* No mounted filesystem */
	  mf->mf_error != 0 ||	/* There was a mntfs error */
	  xp->am_error != 0 ||	/* There was a mount error */
	  !(mf->mf_flags & MFF_MOUNTED) ||	/* The fs is not mounted */
	  (mf->mf_server->fs_flags & FSF_DOWN))	/* The fs may be down */
	 )
    xp = xp->am_osib;

  return xp;
}


/*
 * This readdir function which call a special version of it that allows
 * browsing if browsable_dirs=yes was set on the map.
 */
int
amfs_auto_readdir(am_node *mp, nfscookie cookie, nfsdirlist *dp, nfsentry *ep, int count)
{
  u_int gen = *(u_int *) cookie;
  am_node *xp;
  mntent_t mnt;
#ifdef DEBUG
  nfsentry *ne;
  static int j;
#endif /* DEBUG */

  dp->dl_eof = FALSE;		/* assume readdir not done */

  /* check if map is browsable */
  if (mp->am_mnt && mp->am_mnt->mf_mopts) {
    mnt.mnt_opts = mp->am_mnt->mf_mopts;
    if (hasmntopt(&mnt, "fullybrowsable"))
      return amfs_auto_readdir_browsable(mp, cookie, dp, ep, count, TRUE);
    if (hasmntopt(&mnt, "browsable"))
      return amfs_auto_readdir_browsable(mp, cookie, dp, ep, count, FALSE);
  }

  /* when gen is 0, we start reading from the beginning of the directory */
  if (gen == 0) {
    /*
     * In the default instance (which is used to start a search) we return
     * "." and "..".
     *
     * This assumes that the count is big enough to allow both "." and ".."
     * to be returned in a single packet.  If it isn't (which would be
     * fairly unbelievable) then tough.
     */
#ifdef DEBUG
    dlog("amfs_auto_readdir: default search");
#endif /* DEBUG */
    /*
     * Check for enough room.  This is extremely approximate but is more
     * than enough space.  Really need 2 times:
     *      4byte fileid
     *      4byte cookie
     *      4byte name length
     *      4byte name
     * plus the dirlist structure */
    if (count < (2 * (2 * (sizeof(*ep) + sizeof("..") + 4) + sizeof(*dp))))
      return EINVAL;

    xp = next_nonerror_node(mp->am_child);
    dp->dl_entries = ep;

    /* construct "." */
    ep[0].ne_fileid = mp->am_gen;
    ep[0].ne_name = ".";
    ep[0].ne_nextentry = &ep[1];
    *(u_int *) ep[0].ne_cookie = 0;

    /* construct ".." */
    if (mp->am_parent)
      ep[1].ne_fileid = mp->am_parent->am_gen;
    else
      ep[1].ne_fileid = mp->am_gen;
    ep[1].ne_name = "..";
    ep[1].ne_nextentry = 0;
    *(u_int *) ep[1].ne_cookie = (xp ? xp->am_gen : DOT_DOT_COOKIE);

    if (!xp)
      dp->dl_eof = TRUE;	/* by default assume readdir done */

#ifdef DEBUG
    amuDebug(D_READDIR)
      for (j=0,ne=ep; ne; ne=ne->ne_nextentry)
	plog(XLOG_DEBUG, "gen1 key %4d \"%s\" fi=%d ck=%d",
	     j++, ne->ne_name, ne->ne_fileid, *(u_int *)ne->ne_cookie);
#endif /* DEBUG */
    return 0;
  }
#ifdef DEBUG
  dlog("amfs_auto_readdir: real child");
#endif /* DEBUG */

  if (gen == DOT_DOT_COOKIE) {
#ifdef DEBUG
    dlog("amfs_auto_readdir: End of readdir in %s", mp->am_path);
#endif /* DEBUG */
    dp->dl_eof = TRUE;
    dp->dl_entries = 0;
#ifdef DEBUG
    amuDebug(D_READDIR)
      plog(XLOG_DEBUG, "end of readdir eof=TRUE, dl_entries=0\n");
#endif /* DEBUG */
    return 0;
  }

  /* non-browsable directories code */
  xp = mp->am_child;
  while (xp && xp->am_gen != gen)
    xp = xp->am_osib;

  if (xp) {
    int nbytes = count / 2;	/* conservative */
    int todo = MAX_READDIR_ENTRIES;

    dp->dl_entries = ep;
    do {
      am_node *xp_next = next_nonerror_node(xp->am_osib);

      if (xp_next) {
	*(u_int *) ep->ne_cookie = xp_next->am_gen;
      } else {
	*(u_int *) ep->ne_cookie = DOT_DOT_COOKIE;
	dp->dl_eof = TRUE;
      }

      ep->ne_fileid = xp->am_gen;
      ep->ne_name = xp->am_name;
      nbytes -= sizeof(*ep) + 1;
      if (xp->am_name)
	nbytes -= strlen(xp->am_name);

      xp = xp_next;

      if (nbytes > 0 && !dp->dl_eof && todo > 1) {
	ep->ne_nextentry = ep + 1;
	ep++;
	--todo;
      } else {
	todo = 0;
      }
    } while (todo > 0);

    ep->ne_nextentry = 0;

#ifdef DEBUG
    amuDebug(D_READDIR)
      for (j=0,ne=ep; ne; ne=ne->ne_nextentry)
	plog(XLOG_DEBUG, "gen2 key %4d \"%s\" fi=%d ck=%d",
	     j++, ne->ne_name, ne->ne_fileid, *(u_int *)ne->ne_cookie);
#endif /* DEBUG */
    return 0;
  }
  return ESTALE;
}


/* This one is called only if map is browsable */
static int
amfs_auto_readdir_browsable(am_node *mp, nfscookie cookie, nfsdirlist *dp, nfsentry *ep, int count, int fully_browsable)
{
  u_int gen = *(u_int *) cookie;
  int chain_length, i;
  static nfsentry *te, *te_next;
#ifdef DEBUG
  nfsentry *ne;
  static int j;
#endif /* DEBUG */

  dp->dl_eof = FALSE;		/* assume readdir not done */

#ifdef DEBUG
  amuDebug(D_READDIR)
    plog(XLOG_DEBUG, "amfs_auto_readdir_browsable gen=%u, count=%d",
	 gen, count);
#endif /* DEBUG */

  if (gen == 0) {
    /*
     * In the default instance (which is used to start a search) we return
     * "." and "..".
     *
     * This assumes that the count is big enough to allow both "." and ".."
     * to be returned in a single packet.  If it isn't (which would be
     * fairly unbelievable) then tough.
     */
#ifdef DEBUG
    dlog("amfs_auto_readdir_browsable: default search");
#endif /* DEBUG */
    /*
     * Check for enough room.  This is extremely approximate but is more
     * than enough space.  Really need 2 times:
     *      4byte fileid
     *      4byte cookie
     *      4byte name length
     *      4byte name
     * plus the dirlist structure */
    if (count < (2 * (2 * (sizeof(*ep) + sizeof("..") + 4) + sizeof(*dp))))
      return EINVAL;

    /*
     * compute # of entries to send in this chain.
     * heuristics: 128 bytes per entry.
     * This is too much probably, but it seems to work better because
     * of the re-entrant nature of nfs_readdir, and esp. on systems
     * like OpenBSD 2.2.
     */
    chain_length = count / 128;

    /* reset static state counters */
    te = te_next = NULL;

    dp->dl_entries = ep;

    /* construct "." */
    ep[0].ne_fileid = mp->am_gen;
    ep[0].ne_name = ".";
    ep[0].ne_nextentry = &ep[1];
    *(u_int *) ep[0].ne_cookie = 0;

    /* construct ".." */
    if (mp->am_parent)
      ep[1].ne_fileid = mp->am_parent->am_gen;
    else
      ep[1].ne_fileid = mp->am_gen;

    ep[1].ne_name = "..";
    ep[1].ne_nextentry = 0;
    *(u_int *) ep[1].ne_cookie = DOT_DOT_COOKIE;

    /*
     * If map is browsable, call a function make_entry_chain() to construct
     * a linked list of unmounted keys, and return it.  Then link the chain
     * to the regular list.  Get the chain only once, but return
     * chunks of it each time.
     */
    te = make_entry_chain(mp, dp->dl_entries, fully_browsable);
    if (!te)
      return 0;
#ifdef DEBUG
    amuDebug(D_READDIR)
      for (j=0,ne=te; ne; ne=ne->ne_nextentry)
	plog(XLOG_DEBUG, "gen1 key %4d \"%s\"", j++, ne->ne_name);
#endif /* DEBUG */

    /* return only "chain_length" entries */
    te_next = te;
    for (i=1; i<chain_length; ++i) {
      te_next = te_next->ne_nextentry;
      if (!te_next)
	break;
    }
    if (te_next) {
      nfsentry *te_saved = te_next->ne_nextentry;
      te_next->ne_nextentry = NULL; /* terminate "te" chain */
      te_next = te_saved;	/* save rest of "te" for next iteration */
      dp->dl_eof = FALSE;	/* tell readdir there's more */
    } else {
      dp->dl_eof = TRUE;	/* tell readdir that's it */
    }
    ep[1].ne_nextentry = te;	/* append this chunk of "te" chain */
#ifdef DEBUG
    amuDebug(D_READDIR) {
      for (j=0,ne=te; ne; ne=ne->ne_nextentry)
	plog(XLOG_DEBUG, "gen2 key %4d \"%s\"", j++, ne->ne_name);
      for (j=0,ne=ep; ne; ne=ne->ne_nextentry)
	plog(XLOG_DEBUG, "gen2+ key %4d \"%s\" fi=%d ck=%d",
	     j++, ne->ne_name, ne->ne_fileid, *(u_int *)ne->ne_cookie);
      plog(XLOG_DEBUG, "EOF is %d", dp->dl_eof);
    }
#endif /* DEBUG_READDIR */
    return 0;
  } /* end of "if (gen == 0)" statement */

#ifdef DEBUG
  dlog("amfs_auto_readdir_browsable: real child");
#endif /* DEBUG */

  if (gen == DOT_DOT_COOKIE) {
#ifdef DEBUG
    dlog("amfs_auto_readdir_browsable: End of readdir in %s", mp->am_path);
#endif /* DEBUG */
    dp->dl_eof = TRUE;
    dp->dl_entries = 0;
    return 0;
  }

  /*
   * If browsable directories, then continue serving readdir() with another
   * chunk of entries, starting from where we left off (when gen was equal
   * to 0).  Once again, assume last chunk served to readdir.
   */
  dp->dl_eof = TRUE;
  dp->dl_entries = ep;

  te = te_next;			/* reset 'te' from last saved te_next */
  if (!te) {			/* another indicator of end of readdir */
    dp->dl_entries = 0;
    return 0;
  }
  /*
   * compute # of entries to send in this chain.
   * heuristics: 128 bytes per entry.
   */
  chain_length = count / 128;

  /* return only "chain_length" entries */
  for (i=1; i<chain_length; ++i) {
    te_next = te_next->ne_nextentry;
    if (!te_next)
      break;
  }
  if (te_next) {
    nfsentry *te_saved = te_next->ne_nextentry;
    te_next->ne_nextentry = NULL; /* terminate "te" chain */
    te_next = te_saved;		/* save rest of "te" for next iteration */
    dp->dl_eof = FALSE;		/* tell readdir there's more */
  }
  ep = te;			/* send next chunk of "te" chain */
  dp->dl_entries = ep;
#ifdef DEBUG
  amuDebug(D_READDIR) {
    plog(XLOG_DEBUG, "dl_entries=0x%lx, te_next=0x%lx, dl_eof=%d",
	 (long) dp->dl_entries, (long) te_next, dp->dl_eof);
    for (ne=te; ne; ne=ne->ne_nextentry)
      plog(XLOG_DEBUG, "gen3 key %4d \"%s\"", j++, ne->ne_name);
  }
#endif /* DEBUG */
  return 0;
}


int
amfs_auto_fmount(am_node *mp)
{
  mntfs *mf = mp->am_mnt;
  return (*mf->mf_ops->fmount_fs) (mf);
}


int
amfs_auto_fumount(am_node *mp)
{
  mntfs *mf = mp->am_mnt;
  return (*mf->mf_ops->fumount_fs) (mf);
}
