/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
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
 * $FreeBSD$
 */

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>

#include <sys/time.h>
#include <err.h>
#include <errno.h>
#include <histedit.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LINELEN 2048
static char Buffer[LINELEN], Command[LINELEN];

static int
usage()
{
    fprintf(stderr, "usage: pppctl [-v] [-t n] [-p passwd] "
            "Port|LocalSock [command[;command]...]\n");
    fprintf(stderr, "              -v tells pppctl to output all"
            " conversation\n");
    fprintf(stderr, "              -t n specifies a timeout of n"
            " seconds when connecting (default 2)\n");
    fprintf(stderr, "              -p passwd specifies your password\n");
    exit(1);
}

static int TimedOut = 0;
static void
Timeout(int Sig)
{
    TimedOut = 1;
}

#define REC_PASSWD  (1)
#define REC_SHOW    (2)
#define REC_VERBOSE (4)

static char *passwd;
static char *prompt;

static char *
GetPrompt(EditLine *e)
{
    if (prompt == NULL)
        prompt = "";
    return prompt;
}

static int
Receive(int fd, int display)
{
    int Result;
    int len;
    char *last;

    prompt = Buffer;
    len = 0;
    while (Result = read(fd, Buffer+len, sizeof(Buffer)-len-1), Result != -1) {
        if (Result == 0 && errno != EINTR) {
          Result = -1;
          break;
        }
        len += Result;
        Buffer[len] = '\0';
        if (len > 2 && !strcmp(Buffer+len-2, "> ")) {
            prompt = strrchr(Buffer, '\n');
            if (display & (REC_SHOW|REC_VERBOSE)) {
                if (display & REC_VERBOSE)
                    last = Buffer+len-1;
                else
                    last = prompt;
                if (last) {
                    last++;
                    write(1, Buffer, last-Buffer);
                }
            }
            prompt = prompt == NULL ? Buffer : prompt+1;
            for (last = Buffer+len-2; last > Buffer && *last != ' '; last--)
                ;
            if (last > Buffer+3 && !strncmp(last-3, " on", 3)) {
                 /* a password is required ! */
                 if (display & REC_PASSWD) {
                    /* password time */
                    if (!passwd)
                        passwd = getpass("Password: ");
                    sprintf(Buffer, "passwd %s\n", passwd);
                    memset(passwd, '\0', strlen(passwd));
                    if (display & REC_VERBOSE)
                        write(1, Buffer, strlen(Buffer));
                    write(fd, Buffer, strlen(Buffer));
                    memset(Buffer, '\0', strlen(Buffer));
                    return Receive(fd, display & ~REC_PASSWD);
                }
                Result = 1;
            } else
                Result = 0;
            break;
        }
        if (len == sizeof Buffer - 1) {
            int flush;
            if ((last = strrchr(Buffer, '\n')) == NULL)
                /* Yeuch - this is one mother of a line ! */
                flush = sizeof Buffer / 2;
            else
                flush = last - Buffer + 1;
            write(1, Buffer, flush);
            strcpy(Buffer, Buffer + flush);
            len -= flush;
        }
    }

    return Result;
}

static int data = -1;
static jmp_buf pppdead;

static void
check_fd(int sig)
{
  if (data != -1) {
    struct timeval t;
    fd_set f;
    static char buf[LINELEN];
    int len;

    FD_ZERO(&f);
    FD_SET(data, &f);
    t.tv_sec = t.tv_usec = 0;
    if (select(data+1, &f, NULL, NULL, &t) > 0) {
      len = read(data, buf, sizeof buf);
      if (len > 0)
        write(1, buf, len);
      else
        longjmp(pppdead, -1);
    }
  }
}

static const char *
smartgets(EditLine *e, int *count, int fd)
{
  const char *result;

  data = fd;
  signal(SIGALRM, check_fd);
  ualarm(500000, 500000);
  result = setjmp(pppdead) ? NULL : el_gets(e, count);
  ualarm(0,0);
  signal(SIGALRM, SIG_DFL);
  data = -1;

  return result;
}

