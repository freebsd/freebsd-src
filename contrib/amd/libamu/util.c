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
 * $Id: util.c,v 1.3.2.3 2002/12/27 22:45:13 ezk Exp $
 *
 */

/*
 * General Utilities.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#include <am_defs.h>
#include <amu.h>


char *
strnsave(const char *str, int len)
{
  char *sp = (char *) xmalloc(len + 1);
  memmove(sp, str, len);
  sp[len] = 0;

  return sp;
}


/*
 * Concatenate three strings and store in buffer pointed to
 * by p, making p large enough to hold the strings
 */
char *
str3cat(char *p, char *s1, char *s2, char *s3)
{
  int l1 = strlen(s1);
  int l2 = strlen(s2);
  int l3 = strlen(s3);

  p = (char *) xrealloc(p, l1 + l2 + l3 + 1);
  memmove(p, s1, l1);
  memmove(p + l1, s2, l2);
  memmove(p + l1 + l2, s3, l3 + 1);
  return p;
}


/*
 * Make all the directories in the path.
 */
int
mkdirs(char *path, int mode)
{
  /*
   * take a copy in case path is in readonly store
   */
  char *p2 = strdup(path);
  char *sp = p2;
  struct stat stb;
  int error_so_far = 0;

  /*
   * Skip through the string make the directories.
   * Mostly ignore errors - the result is tested at the end.
   *
   * This assumes we are root so that we can do mkdir in a
   * mode 555 directory...
   */
  while ((sp = strchr(sp + 1, '/'))) {
    *sp = '\0';
    if (mkdir(p2, mode) < 0) {
      error_so_far = errno;
    } else {
#ifdef DEBUG
      dlog("mkdir(%s)", p2);
#endif /* DEBUG */
    }
    *sp = '/';
  }

  if (mkdir(p2, mode) < 0) {
    error_so_far = errno;
  } else {
#ifdef DEBUG
    dlog("mkdir(%s)", p2);
#endif /* DEBUG */
  }

  XFREE(p2);

  return stat(path, &stb) == 0 &&
    (stb.st_mode & S_IFMT) == S_IFDIR ? 0 : error_so_far;
}


/*
 * Remove as many directories in the path as possible.
 * Give up if the directory doesn't appear to have
 * been created by Amd (not mode dr-x) or an rmdir
 * fails for any reason.
 */
void
rmdirs(char *dir)
{
  char *xdp = strdup(dir);
  char *dp;

  do {
    struct stat stb;
    /*
     * Try to find out whether this was
     * created by amd.  Do this by checking
     * for owner write permission.
     */
    if (stat(xdp, &stb) == 0 && (stb.st_mode & 0200) == 0) {
      if (rmdir(xdp) < 0) {
	if (errno != ENOTEMPTY &&
	    errno != EBUSY &&
	    errno != EEXIST &&
	    errno != EINVAL)
	  plog(XLOG_ERROR, "rmdir(%s): %m", xdp);
	break;
      } else {
#ifdef DEBUG
	dlog("rmdir(%s)", xdp);
#endif /* DEBUG */
      }
    } else {
      break;
    }

    dp = strrchr(xdp, '/');
    if (dp)
      *dp = '\0';
  } while (dp && dp > xdp);

  XFREE(xdp);
}
