/*
 *
 * Simple authentication database handling code.
 *
 * Copyright (c) 1998
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <paths.h>
#include <fcntl.h>
#include <libutil.h>

static properties P;

static int
initauthconf(const char *path)
{
    int fd;

    if (!P) {
	if ((fd = open(path, O_RDONLY)) < 0) {
	    syslog(LOG_ERR, "initauthconf: unable to open file: %s", path);
	    return 1;
	}
	P = properties_read(fd);
	close(fd);
	if (!P) {
	    syslog(LOG_ERR, "initauthconf: unable to parse file: %s", path);
	    return 1;
	}
    }
    return 0;
}

char *
auth_getval(const char *name)
{
    if (!P && initauthconf(_PATH_AUTHCONF))
	return NULL;
    else
	return property_find(P, name);
}
