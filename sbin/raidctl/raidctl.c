/*-
 * Copyright (c) 2002 Scott Long <scottl@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

/*      $NetBSD: raidctl.c,v 1.25 2000/10/31 14:18:39 lukem Exp $   */
/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* 
 * This program is a re-write of the original rf_ctrl program 
 * distributed by CMU with RAIDframe 1.1.
 *
 * This program is the user-land interface to the RAIDframe kernel
 * driver in NetBSD.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#if defined(__FreeBSD__)
#include <sys/linker.h>
#include <sys/module.h>
#endif

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __FreeBSD__
#include <paths.h>
#endif
#if defined(__NetBSD__)
#include <util.h>
#endif

#include <dev/raidframe/rf_raidframe.h>

int     main(int, char *[]);
void	do_ioctl(int, u_long, void *, const char *);
static  void rf_configure(int, char*, int);
static  const char *device_status(RF_DiskStatus_t);
static  void rf_get_device_status(int);
static  void get_component_number(int, char *, int *, int *);
static  void rf_fail_disk(int, char *, int);
static  void usage(void);
static  void get_component_label(int, char *);
static  void set_component_label(int, char *);
static  void init_component_labels(int, int);
static  void set_autoconfig(int, char *, char *);
static  void add_hot_spare(int, char *);
static  void remove_hot_spare(int, char *);
static  void rebuild_in_place(int, char *);
static  void check_status(int,int);
static  void check_parity(int,int, char *);
static  void do_meter(int, u_long);
static  void get_bar(char *, double, int);
static  void get_time_string(char *, int);
#if defined(__FreeBSD__)
static	void check_driver(void);

extern char *__progname;
#define PROGNAME __progname

#define RAIDCTLDEV "/dev/raidctl"
#elif defined(__NetBSD__)
#define PROGNAME getprogname()
#endif

int verbose;

