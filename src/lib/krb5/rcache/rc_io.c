/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc_io.c */
/*
 * This file of the Kerberos V5 software is derived from public-domain code
 * contributed by Daniel J. Bernstein, <brnstnd@acf10.nyu.edu>.
 *
 */

/*
 * I/O functions for the replay cache default implementation.
 */

#if defined(_WIN32)
#  define PATH_SEPARATOR "\\"
#else
#  define PATH_SEPARATOR "/"
#endif

#define KRB5_RC_VNO     0x0501          /* krb5, rcache v 1 */

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "k5-int.h"
#include <stdio.h> /* for P_tmpdir */
#include "rc_base.h"
#include "rc_dfl.h"
#include "rc_io.h"

#ifndef O_BINARY
#define O_BINARY    0
#endif

#ifdef HAVE_NETINET_IN_H
#if !defined(_WINSOCKAPI_)
#include <netinet/in.h>
#endif
#else
#error find some way to use net-byte-order file version numbers.
#endif

#define UNIQUE getpid() /* hopefully unique number */

#define GETDIR (dir = getdir(), dirlen = strlen(dir) + sizeof(PATH_SEPARATOR) - 1)

static char *
getdir(void)
{
    char *dir;

    if (!(dir = getenv("KRB5RCACHEDIR"))) {
#if defined(_WIN32)
        if (!(dir = getenv("TEMP")))
            if (!(dir = getenv("TMP")))
                dir = "C:";
#else
        if (!(dir = getenv("TMPDIR"))) {
#ifdef RCTMPDIR
            dir = RCTMPDIR;
#else
            dir = "/tmp";
#endif
        }
#endif
    }
    return dir;
}

/*
 * Called from krb5_rc_io_creat(); calls mkstemp() and does some
 * sanity checking on the file modes in case some broken mkstemp()
 * implementation creates the file with overly permissive modes.  To
 * avoid race conditions, do not fchmod() a file for which mkstemp set
 * incorrect modes.
 */
static krb5_error_code
krb5_rc_io_mkstemp(krb5_context context, krb5_rc_iostuff *d, char *dir)
{
    krb5_error_code retval = 0;
#if HAVE_SYS_STAT_H
    struct stat stbuf;

    memset(&stbuf, 0, sizeof(stbuf));
#endif
    if (asprintf(&d->fn, "%s%skrb5_RCXXXXXX",
                 dir, PATH_SEPARATOR) < 0) {
        d->fn = NULL;
        return KRB5_RC_IO_MALLOC;
    }
    d->fd = mkstemp(d->fn);
    if (d->fd == -1) {
        /*
         * This return value is deliberate because d->fd == -1 causes
         * caller to go into errno interpretation code.
         */
        return 0;
    }
#if HAVE_SYS_STAT_H
    /*
     * Be paranoid and check that mkstemp made the file accessible
     * only to the user.
     */
    retval = fstat(d->fd, &stbuf);
    if (retval) {
        k5_setmsg(context, retval,
                  _("Cannot fstat replay cache file %s: %s"),
                  d->fn, strerror(errno));
        return KRB5_RC_IO_UNKNOWN;
    }
    if (stbuf.st_mode & 077) {
        k5_setmsg(context, retval,
                  _("Insecure mkstemp() file mode for replay cache file %s; "
                    "try running this program with umask 077"), d->fn);
        return KRB5_RC_IO_UNKNOWN;
    }
#endif
    return 0;
}

#if 0
static krb5_error_code rc_map_errno (int) __attribute__((cold));
#endif

static krb5_error_code
rc_map_errno (krb5_context context, int e, const char *fn,
              const char *operation)
{
    switch (e) {
    case EFBIG:
#ifdef EDQUOT
    case EDQUOT:
#endif
    case ENOSPC:
        return KRB5_RC_IO_SPACE;

    case EIO:
        return KRB5_RC_IO_IO;

    case EPERM:
    case EACCES:
    case EROFS:
    case EEXIST:
        k5_setmsg(context, KRB5_RC_IO_PERM,
                  _("Cannot %s replay cache file %s: %s"),
                  operation, fn, strerror(e));
        return KRB5_RC_IO_PERM;

    default:
        k5_setmsg(context, KRB5_RC_IO_UNKNOWN, _("Cannot %s replay cache: %s"),
                  operation, strerror(e));
        return KRB5_RC_IO_UNKNOWN;
    }
}


