/* Stopgap, until Paul does the right thing */
#define ESC 27
#define TAB 9

#define DKTYPENAMES
#include <sys/param.h>
#define DKTYPENAMES
#include <sys/param.h>
#include <ufs/ffs/fs.h>
#include <sys/types.h>
#include <string.h>
#include <sys/disklabel.h>
#include <ufs/ffs/fs.h>

#include <string.h>
#include <dialog.h>
#include "sysinstall.h"

int sectstoMb(int, int);

char *partname[MAXPARTITIONS] = {"a", "b", "c", "d", "e", "f", "g", "h"};

#define EDITABLES 3
#define FSTYPE 0
#define UPARTSIZES 1
#define MOUNTPOINTS 2
struct field {
	int y;
	int x;
	int width;
	char field[80];
} field;

struct field label_fields[MAXPARTITIONS][EDITABLES];

void
setup_label_fields(struct disklabel *lbl)
{
	int i;

	for (i=0; i < MAXPARTITIONS; i++) {
		label_fields[i][0].y = 4 + (i * 2);
		label_fields[i][0].x = 15;
		label_fields[i][0].width = 15;
		sprintf(label_fields[i][0].field, "%s",
				  fstypenames[lbl->d_partitions[i].p_fstype]);
		label_fields[i][1].y = 4 + (i * 2);
		label_fields[i][1].x = 35;
		label_fields[i][1].width = 9;
		sprintf(label_fields[i][1].field, "%d",
				  sectstoMb(lbl->d_partitions[i].p_size, lbl->d_secsize));
		label_fields[i][2].y = 4 + (i * 2);
		label_fields[i][2].x = 45;
		label_fields[i][2].width = 30;
	}
		sprintf(label_fields[0][2].field, "%s", "/");
		sprintf(label_fields[1][2].field, "%s", "swap");
		sprintf(label_fields[4][2].field, "%s", "/usr");
}

void
update_label_form(WINDOW *window, struct disklabel *lbl)
{
	int i;

	mvwprintw(window, 2, 2, "Partition");
	mvwprintw(window, 2, 15, "Filesystem Type");
	mvwprintw(window, 2, 35, "Size");
	mvwprintw(window, 2, 45, "Mount point");
	for (i=0; i < MAXPARTITIONS; i++) {
		mvwprintw(window, 4+(i*2), 6, "%s", partname[i]);
		mvwprintw(window, label_fields[i][0].y, label_fields[i][0].x, "%s",
					 &label_fields[i][0].field);
		mvwprintw(window, label_fields[i][1].y, label_fields[i][1].x, "%s",
					&label_fields[i][1].field); 
		if (label_fields[i][2].field)
			mvwprintw(window, label_fields[i][2].y, label_fields[i][2].x, "%s",
						 &label_fields[i][2].field);
	}
	wrefresh(window);
}

