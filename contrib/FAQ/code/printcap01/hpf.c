/*
source to my hp filter, installed as /usr/libexec/lpr/hpf:
*/
#include "stdio.h"
#include <signal.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sgtty.h> 

main(ac, av)
int ac;
char **av;
{
        int c;
        struct sgttyb nbuf;
        unsigned long lbits;

        setbuf(stdout, NULL);
        lbits = LDECCTQ | LPASS8 | LLITOUT;
        ioctl(fileno(stdout), TIOCLSET, &lbits);
        ioctl(fileno(stdout), TIOCGETP, &nbuf);
        nbuf.sg_flags &= ~(ECHO | XTABS | CRMOD);
        ioctl(fileno(stdout), TIOCSETP, &nbuf);

        fputs("\033E\033&k2G", stdout);

        while (1) {
                if ((c = getchar()) != EOF) {
                        putchar(c);
                } else {
                        break;
                }
        }

        fputs("\033&l0H", stdout);

        exit(0);
}
