/*
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

#define DEBUG

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>
#include <ncurses.h>
#include <dialog.h>
#include <sys/param.h>
#include <ufs/ffs/fs.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sysinstall.h"

/* Don't like this */
#define DEFFSIZE 1024
#define DEFFRAG 8

#define DEFROOTSIZE 16
#define DEFSWAPSIZE 32
#define DEFUSRSIZE  120

extern void attr_clear(WINDOW *, int, int, chtype);
extern unsigned short dkcksum(struct disklabel *);

void abort_task(char *);
void leave_sysinstall(void);
void cleanup(void);
void fatal(char *);
void write_bootblocks(int, off_t, int);
void build_bootblocks(struct disklabel *);
int calc_sects(int, struct disklabel *);
void build_disklabel(struct disklabel *, int, int);
char *part_type(int);
void query_disks(void);
int disk_size(int);
int select_disk(void);
int select_partition(int);
int read_sector(int, void *);
int get_DOS_partitions(void);
#ifdef DEBUG
void print_disklabel(int);
#endif

char xxboot[] = "/usr/mdec/sdboot";
char bootxx[] = "/usr/mdec/bootsd";

struct disklabel *avail_disklabels;
int *avail_fds;
unsigned char **options;
unsigned char **avail_disknames;
unsigned char *scratch;

struct mboot mboot;

int no_disks = 0;
int inst_disk = 0;
int inst_part = 0;
int custom_install;


/* To make the binary as small as possible these should be malloc'd */
char selection[30];
char bootblocks[BBSIZE];

void
abort_task(char *prompt)
{
	strcat(prompt,"\n\n Do you wish to abort the installation ?");
	if (!dialog_yesno("ABORT",prompt,15,40))
		leave_sysinstall();
	attr_clear(stdscr, LINES, COLS, A_NORMAL);
	refresh();
}

void
leave_sysinstall()
{
	sprintf(scratch,"Are you sure you want to exit sysinstall?");
	if (!dialog_yesno("Exit sysinstall",scratch,15,40)) {
		cleanup();
		exit(0);
	}
	attr_clear(stdscr, LINES, COLS, A_NORMAL);
	refresh();
}

void
alloc_memory()
{
	int i;

	scratch = (char *) calloc(100,sizeof(char));
	avail_disklabels = (struct disklabel *) calloc(MAX_NO_DISKS, sizeof(struct disklabel));
	if (!(avail_disklabels)) 
		fatal("Couldn't malloc memory for disklabels");

	avail_fds = (int *) calloc(MAX_NO_DISKS, sizeof(int));
	if (!(avail_fds)) 
		fatal("Couldn't malloc memory for file descriptors");

	avail_disknames = (unsigned char **) calloc(MAX_NO_DISKS, sizeof(char *));
	for (i=0;i<MAX_NO_DISKS;i++)
		avail_disknames[i] = (char *) calloc(15, sizeof(char));

	options = (unsigned char **) calloc(MAX_NO_DISKS, sizeof(char *));
	for (i=0;i<MAX_NO_DISKS;i++)
		options[i] = (char *) calloc(100, sizeof(char));
}

void
free_memory()
{
	int i;

	free(scratch);
	free(avail_disklabels);
	free(avail_fds);

	for (i=0;i<MAX_NO_DISKS;i++)
		free(avail_disknames[i]);
	free(avail_disknames);

	for (i=0;i<MAX_NO_DISKS;i++)
		free(options[i]);
	free(options);
}

void
cleanup()
{
	free_memory();
	attr_clear(stdscr, LINES, COLS, A_NORMAL);
	refresh();
	endwin();
}

void
fatal(char *prompt)
{
	dialog_msgbox("Fatal Error -- Aborting installation",prompt,15,40,20);
	cleanup();
	exit(1);
}

void
enable_label(int fd)
{
	int flag = 1;
	if (ioctl(fd, DIOCWLABEL, &flag) < 0)
		fatal("Couldn't write enable disklabel sector");
}

void
disable_label(int fd)
{
	int flag = 0;
	(void) ioctl(fd, DIOCWLABEL, &flag);
}

void
write_bootblocks(int fd, off_t offset, int bbsize)
{
	if (ioctl(fd, DIOCSDINFO, &avail_disklabels[inst_disk]) < 0 &&
	    errno != ENODEV && errno != ENOTTY)
		fatal("Couldn't change in-core disklabel");

	if (lseek(fd, (offset * avail_disklabels[inst_disk].d_secsize), SEEK_SET) < 0)
		fatal("Couldn't lseek for disklabel write");

	if (write(fd, bootblocks, bbsize) != bbsize)
		fatal("Couldn't write disklabel");
}

