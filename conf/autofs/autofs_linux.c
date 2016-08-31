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
 * File: am-utils/conf/autofs/autofs_linux.c
 *
 */

/*
 * Automounter filesystem for Linux
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

#define AUTOFS_MIN_VERSION 3
#if AUTOFS_MAX_PROTO_VERSION >= 5
/*
 * Autofs version 5 support is experimental; change this to 5 you want
 * to play with, it. There are reports it does not work.
 */
#define AUTOFS_MAX_VERSION 4	/* we only know up to version 5 */
#else
#define AUTOFS_MAX_VERSION AUTOFS_MAX_PROTO_VERSION
#endif

/*
 * STRUCTURES:
 */

/*
 * VARIABLES:
 */

static int autofs_max_fds;
static am_node **hash;
static int *list;
static int numfds = 0;
static int bind_works = 1;


static void
hash_init(void)
{
  int i;
  struct rlimit rlim;

  if (getrlimit(RLIMIT_NOFILE, &rlim) < 0) {
    plog(XLOG_ERROR, "getrlimit failed, defaulting to 256 fd's");
    autofs_max_fds = 256;
  } else {
    autofs_max_fds = (rlim.rlim_cur > 1024) ? 1024 : rlim.rlim_cur;
    plog(XLOG_INFO, "%d fd's available for autofs", autofs_max_fds);
  }

  list = malloc(autofs_max_fds * sizeof(*list));
  hash = malloc(autofs_max_fds * sizeof(*hash));

  for (i = 0 ; i < autofs_max_fds; i++) {
    hash[i] = NULL;
    list[i] = -1;
  }
}


static void
hash_insert(int fd, am_node *mp)
{
  if (hash[fd] != 0)
    plog(XLOG_ERROR, "file descriptor %d already in the hash", fd);

  hash[fd] = mp;
  list[numfds] = fd;
  numfds++;
}


static void
hash_delete(int fd)
{
  int i;

  if (hash[fd] == 0)
    plog(XLOG_WARNING, "file descriptor %d not in the hash", fd);

  hash[fd] = NULL;
  numfds--;
  for (i = 0; i < numfds; i++)
    if (list[i] == fd) {
      list[i] = list[numfds];
      break;
    }
}


int
autofs_get_fh(am_node *mp)
{
  int fds[2];
  autofs_fh_t *fh;

  plog(XLOG_DEBUG, "autofs_get_fh for %s", mp->am_path);
  if (pipe(fds) < 0)
    return errno;

  /* sanity check */
  if (fds[0] > autofs_max_fds) {
    close(fds[0]);
    close(fds[1]);
    return EMFILE;
  }

  fh = ALLOC(autofs_fh_t);
  fh->fd = fds[0];
  fh->kernelfd = fds[1];
  fh->ioctlfd = -1;
  fh->pending_mounts = NULL;
  fh->pending_umounts = NULL;

  mp->am_autofs_fh = fh;

  return 0;
}


void
autofs_mounted(am_node *mp)
{
  autofs_fh_t *fh = mp->am_autofs_fh;
  unsigned long timeout = gopt.am_timeo;

  close(fh->kernelfd);
  fh->kernelfd = -1;

  autofs_get_mp(mp);

  /* Get autofs protocol version */
  if (ioctl(fh->ioctlfd, AUTOFS_IOC_PROTOVER, &fh->version) < 0) {
    plog(XLOG_ERROR, "AUTOFS_IOC_PROTOVER: %s", strerror(errno));
    fh->version = AUTOFS_MIN_VERSION;
    plog(XLOG_ERROR, "autofs: assuming protocol version %d", fh->version);
  } else
    plog(XLOG_INFO, "autofs: using protocol version %d", fh->version);

  /* set expiration timeout */
  if (ioctl(fh->ioctlfd, AUTOFS_IOC_SETTIMEOUT, &timeout) < 0)
    plog(XLOG_ERROR, "AUTOFS_IOC_SETTIMEOUT: %s", strerror(errno));

  /* tell the daemon to call us for expirations */
  mp->am_autofs_ttl = clocktime(NULL) + gopt.am_timeo_w;
}