krb5_error_code
krb5_rc_io_creat(krb5_context context, krb5_rc_iostuff *d, char **fn)
{
    krb5_int16 rc_vno = htons(KRB5_RC_VNO);
    krb5_error_code retval = 0;
    int flags, do_not_unlink = 0;
    char *dir;
    size_t dirlen;

    GETDIR;
    if (fn && *fn) {
        if (asprintf(&d->fn, "%s%s%s", dir, PATH_SEPARATOR, *fn) < 0)
            return KRB5_RC_IO_MALLOC;
        d->fd = -1;
        do {
            if (unlink(d->fn) == -1 && errno != ENOENT)
                break;
            flags = O_WRONLY | O_CREAT | O_TRUNC | O_EXCL | O_BINARY;
            d->fd = THREEPARAMOPEN(d->fn, flags, 0600);
        } while (d->fd == -1 && errno == EEXIST);
    } else {
        retval = krb5_rc_io_mkstemp(context, d, dir);
        if (retval)
            goto cleanup;
        if (d->fd != -1 && fn) {
            *fn = strdup(d->fn + dirlen);
            if (*fn == NULL) {
                free(d->fn);
                return KRB5_RC_IO_MALLOC;
            }
        }
    }
    if (d->fd == -1) {
        retval = rc_map_errno(context, errno, d->fn, "create");
        if (retval == KRB5_RC_IO_PERM)
            do_not_unlink = 1;
        goto cleanup;
    }
    set_cloexec_fd(d->fd);
    retval = krb5_rc_io_write(context, d, (krb5_pointer)&rc_vno,
                              sizeof(rc_vno));
    if (retval)
        goto cleanup;

    retval = krb5_rc_io_sync(context, d);

cleanup:
    if (retval) {
        if (d->fn) {
            if (!do_not_unlink)
                (void) unlink(d->fn);
            free(d->fn);
            d->fn = NULL;
        }
        if (d->fd != -1) {
            (void) close(d->fd);
        }
    }
    return retval;
}

static krb5_error_code
krb5_rc_io_open_internal(krb5_context context, krb5_rc_iostuff *d, char *fn,
                         char* full_pathname)
{
    krb5_int16 rc_vno;
    krb5_error_code retval = 0;
    int do_not_unlink = 1;
#ifndef NO_USERID
    struct stat sb1, sb2;
#endif
    char *dir;

    dir = getdir();
    if (full_pathname) {
        if (!(d->fn = strdup(full_pathname)))
            return KRB5_RC_IO_MALLOC;
    } else {
        if (asprintf(&d->fn, "%s%s%s", dir, PATH_SEPARATOR, fn) < 0)
            return KRB5_RC_IO_MALLOC;
    }

#ifdef NO_USERID
    d->fd = THREEPARAMOPEN(d->fn, O_RDWR | O_BINARY, 0600);
    if (d->fd == -1) {
        retval = rc_map_errno(context, errno, d->fn, "open");
        goto cleanup;
    }
#else
    d->fd = -1;
    retval = lstat(d->fn, &sb1);
    if (retval != 0) {
        retval = rc_map_errno(context, errno, d->fn, "lstat");
        goto cleanup;
    }
    d->fd = THREEPARAMOPEN(d->fn, O_RDWR | O_BINARY, 0600);
    if (d->fd < 0) {
        retval = rc_map_errno(context, errno, d->fn, "open");
        goto cleanup;
    }
    retval = fstat(d->fd, &sb2);
    if (retval < 0) {
        retval = rc_map_errno(context, errno, d->fn, "fstat");
        goto cleanup;
    }
    /* check if someone was playing with symlinks */
    if ((sb1.st_dev != sb2.st_dev || sb1.st_ino != sb2.st_ino)
        || (sb1.st_mode & S_IFMT) != S_IFREG)
    {
        retval = KRB5_RC_IO_PERM;
        k5_setmsg(context, retval, "rcache not a file %s", d->fn);
        goto cleanup;
    }
    /* check that non other can read/write/execute the file */
    if (sb1.st_mode & 077) {
        k5_setmsg(context, retval,
                  _("Insecure file mode for replay cache file %s"), d->fn);
        return KRB5_RC_IO_UNKNOWN;
    }
    /* owned by me */
    if (sb1.st_uid != geteuid()) {
        retval = KRB5_RC_IO_PERM;
        k5_setmsg(context, retval, _("rcache not owned by %d"),
                  (int)geteuid());
        goto cleanup;
    }
#endif
    set_cloexec_fd(d->fd);

    do_not_unlink = 0;
    retval = krb5_rc_io_read(context, d, (krb5_pointer) &rc_vno,
                             sizeof(rc_vno));
    if (retval)
        goto cleanup;

    if (ntohs(rc_vno) != KRB5_RC_VNO)
        retval = KRB5_RCACHE_BADVNO;

cleanup:
    if (retval) {
        if (!do_not_unlink)
            (void) unlink(d->fn);
        free(d->fn);
        d->fn = NULL;
        if (d->fd >= 0)
            (void) close(d->fd);
    }
    return retval;
}

