/*
 * Copryight 1997 Sean Eric Fagan
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
 *	This product includes software developed by Sean Eric Fagan
 * 4. Neither the name of the author may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 */

/*
 * This file has routines used to print out system calls and their
 * arguments.
 */
/*
 * $Id: syscalls.c,v 1.2 1997/12/06 06:51:14 sef Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "syscall.h"

/*
 * This should probably be in its own file.
 */

struct syscall syscalls[] = {
	{ "readlink", 1, 3,
	  { { String, 0 } , { String | OUT, 1 }, { Int, 2 }}},
	{ "lseek", 2, 3,
	  { { Int, 0 }, {Quad, 2 }, { Int, 4 }}},
	{ "mmap", 2, 6,
	  { { Hex, 0 }, {Int, 1}, {Hex, 2}, {Hex, 3}, {Int, 4}, {Quad, 6}}},
	{ "open", 1, 3,
	  { { String | IN, 0} , { Int, 1}, {Octal, 2}}},
	{ "linux_open", 1, 3,
	  { { String, 0 }, { Int, 1}, { Octal, 2 }}},
	{ "close", 1, 1, { { Int, 0 } } },
	{ "fstat", 1, 2,
	  { { Int, 0},  {Ptr | OUT , 1 }}},
	{ "stat", 1, 2,
	  { { String | IN, 0 }, { Ptr | OUT, 1 }}},
	{ "linux_newstat", 1, 2,
	  { { String | IN, 0 }, { Ptr | OUT, 1 }}},
	{ "linux_newfstat", 1, 2,
	  { { Int, 0 }, { Ptr | OUT, 1 }}},
	{ "write", 1, 3,
	  { { Int, 0}, { Ptr | IN, 1 }, { Int, 2 }}},
	{ "ioctl", 1, 3,
	  { { Int, 0}, { Ioctl, 1 }, { Hex, 2 }}},
	{ "break", 1, 1, { { Hex, 0 }}},
	{ "exit", 0, 1, { { Hex, 0 }}},
	{ 0, 0, 0, { 0, 0 } },
};

/*
 * If/when the list gets big, it might be desirable to do it
 * as a hash table or binary search.
 */

struct syscall *
get_syscall(const char *name) {
	struct syscall *sc = syscalls;

	while (sc->name) {
		if (!strcmp(name, sc->name))
			return sc;
		sc++;
	}
	return NULL;
}

/*
 * get_string
 * Copy a string from the process.  Note that it is
 * expected to be a C string, but if max is set, it will
 * only get that much.
 */

char *
get_string(int procfd, void *offset, int max) {
	char *buf, *tmp;
	int size, len, c;
	FILE *p;

	if ((p = fdopen(procfd, "r")) == NULL) {
		perror("fdopen");
		exit(1);
	}
	buf = malloc( size = (max ? max : 64 ) );
	len = 0;
	fseek(p, (long)offset, SEEK_SET);
	while ((c = fgetc(p)) != EOF) {
		buf[len++] = c;
		if (c == 0 || len == max) {
			buf[len] = 0;
			break;
		}
		if (len == size) {
			char *tmp = buf;
			tmp = realloc(buf, size+64);
			if (tmp == NULL) {
				buf[len] = 0;
				return buf;
			}
			size += 64;
		}
	}
	return buf;
}


/*
 * Gag.  This is really unportable.  Multiplication is more portable.
 * But slower, from the code I saw.
 */

static long long
make_quad(unsigned long p1, unsigned long p2) {
  union {
    long long ll;
    unsigned long l[2];
  } t;
  t.l[0] = p1;
  t.l[1] = p2;
  return t.ll;
}


/*
 * print_arg
 * Converts a syscall argument into a string.  Said string is
 * allocated via malloc(), so needs to be free()'d.  The file
 * descriptor is for the process' memory (via /proc), and is used
 * to get any data (where the argument is a pointer).  sc is
 * a pointer to the syscall description (see above); args is
 * an array of all of the system call arguments.
 */

char *
print_arg(int fd, struct syscall_args *sc, unsigned long *args) {
  char *tmp;
  switch (sc->type & ARG_MASK) {
  case Hex:
    tmp = malloc(12);
    sprintf(tmp, "0x%x", args[sc->offset]);
    break;
  case Octal:
    tmp = malloc(13);
    sprintf(tmp, "0%o", args[sc->offset]);
    break;
  case Int:
    tmp = malloc(12);
    sprintf(tmp, "%d", args[sc->offset]);
    break;
  case String:
    {
      char *tmp2;
      tmp2 = get_string(fd, (void*)args[sc->offset], 0);
      tmp = malloc(strlen(tmp2) + 3);
      sprintf(tmp, "\"%s\"", tmp2);
      free(tmp2);
    }
  break;
  case Quad:
    {
      unsigned long long t;
      unsigned long l1, l2;
      l1 = args[sc->offset];
      l2 = args[sc->offset+1];
      t = make_quad(l1, l2);
      tmp = malloc(24);
      sprintf(tmp, "0x%qx", t);
      break;
    }
  case Ptr:
    tmp = malloc(12);
    sprintf(tmp, "0x%x", args[sc->offset]);
    break;
  case Ioctl:
    {
      char *temp = ioctlname(args[sc->offset]);
      if (temp)
	tmp = strdup(temp);
      else {
	tmp = malloc(12);
	sprintf(tmp, "0x%x", args[sc->offset]);
      }
    }
  }
  return tmp;
}

/*
 * print_syscall
 * Print (to outfile) the system call and its arguments.  Note that
 * nargs is the number of arguments (not the number of words; this is
 * potentially confusing, I know).
 */

void
print_syscall(FILE *outfile, const char *name, int nargs, char **s_args) {
  int i;
  fprintf(outfile, "syscall %s(", name);
  for (i = 0; i < nargs; i++) {
    if (s_args[i])
      fprintf(outfile, "%s", s_args[i]);
    else
      fprintf(outfile, "<missing argument>");
    fprintf(outfile, "%s", i < (nargs - 1) ? "," : "");
  }
  fprintf(outfile, ")\n\t");
}
