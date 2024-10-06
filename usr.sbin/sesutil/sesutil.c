/*-
 * Copyright (c) 2019 Klara Inc.
 * Copyright (c) 2015 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2015 Allan Jude <allanjude@FreeBSD.org>
 * Copyright (c) 2000 by Matthew Jacob
 * All rights reserved.
 *
 * Portions of this software were developed by Edward Tomasz Napierala
 * under sponsorship from Klara Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <glob.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libxo/xo.h>

#include <cam/scsi/scsi_enc.h>

#include "eltsub.h"

#define SESUTIL_XO_VERSION	"1"

#define TEMPERATURE_OFFSET	20

#define PRINT_STYLE_DASHED	0
#define PRINT_STYLE_DASHED_2	1
#define PRINT_STYLE_CSV 	2
#define PRINT_STYLE_CSV_2	3

static int encstatus(int argc, char **argv);
static int fault(int argc, char **argv);
static int locate(int argc, char **argv);
static int objmap(int argc, char **argv);
static int sesled(int argc, char **argv, bool fault);
static int show(int argc, char **argv);
static void sesutil_print(int *style, const char *fmt, ...) __printflike(2,3);

static struct command {
	const char *name;
	const char *param;
	const char *desc;
	int (*exec)(int argc, char **argv);
} cmds[] = {
	{ "fault",
	    "(<disk>|<sesid>|all) (on|off)",
	    "Change the state of the fault LED associated with a disk",
	    fault },
	{ "locate",
	    "(<disk>|<sesid>|all) (on|off)",
	    "Change the state of the locate LED associated with a disk",
	    locate },
	{ "map", "",
	    "Print a map of the devices managed by the enclosure", objmap } ,
	{ "show", "",
	    "Print a human-friendly summary of the enclosure", show } ,
	{ "status", "", "Print the status of the enclosure",
	    encstatus },
};

static const int nbcmds = nitems(cmds);
static const char *uflag;

static void
usage(FILE *out, const char *subcmd)
{
	int i;

	if (subcmd == NULL) {
		fprintf(out, "Usage: %s [-u /dev/ses<N>] <command> [options]\n",
		    getprogname());
		fprintf(out, "Commands supported:\n");
	}
	for (i = 0; i < nbcmds; i++) {
		if (subcmd != NULL) {
			if (strcmp(subcmd, cmds[i].name) == 0) {
				fprintf(out, "Usage: %s %s [-u /dev/ses<N>] "
				    "%s\n\t%s\n", getprogname(), subcmd,
				    cmds[i].param, cmds[i].desc);
				break;
			}
			continue;
		}
		fprintf(out, "    %-12s%s\n\t\t%s\n\n", cmds[i].name,
		    cmds[i].param, cmds[i].desc);
	}

	exit(EXIT_FAILURE);
}

static void
do_led(int fd, unsigned int idx, elm_type_t type, bool onoff, bool setfault)
{
	int state = onoff ? 1 : 0;
	encioc_elm_status_t o;
	struct ses_ctrl_dev_slot *slot;

	o.elm_idx = idx;
	if (ioctl(fd, ENCIOC_GETELMSTAT, (caddr_t) &o) < 0) {
		close(fd);
		xo_err(EXIT_FAILURE, "ENCIOC_GETELMSTAT");
	}
	ses_status_to_ctrl(type, &o.cstat[0]);
	switch (type) {
	case ELMTYP_DEVICE:
	case ELMTYP_ARRAY_DEV:
		slot = (struct ses_ctrl_dev_slot *) &o.cstat[0];
		ses_ctrl_common_set_select(&slot->common, 1);
		if (setfault)
			ses_ctrl_dev_slot_set_rqst_fault(slot, state);
		else
			ses_ctrl_dev_slot_set_rqst_ident(slot, state);
		break;
	default:
		return;
	}
	if (ioctl(fd, ENCIOC_SETELMSTAT, (caddr_t) &o) < 0) {
		close(fd);
		xo_err(EXIT_FAILURE, "ENCIOC_SETELMSTAT");
	}
}

static bool
disk_match(const char *devnames, const char *disk, size_t len)
{
	const char *dname;

	dname = devnames;
	while ((dname = strstr(dname, disk)) != NULL) {
		if (dname[len] == '\0' || dname[len] == ',') {
			return (true);
		}
		dname++;
	}

	return (false);
}

static int
sesled(int argc, char **argv, bool setfault)
{
	encioc_elm_devnames_t objdn;
	encioc_element_t *objp;
	glob_t g;
	char *disk, *endptr;
	size_t len, i, ndisks;
	int fd;
	unsigned int nobj, j, sesid;
	bool all, isses, onoff;

	isses = false;
	all = false;
	onoff = false;

	if (argc != 3) {
		usage(stderr, (setfault ? "fault" : "locate"));
	}

	disk = argv[1];

	sesid = strtoul(disk, &endptr, 10);
	if (*endptr == '\0') {
		endptr = strrchr(uflag, '*');
		if (endptr != NULL && *endptr == '*') {
			xo_warnx("Must specifying a SES device (-u) to use a SES "
			    "id# to identify a disk");
			usage(stderr, (setfault ? "fault" : "locate"));
		}
		isses = true;
	}

	if (strcmp(argv[2], "on") == 0) {
		onoff = true;
	} else if (strcmp(argv[2], "off") == 0) {
		onoff = false;
	} else {
		usage(stderr, (setfault ? "fault" : "locate"));
	}

	if (strcmp(disk, "all") == 0) {
		all = true;
	}
	len = strlen(disk);

	/* Get the list of ses devices */
	if (glob((uflag != NULL ? uflag : "/dev/ses[0-9]*"), 0, NULL, &g) ==
	    GLOB_NOMATCH) {
		globfree(&g);
		xo_errx(EXIT_FAILURE, "No SES devices found");
	}

	ndisks = 0;
	for (i = 0; i < g.gl_pathc; i++) {
		/* ensure we only got numbers after ses */
		if (strspn(g.gl_pathv[i] + 8, "0123456789") !=
		    strlen(g.gl_pathv[i] + 8)) {
			continue;
		}
		if ((fd = open(g.gl_pathv[i], O_RDWR)) < 0) {
			/*
			 * Don't treat non-access errors as critical if we are
			 * accessing all devices
			 */
			if (errno == EACCES && g.gl_pathc > 1) {
				xo_err(EXIT_FAILURE, "unable to access SES device");
			}
			xo_warn("unable to access SES device: %s", g.gl_pathv[i]);
			continue;
		}

		if (ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj) < 0) {
			close(fd);
			xo_err(EXIT_FAILURE, "ENCIOC_GETNELM");
		}

		objp = calloc(nobj, sizeof(encioc_element_t));
		if (objp == NULL) {
			close(fd);
			xo_err(EXIT_FAILURE, "calloc()");
		}

		if (ioctl(fd, ENCIOC_GETELMMAP, (caddr_t) objp) < 0) {
			free(objp);
			close(fd);
			xo_err(EXIT_FAILURE, "ENCIOC_GETELMMAP");
		}

		if (isses) {
			if (sesid >= nobj) {
				free(objp);
				close(fd);
				xo_errx(EXIT_FAILURE,
				     "Requested SES ID does not exist");
			}
			do_led(fd, sesid, objp[sesid].elm_type, onoff, setfault);
			ndisks++;
			free(objp);
			close(fd);
			break;
		}
		for (j = 0; j < nobj; j++) {
			const int devnames_size = 128;
			char devnames[devnames_size];

			if (all) {
				encioc_elm_status_t es;
				memset(&es, 0, sizeof(es));
				es.elm_idx = objp[j].elm_idx;
				if (ioctl(fd, ENCIOC_GETELMSTAT, &es) < 0) {
					close(fd);
					xo_err(EXIT_FAILURE,
						"ENCIOC_GETELMSTAT");
				}
				if ((es.cstat[0] & 0xf) == SES_OBJSTAT_NOACCESS)
					continue;
				do_led(fd, objp[j].elm_idx, objp[j].elm_type,
				    onoff, setfault);
				continue;
			}
			memset(&objdn, 0, sizeof(objdn));
			memset(devnames, 0, devnames_size);
			objdn.elm_idx = objp[j].elm_idx;
			objdn.elm_names_size = devnames_size;
			objdn.elm_devnames = devnames;
			if (ioctl(fd, ENCIOC_GETELMDEVNAMES,
			    (caddr_t) &objdn) <0) {
				continue;
			}
			if (objdn.elm_names_len > 0) {
				if (disk_match(objdn.elm_devnames, disk, len)) {
					do_led(fd, objdn.elm_idx, objp[j].elm_type,
					    onoff, setfault);
					ndisks++;
					break;
				}
			}
		}
		free(objp);
		close(fd);
	}
	globfree(&g);
	if (ndisks == 0 && all == false) {
		xo_errx(EXIT_FAILURE, "Could not find the SES id of device '%s'",
		    disk);
	}

	return (EXIT_SUCCESS);
}