krb5_error_code
krb5_rc_io_open(krb5_context context, krb5_rc_iostuff *d, char *fn)
{
    return krb5_rc_io_open_internal(context, d, fn, NULL);
}

krb5_error_code
krb5_rc_io_move(krb5_context context, krb5_rc_iostuff *new1,
                krb5_rc_iostuff *old)
{
#if defined(_WIN32) || defined(__CYGWIN__)
    char *new_fn = NULL;
    char *old_fn = NULL;
    off_t offset = 0;
    krb5_error_code retval = 0;
    /*
     * Initial work around provided by Tom Sanfilippo to work around
     * poor Windows emulation of POSIX functions.  Rename and dup has
     * different semantics!
     *
     * Additional fixes and explanation provided by dalmeida@mit.edu:
     *
     * First, we save the offset of "old".  Then, we close and remove
     * the "new" file so we can do the rename.  We also close "old" to
     * make sure the rename succeeds (though that might not be
     * necessary on some systems).
     *
     * Next, we do the rename.  If all goes well, we seek the "new"
     * file to the position "old" was at.
     *
     * --- WARNING!!! ---
     *
     * Since "old" is now gone, we mourn its disappearance, but we
     * cannot emulate that Unix behavior...  THIS BEHAVIOR IS
     * DIFFERENT FROM UNIX.  However, it is ok because this function
     * gets called such that "old" gets closed right afterwards.
     */
    offset = lseek(old->fd, 0, SEEK_CUR);

    new_fn = new1->fn;
    new1->fn = NULL;
    close(new1->fd);
    new1->fd = -1;

    unlink(new_fn);

    old_fn = old->fn;
    old->fn = NULL;
    close(old->fd);
    old->fd = -1;

    if (rename(old_fn, new_fn) == -1) { /* MUST be atomic! */
        retval = KRB5_RC_IO_UNKNOWN;
        goto cleanup;
    }

    retval = krb5_rc_io_open_internal(context, new1, 0, new_fn);
    if (retval)
        goto cleanup;

    if (lseek(new1->fd, offset, SEEK_SET) == -1) {
        retval = KRB5_RC_IO_UNKNOWN;
        goto cleanup;
    }

cleanup:
    free(new_fn);
    free(old_fn);
    return retval;
#else
    char *fn = NULL;
    if (rename(old->fn, new1->fn) == -1) /* MUST be atomic! */
        return KRB5_RC_IO_UNKNOWN;
    fn = new1->fn;
    new1->fn = NULL;            /* avoid clobbering */
    (void) krb5_rc_io_close(context, new1);
    new1->fn = fn;
    new1->fd = dup(old->fd);
    set_cloexec_fd(new1->fd);
    return 0;
#endif
}

