/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *    must display the following acknowledgement:
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI cwd.c,v 2.2 1996/04/08 19:32:25 bostic Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "doscmd.h"
#include "cwd.h"

/* Local functions */
static inline int	isvalid(unsigned);
static inline int	isdot(unsigned);
static inline int	isslash(unsigned);
static void		to_dos_fcb(u_char *, u_char *);

#define	D_REDIR         0x0080000	/* XXX - ack */
#define	D_TRAPS3	0x0200000

typedef struct {
    u_char		*path;
    u_char		*cwd;
    int			len;
    int			maxlen;
    int			read_only:1;
} Path_t;

typedef struct Name_t {
    u_char		*real;
    struct Name_t	*next;
    u_char		name[9];
    u_char		ext[4];
} Name_t;


#define	MAX_DRIVE	26

static Path_t paths[MAX_DRIVE];
static Name_t *names;

/*
 * Initialize the drive to be based at 'base' in the BSD filesystem
 */
void
init_path(int drive, const u_char *base, const u_char *dir)
{
    Path_t *d;

    if (drive < 0 || drive >= MAX_DRIVE)
	return;

    debug(D_TRAPS3, "init_path(%d, %s, %s)\n", drive, base, dir);

    d = &paths[drive];

    if (d->path)
	free(d->path);

    if ((d->path = ustrdup(base)) == NULL)
	fatal("strdup in init_path for %c:%s: %s", drntol(drive), base,
	      strerror(errno));

    if (d->maxlen < 2) {
	d->maxlen = 128;
	if ((d->cwd = (u_char *)malloc(d->maxlen)) == NULL)
	    fatal("malloc in init_path for %c:%s: %s", drntol(drive), base,
		  strerror(errno));
    }

    d->cwd[0] = '\\';
    d->cwd[1] = 0;
    d->len = 1;
    if (dir) {
	if (ustrncmp(base, dir, ustrlen(base)) == 0)
		dir += ustrlen(base);
	while (*dir == '/')
	    ++dir;

    	while (*dir) {
	    u_char dosname[15];
	    u_char realname[256];
	    u_char *r = realname;;

	    while ((*r = *dir) && *dir++ != '/') {
		++r;
	    }
    	    *r = 0;
	    while (*dir == '/')
		++dir;

	    dosname[0] = drntol(drive);
	    dosname[1] = ':';
	    real_to_dos(realname, &dosname[2]);

	    if (dos_setcwd(dosname)) {
		fprintf(stderr, "Failed to CD to directory %s in %s\n",
				 dosname, d->cwd);
	    }
	}
    }
}

/*
 * Mark this drive as read only
 */
void
dos_makereadonly(int drive)
{

    if (drive < 0 || drive >= MAX_DRIVE)
	return;
    paths[drive].read_only = 1;
}

/*
 * Return read-only status of drive
 */
int
dos_readonly(int drive)
{

    if (drive < 0 || drive >= MAX_DRIVE)
	return (0);
    debug(D_REDIR, "dos_readonly(%d) -> %d\n", drive, paths[drive].read_only);
    return (paths[drive].read_only);
}

/*
 * Return DOS's idea of the CWD for drive
 * Return 0 if the drive specified is not mapped (or bad)
 */
u_char *
dos_getcwd(int drive)
{

    if (drive < 0 || drive >= MAX_DRIVE)
	return (0);
    debug(D_REDIR, "dos_getcwd(%d) -> %s\n", drive, paths[drive].cwd);
    return (paths[drive].cwd);
}

/*
 * Return DOS's idea of the CWD for drive
 * Return 0 if the drive specified is not mapped (or bad)
 */
u_char *
dos_getpath(int drive)
{

    if (drive < 0 || drive >= MAX_DRIVE)
	return (0);
    debug(D_REDIR, "dos_getpath(%d) -> %s\n", drive, paths[drive].path);
    return (paths[drive].path);
}

/*
 * Fix up a DOS path name.  Strip out all '.' and '..' entries, turn
 * '/' into '\\' and convert all lowercase to uppercase.
 * Returns 0 on success or DOS errno
 */