static int
locate(int argc, char **argv)
{

	return (sesled(argc, argv, false));
}

static int
fault(int argc, char **argv)
{

	return (sesled(argc, argv, true));
}

static void
sesutil_print(int *style, const char *fmt, ...)
{
	va_list args;

	if (*style == PRINT_STYLE_DASHED) {
		xo_open_container("extra_status");
		xo_emit("\t\tExtra status:\n");
		*style = PRINT_STYLE_DASHED_2;
	} else if (*style == PRINT_STYLE_CSV) {
		xo_open_container("extra_status");
		*style = PRINT_STYLE_CSV_2;
	}

	if (*style == PRINT_STYLE_DASHED_2)
		xo_emit("\t\t- ");
	else if (*style == PRINT_STYLE_CSV_2)
		xo_emit(", ");
	va_start(args, fmt);
	xo_emit_hv(NULL, fmt, args);
	va_end(args);
	if (*style == PRINT_STYLE_DASHED_2)
		xo_emit("\n");
}

static void
print_extra_status(int eletype, u_char *cstat, int style)
{

	if (cstat[0] & 0x40) {
		sesutil_print(&style, "{e:predicted_failure/true} Predicted Failure");
	}
	if (cstat[0] & 0x20) {
		sesutil_print(&style, "{e:disabled/true} Disabled");
	}
	if (cstat[0] & 0x10) {
		sesutil_print(&style, "{e:swapped/true} Swapped");
	}
	switch (eletype) {
	case ELMTYP_DEVICE:
	case ELMTYP_ARRAY_DEV:
		if (cstat[2] & 0x02) {
			sesutil_print(&style, "LED={q:led/locate}");
		}
		if (cstat[2] & 0x20) {
			sesutil_print(&style, "LED={q:led/fault}");
		}
		break;
	case ELMTYP_FAN:
		sesutil_print(&style, "Speed: {:speed/%d}{Uw:rpm}",
		    (((0x7 & cstat[1]) << 8) + cstat[2]) * 10);
		break;
	case ELMTYP_THERM:
		if (cstat[2]) {
			sesutil_print(&style, "Temperature: {:temperature/%d}{Uw:C}",
			    cstat[2] - TEMPERATURE_OFFSET);
		} else {
			sesutil_print(&style, "Temperature: -{q:temperature/reserved}");
		}
		break;
	case ELMTYP_VOM:
		sesutil_print(&style, "Voltage: {:voltage/%.2f}{Uw:V}",
		    be16dec(cstat + 2) / 100.0);
		break;
	}
	if (style) {
		xo_close_container("extra_status");
	}
}

