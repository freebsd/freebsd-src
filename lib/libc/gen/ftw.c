/*
 * Copyright (c) 2003 by Joel Baker.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Author nor the names of any contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h> /* Because fts(3) says so */
#include <sys/stat.h>
#include <fts.h>

#include <unistd.h> /* We want strcpy */

#include <errno.h> /* Because errno is our friend */

#include "ftw.h"

/* I like symbolic values - this is only used in this file. */

enum __ftw_modes {
    MODE_FTW,
    MODE_NFTW
};

/* Prototype this so that we can have it later */

static int __ftw_core(const char *, void *, int, int, enum __ftw_modes);

/*
 * The external function calls are really just wrappers around __ftw_core,
 * since the work they do is 90% the same.
 */

int ftw (const char *dir, __ftw_func_t func, int descr) {
    return __ftw_core(dir, func, descr, 0, MODE_FTW);
}

int nftw (const char *dir, __nftw_func_t func, int descr, int flags) {
    return __ftw_core(dir, func, descr, flags, MODE_NFTW);
}

/*
typedef int (*__ftw_func_t) \
    (const char *file, const struct stat status, int flag);
typedef int (*__nftw_func_t) \
    (const char *file, const struct stat status, int flag, struct FTW detail);
*/

static int __ftw_core(const char *dir, void *func, int descr, int flags,
                      enum __ftw_modes mode) {
    FTS *hierarchy;
    FTSENT *entry;
    int fts_options;
    const char *paths[2];
    int ftw_flag, func_ret;
    struct FTW ftw_st;
    __ftw_func_t ftw_func;
    __nftw_func_t nftw_func;
    int saved_errno;

    errno = 0;

    /* We need at least one descriptor to call fts */

    if (descr < 1) {
        errno = EINVAL;
        return -1;
    }

    /* Decide which mode we're running in, and set the FTS options suitably. */

    if (MODE_NFTW == mode) { /* NFTW mode, with all the bells and whistles. */
        fts_options = (flags & FTW_PHYS) ? FTS_PHYSICAL : FTS_LOGICAL;
        fts_options |= (flags & FTW_CHDIR) ? 0 : FTS_NOCHDIR;
        fts_options |= (flags & FTW_MOUNT) ? FTS_XDEV : 0;
    } else { /* We must be in FTW mode. Nothing else makes sense. */
        fts_options = FTS_LOGICAL;
    }

    /* FTW gets a const char *, but FTS expects a null-term array of them. */

    paths[0] = dir;
    paths[1] = NULL;

    /* Open the file hierarchy. */

    if (!(hierarchy = fts_open((char * const *)paths, fts_options, NULL))) {
        if (EACCES == errno) {
            return 0;
        } else {
            return -1;
        }
    }

    /* The main loop. Is it not nifty? Worship the loop. */

    while ((entry = fts_read(hierarchy))) {
        switch (entry->fts_info) {

            case FTS_D:
                if ((MODE_NFTW != mode) || !(flags & FTW_DEPTH)) {
                    ftw_flag = FTW_D;
                }
                break;

            case FTS_DNR:
                ftw_flag = FTW_DNR;
                break;

            case FTS_F:
                ftw_flag = FTW_F;
                break;

            case FTS_SL:
                ftw_flag = FTW_SL;
                break;

            case FTS_NS:
                ftw_flag = FTW_NS;
                break;

            /* Values that should only occur in nftw mode */

            case FTS_SLNONE:
                if (MODE_NFTW == mode) {
                    ftw_flag = FTW_SLN;
                } else {
                    ftw_flag = FTW_SL;
                }
                break;

            case FTS_DP:
                if ((MODE_NFTW == mode) && (flags & FTW_DEPTH)) {
                    ftw_flag = FTW_D;
                }
                break;

            default:
                /* I'm not sure this is right, but we don't have a valid FTW
                 * type to call with, so cowardice seems the better part of
                 * guessing.
                 */
		break;
        }

        if (MODE_FTW == mode) {
            ftw_func = (__ftw_func_t) func;
            func_ret = (*ftw_func) 
                (entry->fts_path, entry->fts_statp, ftw_flag);
        } else if (MODE_NFTW == mode) {
            ftw_st.base = (entry->fts_pathlen - entry->fts_namelen);
            ftw_st.level = entry->fts_level;

            nftw_func = (__nftw_func_t) func;
            func_ret = (*nftw_func)
                (entry->fts_path, entry->fts_statp, ftw_flag, &ftw_st);
        }

        if (0 != func_ret) {
	    saved_errno = errno;
	    fts_close(hierarchy);
	    errno = saved_errno;
            return func_ret;
        }
    }

    /* The janitors will be upset if we don't clean up after ourselves. */

    saved_errno = errno;
    fts_close(hierarchy);
    if (0 != saved_errno) { /* fts_read returned NULL, and set errno - bail */
	errno = saved_errno;
    }

    return errno ? -1 : 0;
}
