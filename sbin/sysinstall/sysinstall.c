/*
#define DEBUG
 * Copyright (c) 1994, Paul Richards.
 *
 * All rights reserved.
 *
 * This software may be used, modified, copied, distributed, and
 * sold, in both source and binary form provided that the above
 * copyright and these terms are retained, verbatim, as the first
 * lines of this file.  Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with
 * its use.
 */

#include <dialog.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <ncurses.h>

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <ufs/ffs/fs.h>
#include <machine/console.h>

#include "mbr.h"
#include "bootarea.h"
#include "sysinstall.h"

struct disklabel *avail_disklabels;
int *avail_fds;
unsigned char **options;
unsigned char **avail_disknames;
unsigned char *scratch;
unsigned char *errmsg;
unsigned char *bootblocks;
struct mbr *mbr;
struct utsname utsname;
unsigned char *title = utsname.sysname;

struct sysinstall *sysinstall;
struct sysinstall *sequence;

int no_disks = 0;
int inst_disk = 0;
int inst_part = 0;
int custom_install;
int dialog_active = 0;

void exit_sysinstall();
void abort_installation(char *);
void exit_prompt();
void fatal(char *);
extern char *part_type(int);
extern int disk_size(int);

/* To make the binary as small as possible these should be malloc'd */
char selection[30];

void
abort_installation(char *prompt)
{
	strcpy(scratch, prompt);
	strcat(scratch,"\n\n Do you wish to abort the installation ?");
	if (!dialog_yesno("Abort installation ?",scratch,10,75))
		exit_prompt();
	clear();
}

void
exit_prompt()
{
	sprintf(scratch,"Are you sure you want to exit sysinstall?");
	if (!dialog_yesno("Exit sysinstall",scratch,10,75))
		exit_sysinstall();
	clear();
}

int
alloc_memory()
{
	int i;

	scratch = (char *) calloc(SCRATCHSIZE, sizeof(char));
	if (!scratch)
		return(-1);

	errmsg = (char *) calloc(ERRMSGSIZE, sizeof(char));
	if (!errmsg)
		return(-1);

	avail_disklabels = (struct disklabel *) calloc(MAX_NO_DISKS, sizeof(struct disklabel));
	if (!avail_disklabels) 
		return(-1);

	avail_fds = (int *) calloc(MAX_NO_DISKS, sizeof(int));
	if (!avail_fds) 
		return(-1);

	avail_disknames = (unsigned char **) calloc(MAX_NO_DISKS, sizeof(char *));
	if (!avail_disknames)
		return(-1);
	for (i=0;i<MAX_NO_DISKS;i++) {
		avail_disknames[i] = (char *) calloc(15, sizeof(char));
		if (!avail_disknames[i])
			return(-1);
	}

	options = (unsigned char **) calloc(MAX_NO_DISKS, sizeof(char *));
	if (!options)
		return(-1);
	for (i=0;i<MAX_NO_DISKS;i++) {
		options[i] = (char *) calloc(100, sizeof(char));
		if (!options[i])
			return(-1);
	}

	mbr = (struct mbr *) malloc(sizeof(struct mbr));
	if (!mbr)
		return(-1);

	bootblocks = (char *) malloc(BBSIZE);
	if (!bootblocks)
		return(-1);

	sysinstall = (struct sysinstall *) malloc(sizeof(struct sysinstall));
	if (!sysinstall)
		return(-1);

	sequence = (struct sysinstall *) malloc(sizeof(struct sysinstall));
	if (!sequence)
		return(-1);

	return(0);
}

void
free_memory()
{
	int i;

	free(scratch);
	free(errmsg);
	free(avail_disklabels);
	free(avail_fds);

	for (i=0;i<MAX_NO_DISKS;i++)
		free(avail_disknames[i]);
	free(avail_disknames);

	for (i=0;i<MAX_NO_DISKS;i++)
		free(options[i]);
	free(options);

	free(mbr);
	free(bootblocks);
	free(sysinstall);
	free(sequence);
}

void
exit_sysinstall()
{
	if (getpid() == 1) {
		if (reboot(RB_AUTOBOOT) == -1)
			if (dialog_active)
				while (1)
					dialog_msgbox("Exit sysinstall",
					              "Reboot failed -- hit reset",
				                 10, 75, 20);
			else {
				fprintf(stderr, "Reboot failed  -- hit reset");
				while (1);
			}
	} else {
		free_memory();
		if (dialog_active)
			end_dialog();
		exit(0);
	}
}