int
dos_makepath(u_char *where, u_char *newpath)
{
    int drive;
    u_char **dirs;
    u_char *np;
    Path_t *d;
    u_char tmppath[1024];
    u_char *snewpath = newpath;

    if (where[0] != '\0' && where[1] == ':') {
	drive = drlton(*where);
	*newpath++ = *where++;
	*newpath++ = *where++;
    } else {
	drive = diskdrive;
	*newpath++ = drntol(diskdrive);
	*newpath++ = ':';
    }

    if (drive < 0 || drive >= MAX_DRIVE) {
	debug(D_REDIR,"drive %c invalid\n", drntol(drive));
	return (DISK_DRIVE_INVALID);
    }

    d = &paths[drive];
    if (d->cwd == NULL) {
	debug(D_REDIR,"no cwd for drive %c\n",drntol(drive));
	return (DISK_DRIVE_INVALID);
    }

    debug(D_REDIR, "dos_makepath(%d, %s)\n", drive, where);

    np = newpath;
    if (*where != '\\' && *where != '/') {
	ustrncpy(tmppath, d->cwd, 1024);
	if (d->cwd[1])
	    ustrncat(tmppath, "/", 1024 - ustrlen(tmppath));
	ustrncat(tmppath, where, 1024 - ustrlen(tmppath));
    } else {
	ustrncpy(tmppath, where, 1024 - ustrlen(tmppath));
    }

    dirs = get_entries(tmppath);
    if (dirs == NULL)
	return (PATH_NOT_FOUND);

    np = newpath;
    while (*dirs) {
	u_char *dir = *dirs++;
	if (*dir == '/' || *dir == '\\') {
	    np = newpath + 1;
	    newpath[0] = '\\';
	} else if (dir[0] == '.' && dir[1] == 0) {
	    ;
	} else if (dir[0] == '.' && dir[1] == '.' && dir[2] == '\0') {
	    while (np[-1] != '/' && np[-1] != '\\')
		--np;
    	    if (np - 1 > newpath)
		--np;
	} else {
    	    if (np[-1] != '\\')
		*np++ = '\\';
	    while ((*np = *dir++) && np - snewpath < 1023)
		++np;
    	}
    }
    *np = 0;

    return (0);
}

/*
 * Set DOS's idea of the CWD for drive to be where.
 * Returns DOS errno on failuer.
 */
int
dos_setcwd(u_char *where)
{
    u_char new_path[1024];
    u_char real_path[1024];
    int drive;
    struct stat sb;
    Path_t *d;
    int error;

    debug(D_REDIR, "dos_setcwd(%s)\n", where);

    error = dos_makepath(where, new_path);
    if (error)
	return (error);

    error = dos_to_real_path(new_path, real_path, &drive);
    if (error)
	return (error);
    
    if (ustat(real_path, &sb) < 0 || !S_ISDIR(sb.st_mode))
	return (PATH_NOT_FOUND);
    if (uaccess(real_path, R_OK | X_OK))
	return (PATH_NOT_FOUND);
    
    d = &paths[drive];
    d->len = ustrlen(new_path + 2);

    if (d->len + 1 > d->maxlen) {
	free(d->cwd);
	d->maxlen = d->len + 1 + 32;
	d->cwd = (u_char *)malloc(d->maxlen);
	if (d->cwd == NULL)
	    fatal("malloc in dos_setcwd for %c:%s: %s", drntol(drive),
		  new_path, strerror(errno));
    }
    ustrncpy(d->cwd, new_path + 2, d->maxlen - d->len);
    return (0);
}

/*
 * Given a DOS path dos_path and a drive, convert it to a BSD pathname
 * and store the result in real_path.
 * Return DOS errno on failure.
 */
int
dos_to_real_path(u_char *dos_path, u_char *real_path, int *drivep)
{
    Path_t *d;
    u_char new_path[1024];
    u_char *rp;
    u_char **dirs;
    u_char *dir;
    int drive;

    debug(D_REDIR, "dos_to_real_path(%s)\n", dos_path);

    if (dos_path[0] != '\0' && dos_path[1] == ':') {
	drive = drlton(*dos_path);
	dos_path++;
	dos_path++;
    } else {
	drive = diskdrive;
    }

    d = &paths[drive];
    if (d->cwd == NULL)
	return (DISK_DRIVE_INVALID);

    ustrcpy(real_path, d->path);

    rp = real_path;
    while (*rp)
	++rp;

    ustrncpy(new_path, dos_path, 1024 - ustrlen(new_path));

    dirs = get_entries(new_path);
    if (dirs == NULL)
	return (PATH_NOT_FOUND);

    /*
     * Skip the leading /
     * There are no . or .. entries to worry about either
     */

    while ((dir = *++dirs) != 0) {
	*rp++ = '/';
	dos_to_real(dir, rp);
	while (*rp)
	    ++rp;
    }

    *drivep = drive;
    return (0);
}

