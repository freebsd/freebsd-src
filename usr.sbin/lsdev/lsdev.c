#include <sys/types.h>
#include <sys/devconf.h>
#include <sys/sysctl.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lsdev.h"

const char *const devtypes[] = DEVTYPENAMES;
int vflag = 0;
const char *whoami;

static void usage(void);
static void badtype(const char *);
static void badname(const char *);

static void print_pretty(struct devconf *);
static void hprint_pretty(void);

int
main(int argc, char **argv)
{
	struct devconf *dc = 0;
	size_t size, osize;
	int ndevs, i;
	int mib[8];
	int c;
	int showonlytype = -1;
	int showonlydev = 0;
	char showonlydevclass[MAXDEVNAME];
	int showonlydevunit = -1;
	void (*prtfcn)(struct devconf *) = print_pretty;
	void (*hprtfcn)(void) = hprint_pretty;

	whoami = argv[0];

	while((c = getopt(argc, argv, "t:vc")) != EOF) {
		switch(c) {
		case 't':
			showonlytype = findtype(optarg);
			if(showonlytype < 0)
				badtype(optarg);
			break;
		case 'v':
			vflag++;
			break;
		case 'c':
			prtfcn = print_config;
			hprtfcn = hprint_config;
			break;
		default:
			usage();
			break;
		}
	}

	if(argc - optind > 1) {
		usage();
	}

	if(argv[optind]) {
		char *s = &argv[optind][strlen(argv[optind])];

		if(s - argv[optind] > MAXDEVNAME)
			badname(argv[optind]);
		s--;		/* step over null */
		while(s > argv[optind] && isdigit(*s))
			s--;
		s++;
		if(*s) {
			showonlydevunit = atoi(s);
			*s = '\0';
		} else {
			showonlydevunit = -1;
		}

		strcpy(showonlydevclass, argv[optind]);
		showonlydev = 1;
	}

	mib[0] = CTL_HW;
	mib[1] = HW_DEVCONF;
	mib[2] = DEVCONF_NUMBER;

	size = sizeof ndevs;
	if(sysctl(mib, 3, &ndevs, &size, 0, 0) < 0) {
		err(1, "sysctl(hw.devconf.number)");
	}
	osize = 0;

	hprtfcn();

	for(i = 1; i <= ndevs; i++) {
		mib[2] = i;
		if(sysctl(mib, 3, 0, &size, 0, 0) < 0) {
			/*
			 * Probably a deleted device; just go on to the next
			 * one.
			 */
			continue;
		}
		if(size > osize) {
			dc = realloc(dc, size);
			if(!dc) {
				err(2, "realloc(%lu)", (unsigned long)size);
			}
		}
		if(sysctl(mib, 3, dc, &size, 0, 0) < 0) {
			err(1, "sysctl(hw.devconf.%d)", i);
		}
		if(!showonlydev && showonlytype < 0) {
			prtfcn(dc);
		} else if(showonlydev) {
			if(!strcmp(showonlydevclass, dc->dc_name)
			   && (showonlydevunit < 0 ||
			       showonlydevunit == dc->dc_unit))
				prtfcn(dc);
		} else if(showonlytype == dc->dc_devtype) {
			prtfcn(dc);
		}
		osize = size;
	}
	return 0;
}

static void
usage(void)
{
	fprintf(stderr,
		"usage:\n"
		"\t%s [-t type] [-v] [name]\n",
		whoami);
	exit(-1);
}

int
findtype(const char *name)
{
	int i;
	for(i = 0; devtypes[i]; i++) {
		if(!strcmp(name, devtypes[i]))
			return i;
	}
	return -1;
}

static void
badtype(const char *name)
{
	int i;

	fprintf(stderr,
		"%s: invalid device type `%s'\n", whoami, name);
	fprintf(stderr,
		"%s: valid types are: ", whoami);
	for(i = 0; devtypes[i]; i++) {
		fprintf(stderr, "%s`%s'", i ? ", " : "", devtypes[i]);
	}
	fputs(".\n", stderr);
	exit(-1);
}

static void
badname(const char *name)
{
	errx(3, "invalid device name `%s'", name);
}

static void
hprint_pretty(void)
{
	printf("%-10.10s ", "Device");
	if(vflag) {
		printf("%-2.2s %-10.10s ", "St", "Parent");
	} else {
		printf("%-15.15s ", "State");
	}

	printf("%s\n", "Description");

	printf("%-10.10s ", "----------");
	if(vflag) {
		printf("%-2.2s %-10.10s ", "--", "----------");
	} else {
		printf("%-15.15s ", "---------------");
	}

	printf("%s\n", "--------------------------------------------------");
}

static const char *const states[] = { "??", "NC", "I", "B" };
static const char *const longstates[] = {
	"Unknown", "Unconfigured", "Idle", "Busy"
};

static void
print_pretty(struct devconf *dc)
{
	char buf[MAXDEVNAME * 2];

	snprintf(buf, sizeof buf, "%s%d", dc->dc_name, dc->dc_unit);

	if(vflag) {
		printf("%-10.10s %2.2s ", buf, states[dc->dc_state]);

		if(dc->dc_punit >= 0) {
			snprintf(buf, sizeof buf, "%s%d", dc->dc_pname,
				 dc->dc_punit);
		} else {
			buf[0] = '-';
			buf[1] = '\0';
		}
		printf("%-10.10s ", buf);
	} else {
		printf("%-10.10s %-15.15s ", buf, longstates[dc->dc_state]);
	}

	printf("%s\n", dc->dc_descr);
}