void
fatal(char *errmsg)
{
	if (dialog_active)
		dialog_msgbox("Fatal Error -- Aborting installation", errmsg, 10, 75, 20);
	else
		fprintf(stderr, "Fatal Error -- Aborting installation:\n%s\n", errmsg);
	exit_sysinstall();
}

void
query_disks()
{
	int i;
	char disk[15];
	char diskname[5];
	struct stat st;
	int fd;

	no_disks = 0;
	for (i=0;i<10;i++) {
		sprintf(diskname,"wd%d",i);
		sprintf(disk,"/dev/r%sd",diskname);
		if ((stat(disk, &st) == 0) && (st.st_mode & S_IFCHR))
			if ((fd = open(disk, O_RDWR)) != -1) {
				avail_fds[no_disks] = fd;
				bcopy(diskname, avail_disknames[no_disks], strlen(diskname));
				if (ioctl(fd, DIOCGDINFO, &avail_disklabels[no_disks++]) == -1)
					no_disks--;
			}
	}

	for (i=0;i<10;i++) {
		sprintf(diskname,"sd%d",i);
		sprintf(disk,"/dev/r%sd",diskname);
		if ((stat(disk, &st) == 0) && (st.st_mode & S_IFCHR))
			if ((fd = open(disk, O_RDWR)) != -1) {
				avail_fds[no_disks] = fd;
				bcopy(diskname, avail_disknames[no_disks], strlen(diskname));
				if (ioctl(fd, DIOCGDINFO, &avail_disklabels[no_disks++]) == -1)
					no_disks--;
			}
	}
}

int
select_disk()
{
	int i;
	int valid;

	do {
		valid = 1;
		sprintf(scratch,"There are %d disks available for installation: ",no_disks);

		for (i=0;i<no_disks;i++) {
			sprintf(options[(i*2)], "%d",i+1);
			sprintf(options[(i*2)+1], "%s, (%dMb) -> %s",avail_disklabels[i].d_typename,disk_size(i),avail_disknames[i]);
		}

		if (dialog_menu("FreeBSD Installation", scratch, 10, 75, 5, no_disks, options, selection)) {
			sprintf(scratch,"You did not select a valid disk");
			abort_installation(scratch);
			valid = 0;
		}
		clear();
	} while (!valid);
	return(atoi(selection) - 1);
}

int
select_partition(int disk)
{
	int valid;
	int i;
	int choice;

	do {
		valid = 1;

		sprintf(scratch,"Select one of the following areas to install to:");
		sprintf(options[0], "%d", 0);
		sprintf(options[1], "%s, (%dMb)", "Install to entire disk",
				  disk_size(disk));
		for (i=0; i < NDOSPART; i++) {
			sprintf(options[(i*2)+2], "%d",i+1);
			sprintf(options[(i*2)+3], "%s, (%ldMb)", 
			part_type(mbr->dospart[i].dp_typ),
						 mbr->dospart[i].dp_size * 512 / (1024 * 1024));
		}
		if (dialog_menu(title,
			 scratch, 10, 75, 5, 5, options, selection)) {
			sprintf(scratch,"You did not select a valid partition");
			abort_installation(scratch);
			valid = 0;
		}
		clear();
		choice = atoi(selection);
		if (!choice)
			if (dialog_yesno(title, "Installing to the whole disk will erase all its present data.\n\nAre you sure you want to do this?", 10, 75))
				valid = 0;
		clear();
	} while (!valid);
	
	return(atoi(selection) - 1);
}

int
exec(char *cmd, char *args, ...)
{
	int pid, w, status;
	char **argv = NULL;
	int arg = 0;
	int no_args = 0;
	va_list ap;
	struct stat dummy;

	if (stat(cmd, &dummy) == -1) {
		sprintf(errmsg, "Executable %s does not exist\n", cmd);
		return(-1);
	}

	va_start(ap, args);
	do {
		if (arg == no_args) {
			no_args += 10;
			if (!(argv = realloc(argv, no_args * sizeof(char *)))) {
				sprintf(errmsg, "Failed to allocate memory during exec of %s\n", cmd);
				return(-1);
			}
			if (arg == 0)
				argv[arg++] = (char *)args;
		}
	} while ((argv[arg++] = va_arg(ap, char *)));
	va_end(ap);

	if ((pid = fork()) == 0) {
		execv(cmd, argv);
		exit(1);
	}
	
	while ((w = wait(&status)) != pid && w != -1)
		;

	free(argv);
	if (w == -1) {
		sprintf(errmsg, "Child process %s terminated abnormally\n", cmd);
		return(-1);
	}

	return(0);
}