void
build_bootblocks(struct disklabel *label)
{

	int fd;

	fd = open(xxboot, O_RDONLY);
	if (fd < 0)
		fatal("Couldn't open boot1 files");

	if (read(fd, bootblocks, (int)label->d_secsize) < 0)
		fatal("Couldn't read boot 1 code");

	(void) close(fd);

	fd = open(bootxx, O_RDONLY);
	if (fd < 0)
		fatal("Couldn't open boot2 files");

	if (read(fd, &bootblocks[label->d_secsize], (int)(label->d_bbsize - label->d_secsize)) < 0)
		fatal("Couldn't read boot 2 code");

	(void) close(fd);

	/* Write the disklabel into the bootblocks */

	label->d_checksum = dkcksum(label);
	bcopy(label, &bootblocks[(LABELSECTOR * label->d_secsize) + LABELOFFSET], sizeof *label);

}

int
calc_sects(int size, struct disklabel *label)
{
	int nsects, ncyls;

	nsects = (size * 1024 * 1024) / label->d_secsize;
	ncyls = nsects / label->d_secpercyl;
	nsects = ++ncyls * label->d_secpercyl;

	return(nsects);
}

void
build_disklabel(struct disklabel *label, int avail_sects, int offset)
{

	int nsects;

	/* Fill in default label entries */
	label->d_magic = DISKMAGIC;
	bcopy("INSTALLATION",label->d_typename, strlen("INSTALLATION"));
	label->d_rpm = 3600;
	label->d_interleave = 1;
	label->d_trackskew = 0;
	label->d_cylskew = 0;
	label->d_magic2 = DISKMAGIC;
	label->d_checksum = 0;
	label->d_bbsize = BBSIZE;
	label->d_sbsize = SBSIZE;
	label->d_npartitions = 5;

	/* Set up c and d as raw partitions for now */
	label->d_partitions[2].p_size = avail_sects;
	label->d_partitions[2].p_offset = offset;
	label->d_partitions[2].p_fsize = DEFFSIZE;
	label->d_partitions[2].p_fstype = FS_UNUSED;
	label->d_partitions[2].p_frag = DEFFRAG;

	label->d_partitions[3].p_size = label->d_secperunit;
	label->d_partitions[3].p_offset = 0;
	label->d_partitions[3].p_fsize = DEFFSIZE;
	label->d_partitions[3].p_fstype = FS_UNUSED;
	label->d_partitions[3].p_frag = DEFFRAG;

	/* Default root */
	nsects = calc_sects(DEFROOTSIZE, label);

	label->d_partitions[0].p_size = nsects;
	label->d_partitions[0].p_offset = offset;
	label->d_partitions[0].p_fsize = DEFFSIZE;
	label->d_partitions[0].p_fstype = FS_BSDFFS;
	label->d_partitions[0].p_frag = DEFFRAG;

	avail_sects -= nsects;
	offset += nsects;
	nsects = calc_sects(DEFSWAPSIZE, label);

	label->d_partitions[1].p_size = nsects;
	label->d_partitions[1].p_offset = offset;
	label->d_partitions[1].p_fsize = DEFFSIZE;
	label->d_partitions[1].p_fstype = FS_SWAP;
	label->d_partitions[1].p_frag = DEFFRAG;

	avail_sects -= nsects;
	offset += nsects;
	nsects = calc_sects(DEFUSRSIZE, label);

	if (avail_sects > nsects)
		nsects = avail_sects;

	label->d_partitions[4].p_size = nsects;
	label->d_partitions[4].p_offset = offset;
	label->d_partitions[4].p_fsize = DEFFSIZE;
	label->d_partitions[4].p_fstype = FS_BSDFFS;
	label->d_partitions[4].p_frag = DEFFRAG;

#ifdef notyet
	if (custom_install)
		customise_label()
#endif

}

char *
part_type(int type)
{
	int num_types = (sizeof(part_types)/sizeof(struct part_type));
	int next_type = 0;
	struct part_type *ptr = part_types;

	while (next_type < num_types) {
		if(ptr->type == type)
			return(ptr->name);
		ptr++;
		next_type++;
	}
	return("Uknown");
}

