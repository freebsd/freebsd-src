/*-
 * Copyright (c) 1998 Brian Somers <brian@Awfulhak.org>
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
 *	$Id$
 */

#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/select.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
  fd_set r, e;
  int n, i;
  char buf[512];

  n = STDIN_FILENO > STDOUT_FILENO ? STDIN_FILENO : STDOUT_FILENO;
  n++;

  while (1) {
    FD_ZERO(&r);
    FD_SET(STDIN_FILENO, &r);
    FD_SET(STDOUT_FILENO, &r);

    FD_ZERO(&e);
    FD_SET(STDIN_FILENO, &e);
    FD_SET(STDOUT_FILENO, &e);

    i = select(n, &r, NULL, &e, NULL);

    if (i > 0) {
      if (FD_ISSET(STDIN_FILENO, &e) || FD_ISSET(STDOUT_FILENO, &e))
        break;
      if (FD_ISSET(STDIN_FILENO, &r)) {
        i = read(STDIN_FILENO, buf, sizeof buf);
        if (i <= 0 || write(STDOUT_FILENO, buf, i) != i)
          break;
      }
      if (FD_ISSET(STDOUT_FILENO, &r)) {
        i = read(STDOUT_FILENO, buf, sizeof buf);
        if (i <= 0 || write(STDIN_FILENO, buf, i) != i)
          break;
      }
    } else if (i == 0 || errno != EINTR)
      break;
  }

  return 0;
}