int
set_termcap()
{
	char *term;

	term = getenv("TERM");
	if (term == NULL) {
		int color_display;

		if (setenv("TERMCAP", "/etc/termcap.small", 1) < 0)
			return -1;
		if (ioctl(STDERR_FILENO, GIO_COLOR, &color_display) < 0) {
			char buf[64];
			int len;

			/* serial console */
			fprintf(stderr, "Enter your terminal type (must be present in /etc/termcap.small): ");
			if (fgets(buf, sizeof(buf), stdin) == NULL)
				return -1;
			len = strlen(buf);
			if (len > 0 && buf[len-1] == '\n')
				buf[len-1] = '\0';
			if (setenv("TERM", buf, 1) < 0)
				return -1;
		} else if (color_display) {

			/* color console */
			if (setenv("TERM", "cons25", 1) < 0)
				return -1;
		} else {

			/* mono console */
			if (setenv("TERM", "cons25-m", 1) < 0)
				return -1;
		}
	}
	return 0;
}

int
read_status(char *file, struct sysinstall *sysinstall)
{
	FILE *fd;

	if (!(fd = fopen(file, "r"))) {
		sprintf(errmsg, "Couldn't open status file %s for reading\n", file);
		return(-1);
	}
	if (fscanf(fd, "Root device: %s\n", sysinstall->root_dev) == -1) {
		sprintf(errmsg, "Failed to read root device from file %s\n", file);
		return(-1);
	}
	if (fscanf(fd, "Installation media: %s\n", sysinstall->media) == -1) {
		sprintf(errmsg, "Failed to read installation media from file %s\n", file);
		return(-1);
	}
	if (fscanf(fd, "Installation status: %d\n", &sysinstall->status) == -1) {
		sprintf(errmsg, "Status file %s has invalid format\n", file);
		return(-1);
	}
	if (fscanf(fd, "Sequence name: %s\n", sysinstall->seq_name) == -1) {
		sprintf(errmsg, "Failed to read sequence name from file %s\n", file);
		return(-1);
	}
	if (fscanf(fd, "Sequence number: %d of %d\n",
					&sysinstall->seq_no, &sysinstall->seq_size) == -1) {
		sprintf(errmsg, "Failed to read sequence information from file %s\n", file);
		return(-1);
	}
	if (fscanf(fd, "Archive: %s\n", sysinstall->archive) == -1) {
		sprintf(errmsg, "Failed to read archive name from file %s\n", file);
		return(-1);
	}
	if (fclose(fd) != 0) {
		sprintf(errmsg, "Couldn't close file %s after reading status\n", file);
		return(-1);
	}
	return(0);
}

int
write_status(char *file, struct sysinstall *sysinstall)
{
	FILE *fd;

	if (!(fd = fopen(file, "w"))) {
		sprintf(errmsg, "Couldn't open status file %s for writing\n", file);
		return(-1);
	}
	if (fprintf(fd, "Root device: %s\n", sysinstall->root_dev) == -1) {
		sprintf(errmsg, "Failed to write root device to file %s\n", file);
		return(-1);
	}
	if (fprintf(fd, "Installation media: %s\n", sysinstall->media) == -1) {
		sprintf(errmsg, "Failed to write installation media to file %s\n", file);
		return(-1);
	}
	if (fprintf(fd, "Installation status: %d\n", sysinstall->status) == -1) {
		sprintf(errmsg, "Failed to write status information to file %s\n", file);
		return(-1);
	}
	if (fprintf(fd, "Sequence name: %s\n", sysinstall->seq_name) == -1) {
		sprintf(errmsg, "Failed to write sequence name to file %s\n", file);
		return(-1);
	}
	if (fprintf(fd, "Sequence number: %d of %d\n",
					sysinstall->seq_no, sysinstall->seq_size) == -1) {
		sprintf(errmsg, "Failed to write sequence information to file %s\n", file);
		return(-1);
	}
	if (fprintf(fd, "Archive: %s\n", sysinstall->archive) == -1) {
		sprintf(errmsg, "Failed to write archive name to file %s\n", file);
		return(-1);
	}
	if (fclose(fd) != 0) {
		sprintf(errmsg, "Couldn't close status file %s after status update\n", file);
		return(-1);
	}
	return(0);
}

