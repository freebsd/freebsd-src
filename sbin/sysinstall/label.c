#define DKTYPENAMES
#include <sys/param.h>
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/devconf.h>
#include <ufs/ffs/fs.h>

#include <fstab.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <string.h>
#include <dialog.h>

#include "disk.h"
#include "sysinstall.h"
#include "editor.h"
#include "label.h"

/* Forward decls */
int disk_size(struct disklabel *);
int getfstype(char *);
int sectstoMb(int, int);

char *partname[MAXPARTITIONS] = {"a", "b", "c", "d", "e", "f", "g", "h"};
extern char boot1[];
extern char boot2[];

char *yesno[] = {"yes", "no", 0};

int
disk_size(struct disklabel *lbl)
{
	int size;

	size = lbl->d_secsize * lbl->d_nsectors *
	lbl->d_ntracks * lbl->d_ncylinders;
	return (size / 1024 / 1024);
}

int
sectstoMb(int nsects, int secsize)
{
	int size;

	size = nsects * secsize;
	if (size)
	size /= 1024 * 1024;
	return (size);
}

int
Mbtosects(int Mb, int secsize)
{
	int nsects;
    
	nsects = (Mb * 1024 * 1024) / secsize;
	return(nsects);
}

int
rndtocylbdry(int size, int secpercyl)
{
	int nocyls;

	nocyls = size / secpercyl;
	if ((nocyls * secpercyl) < size)
		nocyls++;
	return (nocyls * secpercyl);
}

char *
diskname(int disk)
{
	sprintf(scratch, "/dev/r%s%dd", disk_list[disk].devconf->dc_name,
											  disk_list[disk].devconf->dc_unit);
	return (scratch);
}

int
get_fs_type(char *fstype)
{
	int i;

	for (i=0; fstypenames[i]; i++)
		if (strcmp(fstype, fstypenames[i]))
			return (i);

	return (FS_OTHER);
}

int
read_disklabel(int disk)
{
	int fd;

	if ((fd = open(diskname(disk), O_RDONLY)) == -1) {
		sprintf(errmsg, "Couldn't open %s to read disklabel\n%s\n",
				  scratch,strerror(errno));
		return (-1);
	}

	if (ioctl(fd, DIOCGDINFO, &disk_list[disk].lbl) == -1) {
		sprintf(errmsg, "Couldn't get disklabel from %s\n%s\n",
				  scratch, strerror(errno));
		return (-1);
	}

	if (close(fd) == -1) {
		sprintf(errmsg, "Couldn't close %s after reading disklabel\n%s\n",
				  scratch, strerror(errno));
		return (-1);
	}
	return (0);
}