static int
objmap(int argc, char **argv __unused)
{
	encioc_string_t stri;
	encioc_elm_devnames_t e_devname;
	encioc_elm_status_t e_status;
	encioc_elm_desc_t e_desc;
	encioc_element_t *e_ptr;
	glob_t g;
	int fd;
	unsigned int j, nobj;
	size_t i;
	char str[32];

	if (argc != 1) {
		usage(stderr, "map");
	}

	memset(&e_desc, 0, sizeof(e_desc));
	/* SES4r02 allows element descriptors of up to 65536 characters */
	e_desc.elm_desc_str = calloc(UINT16_MAX, sizeof(char));
	if (e_desc.elm_desc_str == NULL)
		xo_err(EXIT_FAILURE, "calloc()");

	e_devname.elm_devnames = calloc(128, sizeof(char));
	if (e_devname.elm_devnames == NULL)
		xo_err(EXIT_FAILURE, "calloc()");
	e_devname.elm_names_size = 128;

	/* Get the list of ses devices */
	if (glob(uflag, 0, NULL, &g) == GLOB_NOMATCH) {
		globfree(&g);
		xo_errx(EXIT_FAILURE, "No SES devices found");
	}
	xo_set_version(SESUTIL_XO_VERSION);
	xo_open_container("sesutil");
	xo_open_list("enclosures");
	for (i = 0; i < g.gl_pathc; i++) {
		/* ensure we only got numbers after ses */
		if (strspn(g.gl_pathv[i] + 8, "0123456789") !=
		    strlen(g.gl_pathv[i] + 8)) {
			continue;
		}
		if ((fd = open(g.gl_pathv[i], O_RDWR)) < 0) {
			/*
			 * Don't treat non-access errors as critical if we are
			 * accessing all devices
			 */
			if (errno == EACCES && g.gl_pathc > 1) {
				xo_err(EXIT_FAILURE, "unable to access SES device");
			}
			xo_warn("unable to access SES device: %s", g.gl_pathv[i]);
			continue;
		}

		if (ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj) < 0) {
			close(fd);
			xo_err(EXIT_FAILURE, "ENCIOC_GETNELM");
		}

		e_ptr = calloc(nobj, sizeof(encioc_element_t));
		if (e_ptr == NULL) {
			close(fd);
			xo_err(EXIT_FAILURE, "calloc()");
		}

		if (ioctl(fd, ENCIOC_GETELMMAP, (caddr_t) e_ptr) < 0) {
			close(fd);
			xo_err(EXIT_FAILURE, "ENCIOC_GETELMMAP");
		}

		xo_open_instance("enclosures");
		xo_emit("{t:enc/%s}:\n", g.gl_pathv[i] + 5);
		stri.bufsiz = sizeof(str);
		stri.buf = &str[0];
		if (ioctl(fd, ENCIOC_GETENCNAME, (caddr_t) &stri) == 0)
			xo_emit("\tEnclosure Name: {t:name/%s}\n", stri.buf);
		stri.bufsiz = sizeof(str);
		stri.buf = &str[0];
		if (ioctl(fd, ENCIOC_GETENCID, (caddr_t) &stri) == 0)
			xo_emit("\tEnclosure ID: {t:id/%s}\n", stri.buf);

		xo_open_list("elements");
		for (j = 0; j < nobj; j++) {
			/* Get the status of the element */
			memset(&e_status, 0, sizeof(e_status));
			e_status.elm_idx = e_ptr[j].elm_idx;
			if (ioctl(fd, ENCIOC_GETELMSTAT,
			    (caddr_t) &e_status) < 0) {
				close(fd);
				xo_err(EXIT_FAILURE, "ENCIOC_GETELMSTAT");
			}
			/* Get the description of the element */
			e_desc.elm_idx = e_ptr[j].elm_idx;
			e_desc.elm_desc_len = UINT16_MAX;
			if (ioctl(fd, ENCIOC_GETELMDESC,
			    (caddr_t) &e_desc) < 0) {
				close(fd);
				xo_err(EXIT_FAILURE, "ENCIOC_GETELMDESC");
			}
			e_desc.elm_desc_str[e_desc.elm_desc_len] = '\0';
			/* Get the device name(s) of the element */
			e_devname.elm_idx = e_ptr[j].elm_idx;
			if (ioctl(fd, ENCIOC_GETELMDEVNAMES,
			    (caddr_t) &e_devname) <0) {
				/* Continue even if we can't look up devnames */
				e_devname.elm_devnames[0] = '\0';
			}
			xo_open_instance("elements");
			xo_emit("\tElement {:id/%u}, Type: {:type/%s}\n", e_ptr[j].elm_idx,
			    geteltnm(e_ptr[j].elm_type));
			xo_emit("\t\tStatus: {:status/%s} ({q:status_code/0x%02x 0x%02x 0x%02x 0x%02x})\n",
			    scode2ascii(e_status.cstat[0]), e_status.cstat[0],
			    e_status.cstat[1], e_status.cstat[2],
			    e_status.cstat[3]);
			if (e_desc.elm_desc_len > 0) {
				xo_emit("\t\tDescription: {:description/%s}\n",
				    e_desc.elm_desc_str);
			}
			if (e_devname.elm_names_len > 0) {
				xo_emit("\t\tDevice Names: {:device_names/%s}\n",
				    e_devname.elm_devnames);
			}
			print_extra_status(e_ptr[j].elm_type, e_status.cstat, PRINT_STYLE_DASHED);
			xo_close_instance("elements");
		}
		xo_close_list("elements");
		free(e_ptr);
		close(fd);
	}
	globfree(&g);
	free(e_devname.elm_devnames);
	free(e_desc.elm_desc_str);
	xo_close_list("enclosures");
	xo_close_container("sesutil");
	xo_finish();

	return (EXIT_SUCCESS);
}