int
load_floppy(char *device, int seq_no)
{
	struct ufs_args ufsargs;

	ufsargs.fspec = device;
	if (mount(MOUNT_UFS,"/mnt", 0, (caddr_t) &ufsargs) == -1) {
		sprintf(errmsg, "Failed to mount floppy %s: %s\n",scratch, strerror(errno));
		return(-1);
	}

	strcpy(scratch, "/mnt/");
	strcat(scratch, STATUSFILE);
	if (read_status(scratch, sequence) == -1) {
		if (unmount("/mnt", 0) == -1) {
			strcat(errmsg, "Error unmounting floppy: ");
			strcat(errmsg, strerror(errno));
			fatal(errmsg);
		}
		return(-1);
	}
	
	if ((bcmp(sequence->seq_name, sysinstall->seq_name,
		  sizeof(sequence->seq_name)) != 0) || (sequence->seq_no != seq_no)) {
		sprintf(errmsg, "Mounted floppy is not the one expected\n");
		if (unmount("/mnt", 0) == -1) {
			strcat(errmsg, "Error unmounting floppy: ");
			strcat(errmsg, strerror(errno));
			fatal(errmsg);
		}
		return(-1);
	}

	return(0);
}

void
stage1()
{
	int i;
	int ok = 0;
	int ready = 0;
	struct ufs_args ufsargs;

	while (!ready) {
		ready = 1;

		query_disks();
		inst_disk = select_disk();

#ifdef DEBUG
		read_mbr(avail_fds[inst_disk], mbr);
		show_mbr(mbr);
#endif

		if (read_mbr(avail_fds[inst_disk], mbr) == -1) {
			sprintf(scratch, "The following error occured will trying to read the master boot record:\n\n%s\n\nIn order to install FreeBSD a new master boot record will have to be written which will mean all current data on the hard disk will be lost.", errmsg);
			ok = 0;
			while (!ok) {	
				abort_installation(scratch);
				if (!dialog_yesno(title, "Are you sure you wish to proceed?",
									  10, 75)) {
					clear();
					clear_mbr(mbr);
					ok = 1;
				}
			}
		}
	
		if (custom_install) 
			if (!dialog_yesno(title, "Do you wish to edit the DOS partition table?",
								  10, 75)) {
				clear();
				edit_mbr(mbr, &avail_disklabels[inst_disk]);
			}

		inst_part = select_partition(inst_disk);

		ok = 0;
		while (!ok) {
			if (build_mbr(mbr, &avail_disklabels[inst_disk]))
				ok = 1;
			else {
				sprintf(scratch, "The DOS partition table is inconsistent.\n\n%s\n\nDo you wish to edit it by hand?", errmsg);
				if (!dialog_yesno(title, scratch, 10, 75)) {
					edit_mbr(mbr, &avail_disklabels[inst_disk]);
					clear();
				} else {
					abort_installation("");
					ok = 1;
					ready = 0;
				}
			}
		}

		default_disklabel(&avail_disklabels[inst_disk],
								mbr->dospart[inst_part].dp_size,
								mbr->dospart[inst_part].dp_start);
		build_bootblocks(&avail_disklabels[inst_disk]);

		if (ready) {
			if (dialog_yesno(title, "We are now ready to format the hard disk for FreeBSD.\n\nSome or all of the disk will be overwritten during this process.\n\nAre you sure you wish to proceed ?", 10, 75)) {
				abort_installation("");
				ready = 0;
			}
			clear();
		}
	}

	/* Write master boot record and bootblocks */
	write_mbr(avail_fds[inst_disk], mbr);
	write_bootblocks(avail_fds[inst_disk],
						  mbr->dospart[inst_part].dp_start,
						  avail_disklabels[inst_disk].d_bbsize);

#ifdef DEBUG
	read_mbr(avail_fds[inst_disk], mbr);
	show_mbr(mbr);
#endif

	/* close all the open disks */
	for (i=0; i < no_disks; i++)
		if (close(avail_fds[i]) == -1) {
			sprintf(errmsg, "Error on closing file descriptors: %s\n",
					  strerror(errno));
			fatal(errmsg);
		}

	/* newfs the root partition */
	strcpy(scratch, avail_disknames[inst_disk]);
	strcat(scratch, "a");
	if (exec("/sbin/newfs","/sbin/newfs", scratch, 0) == -1)
		fatal(errmsg);

	/* newfs the /usr partition */
	strcpy(scratch, avail_disknames[inst_disk]);
	strcat(scratch, "e");
	if (exec("/sbin/newfs", "/sbin/newfs", scratch, 0) == -1)
		fatal(errmsg);

	strcpy(scratch, "/dev/");
	strcat(scratch, avail_disknames[inst_disk]);
	strcat(scratch, "a");
	ufsargs.fspec = scratch;
	if (mount(MOUNT_UFS,"/mnt", 0, (caddr_t) &ufsargs) == -1) {
		sprintf(errmsg, "Error mounting %s: %s\n",scratch, strerror(errno));
		fatal(errmsg);
	}

	if (mkdir("/mnt/usr",S_IRWXU) == -1) {
		sprintf(errmsg, "Couldn't create directory /mnt/usr: %s\n",
				  strerror(errno));
		fatal(errmsg);
	}

	if (mkdir("/mnt/mnt",S_IRWXU) == -1) {
		sprintf(errmsg, "Couldn't create directory /mnt/mnt: %s\n",
				  strerror(errno));
		fatal(errmsg);
	}

	strcpy(scratch, "/dev/");
	strcat(scratch, avail_disknames[inst_disk]);
	strcat(scratch, "e");
	ufsargs.fspec = scratch;
	if (mount(MOUNT_UFS,"/mnt/usr", 0, (caddr_t) &ufsargs) == -1) {
		sprintf(errmsg, "Error mounting %s: %s\n",scratch, strerror(errno));
		fatal(errmsg);
	}

	if (exec("/bin/cp","/bin/cp","/kernel","/mnt", 0) == -1) {
		sprintf(errmsg, "Couldn't copy /kernel to /mnt: %s\n",strerror(errno));
		fatal(errmsg);
	}
	if (exec("/bin/cp","/bin/cp","/sysinstall","/mnt", 0) == -1) {
		sprintf(errmsg, "Couldn't copy /sysinstall to /mnt: %s\n",
				  strerror(errno));
		fatal(errmsg);
	}
	if (exec("/bin/cp","/bin/cp","-R","/etc","/mnt", 0) == -1) {
		sprintf(errmsg, "Couldn't copy /etc to /mnt: %s\n",strerror(errno));
		fatal(errmsg);
	}
	if (exec("/bin/cp","/bin/cp","-R","/sbin","/mnt", 0) == -1) {
		sprintf(errmsg, "Couldn't copy /sbin to /mnt: %s\n",strerror(errno));
		fatal(errmsg);
	}
	if (exec("/bin/cp","/bin/cp","-R","/bin","/mnt", 0) == -1) {
		sprintf(errmsg, "Couldn't copy /bin to /mnt: %s\n",strerror(errno));
		fatal(errmsg);
	}
	if (exec("/bin/cp","/bin/cp","-R","/dev","/mnt", 0) == -1) {
		sprintf(errmsg, "Couldn't copy /dev to /mnt: %s\n",strerror(errno));
		fatal(errmsg);
	}
	if (exec("/bin/cp","/bin/cp","-R","/usr","/mnt", 0) == -1) {
		sprintf(errmsg, "Couldn't copy /usr to /mnt: %s\n",strerror(errno));
		fatal(errmsg);
	}

	sysinstall->status = DISK_READY;
	bcopy(avail_disknames[inst_disk], sysinstall->root_dev,
			strlen(avail_disknames[inst_disk]));
	sprintf(scratch, "/mnt/etc/%s", STATUSFILE);
	if (write_status(scratch, sysinstall) == -1)
		fatal(errmsg);

	if (unmount("/mnt/usr", 0) == -1) {
		sprintf(errmsg, "Error unmounting /mnt/usr: %s\n", strerror(errno));
		fatal(errmsg);
	}

	if (unmount("/mnt", 0) == -1) {
		sprintf(errmsg, "Error unmounting /mnt: %s\n", strerror(errno));
		fatal(errmsg);
	}
}