/*
 * Provide a few istype() style functions.
 * isvalid:	True if the character is a valid DOS filename character
 * isdot:	True if '.'
 * isslash:	True if '/' or '\'
 *
 * 0 - invalid
 * 1 - okay
 * 2 - *
 * 3 - dot
 * 4 - slash
 * 5 - colon
 * 6 - ?
 * 7 - lowercase
 */
u_char cattr[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,	/* 0x00 */
    0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,	/* 0x10 */
    0, 1, 0, 1, 1, 1, 1, 1,  1, 1, 2, 0, 0, 1, 3, 4,	/* 0x20 */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 5, 0, 0, 0, 0, 6,	/* 0x30 */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,	/* 0x40 */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 0, 4, 0, 1, 1,	/* 0x50 */
    1, 7, 7, 7, 7, 7, 7, 7,  7, 7, 7, 7, 7, 7, 7, 7,	/* 0x60 */
    7, 7, 7, 7, 7, 7, 7, 7,  7, 7, 7, 1, 0, 1, 1, 0,	/* 0x70 */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,	/* 0x80 */
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
};

static inline int
isvalid(unsigned c)
{
    return (cattr[c & 0xff] == 1);
}

static inline int
isdot(unsigned c)
{
    return (cattr[c & 0xff] == 3);
}

static inline int
isslash(unsigned c)
{
    return (cattr[c & 0xff] == 4);
}

/*
 * Given a real component, compute the DOS component.
 */