void
query_disks()
{
	int i;
	char disk[15];
	char diskname[5];
	struct stat st;
	int fd;

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

#ifdef DEBUG
void
print_disklabel(int disk)
{
	int i;

	printf("Dumping label for disk %d, %s\n", disk, avail_disklabels[disk].d_typename);
	printf("magic = %lu, ",avail_disklabels[disk].d_magic);
	printf("type  = %x, ",avail_disklabels[disk].d_type);
	printf("subtype = %x\n",avail_disklabels[disk].d_subtype);
	printf("typename = %s, ",avail_disklabels[disk].d_typename);
	printf("packname = %s, ",avail_disklabels[disk].d_packname);
	printf("boot0 = %s, ",avail_disklabels[disk].d_boot0);
	printf("boot1 = %s\n",avail_disklabels[disk].d_boot1);
	printf("secsize = %ld, ",avail_disklabels[disk].d_secsize);
	printf("nsectors = %ld, ",avail_disklabels[disk].d_nsectors);
	printf("ntracks = %ld, ",avail_disklabels[disk].d_ntracks);
	printf("ncylinders = %ld\n",avail_disklabels[disk].d_ncylinders);
	printf("secpercyl = %ld, ",avail_disklabels[disk].d_secpercyl);
	printf("secperunit = %ld\n",avail_disklabels[disk].d_secperunit);
	printf("sparespertrack = %d, ",avail_disklabels[disk].d_sparespertrack);
	printf("sparespercyl = %d, ",avail_disklabels[disk].d_sparespercyl);
	printf("acylinders = %ld\n",avail_disklabels[disk].d_acylinders);
	printf("rpm = %d, ",avail_disklabels[disk].d_rpm);
	printf("interleave = %d, ",avail_disklabels[disk].d_interleave);
	printf("trackskew = %d, ",avail_disklabels[disk].d_trackskew);
	printf("cylskew = %d\n",avail_disklabels[disk].d_cylskew);
	printf("headswitch = %ld, ",avail_disklabels[disk].d_headswitch);
	printf("trkseek = %ld, ",avail_disklabels[disk].d_trkseek);
	printf("flags = %ld\n",avail_disklabels[disk].d_flags);
	printf("drivedata");
	for (i=0; i< NDDATA; i++) {
		printf(" : %d = %ld",i,avail_disklabels[disk].d_drivedata[i]);
	}
	printf("\n");
	printf("spare");
	for (i=0; i< NSPARE; i++) {
		printf(" : %d = %ld",i,avail_disklabels[disk].d_spare[i]);
	}
	printf("\n");
	printf("magic2 = %lu, ",avail_disklabels[disk].d_magic2);
	printf("checksum = %d\n",avail_disklabels[disk].d_checksum);
	printf("npartitions = %d, ",avail_disklabels[disk].d_npartitions);
	printf("bbsize = %lu, ",avail_disklabels[disk].d_bbsize);
	printf("sbsize = %lu\n",avail_disklabels[disk].d_sbsize);
	for (i=0; i< MAXPARTITIONS; i++) {
		printf("%d: size = %ld",i,avail_disklabels[disk].d_partitions[i].p_size);
		printf(", offset = %ld",avail_disklabels[disk].d_partitions[i].p_offset);
		printf(", fsize = %ld",avail_disklabels[disk].d_partitions[i].p_fsize);
		printf(", fstype = %d",avail_disklabels[disk].d_partitions[i].p_fstype);
		printf(", frag = %d",avail_disklabels[disk].d_partitions[i].p_frag);
		printf(", cpg = %d",avail_disklabels[disk].d_partitions[i].p_cpg);
		printf("\n");
	}
}
#endif

int
disk_size(int disk)
{
	struct disklabel *label = avail_disklabels + disk;
	int size;

	size = label->d_secsize * label->d_nsectors
			 * label->d_ntracks * label->d_ncylinders;
	return(size/1024/1024);
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

		if (dialog_menu("FreeBSD Installation", scratch, 10, 80, 5, no_disks, options, selection)) {
			sprintf(scratch,"You did not select a valid disk");
			abort_task(scratch);
			valid = 0;
		}
	} while (!valid);
	return(atoi(selection) - 1);
}

int
select_partition(int disk)
{
	struct dos_partition *partp;
	int valid;
	int i;

	do {
		valid = 1;

		sprintf(scratch,"The following partitions were found on this disk");
		for (i=0;i<4;i++) {
			partp = ((struct dos_partition *) &mboot.parts) + i;
			sprintf(options[(i*2)], "%d",i+1);
			sprintf(options[(i*2)+1], "%s, (%ldMb)", 
			part_type(partp->dp_typ), partp->dp_size * 512 / (1024 * 1024));
		}
		if (dialog_menu("FreeBSD Installation", scratch, 10, 80, 5, 4, options, selection)) {
			sprintf(scratch,"You did not select a valid partition");
			abort_task(scratch);
			valid = 0;
		}
	} while (!valid);
	
	return(atoi(selection) - 1);
}

int
read_sector(int sector, void *buf)
{
	lseek(*(avail_fds + inst_disk),(sector * 512), 0);
	return read(*(avail_fds + inst_disk), buf, 512);
}

int
get_DOS_partitions()
{
	/* Read DOS partition area */
	if (read_sector(0, (char *) mboot.bootinst) == -1)
		fatal("Couldn't read DOS partition area");
	if (mboot.signature != BOOT_MAGIC)
		/* Not a DOS partitioned disk */
		return(-1);
	return(0);
}

void
write_DOS_partitions(int fd)
{
	lseek(fd, 0, 0);
	if (write(fd, mboot.bootinst, 512) == -1)
		fatal("Failed to write DOS partition area");
}