int
main(int argc, char **argv)
{
    struct servent *s;
    struct hostent *h;
    struct sockaddr *sock;
    struct sockaddr_in ifsin;
    struct sockaddr_un ifsun;
    int n, socksz, arg, fd, len, verbose, save_errno, hide1, hide1off, hide2;
    unsigned TimeoutVal;
    char *DoneWord = "x", *next, *start;
    struct sigaction act, oact;

    verbose = 0;
    TimeoutVal = 2;
    hide1 = hide1off = hide2 = 0;

    for (arg = 1; arg < argc; arg++)
        if (*argv[arg] == '-') {
            for (start = argv[arg] + 1; *start; start++)
                switch (*start) {
                    case 't':
                        TimeoutVal = (unsigned)atoi
                            (start[1] ? start + 1 : argv[++arg]);
                        start = DoneWord;
                        break;
    
                    case 'v':
                        verbose = REC_VERBOSE;
                        break;

                    case 'p':
                        if (start[1]) {
                          hide1 = arg;
                          hide1off = start - argv[arg];
                          passwd = start + 1;
                        } else {
                          hide1 = arg;
                          hide1off = start - argv[arg];
                          passwd = argv[++arg];
                          hide2 = arg;
                        }
                        start = DoneWord;
                        break;
    
                    default:
                        usage();
                }
        }
        else
            break;


    if (argc < arg + 1)
        usage();

    if (hide1) {
      char title[1024];
      int pos, harg;

      for (harg = pos = 0; harg < argc; harg++)
        if (harg == 0 || harg != hide2) {
          if (harg == 0 || harg != hide1)
            n = snprintf(title + pos, sizeof title - pos, "%s%s",
                            harg ? " " : "", argv[harg]);
          else if (hide1off > 1)
            n = snprintf(title + pos, sizeof title - pos, " %.*s",
                            hide1off, argv[harg]);
          else
            n = 0;
          if (n < 0 || n >= sizeof title - pos)
            break;
          pos += n;
        }
#ifdef __FreeBSD__
      setproctitle("-%s", title);
#else
      setproctitle("%s", title);
#endif
    }

    if (*argv[arg] == '/') {
        sock = (struct sockaddr *)&ifsun;
        socksz = sizeof ifsun;

        memset(&ifsun, '\0', sizeof ifsun);
        ifsun.sun_len = strlen(argv[arg]);
        if (ifsun.sun_len > sizeof ifsun.sun_path - 1) {
            warnx("%s: path too long", argv[arg]);
            return 1;
        }
        ifsun.sun_family = AF_LOCAL;
        strcpy(ifsun.sun_path, argv[arg]);

        if (fd = socket(AF_LOCAL, SOCK_STREAM, 0), fd < 0) {
            warnx("cannot create local domain socket");
            return 2;
        }
    } else {
        char *port, *host, *colon;
        int hlen;

        colon = strchr(argv[arg], ':');
        if (colon) {
            port = colon + 1;
            *colon = '\0';
            host = argv[arg];
        } else {
            port = argv[arg];
            host = "127.0.0.1";
        }
        sock = (struct sockaddr *)&ifsin;
        socksz = sizeof ifsin;
        hlen = strlen(host);

        memset(&ifsin, '\0', sizeof ifsin);
        if (strspn(host, "0123456789.") == hlen) {
            if (!inet_aton(host, &ifsin.sin_addr)) {
                warnx("cannot translate %s", host);
                return 1;
            }
        } else if ((h = gethostbyname(host)) == 0) {
            warnx("cannot resolve %s", host);
            return 1;
        }
        else
            ifsin.sin_addr.s_addr = *(u_long *)h->h_addr_list[0];

        if (colon)
            *colon = ':';

        if (strspn(port, "0123456789") == strlen(port))
            ifsin.sin_port = htons(atoi(port));
        else if (s = getservbyname(port, "tcp"), !s) {
            warnx("%s isn't a valid port or service!", port);
            usage();
        }
        else
            ifsin.sin_port = s->s_port;

        ifsin.sin_len = sizeof(ifsin);
        ifsin.sin_family = AF_INET;

        if (fd = socket(AF_INET, SOCK_STREAM, 0), fd < 0) {
            warnx("cannot create internet socket");
            return 2;
        }
    }

    TimedOut = 0;
    if (TimeoutVal) {
        act.sa_handler = Timeout;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        sigaction(SIGALRM, &act, &oact);
        alarm(TimeoutVal);
    }

    if (connect(fd, sock, socksz) < 0) {
        if (TimeoutVal) {
            save_errno = errno;
            alarm(0);
            sigaction(SIGALRM, &oact, 0);
            errno = save_errno;
        }
        if (TimedOut)
            warnx("timeout: cannot connect to socket %s", argv[arg]);
        else {
            if (errno)
                warn("cannot connect to socket %s", argv[arg]);
            else
                warnx("cannot connect to socket %s", argv[arg]);
        }
        close(fd);
        return 3;
    }

    if (TimeoutVal) {
        alarm(0);
        sigaction(SIGALRM, &oact, 0);
    }

    len = 0;
    Command[sizeof(Command)-1] = '\0';
    for (arg++; arg < argc; arg++) {
        if (len && len < sizeof(Command)-1)
            strcpy(Command+len++, " ");
        strncpy(Command+len, argv[arg], sizeof(Command)-len-1);
        len += strlen(Command+len);
    }

    switch (Receive(fd, verbose | REC_PASSWD))
    {
        case 1:
            fprintf(stderr, "Password incorrect\n");
            break;

        case 0:
            if (len == 0) {
                EditLine *edit;
                History *hist;
                const char *l, *env;
                int size;

                hist = history_init();
                if ((env = getenv("EL_SIZE"))) {
                    size = atoi(env);
                    if (size < 0)
                      size = 20;
                } else
                    size = 20;
#ifdef __NetBSD__
                history(hist, NULL, H_SETSIZE, size);
                edit = el_init("pppctl", stdin, stdout, stderr);
#else
                history(hist, H_EVENT, size);
                edit = el_init("pppctl", stdin, stdout);
#endif
                el_source(edit, NULL);
                el_set(edit, EL_PROMPT, GetPrompt);
                if ((env = getenv("EL_EDITOR"))) {
                    if (!strcmp(env, "vi"))
                        el_set(edit, EL_EDITOR, "vi");
                    else if (!strcmp(env, "emacs"))
                        el_set(edit, EL_EDITOR, "emacs");
                }
                el_set(edit, EL_SIGNAL, 1);
                el_set(edit, EL_HIST, history, (const char *)hist);
                while ((l = smartgets(edit, &len, fd))) {
                    if (len > 1)
#ifdef __NetBSD__
                        history(hist, NULL, H_ENTER, l);
#else
                        history(hist, H_ENTER, l);
#endif
                    write(fd, l, len);
                    if (Receive(fd, REC_SHOW) != 0)
                        break;
                }
                fprintf(stderr, "Connection closed\n");
                el_end(edit);
                history_end(hist);
            } else {
                start = Command;
                do {
                    next = strchr(start, ';');
                    while (*start == ' ' || *start == '\t')
                        start++;
                    if (next)
                        *next = '\0';
                    strcpy(Buffer, start);
                    Buffer[sizeof(Buffer)-2] = '\0';
                    strcat(Buffer, "\n");
                    if (verbose)
                        write(1, Buffer, strlen(Buffer));
                    write(fd, Buffer, strlen(Buffer));
                    if (Receive(fd, verbose | REC_SHOW) != 0) {
                        fprintf(stderr, "Connection closed\n");
                        break;
                    }
                    if (next)
                        start = ++next;
                } while (next && *next);
                if (verbose)
                    puts("");
            }
            break;

        default:
            warnx("ppp is not responding");
            break;
    }

    close(fd);
    
    return 0;
}