int
main(argc,argv)
	int argc;
	char *argv[];
{
	int ch;
	int num_options;
	unsigned long action;
	char config_filename[PATH_MAX];
	char dev_name[PATH_MAX];
	char name[PATH_MAX];
	char component[PATH_MAX];
	char autoconf[10];
	int do_recon;
	int do_rewrite;
	int is_clean;
	int serial_number;
	struct stat st;
	int fd;
	int force;
	int raidID;

	num_options = 0;
	action = 0;
	do_recon = 0;
	do_rewrite = 0;
	is_clean = 0;
	force = 0;

	while ((ch = getopt(argc, argv, "a:A:Bc:C:f:F:g:iI:l:r:R:sSpPuv")) 
	       != -1)
		switch(ch) {
		case 'a':
			action = RAIDFRAME_ADD_HOT_SPARE;
			strncpy(component, optarg, PATH_MAX);
			num_options++;
			break;
		case 'A':
			action = RAIDFRAME_SET_AUTOCONFIG;
			strncpy(autoconf, optarg, 10);
			num_options++;
			break;
		case 'B':
			action = RAIDFRAME_COPYBACK;
			num_options++;
			break;
		case 'c':
		case 'C':
			strncpy(config_filename,optarg,PATH_MAX);
			action = RAIDFRAME_CONFIGURE;
			force = (ch == 'c') ? 0 : 1;
#if defined(__FreeBSD__)
			check_driver();
			fd = open(RAIDCTLDEV, O_RDWR);
			if (fd < 0) {
				fprintf(stderr, "%s: unable to open raid "
				    "control device %s\n", PROGNAME,
				    RAIDCTLDEV);
				fprintf(stderr, "Error: %s\n", strerror(errno));
				exit(1);
			}
			rf_configure(fd, config_filename, force);
			close(fd);
			exit(0);
#elif defined(__NetBSD__)
			num_options++;
			break;
#endif
		case 'f':
			action = RAIDFRAME_FAIL_DISK;
			strncpy(component, optarg, PATH_MAX);
			do_recon = 0;
			num_options++;
			break;
		case 'F':
			action = RAIDFRAME_FAIL_DISK;
			strncpy(component, optarg, PATH_MAX);
			do_recon = 1;
			num_options++;
			break;
		case 'g':
			action = RAIDFRAME_GET_COMPONENT_LABEL;
			strncpy(component, optarg, PATH_MAX);
			num_options++;
			break;
		case 'i':
			action = RAIDFRAME_REWRITEPARITY;
			num_options++;
			break;
		case 'I':
			action = RAIDFRAME_INIT_LABELS;
			serial_number = atoi(optarg);
			num_options++;
			break;
		case 'l': 
			action = RAIDFRAME_SET_COMPONENT_LABEL;
			strncpy(component, optarg, PATH_MAX);
			num_options++;
			break;
		case 'r':
			action = RAIDFRAME_REMOVE_HOT_SPARE;
			strncpy(component, optarg, PATH_MAX);
			num_options++;
			break;
		case 'R':
			strncpy(component,optarg,PATH_MAX);
			action = RAIDFRAME_REBUILD_IN_PLACE;
			num_options++;
			break;
		case 's':
			action = RAIDFRAME_GET_INFO;
			num_options++;
			break;
		case 'S':
			action = RAIDFRAME_CHECK_RECON_STATUS_EXT;
			num_options++;
			break;
		case 'p':
			action = RAIDFRAME_CHECK_PARITY;
			num_options++;
			break;
		case 'P':
			action = RAIDFRAME_CHECK_PARITY;
			do_rewrite = 1;
			num_options++;
			break;
		case 'u':
			action = RAIDFRAME_SHUTDOWN;
			num_options++;
			break;
		case 'v':
			verbose = 1;
			/* Don't bump num_options, as '-v' is not 
			   an option like the others */
			/* num_options++; */
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if ((num_options > 1) || (argc == NULL)) 
		usage();

	strncpy(name,argv[0],PATH_MAX);
#if defined(__NetBSD__)
	fd = opendisk(name, O_RDWR, dev_name, sizeof(dev_name), 1);
#elif defined(__FreeBSD__)
	check_driver();

	if (name[0] != '/') {
		char name1[PATH_MAX];
		snprintf(name1, PATH_MAX, "%s%s", _PATH_DEV, name);
		strncpy(name, name1, PATH_MAX);
	}
	fd = open(name, O_RDWR);
#endif
	if (fd == -1) {
		fprintf(stderr, "%s: unable to open device file: %s\n",
			PROGNAME, name);
		exit(1);
	}
	if (fstat(fd, &st) != 0) {
		fprintf(stderr,"%s: stat failure on: %s\n",
			PROGNAME, dev_name);
		exit(1);
	}
	if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode)) {
		fprintf(stderr,"%s: invalid device: %s\n",
			PROGNAME, dev_name);
		exit(1);
	}

	switch(action) {
	case RAIDFRAME_ADD_HOT_SPARE:
		add_hot_spare(fd, component);
		break;
	case RAIDFRAME_REMOVE_HOT_SPARE:
		remove_hot_spare(fd, component);
		break;
#if defined(__NetBSD__)
	case RAIDFRAME_CONFIGURE:
		rf_configure(fd, config_filename, force);
		break;
#endif
	case RAIDFRAME_SET_AUTOCONFIG:
		set_autoconfig(fd, name, autoconf);
		break;
	case RAIDFRAME_COPYBACK:
		printf("Copyback.\n");
		do_ioctl(fd, RAIDFRAME_COPYBACK, NULL, "RAIDFRAME_COPYBACK");
		if (verbose) {
			sleep(3); /* XXX give the copyback a chance to start */
			printf("Copyback status:\n");
			do_meter(fd,RAIDFRAME_CHECK_COPYBACK_STATUS_EXT);
		}
		break;
	case RAIDFRAME_FAIL_DISK:
		rf_fail_disk(fd, component, do_recon);
		break;
	case RAIDFRAME_SET_COMPONENT_LABEL:
		set_component_label(fd, component);
		break;
	case RAIDFRAME_GET_COMPONENT_LABEL:
		get_component_label(fd, component);
		break;
	case RAIDFRAME_INIT_LABELS:
		init_component_labels(fd, serial_number);
		break;
	case RAIDFRAME_REWRITEPARITY:
		printf("Initiating re-write of parity\n");
		do_ioctl(fd, RAIDFRAME_REWRITEPARITY, NULL, 
			 "RAIDFRAME_REWRITEPARITY");
		if (verbose) {
			sleep(3); /* XXX give it time to get started */
			printf("Parity Re-write status:\n");
			do_meter(fd, RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT);
		}
		break;
	case RAIDFRAME_CHECK_RECON_STATUS_EXT:
		check_status(fd,1);
		break;
	case RAIDFRAME_GET_INFO:
		rf_get_device_status(fd);
		break;
	case RAIDFRAME_REBUILD_IN_PLACE:
		rebuild_in_place(fd, component);
		break;
	case RAIDFRAME_CHECK_PARITY:
		check_parity(fd, do_rewrite, dev_name);
		break;
	case RAIDFRAME_SHUTDOWN:
#if defined(__NetBSD__)
                do_ioctl(fd, RAIDFRAME_SHUTDOWN, NULL, "RAIDFRAME_SHUTDOWN");
#elif defined(__FreeBSD__)
		/* Find out the unit number of the raid device */
		do_ioctl(fd, RAIDFRAME_GET_UNIT, &raidID, "RAIDFRAME_GET_UNIT");
		close (fd);

		fd = open(RAIDCTLDEV, O_RDWR);
		if (fd < 0) {
			fprintf(stderr, "%s: unable to open raid control "
				"device %s\n", PROGNAME, RAIDCTLDEV);
			fprintf(stderr, "Error: %s\n", strerror(errno));
			exit(1);
		}
		do_ioctl(fd, RAIDFRAME_SHUTDOWN, &raidID, "RAIDFRAME_SHUTDOWN");
		close(fd);
#endif
		break;
	default:
		break;
	}

	close(fd);
	exit(0);
}