void
autofs_get_mp(am_node *mp)
{
  autofs_fh_t *fh = mp->am_autofs_fh;
  dlog("autofs: getting mount point");
  if (fh->ioctlfd < 0)
    fh->ioctlfd = open(mp->am_path, O_RDONLY);
  hash_insert(fh->fd, mp);
}


void
autofs_release_mp(am_node *mp)
{
  autofs_fh_t *fh = mp->am_autofs_fh;
  dlog("autofs: releasing mount point");
  if (fh->ioctlfd >= 0) {
    close(fh->ioctlfd);
    fh->ioctlfd = -1;
  }
  /*
   * take the kernel fd out of the hash/fdset
   * so select() doesn't go crazy if the umount succeeds
   */
  if (fh->fd >= 0)
    hash_delete(fh->fd);
}


void
autofs_release_fh(am_node *mp)
{
  autofs_fh_t *fh = mp->am_autofs_fh;
  struct autofs_pending_mount **pp, *p;
  struct autofs_pending_umount **upp, *up;

  dlog("autofs: releasing file handle");
  if (fh) {
    /*
     * if a mount succeeded, the kernel fd was closed on
     * the amd side, so it might have been reused.
     * we set it to -1 after closing it, to avoid the problem.
     */
    if (fh->kernelfd >= 0)
      close(fh->kernelfd);

    if (fh->ioctlfd >= 0)
      close(fh->ioctlfd);

    if (fh->fd >= 0)
      close(fh->fd);

    pp = &fh->pending_mounts;
    while (*pp) {
      p = *pp;
      XFREE(p->name);
      *pp = p->next;
      XFREE(p);
    }

    upp = &fh->pending_umounts;
    while (*upp) {
      up = *upp;
      XFREE(up->name);
      *upp = up->next;
      XFREE(up);
    }

    XFREE(fh);
    mp->am_autofs_fh = NULL;
  }
}


void
autofs_add_fdset(fd_set *readfds)
{
  int i;
  for (i = 0; i < numfds; i++)
    FD_SET(list[i], readfds);
}


static ssize_t
autofs_get_pkt(int fd, void *buf, size_t bytes)
{
  ssize_t i;

  do {
    i = read(fd, buf, bytes);

    if (i <= 0)
      break;

    buf = (char *)buf + i;
    bytes -= i;
  } while (bytes);

  return bytes;
}


static void
send_fail(int fd, autofs_wqt_t token)
{
  if (token == 0)
    return;
  if (ioctl(fd, AUTOFS_IOC_FAIL, token) < 0)
    plog(XLOG_ERROR, "AUTOFS_IOC_FAIL: %s", strerror(errno));
}


static void
send_ready(int fd, autofs_wqt_t token)
{
  if (token == 0)
    return;
  if (ioctl(fd, AUTOFS_IOC_READY, token) < 0)
    plog(XLOG_ERROR, "AUTOFS_IOC_READY: %s", strerror(errno));
}


static void
autofs_lookup_failed(am_node *mp, char *name)
{
  autofs_fh_t *fh = mp->am_autofs_fh;
  struct autofs_pending_mount **pp, *p;

  pp = &fh->pending_mounts;
  while (*pp && !STREQ((*pp)->name, name))
    pp = &(*pp)->next;

  /* sanity check */
  if (*pp == NULL)
    return;

  p = *pp;
  plog(XLOG_INFO, "autofs: lookup of %s failed", name);
  send_fail(fh->ioctlfd, p->wait_queue_token);

  XFREE(p->name);
  *pp = p->next;
  XFREE(p);
}


