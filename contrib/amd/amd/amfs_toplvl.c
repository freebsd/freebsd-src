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
 * $Id: amfs_toplvl.c,v 1.7.2.4 2002/12/27 22:44:32 ezk Exp $
 *
 */

/*
 * Top-level file system
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amd.h>

/****************************************************************************
 *** FORWARD DEFINITIONS                                                  ***
 ****************************************************************************/


/****************************************************************************
 *** OPS STRUCTURES                                                       ***
 ****************************************************************************/
am_ops amfs_toplvl_ops =
{
  "toplvl",
  amfs_auto_match,
  0,				/* amfs_auto_init */
  amfs_toplvl_mount,
  0,
  amfs_toplvl_umount,
  0,
  amfs_auto_lookuppn,
  amfs_auto_readdir,		/* browsable version of readdir() */
  0,				/* amfs_toplvl_readlink */
  amfs_toplvl_mounted,
  0,				/* amfs_toplvl_umounted */
  find_amfs_auto_srvr,
  FS_MKMNT | FS_NOTIMEOUT | FS_BACKGROUND | FS_AMQINFO | FS_DIRECTORY
};


/****************************************************************************
 *** FUNCTIONS                                                             ***
 ****************************************************************************/

/*
 * Mount an automounter directory.
 * The automounter is connected into the system
 * as a user-level NFS server.  mount_amfs_toplvl constructs
 * the necessary NFS parameters to be given to the
 * kernel so that it will talk back to us.
 *
 * NOTE: automounter mounts in themselves are using NFS Version 2.
 */
