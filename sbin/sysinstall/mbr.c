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

#include <stdio.h>
#include <unistd.h>
#include <dialog.h>
#include <fcntl.h>

#ifdef __i386__ /* temp measure delete nov 15 1994 */
#define i386 1
#else
#warning FOO
#endif
#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/uio.h>

#include "mbr.h"
#include "sysinstall.h"

extern struct mbr *mbr;
extern int inst_part;
extern int whole_disk;

struct part_type part_types[] = PARTITION_TYPES

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
    return("Unknown");
}

void
read_dospart(int fd, struct dos_partition *dp)
{
    u_char buf[512];
    if (lseek(fd, 0, SEEK_SET) == -1) 
	Fatal("Couldn't seek for master boot record read\n");
    if (read(fd, buf, 512) != 512) {
	Fatal("Failed to read master boot record\n");
    }
    memcpy(dp, buf+DOSPARTOFF, sizeof(*dp)*NDOSPART);
}

void
write_dospart(int fd, struct dos_partition *dp)
{
    u_char buf[512];
    int flag;
    if (lseek(fd, 0, SEEK_SET) == -1) 
	Fatal("Couldn't seek for master boot record read\n");
    if (read(fd, buf, 512) != 512) {
	Fatal("Failed to read master boot record\n");
    }
    memcpy(buf+DOSPARTOFF, dp, sizeof(*dp)*NDOSPART);
    if (lseek(fd, 0, SEEK_SET) == -1) 
	Fatal("Couldn't seek for master boot record read\n");
    flag=1;
    if (ioctl(fd, DIOCWLABEL, &flag) < 0)
	Fatal("Couldn't enable writing of labels");
    if (write(fd, buf, 512) != 512) 
	Fatal("Failed to write master boot record\n");
    flag=0;
    if (ioctl(fd, DIOCWLABEL, &flag) < 0)
	Fatal("Couldn't disable writing of labels");
}

int
read_mbr(int fd, struct mbr *mbr)
{
    if (lseek(fd, 0, SEEK_SET) == -1) {
	sprintf(errmsg, "Couldn't seek for master boot record read\n");
	return(-1);
    }
    if (read(fd, &(mbr->bootcode), MBRSIZE) == -1) {
	sprintf(errmsg, "Failed to read master boot record\n");
	return(-1);
    }
    return(0);
}

int
write_mbr(int fd, struct mbr *mbr)
{
    if (lseek(fd, 0, SEEK_SET) == -1) {
	sprintf(errmsg, "Couldn't seek for master boot record write\n");
	return(-1);
    }
    
    enable_label(fd);
    
    if (write(fd, mbr->bootcode, MBRSIZE) == -1) {
	sprintf(errmsg, "Failed to write master boot record\n");
	return(-1);
    }
    
    disable_label(fd);
    
    return(0);
}

void
show_mbr(struct mbr *mbr)
{
    int i, j, key = 0;
    int x, y;
    WINDOW *window;
    
    if (use_shadow)
	draw_shadow(stdscr, 1, 1, LINES-2, COLS-2);
    window = newwin(LINES-2, COLS-2, 1, 1);
    keypad(window, TRUE);
    
    draw_box(window, 1, 1, LINES - 2, COLS - 2, dialog_attr, border_attr);
    wattrset(window, dialog_attr);
    
    for (i=0; i<NDOSPART/2; i++) {
	for (j=0; j<NDOSPART/2; j++) {
	    x = (j * 38) + 3;
	    y = (i * 11) + 2;
	    mvwprintw(window, y, x, "Partition %d: flags = %x",
		      (i*2)+j, mbr->dospart[(i*2)+j].dp_flag);
	    mvwprintw(window, y+1, x, "Starting at (C%d, H%d, S%d)",
		      mbr->dospart[(i*2)+j].dp_scyl,
		      mbr->dospart[(i*2)+j].dp_shd,
		      mbr->dospart[(i*2)+j].dp_ssect);
	    mvwprintw(window, y+2, x, "Type: %s (%x)",
		      part_type(mbr->dospart[(i*2)+j].dp_typ),
		      mbr->dospart[(i*2)+j].dp_typ);
	    mvwprintw(window, y+3, x, "Ending at (C%d, H%d, S%d)",
		      mbr->dospart[(i*2)+j].dp_ecyl,
		      mbr->dospart[(i*2)+j].dp_ehd,
		      mbr->dospart[(i*2)+j].dp_esect);
	    mvwprintw(window, y+4, x, "Absolute start sector %ld",
		      mbr->dospart[(i*2)+j].dp_start);
	    mvwprintw(window, y+5, x, "Size (in sectors) %ld", mbr->dospart[(i*2)+j].dp_size);
	}
    }
    dialog_update();
    
    while (key != '\n' && key != ' ' && key != '\033')
	key = wgetch(window);
    
    delwin(window);
    dialog_clear();
}