/*
 * Get rid of the 'passN' devices, unless there's nothing else to show.
 */
static void
skip_pass_devices(char *devnames, size_t devnameslen)
{
	char *dev, devs[128], passes[128], *tmp;

	devs[0] = passes[0] = '\0';
	tmp = devnames;

	while ((dev = strsep(&tmp, ",")) != NULL) {
		if (strncmp(dev, "pass", 4) == 0) {
			if (passes[0] != '\0')
				strlcat(passes, ",", sizeof(passes));
			strlcat(passes, dev, sizeof(passes));
		} else {
			if (devs[0] != '\0')
				strlcat(devs, ",", sizeof(devs));
			strlcat(devs, dev, sizeof(devs));
		}
	}
	strlcpy(devnames, devs, devnameslen);
	if (devnames[0] == '\0')
		strlcpy(devnames, passes, devnameslen);
}

static void
fetch_device_details(char *devnames, char **model, char **serial, off_t *size)
{
	char ident[DISK_IDENT_SIZE];
	struct diocgattr_arg arg;
	char *tmp;
	off_t mediasize;
	int comma;
	int fd;

	comma = (int)strcspn(devnames, ",");
	asprintf(&tmp, "/dev/%.*s", comma, devnames);
	if (tmp == NULL)
		err(1, "asprintf");
	fd = open(tmp, O_RDONLY);
	free(tmp);
	if (fd < 0) {
		/*
		 * This can happen with a disk so broken it cannot
		 * be probed by GEOM.
		 */
		*model = strdup("?");
		*serial = strdup("?");
		*size = -1;
		close(fd);
		return;
	}

	strlcpy(arg.name, "GEOM::descr", sizeof(arg.name));
	arg.len = sizeof(arg.value.str);
	if (ioctl(fd, DIOCGATTR, &arg) == 0)
		*model = strdup(arg.value.str);
	else
		*model = NULL;

	if (ioctl(fd, DIOCGIDENT, ident) == 0)
		*serial = strdup(ident);
	else
		*serial = NULL;

	if (ioctl(fd, DIOCGMEDIASIZE, &mediasize) == 0)
		*size = mediasize;
	else
		*size = -1;
	close(fd);
}

