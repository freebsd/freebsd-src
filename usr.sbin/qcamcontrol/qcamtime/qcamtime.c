/*
 * Print out timing statistics from a QuickCam scan run yes, this is ugly,
 * it's just for simple analysis of driver timing.  This is not normally
 * part of the system.
 * 
 * Paul Traina, Feburary 1996
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <paths.h>
#include <nlist.h>

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
		if ((kmem = open(_PATH_KMEM, 0, 0)) < 0) {
			perror("open kmem");
			exit(1);
		}
		(void) fcntl(kmem, F_SETFD, 1);

		for (i = 0; i < MAX_SYMBOLS; i++) {
			if (nlist("/kernel", &names[i]) < 0) {
				perror("nlist");
				exit(1);
			}
			if (names[i].n_value == 0) {
				fprintf(stderr, "couldn't find names[%d]\n", i);
				exit(1);
			}
		}
	}
}

void
getdata(void)
{
	if (lseek(kmem, (off_t) names[0].n_value, SEEK_SET) < 0) {
		perror("lseek high");
		exit(1);
	}
	if (read(kmem, (u_short *) high_times, sizeof(high_times)) < 0) {
		perror("read high");
		exit(1);
	}
	if (lseek(kmem, (off_t) names[1].n_value, SEEK_SET) < 0) {
		perror("lseek low");
		exit(1);
	}
	if (read(kmem, (u_short *) low_times, sizeof(low_times)) < 0) {
		perror("read low");
		exit(1);
	}
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
}

void
main(void)
{
	getaddrs();
	getdata();
	printdata(high_times, FBUFSIZE);
	printdata(low_times, FBUFSIZE);
}
