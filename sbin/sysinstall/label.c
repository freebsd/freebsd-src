#include <sys/types.h>
#include <sys/disklabel.h>

#include <dialog.h>
#include "sysinstall.h"

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