static void
show_device(int fd, int elm_idx, encioc_elm_status_t e_status, encioc_elm_desc_t e_desc)
{
	encioc_elm_devnames_t e_devname;
	char *model = NULL, *serial = NULL;
	off_t size;

	/* Get the device name(s) of the element */
	memset(&e_devname, 0, sizeof(e_devname));
	e_devname.elm_idx = elm_idx;
	e_devname.elm_names_size = 128;
	e_devname.elm_devnames = calloc(128, sizeof(char));
	if (e_devname.elm_devnames == NULL) {
		close(fd);
		xo_err(EXIT_FAILURE, "calloc()");
	}

	if (ioctl(fd, ENCIOC_GETELMDEVNAMES,
	    (caddr_t) &e_devname) < 0) {
		/* We don't care if this fails */
		e_devname.elm_devnames[0] = '\0';
		size = -1;
	} else {
		skip_pass_devices(e_devname.elm_devnames, 128);
		fetch_device_details(e_devname.elm_devnames, &model, &serial, &size);
	}
	xo_open_instance("elements");
	xo_emit("{e:type/device_slot}");
	xo_emit("{d:description/%-15s} ", e_desc.elm_desc_len > 0 ? e_desc.elm_desc_str : "-");
	xo_emit("{e:description/%-15s}", e_desc.elm_desc_len > 0 ? e_desc.elm_desc_str : "");
	xo_emit("{d:device_names/%-7s} ", e_devname.elm_names_len > 0 ? e_devname.elm_devnames : "-");
	xo_emit("{e:device_names/%s}", e_devname.elm_names_len > 0 ? e_devname.elm_devnames : "");
	xo_emit("{d:model/%-25s} ", model ? model : "-");
	xo_emit("{e:model/%s}", model ? model : "");
	xo_emit("{d:serial/%-20s} ", serial != NULL ? serial : "-");
	xo_emit("{e:serial/%s}", serial != NULL ? serial : "");
	if ((e_status.cstat[0] & 0xf) == SES_OBJSTAT_OK && size >= 0) {
		xo_emit("{h,hn-1000:size/%ld}{e:status/%s}",
		    size, scode2ascii(e_status.cstat[0]));
	} else {
		xo_emit("{:status/%s}", scode2ascii(e_status.cstat[0]));
	}
	print_extra_status(ELMTYP_ARRAY_DEV, e_status.cstat, PRINT_STYLE_CSV);
	xo_emit("\n");
	xo_close_instance("elements");
	free(serial);
	free(model);
	free(e_devname.elm_devnames);
}