void
do_ioctl(fd, command, arg, ioctl_name)
	int fd;
	unsigned long command;
	void *arg;
	const char *ioctl_name;
{
	if (ioctl(fd, command, arg) < 0) {
		warn("ioctl (%s) failed", ioctl_name);
		exit(1);
	}
}


static void
rf_configure(fd,config_file,force)
	int fd;
	char *config_file;
	int force;
{
	void *generic;
	RF_Config_t cfg;

	if (rf_MakeConfig( config_file, &cfg ) != 0) {
		fprintf(stderr,"%s: unable to create RAIDframe %s\n",
			PROGNAME, "configuration structure\n");
		exit(1);
	}
	
	cfg.force = force;

	/* 
	 * Note the extra level of redirection needed here, since
	 * what we really want to pass in is a pointer to the pointer to 
	 * the configuration structure. 
	 */

	generic = (void *) &cfg;
	do_ioctl(fd, RAIDFRAME_CONFIGURE, &generic, "RAIDFRAME_CONFIGURE");
}

static const char *
device_status(status)
	RF_DiskStatus_t status;
{

	switch (status) {
	case rf_ds_optimal:
		return ("optimal");
		break;
	case rf_ds_failed:
		return ("failed");
		break;
	case rf_ds_reconstructing:
		return ("reconstructing");
		break;
	case rf_ds_dist_spared:
		return ("dist_spared");
		break;
	case rf_ds_spared:
		return ("spared");
		break;
	case rf_ds_spare:
		return ("spare");
		break;
	case rf_ds_used_spare:
		return ("used_spare");
		break;
	default:
		return ("UNKNOWN");
	}
	/* NOTREACHED */
}

