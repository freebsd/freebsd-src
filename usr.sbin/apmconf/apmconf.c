/*
 * LP (Laptop Package)
 *
 * Copyright (C) 1994 by HOSOKAWA Tatasumi <hosokawa@mt.cs.keio.ac.jp>
 *
 * This software may be used, modified, copied, distributed, and sold,
 * in both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author 
 * responsible for the proper functioning of this software, nor does 
 * the author assume any responsibility for damages incurred with its 
 * use.
 *
 * Sep., 1994	Implemented on FreeBSD 1.1.5.1R (Toshiba AVS001WD)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <machine/apm_bios.h>

#define CONFIGFILE	"/etc/apm.conf"
#define APMDEV		"/dev/apm"

static const char	*config_file = CONFIGFILE;
static int		init = 0;
static int		flush = 0, verbose = 0;
static int		enable = 0, disable = 0;
static int		haltcpu = 0, nothaltcpu = 0;
static int		main_argc;
static char		**main_argv;

static void 
parse_option(void)
{
	int	i, option;
	char	*optarg;
	enum {OPT_NONE, OPT_INIT, OPT_CONFFILE, OPT_FLUSH, OPT_VERBOSE, 
		OPT_ENABLE, OPT_DISABLE, OPT_HALTCPU, OPT_NOTHALTCPU} mode;

	for (i = 1; i < main_argc; i++) {
		option = 0;
		mode = OPT_NONE;
		if (main_argv[i][0] == '-') {
			switch (main_argv[i][1]) {
			case 'f':
				mode = OPT_CONFFILE;
				option = 1;
				break;
			case 'i':
				mode = OPT_INIT;
				option = 0;
				break;
			case 'x':
				mode = OPT_FLUSH;
				option = 0;
				break;
			case 'v':
				mode = OPT_VERBOSE;
				option = 0;
				break;
			case 'e':
				mode = OPT_ENABLE;
				option = 0;
				break;
			case 'd':
				mode = OPT_DISABLE;
				option = 0;
				break;
			case 'h':
				mode = OPT_HALTCPU;
				option = 0;
				break;
			case 't':
				mode = OPT_NOTHALTCPU;
				option = 0;
				break;
			default:
				fprintf(stderr, "%s: Unknown option '%s.'\n", main_argv[0], main_argv[i]);
				exit(1);
			}
		}
		if (option) {
			if (i == main_argc - 1) {
				fprintf(stderr, "%s: Option '%s' needs arguments.\n", main_argv[0], main_argv[i]);
				exit(1);
			}
			optarg = main_argv[++i];
		}

		switch (mode) {
		case OPT_CONFFILE:
			config_file = optarg;
			break;
		case OPT_INIT:
			init = 1;
			break;
		case OPT_FLUSH:
			flush = 1;
			break;
		case OPT_VERBOSE:
			verbose = 1;
			break;
		case OPT_ENABLE:
			enable = 1;
			break;
		case OPT_DISABLE:
			disable = 1;
			break;
		case OPT_HALTCPU:
			haltcpu = 1;
			break;
		case OPT_NOTHALTCPU:
			nothaltcpu = 1;
			break;
		}
	}
}

static struct apm_eqv_event eqv_event[APM_MAX_EQUIV_EVENTS];
static int eqv_num = 0;

static apm_eqv_event_t
get_eqv_event(int id)
{
	int	i;

	for (i = 0; i < eqv_num; i++) {
		if (id == eqv_event[i].aee_event) {
			return &eqv_event[i];
		}
	}
	return NULL;
}

#define PMEV_SYMBOL(name)	{#name, name}
#define ARRAY_SIZEOF(array)	(sizeof(array) / sizeof(array[0]))

typedef struct pmev_symbol {
	char	*name;
	int	id;
} *pmev_symbol_t;

static struct pmev_symbol pmev_symbols[] = {
	PMEV_SYMBOL(PMEV_NOEVENT),
	PMEV_SYMBOL(PMEV_STANDBYREQ),
	PMEV_SYMBOL(PMEV_SUSPENDREQ),
	PMEV_SYMBOL(PMEV_NORMRESUME),
	PMEV_SYMBOL(PMEV_CRITRESUME),
	PMEV_SYMBOL(PMEV_BATTERYLOW),
	PMEV_SYMBOL(PMEV_POWERSTATECHANGE),
	PMEV_SYMBOL(PMEV_UPDATETIME),
	PMEV_SYMBOL(PMEV_CRITSUSPEND),
	PMEV_SYMBOL(PMEV_USERSTANDBYREQ),
	PMEV_SYMBOL(PMEV_USERSUSPENDREQ),
	PMEV_SYMBOL(PMEV_STANDBYRESUME),
};

static int 
pmev(char *name, int *id)
{
	int	i;

	for (i = 0; i < ARRAY_SIZEOF(pmev_symbols); i++) {
		if (strcasecmp(pmev_symbols[i].name, name) == 0) {
			*id = pmev_symbols[i].id;
			return 0;
		}
	}
	return 1;
}

static void 
parse_config_file(FILE *fp)
{
	int		i, id, id_eqv, n;
	char		*bufp, *lastp;
	char		buffer[1024];
	char		mode[64], event[64], equiv[64];
	apm_eqv_event_t	table;
	enum {MODE_DEFAULT, MODE_RESUME} mode_type;

	for (i = 1; fgets(buffer, sizeof(buffer), fp); i++) {
		if (*buffer == '#') {
			continue;
		}
		n = sscanf(buffer, "%s %s %s", mode, event, equiv);
		if (n != 3) {
			if (n == EOF) {
				continue; /* Empty line */
			}
			fprintf(stderr, "%s:%d: Syntax error\n", config_file,  i);
			exit(1);
		}
		if (pmev(event, &id)) {
			id = strtol(event, NULL, 0);
		}
		if (pmev(equiv, &id_eqv)) {
			id_eqv = strtol(equiv, NULL, 0);
		}
		if (!(table = get_eqv_event(id))) {
			if (eqv_num == APM_MAX_EQUIV_EVENTS - 1) {
				fprintf(stderr, "%s: Too many rules\n", main_argv[0]);
				exit(1);
			}
			table = &eqv_event[eqv_num++];
			table->aee_event = id;
			table->aee_equiv = PMEV_DEFAULT;
			table->aee_resume = PMEV_DEFAULT;
		}
		if (strcasecmp(mode, "default") == 0) {
			mode_type = MODE_DEFAULT;
		}
		else if (strcasecmp(mode, "resume") == 0) {
			mode_type = MODE_RESUME;
		}
		else {
			fprintf(stderr, "%s: %d : Unknown keyword '%s'\n", config_file, i, mode);
			exit(1);
		}
		switch (mode_type) {
		case MODE_DEFAULT:
			table->aee_equiv = table->aee_resume = id_eqv;
			break;
		case MODE_RESUME:
			table->aee_resume = id_eqv;
			break;
		}
	}
	if (verbose) {
		printf("%-14s%-14s%-14s\n", "   Original   ", "  Equivalent  ", " After Resume ");
		for (i = 0; i < eqv_num; i++) {
			printf("    0x%04x    ", eqv_event[i].aee_event);
			if (eqv_event[i].aee_equiv != PMEV_DEFAULT) {
				printf("    0x%04x    ", eqv_event[i].aee_equiv);
			}
			else {
				printf("  default    ");
			}
			if (eqv_event[i].aee_resume != PMEV_DEFAULT) {
				printf("    0x%04x    ", eqv_event[i].aee_resume);
			}
			else {
				printf("  default    ");
			}
			printf("\n");
		}
	}
}