static void
autofs_expire_one(am_node *mp, char *name, autofs_wqt_t token)
{
  autofs_fh_t *fh;
  am_node *ap;
  struct autofs_pending_umount *p;
  char *ap_path;

  fh = mp->am_autofs_fh;

  ap_path = str3cat(NULL, mp->am_path, "/", name);
  if (amuDebug(D_TRACE))
    plog(XLOG_DEBUG, "\tumount(%s)", ap_path);

  p = fh->pending_umounts;
  while (p && p->wait_queue_token != token)
    p = p->next;

  if (p) {
    /* already pending */
    dlog("Umounting of %s already pending", ap_path);
    amd_stats.d_drops++;
    goto out;
  }

  ap = find_ap(ap_path);
  if (ap == NULL) {
    /* not found??? not sure what to do here... */
    send_fail(fh->ioctlfd, token);
    goto out;
  }

  p = ALLOC(struct autofs_pending_umount);
  p->wait_queue_token = token;
  p->name = xstrdup(name);
  p->next = fh->pending_umounts;
  fh->pending_umounts = p;

  unmount_mp(ap);

out:
  XFREE(ap_path);
}


static void
autofs_missing_one(am_node *mp, autofs_wqt_t wait_queue_token, char *name)
{
  autofs_fh_t *fh;
  mntfs *mf;
  am_node *ap;
  struct autofs_pending_mount *p;
  int error;

  mf = mp->am_al->al_mnt;
  fh = mp->am_autofs_fh;

  p = fh->pending_mounts;
  while (p && p->wait_queue_token != wait_queue_token)
    p = p->next;

  if (p) {
    /* already pending */
    dlog("Mounting of %s/%s already pending",
	 mp->am_path, name);
    amd_stats.d_drops++;
    return;
  }

  p = ALLOC(struct autofs_pending_mount);
  p->wait_queue_token = wait_queue_token;
  p->name = xstrdup(name);
  p->next = fh->pending_mounts;
  fh->pending_mounts = p;

  if (amuDebug(D_TRACE))
    plog(XLOG_DEBUG, "\tlookup(%s, %s)", mp->am_path, name);
  ap = mf->mf_ops->lookup_child(mp, name, &error, VLOOK_CREATE);
  if (ap && error < 0)
    ap = mf->mf_ops->mount_child(ap, &error);

  /* some of the rest can be done in amfs_auto_cont */

  if (ap == 0) {
    if (error < 0) {
      dlog("Mount still pending, not sending autofs reply yet");
      return;
    }
    autofs_lookup_failed(mp, name);
  }
  mp->am_stats.s_lookup++;
}


static void
autofs_handle_expire(am_node *mp, struct autofs_packet_expire *pkt)
{
  autofs_expire_one(mp, pkt->name, 0);
}


static void
autofs_handle_missing(am_node *mp, struct autofs_packet_missing *pkt)
{
  autofs_missing_one(mp, pkt->wait_queue_token, pkt->name);
}


#if AUTOFS_MAX_PROTO_VERSION >= 4
static void
autofs_handle_expire_multi(am_node *mp, struct autofs_packet_expire_multi *pkt)
{
  autofs_expire_one(mp, pkt->name, pkt->wait_queue_token);
}
#endif /* AUTOFS_MAX_PROTO_VERSION >= 4 */


#if AUTOFS_MAX_PROTO_VERSION >= 5
static void
autofs_handle_expire_direct(am_node *mp,
  autofs_packet_expire_direct_t *pkt)
{
  autofs_expire_one(mp, pkt->name, 0);
}

static void
autofs_handle_expire_indirect(am_node *mp,
  autofs_packet_expire_indirect_t *pkt)
{
  autofs_expire_one(mp, pkt->name, 0);
}


static void
autofs_handle_missing_direct(am_node *mp,
  autofs_packet_missing_direct_t *pkt)
{
  autofs_missing_one(mp, pkt->wait_queue_token, pkt->name);
}


static void
autofs_handle_missing_indirect(am_node *mp,
  autofs_packet_missing_indirect_t *pkt)
{
  autofs_missing_one(mp, pkt->wait_queue_token, pkt->name);
}
#endif /* AUTOFS_MAX_PROTO_VERSION >= 5 */


