/*
 * ntp_ppsdev.c - PPS-device support
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 * ---------------------------------------------------------------------
 * Helper code to work around (or with) a Linux 'specialty': PPS devices
 * are created via attaching the PPS line discipline to a TTY.  This
 * creates new pps devices, and the PPS API is *not* available through
 * the original TTY fd.
 *
 * Findig the PPS device associated with a TTY is possible but needs
 * quite a bit of file system traversal & lookup in the 'sysfs' tree.
 *
 * The code below does the job for kernel versions 4 & 5, and will
 * probably work for older and newer kernels, too... and in any case, if
 * the device or symlink to the PPS device with the given name exists,
 * it will take precedence anyway.
 * ---------------------------------------------------------------------
 */
#ifdef __linux__
# define _GNU_SOURCE
#endif

#include "config.h"

#include "ntpd.h"

#ifdef REFCLOCK

#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif
#if defined(HAVE_FCNTL_H)
# include <fcntl.h>
#endif

#include <stdlib.h>

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
#if defined(__linux__) && defined(HAVE_OPENAT) && defined(HAVE_FDOPENDIR)
#define WITH_PPSDEV_MATCH
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <linux/tty.h>

typedef int BOOL;
#ifndef TRUE
# define TRUE 1
#endif
#ifndef FALSE
# define FALSE 0
#endif

static const int OModeF = O_CLOEXEC|O_RDONLY|O_NOCTTY;
static const int OModeD = O_CLOEXEC|O_RDONLY|O_DIRECTORY;

/* ------------------------------------------------------------------ */
/* extended directory stream
 */
typedef struct {
	int  dfd;	/* file descriptor for dir for 'openat()' */
	DIR *dir;	/* directory stream for iteration         */
} XDIR;

static void
xdirClose(
	XDIR *pxdir)
{
	if (NULL != pxdir->dir)
		closedir(pxdir->dir); /* closes the internal FD, too! */
	else if (-1 != pxdir->dfd)
		close(pxdir->dfd);    /* otherwise _we_ have to do it */
	pxdir->dfd = -1;
	pxdir->dir = NULL;
}

static BOOL
xdirOpenAt(
	XDIR       *pxdir,
	int         fdo  ,
	const char *path )
{
	/* Officially, the directory stream owns the file discriptor it
	 * received via 'fdopendir()'.  But for the purpose of 'openat()'
	 * it's ok to keep the value around -- even if we should do
	 * _absolutely_nothing_ with it apart from using it as a path
	 * reference!
	 */
	pxdir->dir = NULL;
	if (-1 == (pxdir->dfd = openat(fdo, path, OModeD)))
		goto fail;
	if (NULL == (pxdir->dir = fdopendir(pxdir->dfd)))
		goto fail;
	return TRUE;
	
  fail:
	xdirClose(pxdir);
	return FALSE;
}

/* --------------------------------------------------------------------
 * read content of a file (with a size limit) into a piece of allocated
 * memory and trim any trailing whitespace.
 *
 * The issue here is that several files in the 'sysfs' tree claim a size
 * of 4096 bytes when you 'stat' them -- but reading gives EOF after a
 * few chars.  (I *can* understand why the kernel takes this shortcut.
 * it's just a bit unwieldy...)
 */
static char*
readFileAt(
	int         rfd ,
	const char *path)
{
	struct stat sb;
	char *ret = NULL;
	ssize_t rdlen;
	int dfd;
	
	if (-1 == (dfd = openat(rfd, path, OModeF)) || -1 == fstat(dfd, &sb))
		goto fail;
	if ((sb.st_size > 0x2000) || (NULL == (ret = malloc(sb.st_size + 1))))
		goto fail;
	if (1 > (rdlen = read(dfd, ret, sb.st_size)))
		goto fail;
	close(dfd);

	while (rdlen > 0 && ret[rdlen - 1] <= ' ')
		--rdlen;
	ret[rdlen] = '\0';
	return ret;

  fail:
	free(ret);
	if (-1 != dfd)
		close(dfd);
	return NULL;    
}

/* --------------------------------------------------------------------
 * Scan the "/dev" directory for a device with a given major and minor 
 * device id. Return the path if found.
 */
static char*
findDevByDevId(
	dev_t rdev)
{
	struct stat    sb;
	struct dirent *dent;
	XDIR           xdir;
	char          *name = NULL;
	
	if (!xdirOpenAt(&xdir, AT_FDCWD, "/dev"))
		goto done;
	
	while (!name && (dent = readdir(xdir.dir))) {
		if (-1 == fstatat(xdir.dfd, dent->d_name,
				  &sb, AT_SYMLINK_NOFOLLOW))
			continue;
		if (!S_ISCHR(sb.st_mode))
			continue;
		if (sb.st_rdev == rdev) {
			if (-1 == asprintf(&name, "/dev/%s", dent->d_name))
				name = NULL;
		}
	}
	xdirClose(&xdir);

  done:
	return name;
}