static void
rf_get_device_status(fd)
	int fd;
{
	RF_DeviceConfig_t device_config;
	void *cfg_ptr;
	int is_clean;
	int i;

	cfg_ptr = &device_config;
	printf("Address= %p\n", &cfg_ptr);
	do_ioctl(fd, RAIDFRAME_GET_INFO, &cfg_ptr, "RAIDFRAME_GET_INFO");

	printf("Components:\n");
	for(i=0; i < device_config.ndevs; i++) {
		printf("%20s: %s\n", device_config.devs[i].devname, 
		       device_status(device_config.devs[i].status));
	}
	if (device_config.nspares > 0) {
		printf("Spares:\n");
		for(i=0; i < device_config.nspares; i++) {
			printf("%20s: %s\n",
			       device_config.spares[i].devname, 
			       device_status(device_config.spares[i].status));
		}
	} else {
		printf("No spares.\n");
	}
	for(i=0; i < device_config.ndevs; i++) {
		if (device_config.devs[i].status == rf_ds_optimal) {
			get_component_label(fd, device_config.devs[i].devname);
		} else {
			printf("%s status is: %s.  Skipping label.\n",
			       device_config.devs[i].devname,
			       device_status(device_config.devs[i].status));
		}
	}

	if (device_config.nspares > 0) {
		for(i=0; i < device_config.nspares; i++) {
			if ((device_config.spares[i].status == 
			     rf_ds_optimal) ||
			    (device_config.spares[i].status == 
			     rf_ds_used_spare)) {
				get_component_label(fd, 
					    device_config.spares[i].devname);
			} else {
				printf("%s status is: %s.  Skipping label.\n",
				       device_config.spares[i].devname,
				       device_status(device_config.spares[i].status));
			}		
		}
	}

	do_ioctl(fd, RAIDFRAME_CHECK_PARITY, &is_clean,
		 "RAIDFRAME_CHECK_PARITY");
	if (is_clean) {
		printf("Parity status: clean\n");
	} else {
		printf("Parity status: DIRTY\n");
	}
	check_status(fd,0);
}

static void
get_component_number(fd, component_name, component_number, num_columns)
	int fd;
	char *component_name;
	int *component_number;
	int *num_columns;
{
	RF_DeviceConfig_t device_config;
	void *cfg_ptr;
	int i;
	int found;

	*component_number = -1;
		
	/* Assuming a full path spec... */
	cfg_ptr = &device_config;
	do_ioctl(fd, RAIDFRAME_GET_INFO, &cfg_ptr, "RAIDFRAME_GET_INFO");

	*num_columns = device_config.cols;
	
	found = 0;
	for(i=0; i < device_config.ndevs; i++) {
		if (strncmp(component_name, device_config.devs[i].devname,
			    PATH_MAX)==0) {
			found = 1;
			*component_number = i;
		}
	}
	if (!found) { /* maybe it's a spare? */
		for(i=0; i < device_config.nspares; i++) {
			if (strncmp(component_name, 
				    device_config.spares[i].devname,
				    PATH_MAX)==0) {
				found = 1;
				*component_number = i + device_config.ndevs;
				/* the way spares are done should
				   really change... */
				*num_columns = device_config.cols + 
					device_config.nspares;
			}
		}
	}

	if (!found) {
		fprintf(stderr,"%s: %s is not a component %s", PROGNAME, 
			component_name, "of this device\n");
		exit(1);
	}
}

static void
rf_fail_disk(fd, component_to_fail, do_recon)
	int fd;
	char *component_to_fail;
	int do_recon;
{
	struct rf_recon_req recon_request;
	int component_num;
	int num_cols;

	get_component_number(fd, component_to_fail, &component_num, &num_cols);

	recon_request.row = component_num / num_cols;
	recon_request.col = component_num % num_cols;
	if (do_recon) {
		recon_request.flags = RF_FDFLAGS_RECON;
	} else {
		recon_request.flags = RF_FDFLAGS_NONE;
	}
	do_ioctl(fd, RAIDFRAME_FAIL_DISK, &recon_request, 
		 "RAIDFRAME_FAIL_DISK");
	if (do_recon && verbose) {
		printf("Reconstruction status:\n");
		sleep(3); /* XXX give reconstruction a chance to start */
		do_meter(fd,RAIDFRAME_CHECK_RECON_STATUS_EXT);
	}
}