int
clear_mbr(struct mbr *mbr, char *bootcode)
{
    int i;
    int fd;
    
    /*
     * If installing to the whole disk
     * then clobber any existing bootcode.
     */
    
    sprintf(scratch, "\nLoading MBR code from %s\n", bootcode);
    dialog_msgbox(TITLE, scratch, 5, 60, 0);
    fd = open(bootcode, O_RDONLY);
    if (fd < 0) {
	sprintf(errmsg, "Couldn't open boot file %s\n", bootcode);
	return(-1);
    }  
    
    if (read(fd, mbr->bootcode, MBRSIZE) < 0) {
	sprintf(errmsg, "Couldn't read from boot file %s\n", bootcode);
	return(-1);
    }
    
    if (close(fd) == -1) {
	sprintf(errmsg, "Couldn't close boot file %s\n", bootcode);
	return(-1);
    }
    
    /* Create an empty partition table */
    
    for (i=0; i < NDOSPART; i++) {
	mbr->dospart[i].dp_flag = 0;
	mbr->dospart[i].dp_shd = 0;
	mbr->dospart[i].dp_ssect = 0;
	mbr->dospart[i].dp_scyl = 0;
	mbr->dospart[i].dp_typ = 0;
	mbr->dospart[i].dp_ehd = 0;
	mbr->dospart[i].dp_esect = 0;
	mbr->dospart[i].dp_ecyl = 0;
	mbr->dospart[i].dp_start = 0;
	mbr->dospart[i].dp_size = 0;
    }
    
    mbr->magic = MBR_MAGIC;
    
    dialog_clear();
    return(0);
}

int
build_mbr(struct mbr *mbr, char *bootcode, struct disklabel *lb)
{
    int i;
    struct dos_partition *dp = &mbr->dospart[inst_part];
    
    if (whole_disk) {
	/* Install to entire disk */
	if (clear_mbr(mbr, bootcode) == -1)
	    return(-1);
	dp->dp_scyl = 0;
	dp->dp_shd = 1;
	dp->dp_ssect = 1;
	dp->dp_ecyl = lb->d_ncylinders - 1;
	dp->dp_ehd = lb->d_ntracks - 1;
	dp->dp_esect = lb->d_nsectors;
	dp->dp_start = (dp->dp_scyl * lb->d_ntracks * lb->d_nsectors) + 
	    (dp->dp_shd * lb->d_nsectors) +
		dp->dp_ssect - 1;
	dp->dp_size =
	    (lb->d_nsectors * lb->d_ntracks * lb->d_ncylinders) - dp->dp_start;
    }
    
    /* Validate partition - XXX need to spend some time making this robust */
    if (!dp->dp_start) {
	strcpy(errmsg, "The start address of the selected partition is 0\n");
	return(-1);
    }
    
    /* Set partition type to FreeBSD and make it the only active partition */
    
    for (i=0; i < NDOSPART; i++)
	mbr->dospart[i].dp_flag &= ~ACTIVE;
    dp->dp_typ = DOSPTYP_386BSD;
    dp->dp_flag = ACTIVE;
    
    return(0);
}

void
edit_mbr(struct mbr *mbr, struct disklabel *label)
{
    
    dialog_msgbox("DOS partition table editor", 
		  "This editor is still under construction :-)", 10, 75, 1);
    show_mbr(mbr);
}

