#!/bin/sh

# A regression test for seekdir/telldir
# submitted by julian@freebsd.org
# https://reviews.freebsd.org/D2410.
# Fixed by r282485

. ../default.cfg

odir=`pwd`
cd /tmp
sed '1,/^EOF/d' < $odir/$0 > seekdir.c
rm -f /tmp/seekdir
mycc -o seekdir -O2 seekdir.c || exit 1
rm -f seekdir.c
cd $odir

mount | grep -q "$mntpoint " && umount -f $mntpoint
mount -o size=1g -t tmpfs tmpfs $mntpoint

cd $mntpoint
mkdir test2
/tmp/seekdir > /dev/null
[ `echo $mntpoint/test2/* | wc -w` -eq 1 ] ||
    { echo FAIL; status=1; }
cd $odir

while mount | grep $mntpoint | grep -q tmpfs; do
	umount $mntpoint || sleep 1
done
rm -f /tmp/seekdir
exit $status
EOF
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include <err.h>

#define CHUNKSIZE 5
#define TOTALFILES 40

static void
SeekDir(DIR *dirp, long loc)
{
    printf("Seeking back to location %ld\n", loc);
    seekdir(dirp, loc);
}

static long
TellDir(DIR *dirp)
{
    long loc;

    loc = telldir(dirp);
    printf("telldir assigned location %ld\n", loc);
    return (loc);
}

int
main(int argc, char *argv[])
{
    DIR            *dirp;
    int        i;
    int        j;
    long        offset = 0, prev_offset = 0;
    char           *files[100];
    char        filename[100];
    int        fd;
    struct dirent  *dp = NULL;

    if (chdir("./test2") != 0) {
        err(EX_OSERR, "chdir");
    }

    /*****************************************************/
    /* Put a set of sample files in the target directory */
    /*****************************************************/

    for (i=1; i < TOTALFILES ; i++)
    {
        sprintf(filename, "file-%d", i);
        fd = open(filename, O_CREAT, 0666);
        if (fd == -1) {
            err(EX_OSERR, "open");
        }
        close(fd);
    }
    dirp = opendir(".");
    offset = TellDir(dirp);
    for (i = 0; i < 20; i++)
        files[i] = malloc(20);

    /*******************************************************/
    /* enumerate and delete small sets of files, one group */
    /* at a time.                                          */
    /*******************************************************/
    do {

        /*****************************************/
        /* Read in up to CHUNKSIZE file names    */
        /* i will be the number of files we hold */
        /*****************************************/
        for (i = 0; i < CHUNKSIZE; i++) {
            if ((dp = readdir(dirp)) != NULL) {
                strcpy(files[i], dp->d_name);

                printf("readdir (%ld) returned file %s\n",
                    offset, files[i]);

                prev_offset = offset;
                offset = TellDir(dirp);

            } else {
                printf("readdir returned null\n");
                break;
            }
        }

/****************************************************************/
        /* Simuate the last entry not fitting into our (samba's) buffer */
        /* If we read someting in on the last slot, push it back        */
        /* Pretend it didn't fit. This is approximately what SAMBA does.*/
/****************************************************************/
        if (dp != NULL) {
            /* Step back */
            SeekDir(dirp, prev_offset);
            offset = TellDir(dirp);
            i--;
            printf("file %s returned\n", files[i]);
        }

        /*****************************************/
        /* i is the number of names we have left.*/
        /*  Delete them.                         */
        /*****************************************/
        for (j = 0; j < i; j++) {
            if (*files[j] == '.') {
                printf ("skipping %s\n", files[j]);
            } else {
                printf("Unlinking file %s\n", files[j]);
                if (unlink(files[j]) != 0) {
                    err(EX_OSERR, "unlink");
                }
            }
        }
    } while (dp != NULL);

    closedir(dirp);
    //chdir("..");

}