int
edit_disklabel(int disk)
{
	WINDOW *window;
	int key = 0;
	int done;
	int next;
	int cur_field;
	int cur_part;
	int i;
	struct disklabel *lbl = &disk_list[disk].lbl;
	int offset, slop;
	int nsects, hog;
	int avail_sects, free;

	lbl->d_magic = DISKMAGIC;
	bcopy("INSTALLATION", lbl->d_typename, strlen("INSTALLATION"));
	lbl->d_rpm = 3600;
	lbl->d_interleave = 1;
	lbl->d_trackskew = 0;
	lbl->d_cylskew = 0;
	lbl->d_magic2 = DISKMAGIC;
	lbl->d_checksum = 0;
	lbl->d_bbsize = BBSIZE;
	lbl->d_sbsize = SBSIZE;
	lbl->d_npartitions = 8;

	/* Initialise the entries */

	for (i=0; i < MAXPARTITIONS; i++) {

		disk_list[disk].mounts[i].fs_spec = 
			(char *)malloc(80);
		if (!disk_list[disk].mounts[i].fs_spec) {
			sprintf(errmsg, "Couldn't allocate memory for device mounts\n");
			return (-1);
		}
		sprintf(disk_list[disk].mounts[i].fs_spec,
			     "%s%d%s", disk_list[disk].devconf->dc_name,
								disk_list[disk].devconf->dc_unit,
								partname[i]);

		disk_list[disk].mounts[i].fs_file =
			(char *)malloc(80);
		if (!disk_list[disk].mounts[i].fs_file) {
			sprintf(errmsg, "Couldn't allocate memory for mount points\n");
			return (-1);
		}
		sprintf(disk_list[disk].mounts[i].fs_file, "%s", "Not Mounted");

		disk_list[disk].mounts[i].fs_vfstype =
			(char *)malloc(80);
		if (!disk_list[disk].mounts[i].fs_vfstype) {
			sprintf(errmsg, "Couldn't allocate memory for filesystem type\n");
			return (-1);
		}
		switch (lbl->d_partitions[i].p_fstype) {
			case FS_BSDFFS:
				sprintf(disk_list[disk].mounts[i].fs_vfstype, "%s", "ufs");
				break;
			case FS_MSDOS:
				sprintf(disk_list[disk].mounts[i].fs_vfstype, "%s", "msdos");
				break;
			case FS_SWAP:
				sprintf(disk_list[disk].mounts[i].fs_vfstype, "%s", "swap");
				break;
			default:
				sprintf(disk_list[disk].mounts[i].fs_vfstype, "%s", "unused");
		}

		disk_list[disk].mounts[i].fs_mntops =
			(char *)malloc(80);
		if (!disk_list[disk].mounts[i].fs_mntops) {
			sprintf(errmsg, "Couldn't allocate memory for mount options\n");
			return (-1);
		}
		sprintf(disk_list[disk].mounts[i].fs_mntops, "%s", "YES");

		sprintf(label_field[(i*5)+3].field, "%d",
			     sectstoMb(lbl->d_partitions[i].p_size, lbl->d_secsize));
	}

	/*
	 * Setup the RAWPART and OURPART partition ourselves from the MBR
	 * in case either one doesn't exist or the new MBR invalidates them.
	 */

	cur_part = disk_list[disk].inst_part;
	lbl->d_partitions[OURPART].p_size = 
				disk_list[disk].mbr.dospart[cur_part].dp_size;
	lbl->d_partitions[OURPART].p_offset =
				disk_list[disk].mbr.dospart[cur_part].dp_start;
	lbl->d_partitions[RAWPART].p_size = lbl->d_secperunit;
	lbl->d_partitions[RAWPART].p_offset = 0;

	if (!(window = newwin(LINES, COLS, 0, 0))) {
		sprintf(errmsg, "Failed to open window for disklabel editor\n");
		return (-1);
	}

	keypad(window, TRUE);
    
	draw_box(window, 0, 0, LINES, COLS, dialog_attr, border_attr);

	/* Only one toggle to set up */
	for (i=0; i < MAXPARTITIONS; i++)
		label_field[(i*5)+1].misc = yesno;

	cur_field = 1;
	done = 0;
	while (!done && (key != ESC)) {

		/* Update disklabel */

		avail_sects = lbl->d_partitions[OURPART].p_size;
		offset = lbl->d_partitions[OURPART].p_offset;
		slop = rndtocylbdry(offset, lbl->d_secpercyl) - offset;
		for (i=0; i < MAXPARTITIONS; i++) {
			if (i == OURPART)
				continue;
			if (i == RAWPART)
				continue;
			lbl->d_partitions[i].p_offset = offset;
			nsects = atoi(label_field[(i*5)+3].field);
			nsects = Mbtosects(nsects, lbl->d_secsize);
			nsects = rndtocylbdry(nsects, lbl->d_secpercyl);
			if (slop) {
				nsects += slop;
				slop = 0;
			}
			if (nsects > avail_sects)
				nsects = avail_sects;
			avail_sects -= nsects;
			offset += nsects;
			if (nsects == 0)
				lbl->d_partitions[i].p_offset = 0;
			lbl->d_partitions[i].p_size = nsects;
			lbl->d_partitions[i].p_fsize = DEFFSIZE;
			lbl->d_partitions[i].p_frag = DEFFRAG;
		}

		for (i=0; i < MAXPARTITIONS; i++) {
			sprintf(label_field[(i*5)].field, "%s",
					  disk_list[disk].mounts[i].fs_spec);
			sprintf(label_field[(i*5)+1].field, "%s",
					  disk_list[disk].mounts[i].fs_mntops);
			sprintf(label_field[(i*5)+2].field, "%s",
					  disk_list[disk].mounts[i].fs_vfstype);
			sprintf(label_field[(i*5)+3].field, "%d",
			        sectstoMb(lbl->d_partitions[i].p_size,lbl->d_secsize));
			sprintf(label_field[(i*5)+4].field, "%s",
					  disk_list[disk].mounts[i].fs_file);
		}

		sprintf(label_field[47].field, "%d",
					sectstoMb(avail_sects, lbl->d_secsize));

		disp_fields(window, label_field,
						sizeof(label_field)/sizeof(struct field));

		switch (label_field[cur_field].type) {
			case F_EDIT:
				key = line_edit(window, label_field[cur_field].y,
										label_field[cur_field].x,
										label_field[cur_field].width,
										label_field[cur_field].maxlen,
										item_selected_attr, 1,
										label_field[cur_field].field);

				/* Update mount info */

				for (i=0; i<MAXPARTITIONS; i++) {
					sprintf(disk_list[disk].mounts[i].fs_spec, "%s",
							 label_field[(i*5)].field);
					sprintf(disk_list[disk].mounts[i].fs_file, "%s",
							 label_field[(i*5)+4].field);
					sprintf(disk_list[disk].mounts[i].fs_vfstype, "%s", 
							  label_field[(i*5)+2].field);
					sprintf(disk_list[disk].mounts[i].fs_mntops, "%s",
							 label_field[(i*5)+1].field);
				}
				break;
			case F_BUTTON:
				key = button_press(window, label_field[cur_field]);
				if (!key && !strcmp(label_field[cur_field].field, "OK")) {
					done = 1;
					continue;
				}
				if (!key && !strcmp(label_field[cur_field].field, "Cancel")) {
					sprintf(errmsg, "\nUser aborted.\n");
					dialog_clear_norefresh();
					return (-1);
				}
				break;
			case F_TOGGLE:
				key = toggle_press(window, label_field[cur_field]);
				break;
			case F_TITLE:
			default:
				break;
		}
		next = change_field(label_field[cur_field], key);
		if (next == -1) {
			beep();
		} else
			cur_field = next;
	}

	if (write_bootblocks(disk) == -1)
		return(-1);

	delwin(window);
	dialog_clear();
	return(0);
}