/* --------------------------------------------------------------------
 * Get the mofor:minor device id for a character device file descriptor
 */
static BOOL
getCharDevId(
	int          fd ,
	dev_t       *out,
	struct stat *psb)
{
	BOOL        rc = FALSE;
	struct stat sb;
	
	if (NULL == psb)
		psb = &sb;
	if (-1 != fstat(fd, psb)) {
		rc = S_ISCHR(psb->st_mode);
		if (rc)
			*out = psb->st_rdev;
		else
			errno = EINVAL;
	}
	return rc;
}

/* --------------------------------------------------------------------
 * given the dir-fd of a pps instance dir in the linux sysfs tree, get
 * the device IDs for the PPS device and the associated TTY.
 */
static BOOL
getPpsTuple(
	int   fdDir,
	dev_t *pTty,
	dev_t *pPps)
{
	BOOL          rc = FALSE;
	unsigned long dmaj, dmin;
	struct stat   sb;
	char         *bufp, *endp, *scan;

	/* 'path' contains the primary path to the associated TTY:
	 * we 'stat()' for the device id in 'st_rdev'.
	 */
	if (NULL == (bufp = readFileAt(fdDir, "path")))
		goto done;
	if ((-1 == stat(bufp, &sb)) || !S_ISCHR(sb.st_mode))
		goto done;
	*pTty = sb.st_rdev;
	free(bufp);

	/* 'dev' holds the device ID of the PPS device as 'major:minor'
	 * in text format.   *sigh* couldn't that simply be the name of
	 * the PPS device itself, as in 'path' above??? But nooooo....
	 */
	if (NULL == (bufp = readFileAt(fdDir, "dev")))
		goto done;
	dmaj = strtoul((scan = bufp), &endp, 10);
	if ((endp == scan) || (*endp != ':') || (dmaj >= 256))
		goto done;
	dmin = strtoul((scan = endp + 1), &endp, 10);
	if ((endp == scan) || (*endp >= ' ') || (dmin >= 256))
		goto done;
	*pPps = makedev((unsigned int)dmaj, (unsigned int)dmin);
	rc = TRUE;
	
  done:
	free(bufp);
	return rc;	
}

/* --------------------------------------------------------------------
 * for a given (TTY) device id, lookup the corresponding PPS device id
 * by processing the contents of the kernel sysfs tree.
 * Returns false if no such PS device can be found; otherwise set the
 * ouput parameter to the PPS dev id and return true...
 */
static BOOL
findPpsDevId(
	dev_t  ttyId ,
	dev_t *pPpsId)
{
	BOOL           found = FALSE;
	XDIR           ClassDir;
	struct dirent *dent;
	dev_t          othId, ppsId;
	int            fdDevDir;
	
	if (!xdirOpenAt(&ClassDir, AT_FDCWD, "/sys/class/pps"))
		goto done;
		
	while (!found && (dent = readdir(ClassDir.dir))) {

		/* If the entry is not a referring to a PPS device or
		 * if we can't open the directory for reading, skipt it:
		 */
		if (strncmp("pps", dent->d_name, 3))
			continue;
		fdDevDir = openat(ClassDir.dfd, dent->d_name, OModeD);
		if (-1 == fdDevDir)
			continue;

		/* get the data and check if device ID for the TTY
		 * is what we're looking for:
		 */
		found = getPpsTuple(fdDevDir, &othId, &ppsId)
		    && (ttyId == othId);
		close(fdDevDir);
	}
	
	xdirClose(&ClassDir);
	
	if (found)
		*pPpsId = ppsId;
  done:
	return found;
}

/* --------------------------------------------------------------------
 * Return the path to a PPS device related to tghe TT fd given. The
 * function might even try to instantiate such a PPS device when
 * running es effective root.  Returns NULL if no PPS device can be
 * established; otherwise it is a 'malloc()'ed area that should be
 * 'free()'d after use.
 */
