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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * This file has routines used to print out system calls and their
 * arguments.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
	{ "lstat", 1, 2,
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
	{ "access", 1, 2, { { String | IN, 0 }, { Int, 1 }}},
	{ "sigaction", 1, 3,
	  { { Signal, 0 }, { Ptr | IN, 1 }, { Ptr | OUT, 2 }}},
	{ "accept", 1, 3,
	  { { Hex, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ "bind", 1, 3,
	  { { Hex, 0 }, { Sockaddr | IN, 1 }, { Int, 2 } } },
	{ "connect", 1, 3,
	  { { Hex, 0 }, { Sockaddr | IN, 1 }, { Int, 2 } } },
	{ "getpeername", 1, 3,
	  { { Hex, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ "getsockname", 1, 3,
	  { { Hex, 0 }, { Sockaddr | OUT, 1 }, { Ptr | OUT, 2 } } },
	{ 0, 0, 0, { { 0, 0 }}},
};

char * ioctlname __P((int));

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
 * get_struct
 *
 * Copy a fixed amount of bytes from the process.
 */

int
get_struct(int procfd, void *offset, void *buf, int len) {
	char *pos;
	FILE *p;
	int c, fd;

	if ((fd = dup(procfd)) == -1)
		err(1, "dup");
	if ((p = fdopen(fd, "r")) == NULL)
		err(1, "fdopen");
	fseek(p, (long)offset, SEEK_SET);
	for (pos = (char *)buf; len--; pos++) {
		if ((c = fgetc(p)) == EOF)
			return -1;
		*pos = c;
	}
	fclose(p);
	return 0;
}

/*
 * get_string
 * Copy a string from the process.  Note that it is
 * expected to be a C string, but if max is set, it will
 * only get that much.
 */

char *
get_string(int procfd, void *offset, int max) {
	char *buf;
	int size, len, c, fd;
	FILE *p;

	if ((fd = dup(procfd)) == -1)
		err(1, "dup");
	if ((p = fdopen(fd, "r")) == NULL)
		err(1, "fdopen");
	buf = malloc( size = (max ? max : 64 ) );
	len = 0;
	buf[0] = 0;
	fseek(p, (long)offset, SEEK_SET);
	while ((c = fgetc(p)) != EOF) {
		buf[len++] = c;
		if (c == 0 || len == max) {
			buf[len] = 0;
			break;
		}
		if (len == size) {
			char *tmp;
			tmp = realloc(buf, size+64);
			if (tmp == NULL) {
				buf[len] = 0;
				fclose(p);
				return buf;
			}
			size += 64;
			buf = tmp;
		}
	}
	fclose(p);
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
  char *tmp = NULL;
  switch (sc->type & ARG_MASK) {
  case Hex:
    tmp = malloc(12);
    sprintf(tmp, "0x%lx", args[sc->offset]);
    break;
  case Octal:
    tmp = malloc(13);
    sprintf(tmp, "0%lo", args[sc->offset]);
    break;
  case Int:
    tmp = malloc(12);
    sprintf(tmp, "%ld", args[sc->offset]);
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
    sprintf(tmp, "0x%lx", args[sc->offset]);
    break;
  case Ioctl:
    {
      char *temp = ioctlname(args[sc->offset]);
      if (temp)
	tmp = strdup(temp);
      else {
	tmp = malloc(12);
	sprintf(tmp, "0x%lx", args[sc->offset]);
      }
    }
    break;
  case Signal:
    {
      long sig;

      sig = args[sc->offset];
      tmp = malloc(12);
      if (sig > 0 && sig < NSIG) {
	int i;
	sprintf(tmp, "sig%s", sys_signame[sig]);
	for (i = 0; tmp[i] != '\0'; ++i)
	  tmp[i] = toupper(tmp[i]);
      } else {
        sprintf(tmp, "%ld", sig);
      }
    }
    break;
  case Sockaddr:
    {
      struct sockaddr_storage ss;
      char addr[64];
      struct sockaddr_in *sin;
      struct sockaddr_in6 *sin6;
      struct sockaddr_un *sun;
      struct sockaddr *sa;
      char *p;
      u_char *q;
      int i;

      /* yuck: get ss_len */
      if (get_struct(fd, (void *)args[sc->offset], (void *)&ss,
	sizeof(ss.ss_len) + sizeof(ss.ss_family)) == -1)
	err(1, "get_struct %p", (void *)args[sc->offset]);
      /* sockaddr_un never have the length filled in! */
      if (ss.ss_family == AF_UNIX) {
	if (get_struct(fd, (void *)args[sc->offset], (void *)&ss,
	  sizeof(*sun))
	  == -1)
	  err(2, "get_struct %p", (void *)args[sc->offset]);
      } else {
	if (get_struct(fd, (void *)args[sc->offset], (void *)&ss, ss.ss_len)
	  == -1)
	  err(2, "get_struct %p", (void *)args[sc->offset]);
      }

      switch (ss.ss_family) {
      case AF_INET:
	sin = (struct sockaddr_in *)&ss;
	inet_ntop(AF_INET, &sin->sin_addr, addr, sizeof addr);
	asprintf(&tmp, "{ AF_INET %s:%d }", addr, htons(sin->sin_port));
	break;
      case AF_INET6:
	sin6 = (struct sockaddr_in6 *)&ss;
	inet_ntop(AF_INET6, &sin6->sin6_addr, addr, sizeof addr);
	asprintf(&tmp, "{ AF_INET6 [%s]:%d }", addr, htons(sin6->sin6_port));
	break;
      case AF_UNIX:
        sun = (struct sockaddr_un *)&ss;
        asprintf(&tmp, "{ AF_UNIX \"%s\" }", sun->sun_path);
	break;
      default:
	sa = (struct sockaddr *)&ss;
        asprintf(&tmp, "{ sa_len = %d, sa_family = %d, sa_data = {%n%*s } }",
	  (int)sa->sa_len, (int)sa->sa_family, &i,
	  6 * (int)(sa->sa_len - ((char *)&sa->sa_data - (char *)sa)), "");
	if (tmp != NULL) {
	  p = tmp + i;
          for (q = (u_char *)&sa->sa_data; q < (u_char *)sa + sa->sa_len; q++)
            p += sprintf(p, " %#02x,", *q);
	}
      }
    }
    break;
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
  int len = 0;
  len += fprintf(outfile, "%s(", name);
  for (i = 0; i < nargs; i++) {
    if (s_args[i])
      len += fprintf(outfile, "%s", s_args[i]);
    else
      len += fprintf(outfile, "<missing argument>");
    len += fprintf(outfile, "%s", i < (nargs - 1) ? "," : "");
  }
  len += fprintf(outfile, ")");
  for (i = 0; i < 6 - (len / 8); i++)
	fprintf(outfile, "\t");
}

void
print_syscall_ret(FILE *outfile, const char *name, int nargs, char **s_args, int errorp, int retval) {
  print_syscall(outfile, name, nargs, s_args);
  if (errorp) {
    fprintf(outfile, " ERR#%d '%s'\n", retval, strerror(retval));
  } else {
    fprintf(outfile, " = %d (0x%x)\n", retval, retval);
  }
}