void
Fdisk()
{
    int i, j, done=0, diskno, flag;
    char buf[128];
    struct dos_partition dp[NDOSPART];
    struct disklabel *lbl;
    u_long cyl, hd, sec, tsec;
    u_long l, l1, l2, l3, l4;
    
    *buf = 0;
    i = AskEm(stdscr, "Enter number of disk to Fdisk ", buf, 2);
    printf("%d", i);
    if(i != '\n' && i != '\r') return;
    diskno = atoi(buf);
    if(!(diskno >= 0 && diskno < MAX_NO_DISKS && Dname[diskno])) return;
    lbl = Dlbl[diskno];
    lbl->d_bbsize = 8192;
    cyl = lbl->d_ncylinders;
    hd = lbl->d_ntracks;
    sec = lbl->d_nsectors;
    tsec = Dlbl[diskno]->d_partitions[RAWPART].p_size;
    cyl = tsec/(hd*sec);
    read_dospart(Dfd[diskno], dp);
    while(!done) {
	clear(); standend();
	j = 0;
	mvprintw(j++, 0, "%s -- Diskspace editor -- FDISK", TITLE);
	j++;
	mvprintw(j++, 0,
		 "Geometry:  %lu Cylinders, %lu Heads, %lu Sectors, %luMb",
		 cyl, hd, sec, (tsec+1024)/2048);
	j++;
	for(i=0;i<NDOSPART;i++, j+=3) {
	    mvprintw(j++, 0, "%d ", i+1);
#if 0
	    printw("[%02x %02x %02x %02x %02x %02x %02x %02x %08lx %08lx]\n",
		   dp[i].dp_flag, dp[i].dp_shd, dp[i].dp_ssect, dp[i].dp_scyl,
		   dp[i].dp_typ, dp[i].dp_ehd, dp[i].dp_esect, dp[i].dp_ecyl,
		   dp[i].dp_start, dp[i].dp_size);
#endif
	    if(!dp[i].dp_size) {
		printw("Unused");
		continue;
	    }
	    printw("Boot?=%s", dp[i].dp_flag == 0x80 ? "Yes" : "No ");
	    printw("   Type=%s\n", part_type(dp[i].dp_typ));
	    printw("  Phys=(c%d/h%d/s%d..c%d/h%d/s%d)",
		   DPCYL(dp[i].dp_scyl, dp[i].dp_ssect), dp[i].dp_shd,
		   DPSECT(dp[i].dp_ssect),
		   DPCYL(dp[i].dp_ecyl, dp[i].dp_esect), dp[i].dp_ehd,
		   DPSECT(dp[i].dp_esect));
	    printw("   Sector=(%lu..%lu)\n",
		   dp[i].dp_start, dp[i].dp_size + dp[i].dp_start-1);
	    printw("  Size=%lu MB, %lu Cylinders", (dp[i].dp_size+1024L)/2048L,
		   dp[i].dp_size/lbl->d_secpercyl);
	    l = dp[i].dp_size%lbl->d_secpercyl;
	    if(l) {
		printw(" + %lu Tracks", l/lbl->d_nsectors);
		l = l % lbl->d_nsectors;
		if(l) {
		    printw(" + %lu Sectors", l);
		}
	    }
	}
	mvprintw(21, 0, "Commands available:");
	mvprintw(22, 0, "(D)elete  (E)dit  (R)eread  (W)rite  (Q)uit");
	mvprintw(23, 0, "Enter Command> ");
	i=getch();
	switch(i) {

	case 'r': case 'R':
	    read_dospart(Dfd[diskno], dp);
	    break;

	case 'e': case 'E':
	    *buf = 0;
	    i = AskEm(stdscr, "Edit which Slice ? ", buf, 2);
	    if(i != '\n' && i != '\r') break;
	    l = strtol(buf, 0, 0);
	    if(l < 1 || l > NDOSPART) break;
	    l1=sec; l2=tsec;
	    for(i=0;i<NDOSPART;i++) {
		if((i+1) == l) continue;
		if(!dp[i].dp_size) continue;
		if(dp[i].dp_start > l2) continue;
		if((dp[i].dp_start + dp[i].dp_size) <= l1) continue;
		if(dp[i].dp_start > l1)
		    l3 = dp[i].dp_start - l1;
		else
		    l3 = 0;
		if(l2 > (dp[i].dp_start + dp[i].dp_size))
		    l4 = l2 - (dp[i].dp_start + dp[i].dp_size);
		else
		    l4 = 0;
		if(l3 >= l4)
		    l2 = dp[i].dp_start;
		else
		    l1 = dp[i].dp_start + dp[i].dp_size;
	    }
	    sprintf(buf, "%lu", (l2-l1+1024L)/2048L);
	    i = AskEm(stdscr, "Size of slice in MB ", buf, 10);
	    l3=strtol(buf, 0, 0) * 2048L;
	    if(!l3) break;
	    if(l3 > l2-l1)
		l3 = l2-l1;
	    if((l1+l3) % lbl->d_secpercyl) { /* Special for cyl==0 */
		l3 += lbl->d_secpercyl - ((l1+l3) % lbl->d_secpercyl);
	    }
	    if(l3+l1 > tsec)
		l3 = tsec - l1;
	    dp[l-1].dp_start=l1;
	    dp[l-1].dp_size=l3;
	    
	    l3 += l1 - 1;
	    
	    l2 = l1 / (sec*hd);
	    if(l2>1023) l2 = 1023;
	    dp[l-1].dp_scyl = (l2 & 0xff); 
	    dp[l-1].dp_ssect = (l2 >> 2) & 0xc0;
	    l1 -= l2*sec*hd;
	    l2 = l1 / sec;
	    dp[l-1].dp_shd = l2;
	    l1 -= l2*sec;
	    dp[l-1].dp_ssect |= (l1+1) & 0x3f;
	    
	    l2 = l3 / (sec*hd);
	    if(l2>1023) l2 = 1023;
	    dp[l-1].dp_ecyl = (l2 & 0xff); 
	    dp[l-1].dp_esect = (l2 >> 2) & 0xc0;
	    l3 -= l2*sec*hd;
	    l2 = l3 / sec;
	    dp[l-1].dp_ehd = l2;
	    l3 -= l2*sec;
	    dp[l-1].dp_esect |= (l3+1) & 0x3f;
	    
	    l4 = dp[l-1].dp_typ;
	    if(!l4) l4 = MBR_PTYPE_FreeBSD;
	    sprintf(buf, "0x%lx", l4);
	    i = AskEm(stdscr, "Type of slice (0xa5=FreeBSD) ", buf, 5);
	    l3 = strtol(buf, 0, 0);
	    if(l3 == MBR_PTYPE_FreeBSD) {
		for(i=0;i<NDOSPART;i++)
		    if(i != (l-1) && dp[i].dp_typ== MBR_PTYPE_FreeBSD)
			memset(&dp[i], 0, sizeof dp[i]);
		sprintf(buf, "0x80");
	    } else {
		sprintf(buf, "0");
	    }
	    dp[l-1].dp_typ=l3;
	    i = AskEm(stdscr, "Bootflag (0x80 for YES) ", buf, 5);
	    dp[l-1].dp_flag=strtol(buf, 0, 0);
	    break;

	case 'd': case 'D':
	    *buf = 0;
	    i = AskEm(stdscr, "Delete which Slice ? ",  buf,  2);
	    if(i != '\n' && i != '\r') break;
	    l = strtol(buf, 0, 0);
	    if(l < 1 || l > NDOSPART) break;
	    memset(&dp[l-1], 0, sizeof dp[l-1]);
	    break;

	case 'w': case 'W':
	    strcpy(buf, "N");
	    i = AskEm(stdscr, "Confirm write ", buf, 2);
	    if(*buf != 'y' && *buf != 'Y') break;
	    write_dospart(Dfd[diskno], dp);
	    Dlbl[diskno]->d_partitions[OURPART].p_offset = 0;
	    Dlbl[diskno]->d_partitions[OURPART].p_size = 0;
	    for(i=0;i<NDOSPART;i++) {
		if(dp[i].dp_typ == MBR_PTYPE_FreeBSD) {
		    Dlbl[diskno]->d_partitions[OURPART].p_offset = 
			dp[i].dp_start;
		    Dlbl[diskno]->d_partitions[OURPART].p_size = 
			dp[i].dp_size;
		}
	    }
	    Dlbl[diskno]->d_magic = DISKMAGIC;
	    Dlbl[diskno]->d_magic2 = DISKMAGIC;
	    Dlbl[diskno]->d_checksum = 0;
	    Dlbl[diskno]->d_checksum = dkcksum(Dlbl[diskno]);
	    flag=1;
	    enable_label(Dfd[diskno]);
	    if(ioctl(Dfd[diskno], DIOCSDINFO, Dlbl[diskno]) == -1)
		Fatal("Couldn't set label: %s", strerror(errno));
	    if(ioctl(Dfd[diskno], DIOCWDINFO, Dlbl[diskno]) == -1)
		Fatal("Couldn't write label: %s", strerror(errno));
	    flag=0;
	    disable_label(Dfd[diskno]);
	    
	    if (Dlbl[diskno]->d_partitions[OURPART].p_size) 
		build_bootblocks(Dfd[diskno], lbl, dp);
	    break;

	case 'q': case 'Q':
	    return;
	    break;
	}
    }
}
