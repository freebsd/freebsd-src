/*
 * $Id: label.c,v 1.23.2.1 1994/11/21 03:12:01 phk Exp $
 */

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
    int i, j, done = 0, diskno, k;
    char buf[128],*p;
    struct disklabel *lbl, olbl;
    struct dos_partition dp[NDOSPART];
    
    u_long cyl, hd, sec, tsec;
    u_long l1, l2, l3, l4;
    char *yip = NULL;
    u_long allocated_space, ourpart_size, ourpart_offset;
    
    *buf = 0;
    i = AskEm(stdscr, "Enter number of disk to Disklabel> ", buf, 3);
    Debug("%d", i);
    if (i != '\n' && i != '\r') return;
    diskno = atoi(buf);
    if (!(diskno >= 0 && diskno < MAX_NO_DISKS && Dname[diskno])) {
	return;
    }
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
		standout();
		mvprintw(24, 0, yip);
		standend();
		beep();
		yip = NULL;
	}
	j = 0;
	mvprintw(j++, 0, "%s -- Diskspace editor -- DISKLABEL",  TITLE);
	j++;

	allocated_space = 0;
	ourpart_size = lbl->d_partitions[OURPART].p_size;
	ourpart_offset = lbl->d_partitions[OURPART].p_offset;

        mvprintw(j++, 0, "Part  Start       End    Blocks     MB  Type    Action  Mountpoint");
	for (i = 0; i < MAXPARTITIONS; i++) {
	    refresh();
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
		printw("%04x   ", k);
	    else
		printw("%-7.7s ", fstypenames[k]);

	    if(!MP[diskno][i])
		printw("        ");
	    else if(!strcmp(Ftype[MP[diskno][i]],"swap"))
		printw("swap    ");
	    else if(Faction[MP[diskno][i]]) 
		printw("newfs   ");
	    else
		printw("mount   ");
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
	mvprintw(17, 0, "Total size:      %8lu blocks  %5luMb",
	    ourpart_size, (ourpart_size + 1024)/2048);
	mvprintw(18, 0, "Space allocated: %8lu blocks  %5luMb",
	    allocated_space, (allocated_space + 1024)/2048);
	mvprintw(20, 0, "Commands available:   ");
	if (memcmp(lbl, Dlbl[diskno], sizeof *lbl)) {
	    standout();
 	    printw("Use (W)rite to save changes to disk");
	    standend();
	}
	mvprintw(21, 0, "(H)elp  (T)utorial  (E)dit  (A)ssign  (D)elete  (R)eread  (W)rite  (Q)uit");
	mvprintw(22, 0, "(P)reserve  (S)lice");
	mvprintw(23, 0, "Enter Command> ");
	i=getch();
	switch(i) {
        case 'h': case 'H': case '?':
	    clear();
	    mvprintw(0, 0, 
"%s -- Diskspace editor -- DISKLABEL -- Command Help

Basic commands:

(H)elp          - This screen
(T)utorial      - More detailed information on MBRs, disklabels, etc.
(E)dit          - Edit an existing disklabel entry
(A)ssign	- Assign a filesystem (or swap) to a partition
(D)elete        - Delete an existing disklabel entry
(R)eread        - Re-read disklabel from disk, discarding any changes
(W)rite         - Write updated disklabel information to disk
(P)reserve      - Don't newfs the filesystem (preserve old contents)
(S)lice         - Import foreign slice from MBR (for example, DOS)
(Q)uit          - Exit from the disklabel editor
            
Press any key to return to Disklabel editor...
", TITLE);
	    getch();
	    break;
	case 't': case 'T':
            ShowFile(HELPME_FILE,"Help file for disklayout");
	    break;
	case 'd': case 'D':
	    j = AskWhichPartition("Delete which partition> ");
	    if (j < 0) {
		yip = "Invalid partition";
		break;
	    }
	    CleanMount(diskno, j);
	    lbl->d_partitions[j].p_fstype = FS_UNUSED;
	    lbl->d_partitions[j].p_size = 0;
	    lbl->d_partitions[j].p_offset = 0;
	    break;

	case 'p': case 'P':
	    j = AskWhichPartition("Preserve which partition> ");
	    if (j < 0) {
		yip = "Invalid partition";
		break;
	    }
	    if (!MP[diskno][j]) {
		yip = "Unmounted partitions are preserved by default";
		break;
	    }
	    if (lbl->d_partitions[j].p_fstype == FS_SWAP) {
		yip = "swap partitions cannot be preserved.";
		break;
	    }
	    if (lbl->d_partitions[j].p_fstype != FS_BSDFFS) {
		yip = "All non-ufs partitions are preserved by default.";
		break;
	    }
	    if (!fixit && !strcmp(Fmount[MP[diskno][j]],"/")) {
		yip = "/ cannot be preserved.";
		break;
	    }
	    if (!fixit && !strcmp(Fmount[MP[diskno][j]],"/usr")) {
		yip = "/usr cannot be preserved.";
		break;
	    }
	    if (!fixit && !strcmp(Fmount[MP[diskno][j]],"/var")) {
		yip = "/var cannot be preserved.";
		break;
	    }
	    Faction[MP[diskno][j]] = 1 - Faction[MP[diskno][j]];
	    break;

	case 's': case 'S':
	    read_dospart(Dfd[diskno],&dp[0]);
	    *buf = 0;
	    j = AskEm(stdscr,"Import which fdisk slice> ",buf,4);
	    if(j != '\n' && j != '\r') 
		break;
	    i = strtoul(buf,0,0);
	    if (i < 1 || i > 4) {
		yip = "Invalid slice, must be '1' to '4'";
	        break;
	    }
	    if (!dp[i-1].dp_size) {
	        yip = "empty slice cannot be imported";
		break;
	    }
	    j = AskWhichPartition("Import on which partition> ");
	    if (j < 0) {
		yip = "Invalid partition";
		break;
	    }
	    CleanMount(diskno, j);
	    lbl->d_partitions[j].p_offset = dp[i-1].dp_start;
	    lbl->d_partitions[j].p_size = dp[i-1].dp_size;
	    switch (dp[i-1].dp_typ) {
		case 0x01:
		case 0x04:
		case 0x06:
		    lbl->d_partitions[j].p_fstype=FS_MSDOS;
		    break;
		default:
		    lbl->d_partitions[j].p_fstype=FS_OTHER;
		    break;
	    }
	    break;
	
	case 'e': case 'E':
	    j = AskWhichPartition("Change size of which partition> ");
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

	case 'a': case 'A':
	    if (memcmp(lbl, Dlbl[diskno], sizeof *lbl)) {
		yip = "Please (W)rite changed partition information first";
		break;
	    }
	    j = AskWhichPartition("Assign which partition> ");
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
		i = AskEm(stdscr, "Directory mountpoint> ", buf, 28);
		if (i != '\n' && i != '\r') 
		    break;
		p = buf + strlen(buf) - 1;
		
		while (isspace(*p) && p >= buf)
		    *p-- = '\0';
		if (*buf && *buf != '/') {
		    yip = "Mountpoint must start with a '/'";
		    break;
		}
	    }
	    CleanMount(diskno, j);
	    if (!*buf)
		break;
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