static int
mount_amfs_toplvl(char *dir, char *opts)
{
  char fs_hostname[MAXHOSTNAMELEN + MAXPATHLEN + 1];
  int retry, error, genflags;
  mntent_t mnt;
  nfs_args_t nfs_args;
  am_nfs_fh *fhp;
  am_nfs_handle_t anh;
  MTYPE_TYPE type = MOUNT_TYPE_NFS;
#ifndef HAVE_TRANSPORT_TYPE_TLI
  u_short port;
  struct sockaddr_in sin;
#endif /* not HAVE_TRANSPORT_TYPE_TLI */

  memset((voidp) &mnt, 0, sizeof(mnt));
  mnt.mnt_dir = dir;
  mnt.mnt_fsname = pid_fsname;
  mnt.mnt_opts = opts;

  /*
   * Make sure that amd's top-level NFS mounts are hidden by default
   * from df.
   * If they don't appear to support the either the "ignore" mnttab
   * option entry, or the "auto" one, set the mount type to "nfs".
   */
  mnt.mnt_type = HIDE_MOUNT_TYPE;

  retry = hasmntval(&mnt, MNTTAB_OPT_RETRY);
  if (retry <= 0)
    retry = 2;			/* XXX */

  /*
   * SET MOUNT ARGS
   */
  /*
   * get fhandle of remote path for automount point
   */
  fhp = root_fh(dir);
  if (!fhp) {
    plog(XLOG_FATAL, "Can't find root file handle for %s", dir);
    return EINVAL;
  }

#ifndef HAVE_TRANSPORT_TYPE_TLI
  /*
   * Create sockaddr to point to the local machine.  127.0.0.1
   * is not used since that will not work in HP-UX clusters and
   * this is no more expensive.
   */
  memset((voidp) &sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr = myipaddr;
  port = hasmntval(&mnt, MNTTAB_OPT_PORT);
  if (port) {
    sin.sin_port = htons(port);
  } else {
    plog(XLOG_ERROR, "no port number specified for %s", dir);
    return EINVAL;
  }
#endif /* not HAVE_TRANSPORT_TYPE_TLI */

  /*
   * Make a ``hostname'' string for the kernel
   */
  sprintf(fs_hostname, "pid%ld@%s:%s",
	  (long) (foreground ? am_mypid : getppid()),
	  am_get_hostname(),
	  dir);
  /*
   * Most kernels have a name length restriction (64 bytes)...
   */
  if (strlen(fs_hostname) >= MAXHOSTNAMELEN)
    strcpy(fs_hostname + MAXHOSTNAMELEN - 3, "..");
#ifdef HOSTNAMESZ
  /*
   * ... and some of these restrictions are 32 bytes (HOSTNAMESZ)
   * If you need to get the definition for HOSTNAMESZ found, you may
   * add the proper header file to the conf/nfs_prot/nfs_prot_*.h file.
   */
  if (strlen(fs_hostname) >= HOSTNAMESZ)
    strcpy(fs_hostname + HOSTNAMESZ - 3, "..");
#endif /* HOSTNAMESZ */

  /*
   * Finally we can compute the mount genflags set above,
   * and add any automounter specific flags.
   */
  genflags = compute_mount_flags(&mnt);
  genflags |= compute_automounter_mount_flags(&mnt);

  /* setup the many fields and flags within nfs_args */
  memmove(&anh.v2.fhs_fh, fhp, sizeof(*fhp));
#ifdef HAVE_TRANSPORT_TYPE_TLI
  compute_nfs_args(&nfs_args,
		   &mnt,
		   genflags,
		   nfsncp,
		   NULL,	/* remote host IP addr is set below */
		   NFS_VERSION,	/* version 2 */
		   "udp",
		   &anh,
		   fs_hostname,
		   pid_fsname);
  /*
   * IMPORTANT: set the correct IP address AFTERWARDS.  It cannot
   * be done using the normal mechanism of compute_nfs_args(), because
   * that one will allocate a new address and use NFS_SA_DREF() to copy
   * parts to it, while assuming that the ip_addr passed is always
   * a "struct sockaddr_in".  That assumption is incorrect on TLI systems,
   * because they define a special macro HOST_SELF which is DIFFERENT
   * than localhost (127.0.0.1)!
   */
  nfs_args.addr = &nfsxprt->xp_ltaddr;
#else /* not HAVE_TRANSPORT_TYPE_TLI */
  compute_nfs_args(&nfs_args,
		   &mnt,
		   genflags,
		   &sin,
		   NFS_VERSION,	/* version 2 */
		   "udp",
		   &anh,
		   fs_hostname,
		   pid_fsname);
#endif /* not HAVE_TRANSPORT_TYPE_TLI */

  /*************************************************************************
   * NOTE: while compute_nfs_args() works ok for regular NFS mounts	   *
   * the toplvl one is not, and so some options must be corrected by hand  *
   * more carefully, *after* compute_nfs_args() runs.			   *
   *************************************************************************/
  compute_automounter_nfs_args(&nfs_args, &mnt);

  /* This is it!  Here we try to mount amd on its mount points */
#ifdef DEBUG
  amuDebug(D_TRACE) {
    print_nfs_args(&nfs_args, 0);
    plog(XLOG_DEBUG, "Generic mount flags 0x%x", genflags);
  }
#endif /* DEBUG */
  error = mount_fs(&mnt, genflags, (caddr_t) &nfs_args, retry, type,
		   0, NULL, mnttab_file_name);

#ifdef HAVE_TRANSPORT_TYPE_TLI
  free_knetconfig(nfs_args.knconf);
  /*
   * local automounter mounts do not allocate a special address, so
   * no need to XFREE(nfs_args.addr) under TLI.
   */
#endif /* HAVE_TRANSPORT_TYPE_TLI */

  return error;
}


/*
 * Mount the top-level
 */
int
amfs_toplvl_mount(am_node *mp)
{
  mntfs *mf = mp->am_mnt;
  struct stat stb;
  char opts[256], preopts[256];
  int error;
  char *mnttype;

  /*
   * Mounting the automounter.
   * Make sure the mount directory exists, construct
   * the mount options and call the mount_amfs_toplvl routine.
   */

  if (stat(mp->am_path, &stb) < 0) {
    return errno;
  } else if ((stb.st_mode & S_IFMT) != S_IFDIR) {
    plog(XLOG_WARNING, "%s is not a directory", mp->am_path);
    return ENOTDIR;
  }
  if (mf->mf_ops == &amfs_toplvl_ops)
    mnttype = "indirect";
  else if (mf->mf_ops == &amfs_direct_ops)
    mnttype = "direct";
#ifdef HAVE_AMU_FS_UNION
  else if (mf->mf_ops == &amfs_union_ops)
    mnttype = "union";
#endif /* HAVE_AMU_FS_UNION */
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
  error = mount_amfs_toplvl(mf->mf_mount, opts);
  if (error) {
    errno = error;
    plog(XLOG_FATAL, "amfs_toplvl_mount: mount_amfs_toplvl failed: %m");
    return error;
  }
  return 0;
}


void
amfs_toplvl_mounted(mntfs *mf)
{
  amfs_auto_mkcacheref(mf);
}


/*
 * Unmount a top-level automount node
 */
int
amfs_toplvl_umount(am_node *mp)
{
  int error;
  struct stat stb;

again:
  /*
   * The lstat is needed if this mount is type=direct.
   * When that happens, the kernel cache gets confused
   * between the underlying type (dir) and the mounted
   * type (link) and so needs to be re-synced before
   * the unmount.  This is all because the unmount system
   * call follows links and so can't actually unmount
   * a link (stupid!).  It was noted that doing an ls -ld
   * of the mount point to see why things were not working
   * actually fixed the problem - so simulate an ls -ld here.
   */
  if (lstat(mp->am_path, &stb) < 0) {
#ifdef DEBUG
    dlog("lstat(%s): %m", mp->am_path);
#endif /* DEBUG */
  }
  error = UMOUNT_FS(mp->am_path, mnttab_file_name);
  if (error == EBUSY) {
    plog(XLOG_WARNING, "amfs_toplvl_unmount retrying %s in 1s", mp->am_path);
    sleep(1);			/* XXX */
    goto again;
  }
  return error;
}