int
autofs_handle_fdset(fd_set *readfds, int nsel)
{
  int i;
  union {
#if AUTOFS_MAX_PROTO_VERSION >= 5
    union autofs_v5_packet_union pkt5;
#endif
    union autofs_packet_union pkt;
  } p;
  autofs_fh_t *fh;
  am_node *mp;
  size_t len;

  for (i = 0; nsel && i < numfds; i++) {
    if (!FD_ISSET(list[i], readfds))
      continue;

    nsel--;
    FD_CLR(list[i], readfds);
    mp = hash[list[i]];
    fh = mp->am_autofs_fh;

#if AUTOFS_MAX_PROTO_VERSION >= 5
    if (fh->version < 5) {
      len = sizeof(p.pkt);
    } else {
      len = sizeof(p.pkt5);
    }
#else
    len = sizeof(p.pkt);
#endif /* AUTOFS_MAX_PROTO_VERSION >= 5 */

    if (autofs_get_pkt(fh->fd, &p, len))
      continue;

    switch (p.pkt.hdr.type) {
    case autofs_ptype_missing:
      autofs_handle_missing(mp, &p.pkt.missing);
      break;
    case autofs_ptype_expire:
      autofs_handle_expire(mp, &p.pkt.expire);
      break;
#if AUTOFS_MAX_PROTO_VERSION >= 4
    case autofs_ptype_expire_multi:
      autofs_handle_expire_multi(mp, &p.pkt.expire_multi);
      break;
#endif /* AUTOFS_MAX_PROTO_VERSION >= 4 */
#if AUTOFS_MAX_PROTO_VERSION >= 5
    case autofs_ptype_expire_indirect:
      autofs_handle_expire_indirect(mp, &p.pkt5.expire_direct);
      break;
    case autofs_ptype_expire_direct:
      autofs_handle_expire_direct(mp, &p.pkt5.expire_direct);
      break;
    case autofs_ptype_missing_indirect:
      autofs_handle_missing_indirect(mp, &p.pkt5.missing_direct);
      break;
    case autofs_ptype_missing_direct:
      autofs_handle_missing_direct(mp, &p.pkt5.missing_direct);
      break;
#endif /* AUTOFS_MAX_PROTO_VERSION >= 5 */
    default:
      plog(XLOG_ERROR, "Unknown autofs packet type %d",
	   p.pkt.hdr.type);
    }
  }
  return nsel;
}


int
create_autofs_service(void)
{
  hash_init();

  /* not the best place, but... */
  if (linux_version_code() < KERNEL_VERSION(2,4,0))
    bind_works = 0;

  return 0;
}


int
destroy_autofs_service(void)
{
  /* Nothing to do */
  return 0;
}


static int
autofs_bind_umount(char *mountpoint)
{
  int err = 1;
#ifdef MNT2_GEN_OPT_BIND
  if (bind_works && gopt.flags & CFM_AUTOFS_USE_LOFS) {
    struct stat buf;

    if ((err = lstat(mountpoint, &buf)))
      return errno;
    if (S_ISLNK(buf.st_mode))
      goto use_symlink;

    plog(XLOG_INFO, "autofs: un-bind-mounting %s", mountpoint);
    err = umount_fs(mountpoint, mnttab_file_name, 1);
    if (err)
      plog(XLOG_INFO, "autofs: unmounting %s failed: %m", mountpoint);
    else
      err = rmdir(mountpoint);
    goto out;
  }
#endif /* MNT2_GEN_OPT_BIND */
 use_symlink:
  plog(XLOG_INFO, "autofs: deleting symlink %s", mountpoint);
  err = unlink(mountpoint);

 out:
  if (err)
    return errno;
  return 0;
}


