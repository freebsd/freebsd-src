/*
 * swapinfo
 *
 * Swapinfo will provide some information about the state of the swap
 * space for the system.  It'll determine the number of swap areas,
 * their original size, and their utilization.
 *
 * Kevin Lahey, February 16, 1993
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <sys/stat.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/rlist.h>  /* swapmap defined here... */
#include <nlist.h>


static struct nlist nl[] = {{"_swapmap"},  /* list of free swap areas */
#define VM_SWAPMAP	0
			    {"_swdevt"},   /* list of swap devices and sizes */
#define VM_SWDEVT	1
			    {"_nswap"},    /* size of largest swap device */
#define VM_NSWAP	2
			    {"_nswdev"},   /* number of swap devices */
#define VM_NSWDEV	3
			    {"_dmmax"},    /* maximum size of a swap block */
#define VM_DMMAX	4
			    {""}};

char    *getbsize __P((char *, int *, long *));
void	 usage __P((void));
int	 kflag;

main (argc, argv)
int	argc;
char	**argv;
{
	int	i, total_avail, total_free, total_partitions, *by_device, 
		nswap, nswdev, dmmax, ch;
	struct swdevt	*swdevt;
	struct rlist	head;
	static long blocksize;
        static int headerlen;
        static char *header;
        char  **save;

	/* We are trying to be simple here: */

	save = argv;
	kflag = 0;
	while ((ch = getopt(argc, argv, "k")) != EOF)
		switch(ch) {
		case 'k':
			kflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;

	if (!*argv) {
		argv = save;
		argv[0] = ".";
		argv[1] = NULL;
	}

	/* Open up /dev/kmem for reading. */
	
	if (kvm_openfiles (NULL, NULL, NULL) == -1) {
		fprintf (stderr, "%s: kvm_openfiles:  %s\n", 
			 argv [0], kvm_geterr());
		exit (1);
	}

	/* Figure out the offset of the various structures we'll need. */

	if (kvm_nlist (nl) == -1) {
		fprintf (stderr, "%s: kvm_nlist:  %s\n", 
			 argv [0], kvm_geterr());
		exit (1);
	}

	if (kvm_read (nl [VM_NSWAP].n_value, &nswap, sizeof (nswap)) !=
	    sizeof (nswap)) {
		fprintf (stderr, "%s:  didn't read all of nswap\n", 
			 argv [0]);
		exit (5);
	}

	if (kvm_read (nl [VM_NSWDEV].n_value, &nswdev, sizeof (nswdev)) !=
	    sizeof (nswdev)) {
		fprintf (stderr, "%s:  didn't read all of nswdev\n", 
			 argv [0]);
		exit (5);
	}

	if (kvm_read (nl [VM_DMMAX].n_value, &dmmax, sizeof (dmmax)) !=
	    sizeof (dmmax)) {
		fprintf (stderr, "%s:  didn't read all of dmmax\n", 
			 argv [0]);
		exit (5);
	}

	if ((swdevt = malloc (sizeof (struct swdevt) * nswdev)) == NULL ||
	    (by_device = calloc (sizeof (*by_device), nswdev)) == NULL) {
		perror ("malloc");
		exit (5);
	}

	if (kvm_read (nl [VM_SWDEVT].n_value, swdevt, 
		      sizeof (struct swdevt) * nswdev) !=
	    sizeof (struct swdevt) * nswdev) {
		fprintf (stderr, "%s:  didn't read all of swdevt\n", 
			 argv [0]);
		exit (5);
	}

	if (kvm_read (nl [0].n_value, &swapmap, sizeof (struct rlist *)) !=
	    sizeof (struct rlist *)) {
		fprintf (stderr, "%s:  didn't read all of swapmap\n", 
			 argv [0]);
		exit (5);
	}

	/* Traverse the list of free swap space... */

	total_free = 0;
    	while (swapmap) {
		int	top, bottom, next_block;

		if (kvm_read ((long) swapmap, &head, sizeof (struct rlist )) !=
		    sizeof (struct rlist )) {
			fprintf (stderr, "%s:  didn't read all of head\n", 
				 argv [0]);
			exit (5);
		}

		top = head.rl_end;
		bottom = head.rl_start;

		total_free += top - bottom + 1;

		/*
		 * Swap space is split up among the configured disk.
		 * The first dmmax blocks of swap space some from the
		 * first disk, the next dmmax blocks from the next, 
		 * and so on.  The list of free space joins adjacent
		 * free blocks, ignoring device boundries.  If we want
		 * to keep track of this information per device, we'll
		 * just have to extract it ourselves.
		 */

		while (top / dmmax != bottom / dmmax) {
			next_block = ((bottom + dmmax) / dmmax);
			by_device [(bottom / dmmax) % nswdev] +=
				next_block * dmmax - bottom;
			bottom = next_block * dmmax;
		}

		by_device [(bottom / dmmax) % nswdev] +=
			top - bottom + 1;

		swapmap = head.rl_next;
	}

	header = getbsize("swapinfo", &headerlen, &blocksize);
	printf ("%-10s %10s %10s %10s %10s\n",
		"Device", header, "Used", "Available", "Capacity");
	for (total_avail = total_partitions = i = 0; i < nswdev; i++) {
		printf ("/dev/%-5s %10d ",
			devname (swdevt [i].sw_dev, S_IFBLK), 
			swdevt [i].sw_nblks / (blocksize/512));

		/*
		 * Don't report statistics for partitions which have not
		 * yet been activated via swapon(8).
		 */

		if (!swdevt [i].sw_freed) {
			printf (" *** not available for swapping ***\n");
		} else {
			total_partitions++;
			total_avail += swdevt [i].sw_nblks;
			printf ("%10d %10d %7.0f%%\n", 
				(swdevt [i].sw_nblks - by_device [i]) / (blocksize/512),
				by_device [i] / (blocksize/512),
				(double) (swdevt [i].sw_nblks - 
					  by_device [i]) / 
				(double) swdevt [i].sw_nblks * 100.0);
		}
	}

	/* 
	 * If only one partition has been set up via swapon(8), we don't
	 * need to bother with totals.
	 */

	if (total_partitions > 1)
		printf ("%-10s %10d %10d %10d %7.0f%%\n", "Total", 
			total_avail / (blocksize/512), 
			(total_avail - total_free) / (blocksize/512), 
			total_free / (blocksize/512),
			(double) (total_avail - total_free) / 
			(double) total_avail * 100.0);

	exit (0);
}

void
usage()
{
	(void)fprintf(stderr, "usage: swapinfo [-k]\n");
	exit(1);
}