krb5_error_code
krb5_rc_io_write(krb5_context context, krb5_rc_iostuff *d, krb5_pointer buf,
                 unsigned int num)
{
    if (write(d->fd, (char *) buf, num) == -1)
        switch(errno)
        {
#ifdef EDQUOT
        case EDQUOT:
#endif
        case EFBIG:
        case ENOSPC:
            k5_setmsg(context, KRB5_RC_IO_SPACE,
                      _("Can't write to replay cache: %s"), strerror(errno));
            return KRB5_RC_IO_SPACE;
        case EIO:
            k5_setmsg(context, KRB5_RC_IO_IO,
                      _("Can't write to replay cache: %s"), strerror(errno));
            return KRB5_RC_IO_IO;
        case EBADF:
        default:
            k5_setmsg(context, KRB5_RC_IO_UNKNOWN,
                      _("Can't write to replay cache: %s"), strerror(errno));
            return KRB5_RC_IO_UNKNOWN;
        }
    return 0;
}

krb5_error_code
krb5_rc_io_sync(krb5_context context, krb5_rc_iostuff *d)
{
#if defined(_WIN32)
#ifndef fsync
#define fsync _commit
#endif
#endif
    if (fsync(d->fd) == -1) {
        switch(errno)
        {
        case EBADF: return KRB5_RC_IO_UNKNOWN;
        case EIO: return KRB5_RC_IO_IO;
        default:
            k5_setmsg(context, KRB5_RC_IO_UNKNOWN,
                      _("Cannot sync replay cache file: %s"), strerror(errno));
            return KRB5_RC_IO_UNKNOWN;
        }
    }
    return 0;
}

krb5_error_code
krb5_rc_io_read(krb5_context context, krb5_rc_iostuff *d, krb5_pointer buf,
                unsigned int num)
{
    int count;
    if ((count = read(d->fd, (char *) buf, num)) == -1)
        switch(errno)
        {
        case EIO: return KRB5_RC_IO_IO;
        case EBADF:
        default:
            k5_setmsg(context, KRB5_RC_IO_UNKNOWN,
                      _("Can't read from replay cache: %s"), strerror(errno));
            return KRB5_RC_IO_UNKNOWN;
        }
    if (count < 0 || (unsigned int)count != num)
        return KRB5_RC_IO_EOF;
    return 0;
}

krb5_error_code
krb5_rc_io_close(krb5_context context, krb5_rc_iostuff *d)
{
    if (d->fn != NULL) {
        free(d->fn);
        d->fn = NULL;
    }
    if (d->fd != -1) {
        if (close(d->fd) == -1) /* can't happen */
            return KRB5_RC_IO_UNKNOWN;
        d->fd = -1;
    }
    return 0;
}

krb5_error_code
krb5_rc_io_destroy(krb5_context context, krb5_rc_iostuff *d)
{
    if (unlink(d->fn) == -1)
        switch(errno)
        {
        case EIO:
            k5_setmsg(context, KRB5_RC_IO_IO,
                      _("Can't destroy replay cache: %s"), strerror(errno));
            return KRB5_RC_IO_IO;
        case EPERM:
        case EBUSY:
        case EROFS:
            k5_setmsg(context, KRB5_RC_IO_PERM,
                      _("Can't destroy replay cache: %s"), strerror(errno));
            return KRB5_RC_IO_PERM;
        case EBADF:
        default:
            k5_setmsg(context, KRB5_RC_IO_UNKNOWN,
                      _("Can't destroy replay cache: %s"), strerror(errno));
            return KRB5_RC_IO_UNKNOWN;
        }
    return 0;
}

krb5_error_code
krb5_rc_io_mark(krb5_context context, krb5_rc_iostuff *d)
{
    d->mark = lseek(d->fd, (off_t) 0, SEEK_CUR); /* can't fail */
    return 0;
}

krb5_error_code
krb5_rc_io_unmark(krb5_context context, krb5_rc_iostuff *d)
{
    (void) lseek(d->fd, d->mark, SEEK_SET); /* if it fails, tough luck */
    return 0;
}

long
krb5_rc_io_size(krb5_context context, krb5_rc_iostuff *d)
{
    struct stat statb;

    if (fstat(d->fd, &statb) == 0)
        return statb.st_size;
    else
        return 0;
}
