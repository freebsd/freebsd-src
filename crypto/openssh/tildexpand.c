/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#include "includes.h"
RCSID("$OpenBSD: tildexpand.c,v 1.13 2002/06/23 03:25:50 deraadt Exp $");

#include "xmalloc.h"
#include "log.h"
#include "tildexpand.h"

/*
 * Expands tildes in the file name.  Returns data allocated by xmalloc.
 * Warning: this calls getpw*.
 */
char *
tilde_expand_filename(const char *filename, uid_t my_uid)
{
	const char *cp;
	u_int userlen;
	char *expanded;
	struct passwd *pw;
	char user[100];
	int len;

	/* Return immediately if no tilde. */
	if (filename[0] != '~')
		return xstrdup(filename);

	/* Skip the tilde. */
	filename++;

	/* Find where the username ends. */
	cp = strchr(filename, '/');
	if (cp)
		userlen = cp - filename;	/* Something after username. */
	else
		userlen = strlen(filename);	/* Nothing after username. */
	if (userlen == 0)
		pw = getpwuid(my_uid);		/* Own home directory. */
	else {
		/* Tilde refers to someone elses home directory. */
		if (userlen > sizeof(user) - 1)
			fatal("User name after tilde too long.");
		memcpy(user, filename, userlen);
		user[userlen] = 0;
		pw = getpwnam(user);
	}
	if (!pw)
		fatal("Unknown user %100s.", user);

	/* If referring to someones home directory, return it now. */
	if (!cp) {
		/* Only home directory specified */
		return xstrdup(pw->pw_dir);
	}
	/* Build a path combining the specified directory and path. */
	len = strlen(pw->pw_dir) + strlen(cp + 1) + 2;
	if (len > MAXPATHLEN)
		fatal("Home directory too long (%d > %d", len-1, MAXPATHLEN-1);
	expanded = xmalloc(len);
	snprintf(expanded, len, "%s%s%s", pw->pw_dir,
	    strcmp(pw->pw_dir, "/") ? "/" : "", cp + 1);
	return expanded;
}