static char*
findMatchingPpsDev(
	int fdtty)
{
	struct stat sb;
	dev_t       ttyId, ppsId;
	int         fdpps, ldisc = N_PPS;
	char       *dpath = NULL;

	/* Without the device identifier of the TTY, we're busted: */
	if (!getCharDevId(fdtty, &ttyId, &sb))
		goto done;

	/* If we find a matching PPS device ID, return the path to the
	 * device. It might not open, but it's the best we can get.
	 */
	if (findPpsDevId(ttyId, &ppsId)) {
		dpath = findDevByDevId(ppsId);
		goto done;
	}
	
#   ifdef ENABLE_MAGICPPS
	/* 'magic' PPS support -- try to instantiate missing PPS devices
	 * on-the-fly.  Our mileage may vary -- running as root at that
	 * moment is vital for success.  (We *can* create the PPS device
	 * as ordnary user, but we won't be able to open it!)
	 */
	
	/* If we're root, try to push the PPS LDISC to the tty FD. If
	 * that does not work out, we're busted again:
	 */
	if ((0 != geteuid()) || (-1 == ioctl(fdtty, TIOCSETD, &ldisc)))
		goto done;
	msyslog(LOG_INFO, "auto-instantiated PPS device for device %u:%u",
		major(ttyId), minor(ttyId));

	/* We really should find a matching PPS device now. And since
	 * we're root (see above!), we should be able to open that device.
	 */
	if (findPpsDevId(ttyId, &ppsId))
		dpath = findDevByDevId(ppsId);
	if (!dpath)
		goto done;

	/* And since we're 'root', we might as well try to clone the
	 * ownership and access rights from the original TTY to the
	 * PPS device.  If that does not work, we just have to live with
	 * what we've got so far...
	 */
	if (-1 == (fdpps = open(dpath, OModeF))) {
		msyslog(LOG_ERR, "could not open auto-created '%s': %m", dpath);
		goto done;
	}
	if (-1 == fchmod(fdpps, sb.st_mode)) {
		msyslog(LOG_ERR, "could not chmod auto-created '%s': %m", dpath);
	}
	if (-1 == fchown(fdpps, sb.st_uid, sb.st_gid)) {
		msyslog(LOG_ERR, "could not chown auto-created '%s': %m", dpath);
	}
	close(fdpps);
#   else
	(void)ldisc;
#   endif
	
  done:
	/* Whatever we go so far, that's it. */
	return dpath;
}

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
#endif /* linux PPS device matcher */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#include "ntp_clockdev.h"

int
ppsdev_reopen(
	const sockaddr_u *srcadr,
	int         ttyfd  , /* current tty FD, or -1 */
	int         ppsfd  , /* current pps FD, or -1 */
	const char *ppspath, /* path to pps device, or NULL */
	int         omode  , /* open mode for pps device */
	int         oflags ) /* openn flags for pps device */
{
	int retfd = -1;
	const char *altpath;

	/* avoid 'unused' warnings: we might not use all args, no
	 * thanks to conditional compiling:)
	 */
	(void)ppspath;
	(void)omode;
	(void)oflags;

	if (NULL != (altpath = clockdev_lookup(srcadr, 1)))
		ppspath = altpath;

#   if defined(__unix__) && !defined(_WIN32)
	if (-1 == retfd) {	
		if (ppspath && *ppspath) {
			retfd = open(ppspath, omode, oflags);
			msyslog(LOG_INFO, "ppsdev_open(%s) %s",
				ppspath, (retfd != -1 ? "succeeded" : "failed"));
		}
	}
#   endif
	
#   if defined(WITH_PPSDEV_MATCH)
	if ((-1 == retfd) && (-1 != ttyfd)) {	
		char *xpath = findMatchingPpsDev(ttyfd);
		if (xpath && *xpath) {
			retfd = open(xpath, omode, oflags);
			msyslog(LOG_INFO, "ppsdev_open(%s) %s",
				xpath, (retfd != -1 ? "succeeded" : "failed"));
		}
		free(xpath);
	}
#   endif
	
	/* BSDs and probably SOLARIS can use the TTY fd for the PPS API,
	 * and so does Windows where the PPS API is implemented via an
	 * IOCTL.  Likewise does the 'SoftPPS' implementation in Windows
	 * based on COM Events.  So, if everything else fails, simply
	 * try the FD given for the TTY/COMport...
	 */
	if (-1 == retfd)
		retfd = ppsfd;
	if (-1 == retfd)
		retfd = ttyfd;

	/* Close the old pps FD, but only if the new pps FD is neither
	 * the tty FD nor the existing pps FD!
	 */
	if ((retfd != ttyfd) && (retfd != ppsfd))
		ppsdev_close(ttyfd, ppsfd);
	
	return retfd;
}

void
ppsdev_close(
	int ttyfd, /* current tty FD, or -1 */
	int ppsfd) /* current pps FD, or -1 */
{
	/* The pps fd might be the same as the tty fd.  We close the pps
	 * channel only if it's valid and _NOT_ the tty itself:
	 */
	if ((-1 != ppsfd) && (ttyfd != ppsfd))
		close(ppsfd);
}
/* --*-- that's all folks --*-- */
#else
NONEMPTY_TRANSLATION_UNIT
#endif /* !defined(REFCLOCK) */
