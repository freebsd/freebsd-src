#!/bin/sh

# Test scenario by Eric Badger <eric badgerio us>

# userret: returning with the following locks held:
# exclusive sleep mutex process lock (process lock) r = 0 (0xcb714758)
#     locked @ kern/kern_event.c:2125
# panic: witness_warn

# https://people.freebsd.org/~pho/stress/log/kevent9.txt
# Fixed in r302235.

. ../default.cfg

[ `sysctl -n hw.ncpu` -ne 4 ] && echo "For best results use hw.ncpu == 4"

cd /tmp
cat > /tmp/kevent9-1.c <<EOF
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/wait.h>

#include <err.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_PROCS 4000

void *procmaker(void *arg __unused)
{
    pthread_set_name_np(pthread_self(), "procmaker");
    for (int i = 0; i < NUM_PROCS; ++i)
    {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 50000;
        nanosleep(&ts, NULL);
        switch(fork())
        {
            case -1:
                err(1, "fork");
                break;
            case 0:
                execl("./kevent9-2", "kevent9-2", NULL);
                _exit(127);
                break;
            default:
                break;
        }
    }
    printf("done forking\n");
    return NULL;
}

void *reaper(void *arg __unused)
{
    pthread_set_name_np(pthread_self(), "reaper");
    int counter = 0;
    while (counter < NUM_PROCS)
    {
        int status;
        if (wait(&status) > 0)
        {
            ++counter;
        }
    }
    printf("Reaped %d\n", counter);
    return NULL;
}

int main()
{
    pthread_set_name_np(pthread_self(), "main");

    int kqfd = kqueue();
    if (kqfd == -1)
    {
        err(1, "kqueue()");
    }

    struct kevent change;
    memset(&change, 0, sizeof(change));
    change.ident = getpid();
    change.filter = EVFILT_PROC;
    change.flags = EV_ADD | EV_ENABLE;
    change.fflags = NOTE_EXIT | NOTE_EXEC | NOTE_FORK | NOTE_TRACK;

    if (kevent(kqfd, &change, 1, NULL, 0, NULL) == -1)
    {
        err(1, "kevent change");
    }

    pthread_t t;
    pthread_create(&t, NULL, procmaker, NULL);
    pthread_create(&t, NULL, reaper, NULL);

    int numexecs = 0;
    int numexits = 0;
    int numforks = 0;
    int nummults = 0;
    int numchlds = 0;
    int numterrs = 0;

    while (1)
    {
        struct kevent event;
        struct timespec to;
        to.tv_sec = 1;
        to.tv_nsec = 0;
        int ret = kevent(kqfd, NULL, 0, &event, 1, &to);
        if (ret == -1)
        {
            err(1, "kevent event");
        }
        else if (ret == 0)
        {
            printf("numexecs: %d numexits: %d numforks: %d numchlds: %d numterrs: %d nummults: %d\n",
                    numexecs, numexits, numforks, numchlds, numterrs, nummults);

            // Sometimes we miss a NOTE_EXIT. If it hasn't arrived by the timeout, bail out since
            // it will probably never arrive.
            break;
            /*
            if (numexits == NUM_PROCS)
            {
                break;
            }
            else
            {
                continue;
            }
            */
        }

        int numflags = 0;
        if (event.fflags & NOTE_EXEC)
        {
            ++numflags;
            ++numexecs;
        }
        if (event.fflags & NOTE_EXIT)
        {
            ++numflags;
            ++numexits;
        }
        if (event.fflags & NOTE_FORK)
        {
            ++numflags;
            ++numforks;
        }
        if (event.fflags & NOTE_CHILD)
        {
            ++numflags;
            ++numchlds;
        }
        if (event.fflags & NOTE_TRACKERR)
        {
            ++numflags;
            ++numterrs;
        }
        if (numflags > 1)
        {
            ++nummults;
        }

        /*
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 50000;
        nanosleep(&ts, NULL);
        */
    }
    return 0;
}
EOF

cat > /tmp/kevent9-2.c <<EOF
#include <time.h>

int main()
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000;
    nanosleep(&ts, NULL);
    return 0;
}
EOF

mycc -o kevent9-1 -Wall -Wextra -O2 -g kevent9-1.c -lpthread || exit 1
mycc -o kevent9-2 -Wall -Wextra -O2 -g kevent9-2.c           || exit 1
rm kevent9-1.c kevent9-2.c

start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 300 ]; do
	./kevent9-1 > /dev/null
done
rm kevent9-1 kevent9-2
exit 0