static void
entry_eqv(int dh)
{
	int	i;

	for (i = 0; i < eqv_num; i++) {
		if (ioctl(dh, APMIO_DEFEQV, &eqv_event[i]) == -1) {
			fprintf(stderr, "%s: Can't ioctl APMIO_DEFEQV.\n", main_argv[0]);
			exit(1);
		}
	}
}

static void
flush_eqv(int dh)
{
	if (ioctl(dh, APMIO_FLUSHEQV, NULL) == -1) {
		fprintf(stderr, "%s: Can't ioctl APMIO_FLUSHBUF.\n", main_argv[0]);
		exit(1);
	}
}

static void
enable_apm(int dh)
{
	if (ioctl(dh, APMIO_ENABLE, NULL) == -1) {
		fprintf(stderr, "%s: Can't ioctl APMIO_ENABLE.\n", main_argv[0]);
		exit(1);
	}
}

static void
disable_apm(int dh)
{
	if (ioctl(dh, APMIO_DISABLE, NULL) == -1) {
		fprintf(stderr, "%s: Can't ioctl APMIO_DISABLE.\n", main_argv[0]);
		exit(1);
	}
}

static void
haltcpu_apm(int dh)
{
	if (ioctl(dh, APMIO_HALTCPU, NULL) == -1) {
		fprintf(stderr, "%s: Can't ioctl APMIO_HALTCPU.\n", main_argv[0]);
		exit(1);
	}
}

static void
nothaltcpu_apm(int dh)
{
	if (ioctl(dh, APMIO_NOTHALTCPU, NULL) == -1) {
		fprintf(stderr, "%s: Can't ioctl APMIO_NOTHALTCPU.\n", main_argv[0]);
		exit(1);
	}
}
int 
main(int argc, char *argv[]) 
{
	int		i, dh;
	FILE		*fp;

	main_argc = argc;
	main_argv = argv;
	if ((dh = open(APMDEV, O_RDWR)) == -1) {
		fprintf(stderr, "%s: Can't open '%s'\n", argv[0], APMDEV);
		exit(1);
	}
	parse_option();

	/* disable operation is executed first */
	if (disable) {
		disable_apm(dh);
	}
	if (init) {
		if (!(fp = fopen(config_file, "r"))) {
			fprintf(stderr, "%s: Can't open config file\n", CONFIGFILE);
			exit(1);
		}
		parse_config_file(fp);
		entry_eqv(dh);
	}
	if (flush) {
		flush_eqv(dh);
	}
	if (haltcpu) {
		haltcpu_apm(dh);
	}
	if (nothaltcpu) {
		nothaltcpu_apm(dh);
	}
	/* enable operation is executed last */
	if (enable) {
		enable_apm(dh);
	}
	return 0;
}
