/* Stopgap, until Paul does the right thing */
#define ESC 27
#define TAB 9

#include <stdlib.h>
#include <limits.h>
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

/* Forward decls */
int disk_size(struct disklabel *);
int getfstype(char *);
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

int allocated_space;
int ourpart_offset;
int ourpart_size;

void
yelp(char *str)
{
    standout();
    mvprintw(24, 0, str);
    standend();
    beep();
}

void
setup_label_fields(struct disklabel *lbl)
{
    int i;
    
    for (i = 0; i < MAXPARTITIONS; i++) {
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
    long used = 0,ul;
    
    mvwprintw(window, 2, 2, "Partition");
    mvwprintw(window, 2, 15, "Filesystem Type");
    mvwprintw(window, 2, 35, "Size");
    mvwprintw(window, 2, 45, "Mount point");
    for (i=0; i < MAXPARTITIONS; i++) {
	mvwprintw(window, 4+(i*2), 6, "%s", partname[i]);
	mvwprintw(window, label_fields[i][0].y, label_fields[i][0].x, "%s",
		  &label_fields[i][0].field);
	if (i < 2 || i > 3) {
	    ul = strtol(label_fields[i][1].field,0,0);
	    sprintf(label_fields[i][1].field, "%lu",ul);
	    used += ul;
	}
	mvwprintw(window, label_fields[i][1].y, label_fields[i][1].x, "%-5s",
		  &label_fields[i][1].field); 
	if (label_fields[i][2].field)
	    mvwprintw(window, label_fields[i][2].y, label_fields[i][2].x, "%s",
		      &label_fields[i][2].field);
    }
    mvwprintw(window, 20, 10, "Allocated %5luMb, Unallocated %5lu Mb",
	      used, disk_size(lbl) - used);
    
    wrefresh(window);
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
	    if (++y_pos == MAXPARTITIONS)
		y_pos--;
	    break;
	case '\n':
	case TAB:
	    x_pos++;
	    if (x_pos == EDITABLES) {
		x_pos = 0;
		if (++y_pos == MAXPARTITIONS)
		    y_pos--;
	    }
	    break;
	case KEY_BTAB:
	    x_pos--;
	    if (x_pos < 0) {
		x_pos = EDITABLES - 1;
		if (--y_pos < 0)
		    y_pos++;
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
    
    /* Get start of FreeBSD partition from default label */
    offset = lbl->d_partitions[2].p_offset;
    
    for (i=0; i < MAXPARTITIONS; i++) {
	if (strlen(label_fields[i][MOUNTPOINTS].field) &&
	    atoi(label_fields[i][UPARTSIZES].field)) {
	    sprintf(scratch, "%s%s", avail_disknames[inst_disk], partname[i]);
	    Fname[Nfs] = StrAlloc(scratch);
	    Fmount[Nfs] = StrAlloc(label_fields[i][MOUNTPOINTS].field);
	    Nfs++;
	    nsects = Mbtosects(atoi(label_fields[i][UPARTSIZES].field),
			       lbl->d_secsize);
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
    for (i=0; i< NDDATA; i++) {
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

static int
AskWhichPartition(char *prompt)
{
    char buf[10];
    int i;
    *buf = 0;
    i = AskEm(stdscr, prompt, buf, 2);
    if (i != '\n' && i != '\r') return -1;
    if (!strchr("abefghABEFGH",*buf)) return -1;
    return tolower(*buf) - 'a';
}

void
DiskLabel()
{
    int i, j, done = 0, diskno, flag, k;
    char buf[128],*p;
    struct disklabel *lbl, olbl;
    u_long cyl, hd, sec, tsec;
    u_long l1, l2, l3, l4;
    char *yip = NULL;
    
    *buf = 0;
    i = AskEm(stdscr, "Enter number of disk to Disklabel> ", buf, 2);
    printf("%d", i);
    if (i != '\n' && i != '\r') return;
    diskno = atoi(buf);
    if (!(diskno >= 0 && diskno < MAX_NO_DISKS && Dname[diskno]))
	return;
    olbl = *Dlbl[diskno];
    lbl = &olbl;
    cyl = lbl->d_ncylinders;
    hd = lbl->d_ntracks;
    sec = lbl->d_nsectors;
    tsec = lbl->d_secperunit;
    for (i = lbl->d_npartitions; i < MAXPARTITIONS; i++) {
       lbl->d_partitions[i].p_offset = 0;
       lbl->d_partitions[i].p_size = 0;
       lbl->d_partitions[i].p_fstype = 0;
    }
    lbl->d_npartitions = MAXPARTITIONS;

    if(Dname[diskno][0] == 's' && Dname[diskno][1] == 'd')
        lbl->d_type = DTYPE_SCSI;
    else
        lbl->d_type = DTYPE_ST506;
 
    while(!done) {
	clear(); standend();
	if (yip) {
		yelp(yip);
		yip = NULL;
	}
	j = 0;
	mvprintw(j++, 0, "%s -- Diskspace editor -- DISKLABEL",  TITLE);
	j++;

	allocated_space = 0;
	ourpart_size = lbl->d_partitions[OURPART].p_size;
	ourpart_offset = lbl->d_partitions[OURPART].p_offset;

        mvprintw(j++, 0, "Part  Start       End    Blocks     MB   Type       Mountpoint");
	for (i = 0; i < MAXPARTITIONS; i++) {
	    mvprintw(j++, 0, "%c ", 'a'+i);
	    printw(" %8u  %8u  %8u  %5u  ",
		   lbl->d_partitions[i].p_offset,
		   lbl->d_partitions[i].p_offset+
		   (lbl->d_partitions[i].p_size ? 
		    lbl->d_partitions[i].p_size-1 : 0),
		   lbl->d_partitions[i].p_size,
		   (lbl->d_partitions[i].p_size + 1024)/2048);
	    
	    k = lbl->d_partitions[i].p_fstype;
	    if (k > FSMAXTYPES)
		printw("      %04x  ", k);
	    else
		printw("%-10s  ", fstypenames[k]);
	    if (i == OURPART)
		printw("<Entire FreeBSD slice>");
	    else if (i == RAWPART)
		printw("<Entire Disk>");
	    else {
		if (Fmount[MP[diskno][i]])
		    printw(Fmount[MP[diskno][i]]);
		if ((lbl->d_partitions[i].p_offset >= ourpart_offset) &&
		    ((lbl->d_partitions[i].p_offset +
		     lbl->d_partitions[i].p_size) <= 
		     (ourpart_offset + ourpart_size)))
		    allocated_space += lbl->d_partitions[i].p_size;
	    }
	}
	mvprintw(18, 0, "Total size:      %d blocks", ourpart_size);
	mvprintw(19, 0, "Space allocated: %d blocks", allocated_space);
	mvprintw(21, 0, "Commands available:");
	mvprintw(22, 0, "(H)elp  (S)ize  (M)ountpoint  (D)elete  (R)eread  (W)rite  (Q)uit");
	mvprintw(23, 0, "Enter Command> ");
	i=getch();
	switch(i) {
	case 'h': case 'H':
            ShowFile(HELPME_FILE,"Help file for disklayout");
	    break;
	case 'd': case 'D':
	    j = AskWhichPartition("Delete which partition? ");
	    if (j < 0) {
		yip = "Invalid partition";
		break;
	    }
	    CleanMount(diskno, j);
	    lbl->d_partitions[j].p_fstype = FS_UNUSED;
	    lbl->d_partitions[j].p_size = 0;
	    lbl->d_partitions[j].p_offset = 0;
	    break;

	case 's': case 'S':
	    j = AskWhichPartition("Change size of which partition? ");
	    if (j < 0) {
		yip = "Invalid partition";
		break;
	    }
	    if (lbl->d_partitions[j].p_fstype != FS_BSDFFS &&
	       lbl->d_partitions[j].p_fstype != FS_UNUSED &&
	       lbl->d_partitions[j].p_fstype != FS_SWAP) {
		yip = "Invalid partition type";
		break;
	    }
	    if (lbl->d_partitions[OURPART].p_size == 0) {
		yip = "No FreeBSD partition defined?";
		break;
	    }
	    l1=lbl->d_partitions[OURPART].p_offset;
	    l2=lbl->d_partitions[OURPART].p_offset +
		lbl->d_partitions[OURPART].p_size;
	    for (i = 0; i < MAXPARTITIONS; i++) {
		if (i == OURPART) continue;
		if (i == RAWPART) continue;
		if (i == j) continue;
		if (lbl->d_partitions[i].p_size == 0) continue;
		if (lbl->d_partitions[i].p_offset >= l2) continue;
		if ((lbl->d_partitions[i].p_offset+
		    lbl->d_partitions[i].p_size) <= l1) continue;
		l3 = lbl->d_partitions[i].p_offset - l1;
		l4 = l2 - (lbl->d_partitions[i].p_offset+
			   lbl->d_partitions[i].p_size);
		if (l3 > 0 && l3 >= l4)
		    l2 = l1+l3;
		else if (l4 > 0 && l4 > l3)
		    l1 = l2-l4;
		else
		    l2 = l1;
	    }
	    if (!(l2 - l1)) {
		yip = "Sizes unchanged - couldn't find room";
		break;
	    }
	    sprintf(buf, "%lu", (l2-l1+1024L)/2048L);
	    i = AskEm(stdscr, "Size of partition in MB> ", buf, 10);
	    l3= strtol(buf, 0, 0) * 2048L;
	    if (!l3) {
		yip = "Invalid size given";
		break;
	    }
	    if (l3 > l2 - l1)
		l3 = l2 - l1;
	    lbl->d_partitions[j].p_size = l3;
	    lbl->d_partitions[j].p_offset = l1;
	    if (j == 1)
		lbl->d_partitions[j].p_fstype = FS_SWAP;
	    else
		lbl->d_partitions[j].p_fstype = FS_BSDFFS;
	    break;

	case 'r': case 'R':
	    olbl = *Dlbl[diskno];
	    /* XXX be more selective here */
	    for (i = 0; i < MAXPARTITIONS; i++) 
		CleanMount(diskno, i);
	    break;

	case 'm': case 'M':
	    if (memcmp(lbl, Dlbl[diskno], sizeof *lbl)) {
		yip = "Please (W)rite changed partition information first";
		break;
	    }
	    j = AskWhichPartition("Mountpoint of which partition ? ");
	    if (j < 0) {
		yip = "Invalid partition";
		break;
	    }
	    k = lbl->d_partitions[j].p_fstype;
	    if (k != FS_BSDFFS && k != FS_MSDOS && k != FS_SWAP) {
		yip = "Invalid partition type";
		break;
	    }
	    if (!lbl->d_partitions[j].p_size) {
		yip = "Zero partition size";
		break;
	    }
	    if (k == FS_SWAP)
		strcpy(buf, "swap");
	    else if (Fmount[MP[diskno][j]]) 
		strcpy(buf, Fmount[MP[diskno][j]]);
	    else
		*buf = 0;
	    if (k != FS_SWAP) {
		i = AskEm(stdscr, "Mount on directory> ", buf, 28);
		if (i != '\n' && i != '\r') {
		    yip ="Invalid directory name";
		    break;
		}
	    }
	    CleanMount(diskno, j);
	    p = SetMount(diskno,j,buf);
	    yip = p;
	    break;

	case 'w': case 'W':
	    *Dlbl[diskno] = *lbl;
	    Dlbl[diskno]->d_magic = DISKMAGIC;
	    Dlbl[diskno]->d_magic2 = DISKMAGIC;
	    Dlbl[diskno]->d_checksum = 0;
	    Dlbl[diskno]->d_checksum = dkcksum(Dlbl[diskno]);
	    *lbl = *Dlbl[diskno];
	    enable_label(Dfd[diskno]);
	    if (ioctl(Dfd[diskno], DIOCSDINFO, Dlbl[diskno]) == -1)
		Fatal("Couldn't set label: %s", strerror(errno));
	    if (ioctl(Dfd[diskno], DIOCWDINFO, Dlbl[diskno]) == -1)
		Fatal("Couldn't write label: %s", strerror(errno));
	    disable_label(Dfd[diskno]);
	    yip = "Label written successfully.";
	    break;

	case 'q': case 'Q':
	    if (!memcmp(lbl, Dlbl[diskno], sizeof *lbl))
		return;
	    /* XXX be more selective here */
	    for (i = 0; i < MAXPARTITIONS; i++)
		CleanMount(diskno, i);
	    return;
	    break;
	}
    }
}