int
edit_line(WINDOW *window, int y, int x, char *field, int width, int maxlen)
{
	int len;
	int key = 0;
	int fpos, dispos, curpos;
	int i;
	int done = 0;

	len = strlen(field);
	if (len < width) {
		fpos = len;
		curpos = len;
		dispos = 0;
	} else {
		fpos = width;
		curpos = width;
		dispos = len - width;
	};


	do {
		wattrset(window, item_selected_attr);
		wmove(window, y, x);
		for (i=0; i < width; i++)
			if (i < (len - dispos))
				waddch(window, field[dispos+i]);
			else
				waddch(window, ' ');
		wmove(window, y, x + curpos);
		wrefresh(window);

		key = wgetch(window);
		switch (key) {
			case TAB:
			case KEY_BTAB:
			case KEY_UP:
			case KEY_DOWN:
			case ESC:
			case '\n':
				done = 1;
				break;
			case KEY_HOME:
				if (len < width) {
					fpos = len;
					curpos = len;
					dispos = 0;
				} else {
					fpos = width;
					curpos = width;
					dispos = len - width;
				};
				break;
			case KEY_END:
				if (len < width) {
					dispos = 0;
					curpos = len - 1;
				} else {
					dispos = len - width - 1;
					curpos = width - 1;
				}
				fpos = len - 1;
				break;
			case KEY_LEFT:
				if ((!curpos) && (!dispos)) {
					beep();
					break;
				}
				if (--curpos < 0) {
					curpos = 0;
					if (--dispos < 0)
						dispos = 0;
				}
				if (--fpos < 0)
					fpos = 0;
				break;
			case KEY_RIGHT:
				if ((curpos + dispos) == len) {
					beep();
					break;
				}
				if ((curpos == (width-1)) && (dispos == (maxlen - width -1))) {
					beep();
					break;
				}
				if (++curpos >= width) {
					curpos = width - 1;
					dispos++;
				}
				if (dispos >= len)
					dispos = len - 1;
				if (++fpos >= len) {
					fpos = len;
				}
				break;
			case KEY_BACKSPACE:
			case KEY_DC:
				if ((!curpos) && (!dispos)) {
					beep();
					break;
				}
				if (fpos > 0) {
					memmove(field+fpos-1, field+fpos, len - fpos);
					len--;
					fpos--;
					if (curpos > 0)
						--curpos;
					if (!curpos)
						--dispos;
					if (dispos < 0)
						dispos = 0;
				} else
					beep();
				break;
			default:
				if (len < maxlen - 1) {
					memmove(field+fpos+1, field+fpos, len - fpos);
					field[fpos] = key;
					len++;
					fpos++;
					if (++curpos == width) {
						--curpos;
						dispos++;
					}
					if (len == (maxlen - 1)) {
						dispos = (maxlen - width - 1);
					}
				} else
					beep();
				break;
		}
	} while (!done);
	wattrset(window, dialog_attr);
	wmove(window, y, x);
	for (i=0; i < width; i++)
		if (i < (len - dispos))
			waddch(window, field[dispos+i]);
		else
			waddch(window, ' ');
	wmove(window, y, x + curpos);
	wrefresh(window);
	field[len] = 0;
	delwin(window);
	refresh();
	return (key);
}

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

void
default_disklabel(struct disklabel *lbl, int avail_sects, int offset)
{
	int nsects;

	/* Fill in default label entries */

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
	lbl->d_npartitions = 5;

	/* Set up c and d as raw partitions for now */
	lbl->d_partitions[2].p_size = avail_sects;
	lbl->d_partitions[2].p_offset = offset;
	lbl->d_partitions[2].p_fsize = DEFFSIZE; /* XXX */
	lbl->d_partitions[2].p_fstype = FS_UNUSED;
	lbl->d_partitions[2].p_frag = DEFFRAG;

	lbl->d_partitions[3].p_size = lbl->d_secperunit;
	lbl->d_partitions[3].p_offset = 0;
	lbl->d_partitions[3].p_fsize = DEFFSIZE;
	lbl->d_partitions[3].p_fstype = FS_UNUSED;
	lbl->d_partitions[3].p_frag = DEFFRAG;

	/* Default root */
	nsects = rndtocylbdry(Mbtosects(DEFROOTSIZE, lbl->d_secsize),
								 lbl->d_secpercyl);
	offset = rndtocylbdry(offset, lbl->d_secpercyl);

	lbl->d_partitions[0].p_size = nsects;
	lbl->d_partitions[0].p_offset = offset;
	lbl->d_partitions[0].p_fsize = DEFFSIZE;
	lbl->d_partitions[0].p_fstype = FS_BSDFFS;
	lbl->d_partitions[0].p_frag = DEFFRAG;

	avail_sects -= nsects;
	offset += nsects;
	nsects = rndtocylbdry(Mbtosects(DEFSWAPSIZE, lbl->d_secsize),
								 lbl->d_secpercyl);

	lbl->d_partitions[1].p_size = nsects;
	lbl->d_partitions[1].p_offset = offset;
	lbl->d_partitions[1].p_fsize = DEFFSIZE;
	lbl->d_partitions[1].p_fstype = FS_SWAP;
	lbl->d_partitions[1].p_frag = DEFFRAG;

	avail_sects -= nsects;
	offset += nsects;
	nsects = rndtocylbdry(Mbtosects(DEFUSRSIZE, lbl->d_secsize),
								 lbl->d_secpercyl);

	if (avail_sects > nsects)
		nsects = avail_sects;

	lbl->d_partitions[4].p_size = nsects;
	lbl->d_partitions[4].p_offset = offset;
	lbl->d_partitions[4].p_fsize = DEFFSIZE;
	lbl->d_partitions[4].p_fstype = FS_BSDFFS;
	lbl->d_partitions[4].p_frag = DEFFRAG;
}