static void
get_component_label(fd, component)
	int fd;
	char *component;
{
	RF_ComponentLabel_t component_label;
	int component_num;
	int num_cols;

	get_component_number(fd, component, &component_num, &num_cols);

	memset( &component_label, 0, sizeof(RF_ComponentLabel_t));
	component_label.row = component_num / num_cols;
	component_label.column = component_num % num_cols;

	do_ioctl( fd, RAIDFRAME_GET_COMPONENT_LABEL, &component_label,
		  "RAIDFRAME_GET_COMPONENT_LABEL");

	printf("Component label for %s:\n",component);

	printf("   Row: %d, Column: %d, Num Rows: %d, Num Columns: %d\n",
	       component_label.row, component_label.column, 
	       component_label.num_rows, component_label.num_columns);
	printf("   Version: %d, Serial Number: %d, Mod Counter: %d\n",
	       component_label.version, component_label.serial_number,
	       component_label.mod_counter);
	printf("   Clean: %s, Status: %d\n",
	       component_label.clean ? "Yes" : "No", 
	       component_label.status );
	printf("   sectPerSU: %d, SUsPerPU: %d, SUsPerRU: %d\n",
	       component_label.sectPerSU, component_label.SUsPerPU, 
	       component_label.SUsPerRU);
	printf("   Queue size: %d, blocksize: %d, numBlocks: %d\n",
	       component_label.maxOutstanding, component_label.blockSize,
	       component_label.numBlocks);
	printf("   RAID Level: %c\n", (char) component_label.parityConfig);
	printf("   Autoconfig: %s\n", 
	       component_label.autoconfigure ? "Yes" : "No" );
	printf("   Root partition: %s\n",
	       component_label.root_partition ? "Yes" : "No" );
	printf("   Last configured as: raid%d\n", component_label.last_unit );
}

static void
set_component_label(fd, component)
	int fd;
	char *component;
{
	RF_ComponentLabel_t component_label;
	int component_num;
	int num_cols;

	get_component_number(fd, component, &component_num, &num_cols);

	/* XXX This is currently here for testing, and future expandability */

	component_label.version = 1;
	component_label.serial_number = 123456;
	component_label.mod_counter = 0;
	component_label.row = component_num / num_cols;
	component_label.column = component_num % num_cols;
	component_label.num_rows = 0;
	component_label.num_columns = 5;
	component_label.clean = 0;
	component_label.status = 1;
	
	do_ioctl( fd, RAIDFRAME_SET_COMPONENT_LABEL, &component_label,
		  "RAIDFRAME_SET_COMPONENT_LABEL");
}


static void
init_component_labels(fd, serial_number)
	int fd;
	int serial_number;
{
	RF_ComponentLabel_t component_label;

	component_label.version = 0;
	component_label.serial_number = serial_number;
	component_label.mod_counter = 0;
	component_label.row = 0;
	component_label.column = 0;
	component_label.num_rows = 0;
	component_label.num_columns = 0;
	component_label.clean = 0;
	component_label.status = 0;
	
	do_ioctl( fd, RAIDFRAME_INIT_LABELS, &component_label,
		  "RAIDFRAME_SET_COMPONENT_LABEL");
}

static void
set_autoconfig(fd, name, autoconf)
	int fd;
	char *name;
	char *autoconf;
{
	int auto_config;
	int root_config;

	auto_config = 0;
	root_config = 0;

	if (strncasecmp(autoconf,"root", 4) == 0) {
		root_config = 1;
	}

	if ((strncasecmp(autoconf,"yes", 3) == 0) ||
	    root_config == 1) {
		auto_config = 1;
	}

	do_ioctl(fd, RAIDFRAME_SET_AUTOCONFIG, &auto_config,
		 "RAIDFRAME_SET_AUTOCONFIG");

	do_ioctl(fd, RAIDFRAME_SET_ROOT, &root_config,
		 "RAIDFRAME_SET_ROOT");

	printf("%s: Autoconfigure: %s\n", name,
	       auto_config ? "Yes" : "No");

	if (root_config == 1) {
		printf("%s: Root: %s\n", name,
		       auto_config ? "Yes" : "No");
	}
}