void
real_to_dos(u_char *real, u_char *dos)
{
    Name_t *n;
    Name_t *nn;
    u_char *p;
    u_char nm[9], ex[4];
    int ncnt, ecnt;
    int echar = '0';
    int nchar = '0';

    if (real[0] == '.' && (real[1] == '\0'
			   || (real[1] == '.' && real[2] == '\0'))) {
	sprintf((char *)dos, "%.8s", real);
	return;
    }

    n = names;
    while (n) {
	if (ustrcmp(real, n->real) == 0) {
	    if (n->ext[0])
		sprintf((char *)dos, "%.8s.%.3s", n->name, n->ext);
	    else
		sprintf((char *)dos, "%.8s", n->name);
	    return;
	}
	n = n->next;
    }

    p = real;
    ncnt = ecnt = 0;
    while (isvalid(*p) && ncnt < 8) {
	nm[ncnt] = *p;
    	++ncnt;
	++p;
    }
    if (isdot(*p)) {
	++p;
	while (isvalid(*p) && ecnt < 3) {
	    ex[ecnt] = *p;
	    ++ecnt;
	    ++p;
	}
    }
    nm[ncnt] = '\0';
    ex[ecnt] = '\0';

    if (!*p && ncnt <= 8 && ecnt <= 3) {
	n = names;
	while (n) {
	    if (ustrncmp(n->name, nm, 8) == 0 && ustrncmp(n->ext, ex, 3) == 0) {
		break;
	    }
	    n = n->next;
	}
	if (n == 0) {
	    ustrcpy(dos, real);
	    return;
	}
    }

    n = (Name_t *)malloc(sizeof(Name_t));

    if (!n)
	fatal("malloc in real_to_dos: %s\n", strerror(errno));

    n->real = ustrdup(real);

    if (!n->real)
	fatal("strdup in real_to_dos: %s\n", strerror(errno));

    p = real;
    ncnt = ecnt = 0;
    while (*p && ncnt < 8) {
	if (isvalid(*p))
	    n->name[ncnt] = *p;
	else if (islower(*p))
	    n->name[ncnt] = toupper(*p);
	else if (isdot(*p))
	    break;
	else
	    n->name[ncnt] = (*p |= 0x80);
	++ncnt;
	++p;
    }
    if (isdot(*p)) {
	++p;
    	while (*p && ecnt < 3) { 
	    if (isvalid(*p))
	    	n->ext[ecnt] = *p;
    	    else if (islower(*p))
	    	n->ext[ecnt] = toupper(*p);
#if 0
	    else if (isdot(*p))
		ERROR
#endif
    	    else
		n->ext[ecnt] = (*p |= 0x80);
	    ++ecnt;
	    ++p;
    	}
    }
    n->name[ncnt] = '\0';
    n->ext[ecnt] = '\0';

    for (;;) {
	nn = names;
	while (nn) {
	    if (ustrncmp(n->name, nn->name, 8) == 0 &&
		ustrncmp(n->ext, nn->ext, 3) == 0) {
		    break;
	    }
	    nn = nn->next;
	}
    	if (!nn)
	    break;
	/*	
	 * Dang, this name was already in the cache.
	 * Let's munge it a little and try again.
	 */
	if (ecnt < 3) {
	    n->ext[ecnt] = echar;
	    if (echar == '9') {
		echar = 'A';
	    } else if (echar == 'Z') {
		++ecnt;
		echar = '0';
	    } else {
		++echar;
	    }
    	} else if (ncnt < 8) {
	    n->name[ncnt] = nchar;
	    if (nchar == '9') {
		nchar = 'A';
	    } else if (nchar == 'Z') {
		++ncnt;
		nchar = '0';
	    } else {
		++nchar;
	    }
    	} else if (n->ext[2] < 'Z')
	    n->ext[2]++;
    	else if (n->ext[1] < 'Z')
	    n->ext[1]++;
    	else if (n->ext[0] < 'Z')
	    n->ext[0]++;
    	else if (n->name[7] < 'Z')
	    n->name[7]++;
    	else if (n->name[6] < 'Z')
	    n->name[6]++;
    	else if (n->name[5] < 'Z')
	    n->name[5]++;
    	else if (n->name[4] < 'Z')
	    n->name[4]++;
    	else if (n->name[3] < 'Z')
	    n->name[3]++;
    	else if (n->name[2] < 'Z')
	    n->name[2]++;
    	else if (n->name[1] < 'Z')
	    n->name[1]++;
    	else if (n->name[0] < 'Z')
	    n->name[0]++;
	else
	    break;
    }

    if (n->ext[0])
	sprintf((char *)dos, "%.8s.%.3s", n->name, n->ext);
    else
	sprintf((char *)dos, "%.8s", n->name);
    n->next = names;
    names = n;
}


/*
 * Given a DOS component, compute the REAL component.
 */
void
dos_to_real(u_char *dos, u_char *real)
{
    int ncnt = 0;
    int ecnt = 0;
    u_char name[8];
    u_char ext[3];
    Name_t *n = names;

    while (ncnt < 8 && (isvalid(*dos) || islower(*dos))) {
	name[ncnt++] = islower(*dos) ? toupper(*dos) : *dos;
	++dos;
    }
    if (ncnt < 8)
	name[ncnt] = 0;

    if (isdot(*dos)) {
	while (ecnt < 3 && (isvalid(*++dos) || islower(*dos))) {
	    ext[ecnt++] = islower(*dos) ? toupper(*dos) : *dos;
    	}
    }
    if (ecnt < 3)
	ext[ecnt] = 0;

    while (n) {
	if (!ustrncmp(name, n->name, 8) && !ustrncmp(ext, n->ext, 3)) {
	    ustrcpy(real, n->real);
	    return;
	}
	n = n->next;
    }

    if (ext[0])
	sprintf((char *)real, "%-.8s.%-.3s", name, ext);
    else
	sprintf((char *)real, "%-.8s", name);

    while (*real) {
	if (isupper(*real))
	    *real = tolower(*real);
    	++real;
    }
}

/*
 * convert a path into an argv[] like vector of components.
 * If the path starts with a '/' or '\' then the first entry
 * will be "/" or "\".  This is the only case in which a "/"
 * or "\" may appear in an entry.
 * Also convert all lowercase to uppercase.
 * The data returned is in a static area, so a second call will
 * erase the data of the first.
 */
