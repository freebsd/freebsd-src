/*
 * Print out timing statistics from a QuickCam scan run yes, this is ugly,
 * it's just for simple analysis of driver timing.  This is not normally
 * part of the system.
 * 
 * Paul Traina, Feburary 1996
 */

#include <err.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>

#include <machine/qcam.h>

int             kmem = -1;

struct nlist    names[] = {
	{"_qcam_rsbhigh"},
	{"_qcam_rsblow"}
};
#define MAX_SYMBOLS 2

#define	FBUFSIZE (QC_MAX_XSIZE*QC_MAX_YSIZE)+50
static u_short  high_times[FBUFSIZE];
static u_short  low_times[FBUFSIZE];

void
getaddrs(void)
{
	int             i;

	if (kmem < 0) {
		if ((kmem = open(_PATH_KMEM, 0, 0)) < 0)
			err(1, "open kmem");
		(void) fcntl(kmem, F_SETFD, 1);

		for (i = 0; i < MAX_SYMBOLS; i++) {
			if (nlist("/kernel", &names[i]) < 0)
				err(1, "nlist");
			if (names[i].n_value == 0)
				errx(1, "couldn't find names[%d]", i);
		}
	}
}

void
getdata(void)
{
	if (lseek(kmem, (off_t) names[0].n_value, SEEK_SET) < 0)
		err(1, "lseek high");
	if (read(kmem, (u_short *) high_times, sizeof(high_times)) < 0)
		err(1, "read high");
	if (lseek(kmem, (off_t) names[1].n_value, SEEK_SET) < 0)
		err(1, "lseek low");
	if (read(kmem, (u_short *) low_times, sizeof(low_times)) < 0)
		err(1, "read low");
}


/*
 * slow and stupid, who cares?  we're just learning about the camera's
 * behavior
 */
int
printdata(u_short * p, int length)
{
	int             i, j, non_zero;

	for (i = 0; i < length;) {
		non_zero = 0;
		for (j = 0; j < 16; j++)
			if (p[j])
				non_zero++;

		if (non_zero) {
			printf("%8d:", i);

			for (j = 0; j < 16; j++) {
				printf(" %d", *p++);
				i++;
			}
			printf("\n");
		} else
			i += 16;
	}
	return(0);
}

int
main(void)
{
	getaddrs();
	getdata();
	printdata(high_times, FBUFSIZE);
	printdata(low_times, FBUFSIZE);
	return(0);
}