void
display_disklabel(int disk)
{
    int i, key=0;
    WINDOW *window;
	 struct disklabel *lbl = &disk_list[disk].lbl;
    
    if (use_shadow)
	draw_shadow(stdscr, 1, 1, LINES-2, COLS-2);
    window = newwin(LINES-2, COLS-2, 1, 1);
    keypad(window, TRUE);
    
    draw_box(window, 1, 1, LINES - 2, COLS - 2, dialog_attr, border_attr);
    wattrset(window, dialog_attr);
    
    mvwprintw(window, 2, 2, "Dumping label for disk %d, %s\n", disk, lbl->d_typename);
    mvwprintw(window, 3, 2, "magic = %lu",lbl->d_magic);
    mvwprintw(window, 3, 22, "type = %x",lbl->d_type);
    mvwprintw(window, 3, 32, "subtype = %x\n",lbl->d_subtype);
    mvwprintw(window, 4, 2, "Typename = %s",lbl->d_typename);
    mvwprintw(window, 4, 38, "Packname = %s",lbl->d_packname);
    mvwprintw(window, 5, 74, "boot0 = %s",lbl->d_boot0);
    mvwprintw(window, 5, 50, "boot1 = %s\n",lbl->d_boot1);
    mvwprintw(window, 5, 2, "secsize = %ld",lbl->d_secsize);
    mvwprintw(window, 5, 20, "nsectors = %ld",lbl->d_nsectors);
    mvwprintw(window, 5, 30, "ntracks = %ld",lbl->d_ntracks);
    mvwprintw(window, 5, 50, "ncylinders = %ld\n",lbl->d_ncylinders);
    mvwprintw(window, 6, 2, "secpercyl = %ld",lbl->d_secpercyl);
    mvwprintw(window, 6, 40, "secperunit = %ld\n",lbl->d_secperunit);
    mvwprintw(window, 7, 2, "sparespertrack = %d",lbl->d_sparespertrack);
    mvwprintw(window, 7, 20, "sparespercyl = %d",lbl->d_sparespercyl);
    mvwprintw(window, 7, 40, "acylinders = %ld\n",lbl->d_acylinders);
    mvwprintw(window, 8, 2, "rpm = %d",lbl->d_rpm);
    mvwprintw(window, 8, 20, "interleave = %d",lbl->d_interleave);
    mvwprintw(window, 8, 40, "trackskew = %d",lbl->d_trackskew);
    mvwprintw(window, 8, 60, "cylskew = %d\n",lbl->d_cylskew);
    mvwprintw(window, 9, 2, "headswitch = %ld",lbl->d_headswitch);
    mvwprintw(window, 9, 30, "trkseek = %ld",lbl->d_trkseek);
    mvwprintw(window, 9, 55, "flags = %ld\n",lbl->d_flags);
    mvwprintw(window, 10, 2, "Drivedata");
    for (i=0; i< NDDATA; i++) {
	mvwprintw(window, 10, 11 + (i*10), " : %d = %ld",i,lbl->d_drivedata[i]);
    }
    mvwprintw(window, 11, 2, "Spare");
    for (i=0; i< NSPARE; i++) {
	mvwprintw(window, 11, 7 + (i*10), " : %d = %ld",i,lbl->d_spare[i]);
    }
    mvwprintw(window, 12, 2, "magic2 = %lu",lbl->d_magic2);
    mvwprintw(window, 12, 40, "checksum = %d\n",lbl->d_checksum);
    mvwprintw(window, 13, 2,  "npartitions = %d",lbl->d_npartitions);
    mvwprintw(window, 13, 25, "bbsize = %lu",lbl->d_bbsize);
    mvwprintw(window, 13, 50, "sbsize = %lu\n",lbl->d_sbsize);
    for (i=0; i< MAXPARTITIONS; i++) {
	mvwprintw(window, 14+i, 2, "%d: size: %ld",i,lbl->d_partitions[i].p_size);
	mvwprintw(window, 14+i, 20, "offset: %ld",lbl->d_partitions[i].p_offset);
	mvwprintw(window, 14+i, 36, "fsize: %ld",lbl->d_partitions[i].p_fsize);
	mvwprintw(window, 14+i, 49, "fstype: %d",lbl->d_partitions[i].p_fstype);
	mvwprintw(window, 14+i, 60, "frag: %d",lbl->d_partitions[i].p_frag);
	mvwprintw(window, 14+i, 70, "cpg: %d",lbl->d_partitions[i].p_cpg);
    }
    
    dialog_update();
    
    while (key != '\n' && key != ' ' && key != '\033')
	key = wgetch(window);
    delwin(window);
    dialog_clear();
}