u_char **
get_entries(u_char *path)
{
    static u_char *entries[128];	/* Maximum depth... */
    static u_char mypath[1024];
    u_char **e = entries;
    u_char *p = mypath;

    ustrncpy(mypath+1, path, 1022);
    p = mypath+1;
    mypath[1023] = 0;
    if (path[0] == '/' || path[0] == '\\') {
	mypath[0] = path[0];
	*e++ = mypath;
	*p++ = 0;
    }
    while (*p && e < entries + 127) {
	while (*p && (*p == '/' || *p == '\\')) {
	    ++p;
    	}

	if (!*p)
	    break;
    	*e++ = p;
	while (*p && (*p != '/' && *p != '\\')) {
	    if (islower(*p))
	    	*p = tolower(*p);
	    ++p;
    	}
	/*
	 * skip over the '/' or '\'
	 */
    	if (*p)
	    *p++ = 0;
    }
    *e = 0;
    return (entries);
}

/*
 * Return file system statistics for drive.
 * Return the DOS errno on failure.
 */
int
get_space(int drive, fsstat_t *fs)
{
    Path_t *d;
    struct statfs *buf;
    int nfs;
    int i;
    struct statfs *me = 0;

    if (drive < 0 || drive >= MAX_DRIVE)
	return (DISK_DRIVE_INVALID);

    d = &paths[drive];

    if (!d->path)
	return (DISK_DRIVE_INVALID);

    nfs = getfsstat(0, 0, MNT_WAIT);

    buf = (struct statfs *)malloc(sizeof(struct statfs) * nfs);
    if (buf == NULL) {
	perror("get_space");
	return (DISK_DRIVE_INVALID);
    }
    nfs = getfsstat(buf, sizeof(struct statfs) * nfs, MNT_WAIT);

    for (i = 0; i < nfs; ++i) {
	if (strncmp(buf[i].f_mntonname, (char *)d->path, strlen(buf[i].f_mntonname)))
	    continue;
    	if (me && strlen(me->f_mntonname) > strlen(buf[i].f_mntonname))
	    continue;
    	me = buf + i;
    }
    if (!me) {
	free(buf);
	return (3);
    }
    fs->bytes_sector = 512;
    fs->sectors_cluster = me->f_bsize / fs->bytes_sector;
    fs->total_clusters = me->f_blocks / fs->sectors_cluster;
    while (fs->total_clusters > 0xFFFF) {
	fs->sectors_cluster *= 2;
	fs->total_clusters = me->f_blocks / fs->sectors_cluster;
    }
    fs->avail_clusters = me->f_bavail / fs->sectors_cluster;
    free(buf);
    return (0);
}

#if 0
DIR *dp = 0;
u_char searchdir[1024];
u_char *searchend;
#endif

/*
 * Convert a dos filename into normal form (8.3 format, space padded)
 */
static void
to_dos_fcb(u_char *p, u_char *expr)
{
    int i;

    if (expr[0] == '.') {
	p[0] = '.';
	if (expr[1] == '\0') {
	    for (i = 1; i < 11; i++)
		p[i] = ' ';
	    return;
	}
	if (expr[1] == '.') {
	    p[1] = '.';
	    if (expr[2] == '\0') {
		for (i = 2; i < 11; i++)
		    p[i] = ' ';
		return;
	    }
	}
    }

    for (i = 8; i > 0; i--) {
	switch (*expr) {
	case '\0':
	case '.':
    		for (; i > 0; i--)
			*p++ = ' ';
		break;
	case '*':
    		for (; i > 0; i--)
			*p++ = '?';
		break;
	default:
		if (islower(*expr)) {
			*p++ = toupper(*expr++);
			break;
		}
	case '?':
		*p++ = *expr++;
		break;
	}
    }

    while (*expr != '\0' && *expr != '.')
	++expr;
    if (*expr)
	++expr;

    for (i = 3; i > 0; i--) {
	switch (*expr) {
	case '\0':
	case '.':
    		for (; i > 0; i--)
			*p++ = ' ';
		break;
	case '*':
    		for (; i > 0; i--)
			*p++ = '?';
		break;
	default:
		if (islower(*expr)) {
			*p++ = toupper(*expr++);
			break;
		}
	case '?':
		*p++ = *expr++;
		break;
	}
    }
}

/*
** DOS can't handle multiple concurrent searches, and if we leave the
** search instance in the DTA we get screwed as soon as someone starts lots
** of searches without finishing them properly.
** We allocate a single search structure, and recycle it if find_first()
** is called before a search ends.
*/
static search_t dir_search;