static void
show_therm(encioc_elm_status_t e_status, encioc_elm_desc_t e_desc)
{

	if (e_desc.elm_desc_len <= 0) {
		/* We don't have a label to display; might as well skip it. */
		return;
	}

	if (e_status.cstat[2] == 0) {
		/* No temperature to show. */
		return;
	}

	xo_open_instance("elements");
	xo_emit("{e:type/temperature_sensor}");
	xo_emit("{:description/%s}: {:temperature/%d}{Uw:C}",
	    e_desc.elm_desc_str, e_status.cstat[2] - TEMPERATURE_OFFSET);
	xo_close_instance("elements");
}

static void
show_vom(encioc_elm_status_t e_status, encioc_elm_desc_t e_desc)
{

	if (e_desc.elm_desc_len <= 0) {
		/* We don't have a label to display; might as well skip it. */
		return;
	}

	if (e_status.cstat[2] == 0) {
		/* No voltage to show. */
		return;
	}

	xo_open_instance("elements");
	xo_emit("{e:type/voltage_sensor}");
	xo_emit("{:description/%s}: {:voltage/%.2f}{Uw:V}",
	    e_desc.elm_desc_str, be16dec(e_status.cstat + 2) / 100.0);
	xo_close_instance("elements");
}

static int
show(int argc, char **argv __unused)
{
	encioc_string_t stri;
	encioc_elm_status_t e_status;
	encioc_elm_desc_t e_desc;
	encioc_element_t *e_ptr;
	glob_t g;
	elm_type_t prev_type;
	int fd;
	unsigned int j, nobj;
	size_t i;
	bool first_ses;
	char str[32];

	if (argc != 1) {
		usage(stderr, "map");
	}

	first_ses = true;

	e_desc.elm_desc_str = calloc(UINT16_MAX, sizeof(char));
	if (e_desc.elm_desc_str == NULL)
		xo_err(EXIT_FAILURE, "calloc()");

	/* Get the list of ses devices */
	if (glob(uflag, 0, NULL, &g) == GLOB_NOMATCH) {
		globfree(&g);
		xo_errx(EXIT_FAILURE, "No SES devices found");
	}
	xo_set_version(SESUTIL_XO_VERSION);
	xo_open_container("sesutil");
	xo_open_list("enclosures");
	for (i = 0; i < g.gl_pathc; i++) {
		/* ensure we only got numbers after ses */
		if (strspn(g.gl_pathv[i] + 8, "0123456789") !=
		    strlen(g.gl_pathv[i] + 8)) {
			continue;
		}
		if ((fd = open(g.gl_pathv[i], O_RDWR)) < 0) {
			/*
			 * Don't treat non-access errors as critical if we are
			 * accessing all devices
			 */
			if (errno == EACCES && g.gl_pathc > 1) {
				xo_err(EXIT_FAILURE, "unable to access SES device");
			}
			xo_warn("unable to access SES device: %s", g.gl_pathv[i]);
			continue;
		}

		if (ioctl(fd, ENCIOC_GETNELM, (caddr_t) &nobj) < 0) {
			close(fd);
			xo_err(EXIT_FAILURE, "ENCIOC_GETNELM");
		}

		e_ptr = calloc(nobj, sizeof(encioc_element_t));
		if (e_ptr == NULL) {
			close(fd);
			xo_err(EXIT_FAILURE, "calloc()");
		}

		if (ioctl(fd, ENCIOC_GETELMMAP, (caddr_t) e_ptr) < 0) {
			close(fd);
			xo_err(EXIT_FAILURE, "ENCIOC_GETELMMAP");
		}

		xo_open_instance("enclosures");

		if (first_ses)
			first_ses = false;
		else
			xo_emit("\n");

		xo_emit("{t:enc/%s}: ", g.gl_pathv[i] + 5);
		stri.bufsiz = sizeof(str);
		stri.buf = &str[0];
		if (ioctl(fd, ENCIOC_GETENCNAME, (caddr_t) &stri) == 0)
			xo_emit("<{t:name/%s}>; ", stri.buf);
		stri.bufsiz = sizeof(str);
		stri.buf = &str[0];
		if (ioctl(fd, ENCIOC_GETENCID, (caddr_t) &stri) == 0)
			xo_emit("ID: {t:id/%s}", stri.buf);
		xo_emit("\n");

		xo_open_list("elements");
		prev_type = -1;
		for (j = 0; j < nobj; j++) {
			/* Get the status of the element */
			memset(&e_status, 0, sizeof(e_status));
			e_status.elm_idx = e_ptr[j].elm_idx;
			if (ioctl(fd, ENCIOC_GETELMSTAT,
			    (caddr_t) &e_status) < 0) {
				close(fd);
				xo_err(EXIT_FAILURE, "ENCIOC_GETELMSTAT");
			}

			/*
			 * Skip "Unsupported" elements; those usually precede
			 * the actual device entries and are not particularly
			 * interesting.
			 */
			if (e_status.cstat[0] == SES_OBJSTAT_UNSUPPORTED)
				continue;

			/* Get the description of the element */
			e_desc.elm_idx = e_ptr[j].elm_idx;
			e_desc.elm_desc_len = UINT16_MAX;
			if (ioctl(fd, ENCIOC_GETELMDESC,
			    (caddr_t) &e_desc) < 0) {
				close(fd);
				xo_err(EXIT_FAILURE, "ENCIOC_GETELMDESC");
			}
			e_desc.elm_desc_str[e_desc.elm_desc_len] = '\0';

			switch (e_ptr[j].elm_type) {
			case ELMTYP_DEVICE:
			case ELMTYP_ARRAY_DEV:
				if (e_ptr[j].elm_type != prev_type)
					xo_emit("Desc            Dev     Model                     Ident                Size/Status\n");

				show_device(fd, e_ptr[j].elm_idx, e_status, e_desc);
				prev_type = e_ptr[j].elm_type;
				break;
			case ELMTYP_THERM:
				if (e_ptr[j].elm_type != prev_type)
					xo_emit("\nTemperatures: ");
				else
					xo_emit(", ");
				prev_type = e_ptr[j].elm_type;
				show_therm(e_status, e_desc);
				break;
			case ELMTYP_VOM:
				if (e_ptr[j].elm_type != prev_type)
					xo_emit("\nVoltages: ");
				else
					xo_emit(", ");
				prev_type = e_ptr[j].elm_type;
				show_vom(e_status, e_desc);
				break;
			default:
				/*
				 * Ignore stuff not interesting to the user.
				 */
				break;
			}
		}
		if (prev_type != (elm_type_t)-1 &&
		    prev_type != ELMTYP_DEVICE && prev_type != ELMTYP_ARRAY_DEV)
			xo_emit("\n");
		xo_close_list("elements");
		free(e_ptr);
		close(fd);
	}
	globfree(&g);
	free(e_desc.elm_desc_str);
	xo_close_list("enclosures");
	xo_close_container("sesutil");
	xo_finish();

	return (EXIT_SUCCESS);
}

