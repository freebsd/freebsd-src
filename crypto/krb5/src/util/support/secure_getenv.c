/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/secure_getenv.c - secure_getenv() portability support */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file contains the fallback implementation for secure_getenv(), which is
 * currently only provided by glibc 2.17 and later.  The goal is to ignore the
 * environment if this process is (or previously was) running at elevated
 * privilege compared to the calling process.
 *
 * In this fallback version we compare the real and effective uid/gid, and also
 * compare the saved uid/gid if possible.  These comparisons detect a setuid or
 * setgid process which is still running with elevated privilege; if we can
 * fetch the saved uid/gid, we also detect a process which has temporarily
 * dropped privilege with seteuid() or setegid().  These comparisons do not
 * detect the case where a setuid or setgid process has permanently dropped
 * privilege before the library initializer ran; this is not ideal because such
 * a process may possess a privileged resource or have privileged information
 * in its address space.
 *
 * Heimdal also looks at the ELF aux vector in /proc/self/auxv to determine the
 * starting uid/euid/gid/euid on Solaris/Illumos and NetBSD.  On FreeBSD this
 * approach can determine the executable path to do a stat() check.  We do not
 * go to this length due to the amount of code required.
 *
 * The BSDs and Solaris provide issetugid(), but the FreeBSD and NetBSD
 * versions are not useful; they return true if a non-setuid/setgid executable
 * is run by root and drops privilege, such as Apache httpd.  We do not want to
 * ignore the process environment in this case.
 *
 * On some platforms a process may have elevated privilege via mechanisms other
 * than setuid/setgid.  glibc's secure_getenv() should detect these cases on
 * Linux; we do not detect them in this fallback version.
 */

#include "k5-platform.h"

static int elevated_privilege = 0;

MAKE_INIT_FUNCTION(k5_secure_getenv_init);

int
k5_secure_getenv_init()
{
    int saved_errno = errno;

#ifdef HAVE_GETRESUID
    {
        uid_t r, e, s;
        if (getresuid(&r, &e, &s) == 0) {
            if (r != e || r != s)
                elevated_privilege = 1;
        }
    }
#else
    if (getuid() != geteuid())
        elevated_privilege = 1;
#endif

#ifdef HAVE_GETRESGID
    {
        gid_t r, e, s;
        if (!elevated_privilege && getresgid(&r, &e, &s) == 0) {
            if (r != e || r != s)
                elevated_privilege = 1;
        }
    }
#else
    if (!elevated_privilege && getgid() != getegid())
        elevated_privilege = 1;
#endif

    errno = saved_errno;
    return 0;
}

char *
k5_secure_getenv(const char *name)
{
    if (CALL_INIT_FUNCTION(k5_secure_getenv_init) != 0)
        return NULL;
    return elevated_privilege ? NULL : getenv(name);
}
