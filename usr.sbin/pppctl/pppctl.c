#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define LINELEN 2048
static char Buffer[LINELEN], Command[LINELEN];

static int Usage()
{
    fprintf(stderr, "Usage: pppctl [-v] [ -t n ] [ -p passwd ] Port|LocalSock command[;command]...\n");
    fprintf(stderr, "              -v tells pppctl to output all conversation\n");
    fprintf(stderr, "              -t n specifies a timeout of n seconds (default 2)\n");
    fprintf(stderr, "              -p passwd specifies your password\n");
    return 1;
}

static int TimedOut = 0;
void Timeout(int Sig)
{
    TimedOut = 1;
}

#define REC_PASSWD  (1)
#define REC_SHOW    (2)
#define REC_VERBOSE (4)

static char *passwd;

int Receive(int fd, unsigned TimeoutVal, int display)
{
    int Result;
    struct sigaction act, oact;
    int len;
    char *last;

    TimedOut = 0;
    if (TimeoutVal) {
        act.sa_handler = Timeout;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        sigaction(SIGALRM, &act, &oact);
        alarm(TimeoutVal);
    }

    len = 0;
    while (Result = read(fd, Buffer+len, sizeof(Buffer)-len-1), Result != -1) {
        len += Result;
        Buffer[len] = '\0';
        if (TimedOut) {
            if (display & REC_VERBOSE)
                write(1,Buffer,len);
            Result = -1;
            break;
        } else if (len > 2 && !strcmp(Buffer+len-2, "> ")) {
            if (display & (REC_SHOW|REC_VERBOSE)) {
                if (display & REC_VERBOSE)
                    last = Buffer+len-1;
                else
                    last = rindex(Buffer, '\n');
                if (last) {
                    *++last = '\0';
                    write(1, Buffer, last-Buffer);
                }
            }
            for (last = Buffer+len-2; last > Buffer && *last != ' '; last--)
                ;
            if (last > Buffer+3 && !strncmp(last-3, " on", 3)) {
                 /* a password is required ! */
                 if (display & REC_PASSWD) {
                    if (TimeoutVal) {
                        alarm(0);
                        sigaction(SIGALRM, &oact, 0);
                    }
                    /* password time */
                    if (!passwd)
                        passwd = getpass("Password: ");
                    sprintf(Buffer, "passwd %s\n", passwd);
                    bzero(passwd, strlen(passwd));
                    if (display & REC_VERBOSE)
                        write(1, Buffer, strlen(Buffer));
                    write(fd, Buffer, strlen(Buffer));
                    bzero(Buffer, strlen(Buffer));
                    return Receive(fd, TimeoutVal, display & ~REC_PASSWD);
                }
                Result = 1;
            } else
                Result = 0;
            break;
        }
    }

    if (TimedOut)
        Result = -1;

    if (TimeoutVal) {
        alarm(0);
        sigaction(SIGALRM, &oact, 0);
    }
    return Result;
}

int
main(int argc, char **argv)
{
    struct servent *s;
    struct hostent *h;
    struct sockaddr *sock;
    struct sockaddr_in ifsin;
    struct sockaddr_un ifsun;
    int socksz, arg, fd, len, verbose;
    unsigned TimeoutVal;
    char *DoneWord = "x", *next, *start;
    struct sigaction act, oact;

    verbose = 0;
    TimeoutVal = 2;

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
                        passwd = (start[1] ? start + 1 : argv[++arg]);
                        start = DoneWord;
                        break;
    
                    default:
                        return Usage();
                }
        }
        else
            break;


    if (argc < arg + 2)
        return Usage();

    if (*argv[arg] == '/') {
        sock = (struct sockaddr *)&ifsun;
        socksz = sizeof ifsun;

        ifsun.sun_len = strlen(argv[arg]);
        if (ifsun.sun_len > sizeof ifsun.sun_path - 1) {
            fprintf(stderr, "%s: Path too long\n", argv[arg]);
            return 1;
        }
        ifsun.sun_family = AF_LOCAL;
        strcpy(ifsun.sun_path, argv[arg]);

        if (fd = socket(AF_LOCAL, SOCK_STREAM, 0), fd < 0) {
            fprintf(stderr, "Cannot create local domain socket\n");
            return 2;
        }
    } else {
        sock = (struct sockaddr *)&ifsin;
        socksz = sizeof ifsin;

        if ((h = gethostbyname("localhost")) == 0) {
            fprintf(stderr, "Cannot resolve localhost\n");
            return 1;
        }

        if (strspn(argv[arg], "0123456789") == strlen(argv[arg]))
            ifsin.sin_port = htons(atoi(argv[arg]));
        else if (s = getservbyname(argv[arg], "tcp"), !s) {
            fprintf(stderr, "%s isn't a valid port or service!\n", argv[arg]);
            return Usage();
        }
        else
            ifsin.sin_port = s->s_port;

        ifsin.sin_len = sizeof(ifsin);
        ifsin.sin_family = AF_INET;
        ifsin.sin_addr.s_addr = *(u_long *)h->h_addr_list[0];

        if (fd = socket(AF_INET, SOCK_STREAM, 0), fd < 0) {
            fprintf(stderr, "Cannot create internet socket\n");
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
            alarm(0);
            sigaction(SIGALRM, &oact, 0);
        }
        if (TimedOut)
            fputs("Timeout: ", stderr);
        fprintf(stderr, "Cannot connect to socket %s\n", argv[arg]);
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

    switch (Receive(fd, TimeoutVal, verbose | REC_PASSWD))
    {
        case 1:
            fprintf(stderr, "Password incorrect\n");
            break;

        case 0:
            start = Command;
            do {
                next = index(start, ';');
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
                if (Receive(fd, TimeoutVal, verbose | REC_SHOW) != 0) {
                    fprintf(stderr, "No reply from ppp\n");
                    break;
                }
                if (next)
                    start = ++next;
            } while (next && *next);
            if (verbose)
                puts("");
            break;

        default:
            fprintf(stderr, "ppp is not responding\n");
            break;
    }

    close(fd);
    
    return 0;
}