void
stage2()
{
	int i;
	struct ufs_args ufsargs;

	ufsargs.fspec = sysinstall->root_dev;
	if (mount(MOUNT_UFS,"/", 0, (caddr_t) &ufsargs) == -1) {
		sprintf(errmsg, "Failed to mount root read/write: %s\n%s", strerror(errno), ufsargs.fspec);
		fatal(errmsg);
	}

	sprintf(scratch, "Insert floppy %d in drive\n", sysinstall->seq_no + 1);
	dialog_msgbox("Stage 2 installation", scratch, 10, 75, 1);
	i = load_floppy(sysinstall->media, sysinstall->seq_no + 1);
	while (i == -1) {
		dialog_msgbox("Stage 2 installation",errmsg, 10, 75, 1);
		sprintf(scratch, "Please insert installation floppy %d in the boot drive", sysinstall->seq_no + 1);
		dialog_msgbox("Stage 2 installation",scratch, 10, 75, 1);
		i = load_floppy(sysinstall->media, sysinstall->seq_no + 1);
	};
	if (exec("/bin/cp","/bin/cp","/mnt/pax","/bin", 0) == -1) {
		sprintf(errmsg, "Couldn't copy /mnt/pax to /bin %s\n",strerror(errno));
		fatal(errmsg);
	}
	if (exec("/bin/pax", "/bin/pax", "-r", "-f", sequence->archive, 0) == -1) {
		sprintf(errmsg, "Failed to extract from archive file %s\n", sequence->archive);
		fatal(errmsg);
	}

	sysinstall->status = INSTALLED_BASE;
	sprintf(scratch, "/etc/%s", STATUSFILE);
	if (write_status(scratch, sysinstall) == -1)
		fatal(errmsg);

	if (unmount("/mnt", 0) == -1) {
		strcat(errmsg, "Error unmounting floppy: ");
		strcat(errmsg, strerror(errno));
		fatal(errmsg);
	}
}