static int
encstatus(int argc, char **argv __unused)
{
	glob_t g;
	int fd, status;
	size_t i, e;
	u_char estat;

	status = 0;
	if (argc != 1) {
		usage(stderr, "status");
	}

	/* Get the list of ses devices */
	if (glob(uflag, 0, NULL, &g) == GLOB_NOMATCH) {
		globfree(&g);
		xo_errx(EXIT_FAILURE, "No SES devices found");
	}

	xo_set_version(SESUTIL_XO_VERSION);
	xo_open_container("sesutil");
	xo_open_list("enclosures");
	for (i = 0; i < g.gl_pathc; i++) {
		/* ensure we only got numbers after ses */
		if (strspn(g.gl_pathv[i] + 8, "0123456789") !=
		    strlen(g.gl_pathv[i] + 8)) {
			continue;
		}
		if ((fd = open(g.gl_pathv[i], O_RDWR)) < 0) {
			/*
			 * Don't treat non-access errors as critical if we are
			 * accessing all devices
			 */
			if (errno == EACCES && g.gl_pathc > 1) {
				xo_err(EXIT_FAILURE, "unable to access SES device");
			}
			xo_warn("unable to access SES device: %s", g.gl_pathv[i]);
			continue;
		}

		if (ioctl(fd, ENCIOC_GETENCSTAT, (caddr_t) &estat) < 0) {
			xo_err(EXIT_FAILURE, "ENCIOC_GETENCSTAT");
			close(fd);
		}

		xo_open_instance("enclosures");
		xo_emit("{:enc/%s}: ", g.gl_pathv[i] + 5);
		e = 0;
		if (estat == 0) {
			if (status == 0) {
				status = 1;
			}
			xo_emit("{q:status/OK}");
		} else {
			if (estat & SES_ENCSTAT_INFO) {
				xo_emit("{lq:status/INFO}");
				e++;
			}
			if (estat & SES_ENCSTAT_NONCRITICAL) {
				if (e)
					xo_emit(",");
				xo_emit("{lq:status/NONCRITICAL}");
				e++;
			}
			if (estat & SES_ENCSTAT_CRITICAL) {
				if (e)
					xo_emit(",");
				xo_emit("{lq:status/CRITICAL}");
				e++;
				status = -1;
			}
			if (estat & SES_ENCSTAT_UNRECOV) {
				if (e)
					xo_emit(",");
				xo_emit("{lq:status/UNRECOV}");
				e++;
				status = -1;
			}
		}
		xo_close_instance("enclosures");
		xo_emit("\n");
		close(fd);
	}
	globfree(&g);

	xo_close_list("enclosures");
	xo_close_container("sesutil");
	xo_finish();

	if (status == 1) {
		return (EXIT_SUCCESS);
	} else {
		return (EXIT_FAILURE);
	}
}

int
main(int argc, char **argv)
{
	int i, ch;
	struct command *cmd = NULL;

	argc = xo_parse_args(argc, argv);
	if (argc < 0)
		exit(1);

	uflag = "/dev/ses[0-9]*";
	while ((ch = getopt_long(argc, argv, "u:", NULL, NULL)) != -1) {
		switch (ch) {
		case 'u':
			uflag = optarg;
			break;
		case '?':
		default:
			usage(stderr, NULL);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		warnx("Missing command");
		usage(stderr, NULL);
	}

	for (i = 0; i < nbcmds; i++) {
		if (strcmp(argv[0], cmds[i].name) == 0) {
			cmd = &cmds[i];
			break;
		}
	}

	if (cmd == NULL) {
		warnx("unknown command %s", argv[0]);
		usage(stderr, NULL);
	}

	return (cmd->exec(argc, argv));
}