int
autofs_mount_fs(am_node *mp, mntfs *mf)
{
  char *target, *target2 = NULL;
  int err = 0;

  if (mf->mf_flags & MFF_ON_AUTOFS) {
    if ((err = mkdir(mp->am_path, 0555)))
      return errno;
  }

  /*
   * For sublinks, we could end up here with an already mounted f/s.
   * Don't do anything in that case.
   */
  if (!(mf->mf_flags & MFF_MOUNTED))
    err = mf->mf_ops->mount_fs(mp, mf);

  if (err) {
    if (mf->mf_flags & MFF_ON_AUTOFS)
      rmdir(mp->am_path);
    return err;
  }

  if (mf->mf_flags & MFF_ON_AUTOFS)
    /* Nothing else to do */
    return 0;

  if (mp->am_link)
    target = mp->am_link;
  else
    target = mf->mf_fo->opt_fs;

#ifdef MNT2_GEN_OPT_BIND
  if (bind_works && gopt.flags & CFM_AUTOFS_USE_LOFS) {
    struct stat buf;

    /*
     * HACK ALERT!
     *
     * Since the bind mount mechanism doesn't allow mountpoint crossing,
     * we _must_ use symlinks for the host mount case. Otherwise we end up
     * with a bunch of empty mountpoints...
     */
    if (mf->mf_ops == &amfs_host_ops)
      goto use_symlink;

    if (target[0] != '/')
      target2 = str3cat(NULL, mp->am_parent->am_path, "/", target);
    else
      target2 = xstrdup(target);

    /*
     * We need to stat() the destination, because the bind mount does not
     * follow symlinks and/or allow for non-existent destinations.
     * We fall back to symlinks if there are problems.
     *
     * We also need to temporarily change pgrp, otherwise our stat() won't
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
      pid_t pgrp = getpgrp();
      setpgrp();
      err = stat(target2, &buf);
      if ((err = setpgid(0, pgrp))) {
	plog(XLOG_ERROR, "autofs: cannot restore pgrp: %s", strerror(errno));
	plog(XLOG_ERROR, "autofs: aborting the mount");
	goto out;
      }
      if (err)
	goto use_symlink;
    }
    if ((err = lstat(target2, &buf)))
      goto use_symlink;
    if (S_ISLNK(buf.st_mode))
      goto use_symlink;

    plog(XLOG_INFO, "autofs: bind-mounting %s -> %s", mp->am_path, target2);
    mkdir(mp->am_path, 0555);
    err = mount_lofs(mp->am_path, target2, mf->mf_mopts, 1);
    if (err) {
      rmdir(mp->am_path);
      plog(XLOG_INFO, "autofs: bind-mounting %s -> %s failed", mp->am_path, target2);
      goto use_symlink;
    }
    goto out;
  }
#endif /* MNT2_GEN_OPT_BIND */
 use_symlink:
  plog(XLOG_INFO, "autofs: symlinking %s -> %s", mp->am_path, target);
  err = symlink(target, mp->am_path);

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
  if (!(mf->mf_flags & MFF_ON_AUTOFS)) {
    err = autofs_bind_umount(mp->am_path);
    if (err)
      return err;
  }

  /*
   * Multiple sublinks could reference this f/s.
   * Don't actually unmount it unless we're holding the last reference.
   */
  if (mf->mf_refc == 1) {
    err = mf->mf_ops->umount_fs(mp, mf);
    if (err)
      return err;
    if (mf->mf_flags & MFF_ON_AUTOFS)
      rmdir(mp->am_path);
  }
  return 0;
}


int
autofs_umount_succeeded(am_node *mp)
{
  autofs_fh_t *fh = mp->am_parent->am_autofs_fh;
  struct autofs_pending_umount **pp, *p;

  /* Already gone? */
  if (fh == NULL)
	return 0;

  pp = &fh->pending_umounts;
  while (*pp && !STREQ((*pp)->name, mp->am_name))
    pp = &(*pp)->next;

  /* sanity check */
  if (*pp == NULL)
    return -1;

  p = *pp;
  plog(XLOG_INFO, "autofs: unmounting %s succeeded", mp->am_path);
  send_ready(fh->ioctlfd, p->wait_queue_token);

  XFREE(p->name);
  *pp = p->next;
  XFREE(p);
  return 0;
}


int
autofs_umount_failed(am_node *mp)
{
  autofs_fh_t *fh = mp->am_parent->am_autofs_fh;
  struct autofs_pending_umount **pp, *p;

  pp = &fh->pending_umounts;
  while (*pp && !STREQ((*pp)->name, mp->am_name))
    pp = &(*pp)->next;

  /* sanity check */
  if (*pp == NULL)
    return -1;

  p = *pp;
  plog(XLOG_INFO, "autofs: unmounting %s failed", mp->am_path);
  send_fail(fh->ioctlfd, p->wait_queue_token);

  XFREE(p->name);
  *pp = p->next;
  XFREE(p);
  return 0;
}


void
autofs_mount_succeeded(am_node *mp)
{
  autofs_fh_t *fh = mp->am_parent->am_autofs_fh;
  struct autofs_pending_mount **pp, *p;

  /*
   * don't expire the entries -- the kernel will do it for us.
   *
   * but it won't do autofs filesystems, so we expire them the old
   * fashioned way instead.
   */
  if (!(mp->am_al->al_mnt->mf_flags & MFF_IS_AUTOFS))
    mp->am_flags |= AMF_NOTIMEOUT;

  pp = &fh->pending_mounts;
  while (*pp && !STREQ((*pp)->name, mp->am_name))
    pp = &(*pp)->next;

  /* sanity check */
  if (*pp == NULL)
    return;

  p = *pp;
  plog(XLOG_INFO, "autofs: mounting %s succeeded", mp->am_path);
  send_ready(fh->ioctlfd, p->wait_queue_token);

  XFREE(p->name);
  *pp = p->next;
  XFREE(p);
}


void
autofs_mount_failed(am_node *mp)
{
  autofs_fh_t *fh = mp->am_parent->am_autofs_fh;
  struct autofs_pending_mount **pp, *p;

  pp = &fh->pending_mounts;
  while (*pp && !STREQ((*pp)->name, mp->am_name))
    pp = &(*pp)->next;

  /* sanity check */
  if (*pp == NULL)
    return;

  p = *pp;
  plog(XLOG_INFO, "autofs: mounting %s failed", mp->am_path);
  send_fail(fh->ioctlfd, p->wait_queue_token);

  XFREE(p->name);
  *pp = p->next;
  XFREE(p);
}


void
autofs_get_opts(char *opts, size_t l, autofs_fh_t *fh)
{
  xsnprintf(opts, l, "fd=%d,minproto=%d,maxproto=%d",
	    fh->kernelfd, AUTOFS_MIN_VERSION, AUTOFS_MAX_VERSION);
}


int
autofs_compute_mount_flags(mntent_t *mnt)
{
  return 0;
}


#if AUTOFS_MAX_PROTO_VERSION >= 4
static int autofs_timeout_mp_task(void *arg)
{
  am_node *mp = (am_node *)arg;
  autofs_fh_t *fh = mp->am_autofs_fh;
  int now = 0;

  while (ioctl(fh->ioctlfd, AUTOFS_IOC_EXPIRE_MULTI, &now) == 0);
  return 0;
}
#endif /* AUTOFS_MAX_PROTO_VERSION >= 4 */


void autofs_timeout_mp(am_node *mp)
{
  autofs_fh_t *fh = mp->am_autofs_fh;
  time_t now = clocktime(NULL);

  /* update the ttl */
  mp->am_autofs_ttl = now + gopt.am_timeo_w;

  if (fh->version < 4) {
    struct autofs_packet_expire pkt;
    while (ioctl(fh->ioctlfd, AUTOFS_IOC_EXPIRE, &pkt) == 0)
      autofs_handle_expire(mp, &pkt);
    return;
  }

#if AUTOFS_MAX_PROTO_VERSION >= 4
  run_task(autofs_timeout_mp_task, mp, NULL, NULL);
#endif /* AUTOFS_MAX_PROTO_VERSION >= 4 */
}

#endif /* HAVE_FS_AUTOFS */