/*
 * Find the first file on drive which matches the path with the given
 * attributes attr.
 * If found, the result is placed in dir (32 bytes).
 * The DTA is populated as required by DOS, but the state area is ignored.
 * Returns DOS errno on failure.
 */
int
find_first(u_char *path, int attr, dosdir_t *dir, find_block_t *dta)
{
    u_char new_path[1024], real_path[1024];
    u_char *expr, *slash;
    int drive;
    int error;
    search_t *search = &dir_search;

    debug(D_REDIR, "find_first(%s, %x, %x)\n", path, attr, (int)dta);

    error = dos_makepath(path, new_path);
    if (error)
	return (error);

    expr = new_path;
    slash = 0;
    while (*expr != '\0') {
	if (*expr == '\\' || *expr == '/')
	    slash = expr;
	expr++;
    }
    *slash++ = '\0';

    error = dos_to_real_path(new_path, real_path, &drive);
    if (error)
	return (error);

    if (attr == VOLUME_LABEL)	/* never find a volume label */
	return (NO_MORE_FILES);

    if (search->dp)		/* stale search? */
	closedir(search->dp);

    search->dp = opendir(real_path);
    if (search->dp == NULL)
	return (PATH_NOT_FOUND);

    ustrncpy(search->searchdir, real_path, 1024 - ustrlen(real_path));
    search->searchend = search->searchdir;
    while (*search->searchend)
	++search->searchend;
    *search->searchend++ = '/';

    search->dp->dd_fd = squirrel_fd(search->dp->dd_fd);

    dta->drive = drive | 0x80;
    to_dos_fcb(dta->pattern, slash);
    dta->flag = attr;

    return (find_next(dir, dta));
}

/*
 * Continue on where find_first left off.
 * The results will be placed in dir.
 * DTA state area is ignored.
 */
int
find_next(dosdir_t *dir, find_block_t *dta)
{
    search_t *search = &dir_search;
    struct dirent *d;
    struct stat sb;
    u_char name[16];

    if (!search->dp)
	return (NO_MORE_FILES);

#if 0
    debug(D_REDIR, "find_next()\n");
#endif

    while ((d = readdir(search->dp)) != 0) {
    	real_to_dos((u_char *)d->d_name, name);
	to_dos_fcb(dir->name, name);
#if 0
printf("find_next: |%-11.11s| |%-11.11s| |%s| |%s|\n", dta->pattern, dir->name, d->d_name, name);
#endif
    	if (dos_match(dta->pattern, dir->name) == 0)
	    continue;

    	ustrcpy(search->searchend, (u_char *)d->d_name);
    	if (ustat(search->searchdir, &sb) < 0)
	    continue;
#if 0
printf("find_next: %x\n", sb.st_mode);
#endif
    	if (S_ISDIR(sb.st_mode)) {
	    if (!(dta->flag & DIRECTORY)) {
		continue;
	    }
	}
    	dir->attr = (S_ISDIR(sb.st_mode) ? DIRECTORY : 0) |
	    	    (uaccess(search->searchdir, W_OK) < 0 ? READ_ONLY_FILE : 0);
    	encode_dos_file_time(sb.st_mtime, &dir->date, &dir->time);
    	dir->start = 1;
    	dir->size = sb.st_size;
#if 0
printf("find_next: found %s\n",name);
#endif
	return (0);
    }
    closedir(search->dp);
    search->dp = NULL;
    return (NO_MORE_FILES);
}

/*
 * perfrom hokey DOS pattern matching.  pattern may contain the wild cards
 * '*' and '?' only.  Follow the DOS convention that '?*', '*?' and '**' all
 * are the same as '*'.  Also, allow '?' to match the blank padding in a
 * name (hence, ???? matchs all of "a", "ab", "abc" and "abcd" but not "abcde")
 * Return 1 if a match is found, 0 if not.
 * 
 * XXX This appears to be severely busted! (no * handling - normal?)
 */
int
dos_match(u_char *pattern, u_char *string)
{
    int i;

    /*
     * Check the base part first
     */
    for (i = 11; i > 0; i--) {
	if (*pattern != '?' && *string != *pattern)
	    return (0);
	pattern++, string++;
    }
    return (1);
}