static void
add_hot_spare(fd, component)
	int fd;
	char *component;
{
	RF_SingleComponent_t hot_spare;

	hot_spare.row = 0;
	hot_spare.column = 0;
	strncpy(hot_spare.component_name, component, 
		sizeof(hot_spare.component_name));
	
	do_ioctl( fd, RAIDFRAME_ADD_HOT_SPARE, &hot_spare,
		  "RAIDFRAME_ADD_HOT_SPARE");
}

static void
remove_hot_spare(fd, component)
	int fd;
	char *component;
{
	RF_SingleComponent_t hot_spare;
	int component_num;
	int num_cols;

	get_component_number(fd, component, &component_num, &num_cols);

	hot_spare.row = component_num / num_cols;
	hot_spare.column = component_num % num_cols;

	strncpy(hot_spare.component_name, component, 
		sizeof(hot_spare.component_name));
	
	do_ioctl( fd, RAIDFRAME_REMOVE_HOT_SPARE, &hot_spare,
		  "RAIDFRAME_REMOVE_HOT_SPARE");
}

static void
rebuild_in_place( fd, component )
	int fd;
	char *component;
{
	RF_SingleComponent_t comp;
	int component_num;
	int num_cols;

	get_component_number(fd, component, &component_num, &num_cols);

	comp.row = 0;
	comp.column = component_num;
	strncpy(comp.component_name, component, sizeof(comp.component_name));
	
	do_ioctl( fd, RAIDFRAME_REBUILD_IN_PLACE, &comp,
		  "RAIDFRAME_REBUILD_IN_PLACE");

	if (verbose) {
		printf("Reconstruction status:\n");
		sleep(3); /* XXX give reconstruction a chance to start */
		do_meter(fd,RAIDFRAME_CHECK_RECON_STATUS_EXT);
	}

}

static void
check_parity( fd, do_rewrite, dev_name )
	int fd;
	int do_rewrite;
	char *dev_name;
{
	int is_clean;
	int percent_done;

	is_clean = 0;
	percent_done = 0;
	do_ioctl(fd, RAIDFRAME_CHECK_PARITY, &is_clean,
		 "RAIDFRAME_CHECK_PARITY");
	if (is_clean) {
		printf("%s: Parity status: clean\n",dev_name);
	} else {
		printf("%s: Parity status: DIRTY\n",dev_name);
		if (do_rewrite) {
			printf("%s: Initiating re-write of parity\n",
			       dev_name);
			do_ioctl(fd, RAIDFRAME_REWRITEPARITY, NULL, 
				 "RAIDFRAME_REWRITEPARITY");
			sleep(3); /* XXX give it time to
				     get started. */
			if (verbose) {
				printf("Parity Re-write status:\n");
				do_meter(fd, RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT);
			} else {
				do_ioctl(fd, 
					 RAIDFRAME_CHECK_PARITYREWRITE_STATUS, 
					 &percent_done, 
					 "RAIDFRAME_CHECK_PARITYREWRITE_STATUS"
					 );
				while( percent_done < 100 ) {
					sleep(3); /* wait a bit... */
					do_ioctl(fd, RAIDFRAME_CHECK_PARITYREWRITE_STATUS, 
						 &percent_done, "RAIDFRAME_CHECK_PARITYREWRITE_STATUS");
				}

			}
			       printf("%s: Parity Re-write complete\n",
				      dev_name);
		} else {
			/* parity is wrong, and is not being fixed.
			   Exit w/ an error. */
			exit(1);
		}
	}
}