void
edit_disklabel(struct disklabel *lbl)
{
	int key=0;
	int x_pos = 0;
	int y_pos = 0;
	WINDOW *window;

	if (use_shadow)
		draw_shadow(stdscr, 1, 1, LINES-3, COLS-5);

	window = newwin(LINES - 2, COLS - 4, 0, 0);
	keypad(window, TRUE);

	draw_box(window, 1, 1, LINES - 3, COLS - 5, dialog_attr, border_attr);
	wattrset(window, dialog_attr);

	setup_label_fields(lbl);
	do {
		update_label_form(window, lbl);
		key = edit_line(window, label_fields[y_pos][x_pos].y,
					    label_fields[y_pos][x_pos].x,
						 label_fields[y_pos][x_pos].field,
						 label_fields[y_pos][x_pos].width,
						 20);
		switch(key) {
			case KEY_UP:
				if (y_pos != 0)
					y_pos--;
				break;
			case KEY_DOWN:
				if (y_pos != MAXPARTITIONS)
					y_pos++;
				break;
			case TAB:
				x_pos++;
				if (x_pos == EDITABLES)
					x_pos = 0;
				break;
			case KEY_BTAB:
				x_pos--;
				if (x_pos < 0)
					x_pos = EDITABLES - 1;
				break;
			case '\n':
				++y_pos;
				if (y_pos == MAXPARTITIONS) {
					y_pos = 0;
					if (++x_pos == EDITABLES)
						x_pos = 0;
				}
				break;
			default:
				break;
		}
	} while (key != '\033');
	dialog_clear();
}

int
build_disklabel(struct disklabel *lbl)
{
	int i, offset;
	int nsects;
	int total_sects;
	int mounts = 0;

	/* Get start of FreeBSD partition from default label */
	offset = lbl->d_partitions[2].p_offset;

	for (i=0; i < MAXPARTITIONS; i++) {
		if (strlen(label_fields[i][MOUNTPOINTS].field) &&
		    atoi(label_fields[i][UPARTSIZES].field)) {
			sprintf(scratch, "%s%s", avail_disknames[inst_disk], partname[i]);
			devicename[mounts] = StrAlloc(scratch);
			mountpoint[mounts] = StrAlloc(label_fields[i][MOUNTPOINTS].field);
			mounts++;
			nsects = Mbtosects(atoi(label_fields[i][UPARTSIZES].field),
							 lbl->d_secsize);
#if 0  /* Rounding the offset is at best wrong */
			nsects = rndtocylbdry(nsects, lbl->d_secpercyl);
			offset = rndtocylbdry(offset, lbl->d_secpercyl);
#endif
			lbl->d_partitions[i].p_size = nsects;
			lbl->d_partitions[i].p_offset = offset;
			offset += nsects;
			total_sects += nsects;
			lbl->d_partitions[i].p_fstype = 
				getfstype(label_fields[i][FSTYPE].field);
			lbl->d_npartitions = i+1;
		} else if (i < 2 || i > 3) {
			lbl->d_partitions[i].p_size = 0;
			lbl->d_partitions[i].p_offset = 0;
			lbl->d_partitions[i].p_fstype = 0;
		}
		Debug("Part%d: %d sects, %d offset, %d end, %d type", i,
			lbl->d_partitions[i].p_size,
			lbl->d_partitions[i].p_offset,
			lbl->d_partitions[i].p_size+
				lbl->d_partitions[i].p_offset,
			lbl->d_partitions[i].p_fstype);
	}
	return 0;
}

int
getfstype(char *fstype)
{
	int i;

	for (i=0; i < FSMAXTYPES; i++)
		if (!strcasecmp(fstype, fstypenames[i]))
			return(i);
	return(FS_OTHER);
}

