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
	int key;
	int next;
	int cur_field;
	int i;
	struct disklabel *lbl = &disk_list[disk].lbl;
	int offset;
	int nsects;
	int avail_sects;

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
	 lbl->d_boot0 = boot1;
	 lbl->d_boot1 = boot2;

	if (!(window = newwin(24, 79, 0, 0))) {
		sprintf(errmsg, "Failed to open window for disklabel editor\n");
		return (-1);
	}

	keypad(window, TRUE);
    
	draw_box(window, 0, 0, 24, 79, dialog_attr, border_attr);
    
	cur_field = 1;
	while (key != ESC) {
		for (i=0; i < MAXPARTITIONS; i++) {
			sprintf(label_field[(i*5)].field, "%s%d%s", disk_list[disk].devconf->dc_name,
															  disk_list[disk].devconf->dc_unit,
															  partname[i]);
			sprintf(label_field[(i*5)+1].field, "%s",
					  disk_list[disk].mounts[i].fs_mntops);
			sprintf(label_field[(i*5)+2].field, "%s",
					  fstypenames[lbl->d_partitions[i].p_fstype]);
			sprintf(label_field[(i*5)+3].field, "%d",
			        sectstoMb(lbl->d_partitions[i].p_size,lbl->d_secsize));
			sprintf(label_field[(i*5)+4].field, "%s",
					  disk_list[disk].mounts[i].fs_file);
		}

		disp_fields(window, label_field, sizeof(label_field)/sizeof(struct field));
		key = edit_line(window, label_field[cur_field].y,
										label_field[cur_field].x,
										label_field[cur_field].field,
										label_field[cur_field].width,
										label_field[cur_field].maxlen);
		next = change_field(label_field[cur_field], key);
		if (next == -1)
			beep();
		else
			cur_field = next;

		/* Update label */
		for (i=0; i<MAXPARTITIONS; i++) {
			sprintf(disk_list[disk].mounts[i].fs_spec, "%s",
					 label_field[(i*5)].field);
			sprintf(disk_list[disk].mounts[i].fs_mntops, "%s",
					 label_field[(i*5)+1].field);
			sprintf(disk_list[disk].mounts[i].fs_file, "%s",
					 label_field[(i*5)+4].field);
		}

		avail_sects = lbl->d_partitions[OURPART].p_size;
		offset = lbl->d_partitions[OURPART].p_offset;
		for (i=0; i < MAXPARTITIONS; i++) {
			if (i == OURPART)
				continue;
			if (i == RAWPART)
				continue;
			lbl->d_partitions[i].p_offset = offset;
			nsects = atoi(label_field[(i*5)+3].field);
			nsects = Mbtosects(nsects, lbl->d_secsize);
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
	}

	if (write_bootblocks(disk) == -1)
		return(-1);

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