void
exec(char *cmd, char *args, ...)
{
	int pid, w, stat;
	char **argv;
	int arg = 0;
	int no_args = 0;
	va_list ap;

	va_start(ap, args);
	do {
		if (arg == no_args) {
			no_args += 10;
			if (!(argv = realloc(argv, no_args * sizeof(char *))))
				fatal("Couldn't alloc memory in exec");
			if (arg == 0)
				argv[arg++] = (char *)args;
		}
	} while ((argv[arg++] = va_arg(ap, char *)));
	va_end(ap);

	if ((pid = fork()) == 0) {
		execv(cmd, argv);
		exit(1);
	}
	
	while ((w = wait(&stat)) != pid && w != -1)
		;

	if (w == -1)
		fatal("exec failed!");
	free(argv);
}

void
main(int argc, char **argv)
{

	struct ufs_args ufsargs;
	int i;

close(0); open("/dev/console",O_RDWR);
close(1); dup(0);
close(2); dup(0);
putenv("TERM=cons25");

	alloc_memory();
	init_dialog();

	query_disks();
	inst_disk = select_disk();

	enable_label(avail_fds[inst_disk]);

	if (get_DOS_partitions() == -1) {
		build_disklabel(&avail_disklabels[inst_disk], avail_disklabels[inst_disk].d_secperunit, 0);
		build_bootblocks(&avail_disklabels[inst_disk]);
		write_bootblocks(avail_fds[inst_disk], 0, avail_disklabels[inst_disk].d_bbsize);
	} else {
		inst_part = select_partition(inst_disk);
		/* Set partition to be FreeBSD and active */
		for (i=0; i < NDOSPART; i++)
				mboot.parts[i].dp_flag &= ~ACTIVE;
		mboot.parts[inst_part].dp_typ = DOSPTYP_386BSD;
		mboot.parts[inst_part].dp_flag = ACTIVE;
		write_DOS_partitions(avail_fds[inst_disk]);
		build_disklabel(&avail_disklabels[inst_disk], mboot.parts[inst_part].dp_size, mboot.parts[inst_part].dp_start);
		build_bootblocks(&avail_disklabels[inst_disk]);
		write_bootblocks(avail_fds[inst_disk], mboot.parts[inst_part].dp_start, avail_disklabels[inst_disk].d_bbsize);
	}

	/* close all the open disks */
	for (i=0; i < no_disks; i++)
		if (close(avail_fds[i]) == -1)
			printf("Error on closing file descriptors: %s\n",strerror(errno));

	/* newfs the root partition */
	strcpy(scratch, avail_disknames[inst_disk]);
	strcat(scratch, "a");
	exec("/sbin/newfs","/sbin/newfs",scratch, 0);

	/* newfs the /usr partition */
	strcpy(scratch, avail_disknames[inst_disk]);
	strcat(scratch, "e");
	exec("/sbin/newfs", "/sbin/newfs", scratch, 0);


	cleanup();
	printf("Done newfs\n");

	strcpy(scratch, "/dev/");
	strcat(scratch, avail_disknames[inst_disk]);
	strcat(scratch, "a");
	ufsargs.fspec = scratch;
	if (mount(MOUNT_UFS,"/mnt", 0, (caddr_t) &ufsargs) == -1)
		printf("Error mounting %s: %s\n",scratch, strerror(errno));

	printf("Done mount of %s\n",scratch);

	mkdir("/mnt/usr",S_IRWXU);
	printf("Done mkdir \n");

	strcpy(scratch, "/dev/");
	strcat(scratch, avail_disknames[inst_disk]);
	strcat(scratch, "e");
	ufsargs.fspec = scratch;
	mount(MOUNT_UFS,"/mnt/usr", 0, (caddr_t) &ufsargs);

	printf("Done mount of %s\n",scratch);

	/* Copy over a basic system */
	exec("/bin/cp","/bin/cp","-R","/dev","/mnt/dev",0);
	printf("Done dev \n");
	exec("/bin/cp","/bin/cp","-R","/etc","/mnt",0);
	printf("Done etc \n");
	exec("/bin/cp","/bin/cp","-R","/bin","/mnt",0);
	printf("Done bin \n");
	exec("/bin/cp","/bin/cp","-R","/sbin","/mnt",0);
	printf("Done sbin\n");
	exec("/bin/cp","/bin/cp","/.profile","/mnt",0);
	printf("Done profile\n");
	exec("/bin/cp","/bin/cp","/kernel","/mnt",0);
	printf("Done kernel\n");
	exec("/bin/cp","/bin/cp","-R","/usr","/mnt/usr",0);

	disable_label(avail_fds[inst_disk]);

	cleanup();
	exec("/sbin/reboot", "/sbin/reboot", 0);
}