void
display_disklabel(int disk)
{
	int i, key=0;
	WINDOW *window;

	if (use_shadow)
		draw_shadow(stdscr, 1, 1, LINES-2, COLS-2);
	window = newwin(LINES-2, COLS-2, 1, 1);
	keypad(window, TRUE);

	draw_box(window, 1, 1, LINES - 2, COLS - 2, dialog_attr, border_attr);
	wattrset(window, dialog_attr);

	mvwprintw(window, 2, 2, "Dumping label for disk %d, %s\n", disk, avail_disklabels[disk].d_typename);
	mvwprintw(window, 3, 2, "magic = %lu",avail_disklabels[disk].d_magic);
	mvwprintw(window, 3, 22, "type = %x",avail_disklabels[disk].d_type);
	mvwprintw(window, 3, 32, "subtype = %x\n",avail_disklabels[disk].d_subtype);
	mvwprintw(window, 4, 2, "Typename = %s",avail_disklabels[disk].d_typename);
	mvwprintw(window, 4, 38, "Packname = %s",avail_disklabels[disk].d_packname);
	mvwprintw(window, 5, 74, "boot0 = %s",avail_disklabels[disk].d_boot0);
	mvwprintw(window, 5, 50, "boot1 = %s\n",avail_disklabels[disk].d_boot1);
	mvwprintw(window, 5, 2, "secsize = %ld",avail_disklabels[disk].d_secsize);
	mvwprintw(window, 5, 20, "nsectors = %ld",avail_disklabels[disk].d_nsectors);
	mvwprintw(window, 5, 30, "ntracks = %ld",avail_disklabels[disk].d_ntracks);
	mvwprintw(window, 5, 50, "ncylinders = %ld\n",avail_disklabels[disk].d_ncylinders);
	mvwprintw(window, 6, 2, "secpercyl = %ld",avail_disklabels[disk].d_secpercyl);
	mvwprintw(window, 6, 40, "secperunit = %ld\n",avail_disklabels[disk].d_secperunit);
	mvwprintw(window, 7, 2, "sparespertrack = %d",avail_disklabels[disk].d_sparespertrack);
	mvwprintw(window, 7, 20, "sparespercyl = %d",avail_disklabels[disk].d_sparespercyl);
	mvwprintw(window, 7, 40, "acylinders = %ld\n",avail_disklabels[disk].d_acylinders);
	mvwprintw(window, 8, 2, "rpm = %d",avail_disklabels[disk].d_rpm);
	mvwprintw(window, 8, 20, "interleave = %d",avail_disklabels[disk].d_interleave);
	mvwprintw(window, 8, 40, "trackskew = %d",avail_disklabels[disk].d_trackskew);
	mvwprintw(window, 8, 60, "cylskew = %d\n",avail_disklabels[disk].d_cylskew);
	mvwprintw(window, 9, 2, "headswitch = %ld",avail_disklabels[disk].d_headswitch);
	mvwprintw(window, 9, 30, "trkseek = %ld",avail_disklabels[disk].d_trkseek);
	mvwprintw(window, 9, 55, "flags = %ld\n",avail_disklabels[disk].d_flags);
	mvwprintw(window, 10, 2, "Drivedata");
	for(i=0; i< NDDATA; i++) {
		mvwprintw(window, 10, 11 + (i*10), " : %d = %ld",i,avail_disklabels[disk].d_drivedata[i]);
	}
	mvwprintw(window, 11, 2, "Spare");
	for (i=0; i< NSPARE; i++) {
		mvwprintw(window, 11, 7 + (i*10), " : %d = %ld",i,avail_disklabels[disk].d_spare[i]);
	}
	mvwprintw(window, 12, 2, "magic2 = %lu",avail_disklabels[disk].d_magic2);
	mvwprintw(window, 12, 40, "checksum = %d\n",avail_disklabels[disk].d_checksum);
	mvwprintw(window, 13, 2,  "npartitions = %d",avail_disklabels[disk].d_npartitions);
	mvwprintw(window, 13, 25, "bbsize = %lu",avail_disklabels[disk].d_bbsize);
	mvwprintw(window, 13, 50, "sbsize = %lu\n",avail_disklabels[disk].d_sbsize);
	for (i=0; i< MAXPARTITIONS; i++) {
		mvwprintw(window, 14+i, 2, "%d: size: %ld",i,avail_disklabels[disk].d_partitions[i].p_size);
		mvwprintw(window, 14+i, 20, "offset: %ld",avail_disklabels[disk].d_partitions[i].p_offset);
		mvwprintw(window, 14+i, 36, "fsize: %ld",avail_disklabels[disk].d_partitions[i].p_fsize);
		mvwprintw(window, 14+i, 49, "fstype: %d",avail_disklabels[disk].d_partitions[i].p_fstype);
		mvwprintw(window, 14+i, 60, "frag: %d",avail_disklabels[disk].d_partitions[i].p_frag);
		mvwprintw(window, 14+i, 70, "cpg: %d",avail_disklabels[disk].d_partitions[i].p_cpg);
	}

	dialog_update();

	while (key != '\n' && key != ' ' && key != '\033')
		key = wgetch(window);
	delwin(window);
	dialog_clear();
}