/*
 * This is the overall plan:  (phk's version)
 *  
 * If (pid == 1)
 *	reopen stdin, stdout, stderr, and do various other magic.
 *
 * If (file exists /this_is_boot.flp)
 *	stage0:
 *		present /COPYRIGHT
 *		present /README
 *      stage1:
 *		Ask about diskallocation and do the fdisk/disklabel stunt.
 *	stage2:
 *		Do newfs, mount and copy over a minimal world.
 *		make /mnt/etc/fstab.  Install ourself as /mnt/sbin/init
 * Else
 *	stage3:
 *		Read cpio.flp and fiddle around with the bits a bit.
 *	stage4:
 *		Read bin-tarballs:
 *			Using ftp
 *			Using NFS (?)
 *			Using floppy
 *			Using tape
 *			Using shell-prompt
 *	stage5:
 *		Extract bin-tarballs
 *	stage6:
 *		Ask various questions and collect answers into system-config
 *		files.
 *	stage7:
 *		execl("/sbin/init");
 */

void
main(int argc, char **argv)
{
	int i;

	/* phk's main */
	if (argc > 1 && !strcmp(argv[1],"phk")) {
		return Xmain(argc,argv);
	}

	/* paul's main */
	/* Are we running as init? */
	if (getpid() == 1) {
		close(0); open("/dev/console",O_RDWR);
		close(1); dup(0);
		close(2); dup(0);
		i = 1;
		ioctl(0,TIOCSPGRP,&i);
		setlogin("root");
	}

	if (set_termcap() == -1)
		fatal("Can't find terminal entry\n");

	if (alloc_memory() == -1)
		fatal("Couldn't allocate memory\n");

	if (uname(&utsname) == -1) {
		/* Fake uname entry */
		bcopy("FreeBSD", utsname.sysname, strlen("FreeBSD"));
	}

	/* XXX - libdialog has particularly bad return value checking */
	init_dialog();
	/* If we haven't crashed I guess dialog is running ! */
	dialog_active = 1;

	strcpy(scratch, "/etc/");
	strcat(scratch, STATUSFILE);
	if (read_status(scratch, sysinstall) == -1) {
		fatal(errmsg);
	}

	switch(sysinstall->status) {
		case NOT_INSTALLED:
			stage1();
			dialog_msgbox("Stage 1 complete",
							  "Remove all floppy disks from the drives and hit return to reboot from the hard disk",
								10, 75, 1);
			if (reboot(RB_AUTOBOOT) == -1)
				fatal("Reboot failed");
			break;
		case DISK_READY:
			dialog_msgbox("Stage 2 install", "Hi!", 10, 75, 1);
			stage2();
			dialog_msgbox("Stage 2 complete",
							  "Well, this is as far as it goes so far :-)\n",
							  10, 75, 1);
			break;
		case INSTALLED_BASE:
			break;
		default:
			fatal("Unknown installation status");
	}
	exit_sysinstall();
}
