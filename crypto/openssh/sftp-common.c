/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Damien Miller.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"
RCSID("$OpenBSD: sftp-common.c,v 1.10 2003/11/10 16:23:41 jakob Exp $");

#include "buffer.h"
#include "bufaux.h"
#include "log.h"
#include "xmalloc.h"

#include "sftp.h"
#include "sftp-common.h"

/* Clear contents of attributes structure */
void
attrib_clear(Attrib *a)
{
	a->flags = 0;
	a->size = 0;
	a->uid = 0;
	a->gid = 0;
	a->perm = 0;
	a->atime = 0;
	a->mtime = 0;
}

/* Convert from struct stat to filexfer attribs */
void
stat_to_attrib(const struct stat *st, Attrib *a)
{
	attrib_clear(a);
	a->flags = 0;
	a->flags |= SSH2_FILEXFER_ATTR_SIZE;
	a->size = st->st_size;
	a->flags |= SSH2_FILEXFER_ATTR_UIDGID;
	a->uid = st->st_uid;
	a->gid = st->st_gid;
	a->flags |= SSH2_FILEXFER_ATTR_PERMISSIONS;
	a->perm = st->st_mode;
	a->flags |= SSH2_FILEXFER_ATTR_ACMODTIME;
	a->atime = st->st_atime;
	a->mtime = st->st_mtime;
}

/* Convert from filexfer attribs to struct stat */
void
attrib_to_stat(const Attrib *a, struct stat *st)
{
	memset(st, 0, sizeof(*st));

	if (a->flags & SSH2_FILEXFER_ATTR_SIZE)
		st->st_size = a->size;
	if (a->flags & SSH2_FILEXFER_ATTR_UIDGID) {
		st->st_uid = a->uid;
		st->st_gid = a->gid;
	}
	if (a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS)
		st->st_mode = a->perm;
	if (a->flags & SSH2_FILEXFER_ATTR_ACMODTIME) {
		st->st_atime = a->atime;
		st->st_mtime = a->mtime;
	}
}

/* Decode attributes in buffer */
Attrib *
decode_attrib(Buffer *b)
{
	static Attrib a;

	attrib_clear(&a);
	a.flags = buffer_get_int(b);
	if (a.flags & SSH2_FILEXFER_ATTR_SIZE)
		a.size = buffer_get_int64(b);
	if (a.flags & SSH2_FILEXFER_ATTR_UIDGID) {
		a.uid = buffer_get_int(b);
		a.gid = buffer_get_int(b);
	}
	if (a.flags & SSH2_FILEXFER_ATTR_PERMISSIONS)
		a.perm = buffer_get_int(b);
	if (a.flags & SSH2_FILEXFER_ATTR_ACMODTIME) {
		a.atime = buffer_get_int(b);
		a.mtime = buffer_get_int(b);
	}
	/* vendor-specific extensions */
	if (a.flags & SSH2_FILEXFER_ATTR_EXTENDED) {
		char *type, *data;
		int i, count;

		count = buffer_get_int(b);
		for (i = 0; i < count; i++) {
			type = buffer_get_string(b, NULL);
			data = buffer_get_string(b, NULL);
			debug3("Got file attribute \"%s\"", type);
			xfree(type);
			xfree(data);
		}
	}
	return &a;
}

/* Encode attributes to buffer */
void
encode_attrib(Buffer *b, const Attrib *a)
{
	buffer_put_int(b, a->flags);
	if (a->flags & SSH2_FILEXFER_ATTR_SIZE)
		buffer_put_int64(b, a->size);
	if (a->flags & SSH2_FILEXFER_ATTR_UIDGID) {
		buffer_put_int(b, a->uid);
		buffer_put_int(b, a->gid);
	}
	if (a->flags & SSH2_FILEXFER_ATTR_PERMISSIONS)
		buffer_put_int(b, a->perm);
	if (a->flags & SSH2_FILEXFER_ATTR_ACMODTIME) {
		buffer_put_int(b, a->atime);
		buffer_put_int(b, a->mtime);
	}
}

/* Convert from SSH2_FX_ status to text error message */
const char *
fx2txt(int status)
{
	switch (status) {
	case SSH2_FX_OK:
		return("No error");
	case SSH2_FX_EOF:
		return("End of file");
	case SSH2_FX_NO_SUCH_FILE:
		return("No such file or directory");
	case SSH2_FX_PERMISSION_DENIED:
		return("Permission denied");
	case SSH2_FX_FAILURE:
		return("Failure");
	case SSH2_FX_BAD_MESSAGE:
		return("Bad message");
	case SSH2_FX_NO_CONNECTION:
		return("No connection");
	case SSH2_FX_CONNECTION_LOST:
		return("Connection lost");
	case SSH2_FX_OP_UNSUPPORTED:
		return("Operation unsupported");
	default:
		return("Unknown status");
	}
	/* NOTREACHED */
}

/*
 * drwxr-xr-x    5 markus   markus       1024 Jan 13 18:39 .ssh
 */
char *
ls_file(const char *name, const struct stat *st, int remote)
{
	int ulen, glen, sz = 0;
	struct passwd *pw;
	struct group *gr;
	struct tm *ltime = localtime(&st->st_mtime);
	char *user, *group;
	char buf[1024], mode[11+1], tbuf[12+1], ubuf[11+1], gbuf[11+1];

	strmode(st->st_mode, mode);
	if (!remote && (pw = getpwuid(st->st_uid)) != NULL) {
		user = pw->pw_name;
	} else {
		snprintf(ubuf, sizeof ubuf, "%u", (u_int)st->st_uid);
		user = ubuf;
	}
	if (!remote && (gr = getgrgid(st->st_gid)) != NULL) {
		group = gr->gr_name;
	} else {
		snprintf(gbuf, sizeof gbuf, "%u", (u_int)st->st_gid);
		group = gbuf;
	}
	if (ltime != NULL) {
		if (time(NULL) - st->st_mtime < (365*24*60*60)/2)
			sz = strftime(tbuf, sizeof tbuf, "%b %e %H:%M", ltime);
		else
			sz = strftime(tbuf, sizeof tbuf, "%b %e  %Y", ltime);
	}
	if (sz == 0)
		tbuf[0] = '\0';
	ulen = MAX(strlen(user), 8);
	glen = MAX(strlen(group), 8);
	snprintf(buf, sizeof buf, "%s %3u %-*s %-*s %8llu %s %s", mode,
	    (u_int)st->st_nlink, ulen, user, glen, group,
	    (unsigned long long)st->st_size, tbuf, name);
	return xstrdup(buf);
}