static void
check_status( fd, meter )
	int fd;
	int meter;
{
	int recon_percent_done = 0;
	int parity_percent_done = 0;
	int copyback_percent_done = 0;

	do_ioctl(fd, RAIDFRAME_CHECK_RECON_STATUS, &recon_percent_done, 
		 "RAIDFRAME_CHECK_RECON_STATUS");
	printf("Reconstruction is %d%% complete.\n", recon_percent_done);
	do_ioctl(fd, RAIDFRAME_CHECK_PARITYREWRITE_STATUS, 
		 &parity_percent_done, 
		 "RAIDFRAME_CHECK_PARITYREWRITE_STATUS");
	printf("Parity Re-write is %d%% complete.\n", parity_percent_done);
	do_ioctl(fd, RAIDFRAME_CHECK_COPYBACK_STATUS, &copyback_percent_done, 
		 "RAIDFRAME_CHECK_COPYBACK_STATUS");
	printf("Copyback is %d%% complete.\n", copyback_percent_done);

	if (meter) {
		/* These 3 should be mutually exclusive at this point */
		if (recon_percent_done < 100) {
			printf("Reconstruction status:\n");
			do_meter(fd,RAIDFRAME_CHECK_RECON_STATUS_EXT);
		} else if (parity_percent_done < 100) {
			printf("Parity Re-write status:\n");
			do_meter(fd,RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT);
		} else if (copyback_percent_done < 100) {
			printf("Copyback status:\n");
			do_meter(fd,RAIDFRAME_CHECK_COPYBACK_STATUS_EXT);
		}
	}
}

const char *tbits = "|/-\\";

static void
do_meter(fd, option)
	int fd;
	u_long option;
{
	int percent_done;
	int last_value;
	int start_value;
	RF_ProgressInfo_t progressInfo;
	struct timeval start_time;
	struct timeval last_time;
	struct timeval current_time;
	double elapsed;
	int elapsed_sec;
	int elapsed_usec;
	int simple_eta,last_eta;
	double rate;
	int amount;
	int tbit_value;
	int wait_for_more_data;
	char buffer[1024];
	char bar_buffer[1024];
	char eta_buffer[1024];

	if (gettimeofday(&start_time,NULL)) {
		fprintf(stderr,"%s: gettimeofday failed!?!?\n", PROGNAME);
		exit(errno);
	}
	memset(&progressInfo, 0, sizeof(RF_ProgressInfo_t));

	percent_done = 0;
	do_ioctl(fd, option, &progressInfo, "");
	last_value = progressInfo.completed;
	start_value = last_value;
	last_time = start_time;
	current_time = start_time;
	
	wait_for_more_data = 0;
	tbit_value = 0;
	while(progressInfo.completed < progressInfo.total) {

		percent_done = (progressInfo.completed * 100) / 
			progressInfo.total;

		get_bar(bar_buffer, percent_done, 40);
		
		elapsed_sec = current_time.tv_sec - start_time.tv_sec;
		elapsed_usec = current_time.tv_usec - start_time.tv_usec;
		if (elapsed_usec < 0) {
			elapsed_usec-=1000000;
			elapsed_sec++;
		}

		elapsed = (double) elapsed_sec + 
			(double) elapsed_usec / 1000000.0;

		amount = progressInfo.completed - start_value;

		if (amount <= 0) { /* we don't do negatives (yet?) */
			amount = 0;
			wait_for_more_data = 1;
		} else {
			wait_for_more_data = 0;
		}

		if (elapsed == 0)
			rate = 0.0;
		else
			rate = amount / elapsed;

		if (rate > 0.0) {
			simple_eta = (int) (((double)progressInfo.total - 
					     (double) progressInfo.completed) 
					    / rate);
		} else {
			simple_eta = -1;
		}

		if (simple_eta <=0) { 
			simple_eta = last_eta;
		} else {
			last_eta = simple_eta;
		}

		get_time_string(eta_buffer, simple_eta);

		snprintf(buffer,1024,"\r%3d%% |%s| ETA: %s %c",
			 percent_done,bar_buffer,eta_buffer,tbits[tbit_value]);

		write(fileno(stdout),buffer,strlen(buffer));
		fflush(stdout);

		/* resolution wasn't high enough... wait until we get another
		   timestamp and perhaps more "work" done. */

		if (!wait_for_more_data) {
			last_time = current_time;
			last_value = progressInfo.completed;
		}

		if (++tbit_value>3) 
			tbit_value = 0;

		sleep(2);

		if (gettimeofday(&current_time,NULL)) {
			fprintf(stderr,"%s: gettimeofday failed!?!?\n",
				PROGNAME);
			exit(errno);
		}

		do_ioctl( fd, option, &progressInfo, "");
		

	}
	printf("\n");
}
/* 40 '*''s per line, then 40 ' ''s line. */
/* If you've got a screen wider than 160 characters, "tough" */

#define STAR_MIDPOINT 4*40
const char stars[] = "****************************************"
                     "****************************************"
                     "****************************************"
                     "****************************************"
                     "                                        "
                     "                                        "
                     "                                        "
                     "                                        "
                     "                                        ";

static void
get_bar(string,percent,max_strlen)
	char *string;
	double percent;
	int max_strlen;
{
	int offset;

	if (max_strlen > STAR_MIDPOINT) {
		max_strlen = STAR_MIDPOINT;
	}
	offset = STAR_MIDPOINT - 
		(int)((percent * max_strlen)/ 100);
	if (offset < 0)
		offset = 0;
	snprintf(string,max_strlen,"%s",&stars[offset]);
}

static void
get_time_string(string,simple_time)
	char *string;
	int simple_time;
{
	int minutes, seconds, hours;
	char hours_buffer[5];
	char minutes_buffer[5];
	char seconds_buffer[5];

	if (simple_time >= 0) {

		minutes = (int) simple_time / 60;
		seconds = ((int)simple_time - 60*minutes);
		hours = minutes / 60;
		minutes = minutes - 60*hours;
		
		if (hours > 0) {
			snprintf(hours_buffer,5,"%02d:",hours);
		} else {
			snprintf(hours_buffer,5,"   ");
		}
		
		snprintf(minutes_buffer,5,"%02d:",minutes);
		snprintf(seconds_buffer,5,"%02d",seconds);
		snprintf(string,1024,"%s%s%s",
			 hours_buffer, minutes_buffer, seconds_buffer);
	} else {
		snprintf(string,1024,"   --:--");
	}
	
}

static void
usage()
{
	const char *progname = PROGNAME;

	fprintf(stderr, "usage: %s [-v] -a component dev\n", progname);
	fprintf(stderr, "       %s [-v] -A yes | no | root dev\n", progname);
	fprintf(stderr, "       %s [-v] -B dev\n", progname);
	fprintf(stderr, "       %s [-v] -c config_file dev\n", progname);
	fprintf(stderr, "       %s [-v] -C config_file dev\n", progname);
	fprintf(stderr, "       %s [-v] -f component dev\n", progname);
	fprintf(stderr, "       %s [-v] -F component dev\n", progname);
	fprintf(stderr, "       %s [-v] -g component dev\n", progname);
	fprintf(stderr, "       %s [-v] -i dev\n", progname);
	fprintf(stderr, "       %s [-v] -I serial_number dev\n", progname);
	fprintf(stderr, "       %s [-v] -r component dev\n", progname); 
	fprintf(stderr, "       %s [-v] -R component dev\n", progname);
	fprintf(stderr, "       %s [-v] -s dev\n", progname);
	fprintf(stderr, "       %s [-v] -S dev\n", progname);
	fprintf(stderr, "       %s [-v] -u dev\n", progname);
#if 0
	fprintf(stderr, "usage: %s %s\n", progname, 
		"-a | -f | -F | -g | -r | -R component dev");
	fprintf(stderr, "       %s -B | -i | -s | -S -u dev\n", progname);
	fprintf(stderr, "       %s -c | -C config_file dev\n", progname);
	fprintf(stderr, "       %s -I serial_number dev\n", progname);
#endif
	exit(1);
	/* NOTREACHED */
}

#if defined(__FreeBSD__)
static void
check_driver(void)
{
	if (modfind("raidframe") == -1 && kldload("raidframe") == -1) {
		printf("Error: Cannot load RAIDframe driver.\n");
		exit(1);
	}
}
#endif

