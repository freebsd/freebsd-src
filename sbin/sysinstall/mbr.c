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


/* 
 * Guess what ?  This is bteasy-1.4... didn't used to look this way, did it ?
 * Written by Serge Vakulenko, <vak@kiae.su>
 */
static unsigned char bootcode[] = {
51,192,142,192,142,216,250,142,208,188,0,124,251,252,139,244,191,0,6,185,0,
1,242,165,234,99,6,0,0,139,213,88,162,72,7,60,53,116,29,180,16,246,228,5,
174,4,139,240, 246,68,4,255,116,55,198,4,128,232,215,0,138,116,1,139,76,
2,235,8,232,204,0,185,1,0,50,209,187,0,124,184,1,2,205,19,114,23,129,191,
254,1,85,170,117,15,234,0,124,0,0,139,234,66,128,242,179,136,22,58,7,191,
190,7,185,4,0,198,6,45,7,49,50,246,136,45,138,69,4,60,0,116,37,60,5,116,
33,254,198,190,42,7,232,117,0,190,72,7,70,70,139,28,10,255,116,7,58,125,4,
117,243,50,255,141,183,116,7,232,92,0,131,199,16,254,6,45,7,226,201,128,62,
117,4,2,116,11,190,59,7,10,246,117,10,205,24,235,170,190,42,7,232,59,0,232,
56,0,50,228,205,26,139,218,131,195,96,180,1,205,22,117,13,50,228,205,26,59,
211,114,242,160,72,7,235,12,50,228,205,22,138,196,60,28,116,241,4,246,60,
49,114,212,60,53,119,208,80,190,40,7,187,29,6,83,252,172,80,36,127,180,14,
205,16,88,168,128,116,242,195,86,184,1,3,187,0,6,185,1,0,50,246,205,19,94,
198,6,72,7,63,195,13,138,13,10,70,48,32,46,32,46,32,46,160,100,105,115,107,
32,49,13,10,10,68,101,102,97,117,108,116,58,32,70,63,160,0,1,0,4,0,6,3,7,7,
8,10,9,13,10,16,99,20,100,20,101,26,128,26,129,31,130,36,147,42,165,49,159,
53,117,57,82,57,219,60,242,66,0,100,111,243,104,112,102,243,102,115,32,97,
105,248,111,115,178,117,110,105,248,110,111,118,101,108,236,109,105,110,
105,248,108,105,110,117,248,97,109,111,101,98,225,70,114,101,101,66,83,196,
98,115,100,233,112,99,105,248,99,112,237,100,111,115,115,101,227,63,191,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,85,170
};

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
	AskAbort("Couldn't seek for master boot record read\n");
    if (read(fd, buf, 512) != 512) {
	AskAbort("Failed to read master boot record\n");
    }
    memcpy(dp, buf+DOSPARTOFF, sizeof(*dp)*NDOSPART);
}

void
write_dospart(int fd, struct dos_partition *dp)
{
    u_char buf[512];

    if (lseek(fd, 0, SEEK_SET) == -1) 
	AskAbort("Couldn't seek for master boot record read\n");
    if (read(fd, buf, 512) != 512) {
	AskAbort("Failed to read master boot record\n");
    }
    memcpy(buf+DOSPARTOFF, dp, sizeof(*dp)*NDOSPART);
    buf[510] = 0x55;
    buf[511] = 0xaa;
    if (lseek(fd, 0, SEEK_SET) == -1) 
	AskAbort("Couldn't seek for master boot record write\n");
    enable_label(fd);
    if (write(fd, buf, 512) != 512) 
	AskAbort("Failed to write master boot record\n");
    disable_label(fd);
}

void
write_bootcode(int fd)
{
    u_char buf[512];

    if (lseek(fd, 0, SEEK_SET) == -1) 
	AskAbort("Couldn't seek for master boot record read\n");
    if (read(fd, buf, 512) != 512) {
	AskAbort("Failed to read master boot record\n");
    }
    memcpy(buf, bootcode, DOSPARTOFF);
    buf[510] = 0x55;
    buf[511] = 0xaa;
    if (lseek(fd, 0, SEEK_SET) == -1) 
	AskAbort("Couldn't seek for master boot record write\n");
    enable_label(fd);
    if (write(fd, buf, 512) != 512) 
	AskAbort("Failed to write master boot record\n");
    disable_label(fd);
}

int
WriteBootblock(int dfd,struct disklabel *label,struct dos_partition *dospart)
{
    int fd;
    off_t of = label->d_partitions[OURPART].p_offset;
    u_char bootblocks[BBSIZE];

    Debug("Loading boot code from %s", BOOT1);

    fd = open(BOOT1, O_RDONLY);
    if (fd < 0) 
	Fatal("Couldn't open boot file %s\n", BOOT1);

    if (read(fd, bootblocks, MBRSIZE) < 0) 
	Fatal("Couldn't read from boot file %s\n", BOOT1);

    if (close(fd) == -1) 
	Fatal("Couldn't close boot file %s\n", BOOT1);

    Debug("Loading boot code from %s", BOOT2);

    fd = open(BOOT2, O_RDONLY);
    if (fd < 0) 
	Fatal("Couldn't open boot file %s", BOOT2);

    if (read(fd, &bootblocks[MBRSIZE], (int)(label->d_bbsize - MBRSIZE)) < 0) 
	Fatal("Couldn't read from boot file %s\n", BOOT2);

    if (close(fd) == -1) 
	Fatal("Couldn't close boot file %s", BOOT2);

    bcopy(dospart, &bootblocks[DOSPARTOFF],
	  sizeof(struct dos_partition) * NDOSPART);

    label->d_checksum = 0;
    label->d_checksum = dkcksum(label);
    bcopy(label, &bootblocks[(LABELSECTOR * label->d_secsize) + LABELOFFSET],
		    sizeof *label);

    Debug("Seeking to byte %ld ", of * label->d_secsize);

    if (lseek(dfd, (of * label->d_secsize), SEEK_SET) < 0) {
	    Fatal("Couldn't seek to start of partition\n");
    }

    enable_label(dfd);

    if (write(dfd, bootblocks, label->d_bbsize) != label->d_bbsize) {
	    Fatal("Failed to write bootblocks (%p,%d) %d %s\n",
		    bootblocks, label->d_bbsize,
		    errno, strerror(errno)
		    );
    }

    disable_label(dfd);

    return(0);
}

static int
FillIn(struct dos_partition *dp, int sec, int hd)
{
    u_long l2,l3=0,sect,c,s,h;

    sect = dp->dp_start;
    l2 = sect / (sec*hd);
    sect -= l2*sec*hd;
    if(l2>1023) l2 = 1023;
    c = (l2 & 0xff); 
    s = (l2 >> 2) & 0xc0;
    l2 = sect / sec;
    h = l2;
    sect -= l2*sec;
    s |= (sect+1) & 0x3f;
#define NIC(a,b) if (a != b) {a = b; l3++;}
    NIC(dp->dp_ssect, s);
    NIC(dp->dp_scyl, c);
    NIC(dp->dp_shd, h);

    sect = dp->dp_start + dp->dp_size-1;
    l2 = sect / (sec*hd);
    sect -= l2*sec*hd;
    if(l2>1023) l2 = 1023;
    c = (l2 & 0xff); 
    s = (l2 >> 2) & 0xc0;
    l2 = sect / sec;
    h = l2;
    sect -= l2*sec;
    s |= (sect+1) & 0x3f;
    NIC(dp->dp_esect, s);
    NIC(dp->dp_ecyl, c);
    NIC(dp->dp_ehd, h);
#undef NIC
    return l2;
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
    int changed = 0;
    char *grumble = NULL;
    
    *buf = 0;
    i = AskEm(stdscr, "Enter number of disk to Fdisk> ", buf, 2);
    printf("%d", i);
    if(i != '\n' && i != '\r') return;
    diskno = atoi(buf);
    if(!(diskno >= 0 && diskno < MAX_NO_DISKS && Dname[diskno])) return;
    lbl = Dlbl[diskno];
    lbl->d_bbsize = 8192;
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
		 "Disk: %s   Geometry:  %lu Cyl * %lu Hd * %lu Sect",
		 Dname[diskno], cyl, hd, sec);
	printw(" = %luMb = %lu Sect", (tsec+1024)/2048, tsec);
	j++;
	for(i=0;i<NDOSPART;i++, j+=4) {
	    mvprintw(j, 0, "%d ", i+1);
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
	mvprintw(20, 0, "Commands available:   ");
	mvprintw(21, 0, "(H)elp   (T)utorial   (D)elete   (E)dit   (R)eread   (W)rite MBR   (Q)uit");
	mvprintw(22, 0, "(U)se entire disk for FreeBSD   (G)eometry   Write MBR (B)ootcode");
	if (grumble) {
	    standout();
 	    mvprintw(24, 0, grumble);
	    standend();
	    grumble = NULL;
	} else if (changed) {
	    standout();
 	    mvprintw(24, 0, "Use (W)rite to save changes to disk");
	    standend();
	}
	mvprintw(23, 0, "Enter Command> ");
	i=getch();
	switch(i) {

	case 'h': case 'H':
	    clear();
	    mvprintw(0, 0, 
"%s -- Diskspace editor -- FDISK -- Command Help

Basic commands:

(H)elp          - This screen
(T)utorial      - A more detailed discussion of MBR's, disklabels, etc.
(D)elete        - Delete an existing partition
(E)dit          - Edit an existing partition
(R)eread        - Read fdisk information from disk again, abandoning changes
(W)rite MBR     - Write modified fdisk information to disk
(Q)uit          - Exit the FDISK editor

Advanced commands:

(U)se entire disk for FreeBSD   - Assign ALL disk space on current drive
(G)eometry                      - Edit the default disk geometry settings
Write MBR (B)ootcode            - Install multi-OS bootmanager.


Press any key to return to FDISK editor...
", TITLE);
	    getch();
	    break;
	case 't': case 'T':
            ShowFile(HELPME_FILE,"Help file for disklayout");
	    break;

	case 'r': case 'R':
	    read_dospart(Dfd[diskno], dp);
	    changed=0;
	    break;

	case 'b': case 'B':
	    write_bootcode(Dfd[diskno]);
	    grumble = "Wrote boot manager"; 
	    break;

	case 'e': case 'E':
	    *buf = 0;
	    i = AskEm(stdscr, "Edit which Slice> ", buf, 2);
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
	    i = AskEm(stdscr, "Size of slice in MB> ", buf, 10);
	    l3=strtol(buf, 0, 0) * 2048L;
	    if(!l3) break;
	    if(l3 > l2-l1)
		l3 = l2-l1;
	    if((l1+l3) % lbl->d_secpercyl) { /* Special for cyl==0 */
		l3 += lbl->d_secpercyl - ((l1+l3) % lbl->d_secpercyl);
	    }
	    if(l3+l1 > tsec)
		l3 = tsec - l1;
	    changed=1;
	    dp[l-1].dp_start=l1;
	    dp[l-1].dp_size=l3;
	    FillIn(&dp[l-1],sec,hd);
	    
	    l4 = dp[l-1].dp_typ;
	    if(!l4) l4 = MBR_PTYPE_FreeBSD;
	    sprintf(buf, "0x%lx", l4);
	    i = AskEm(stdscr, "Type of slice (0xa5=FreeBSD)> ", buf, 5);
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
	    i = AskEm(stdscr, "Bootflag (0x80 for YES)> ", buf, 5);
	    dp[l-1].dp_flag=strtol(buf, 0, 0);
	    if(dp[l-1].dp_flag)
		    for(i=0;i<NDOSPART;i++)
			    if(i != (l-1))
				    dp[i].dp_flag = 0;
	    break;

	case 'u': case 'U':
	    memset(&dp[0], 0, sizeof dp);
	    changed=1;

	    dp[0].dp_start = 0;
	    dp[0].dp_size = tsec;
	    FillIn(&dp[0],sec,hd);

	    dp[0].dp_typ = MBR_PTYPE_FreeBSD;
	    dp[0].dp_flag = 0x80;
	    break;

	case 'g': case 'G':
	    sprintf(buf,"%lu",sec);
	    i = AskEm(stdscr, "Number of Sectors> ",buf,7);
	    l2 = strtoul(buf,0,0);
	    if(l2 != sec)
		changed++;
	    sec=l2;
	    sprintf(buf,"%lu",hd);
	    i = AskEm(stdscr, "Number of Heads> ",buf,7);
	    l2 = strtoul(buf,0,0);
	    if(l2 != hd)
		changed++;
	    hd=l2;
	    cyl = tsec/(hd*sec);
	    sprintf(buf,"%lu",cyl);
	    i = AskEm(stdscr, "Number of Cylinders> ",buf,7);
	    l2 = strtoul(buf,0,0);
	    if(l2 != cyl)
		changed++;
	    cyl=l2;

	    for (l=0;l<NDOSPART;l++) {
		if (!dp[l].dp_typ || !dp[l].dp_size)
		    continue;
		changed += FillIn(&dp[l], sec, hd);
	    }
	    
	    break;
	    
	case 'd': case 'D':
	    *buf = 0;
	    i = AskEm(stdscr, "Delete which Slice> ",  buf,  2);
	    if(i != '\n' && i != '\r') break;
	    l = strtol(buf, 0, 0);
	    if(l < 1 || l > NDOSPART) break;
	    memset(&dp[l-1], 0, sizeof dp[l-1]);
	    changed=1;
	    break;

	case 'w': case 'W':
	    strcpy(buf, "N");
	    i = AskEm(stdscr, "Confirm write> ", buf, 2);
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
	    Dlbl[diskno]->d_ntracks = hd;
	    Dlbl[diskno]->d_nsectors = sec;
	    Dlbl[diskno]->d_ncylinders = cyl;
	    Dlbl[diskno]->d_secpercyl = hd*sec;
	    Dlbl[diskno]->d_magic = DISKMAGIC;
	    Dlbl[diskno]->d_magic2 = DISKMAGIC;
	    Dlbl[diskno]->d_checksum = 0;
	    Dlbl[diskno]->d_checksum = dkcksum(Dlbl[diskno]);
	    flag=1;
	    enable_label(Dfd[diskno]);
	    if(ioctl(Dfd[diskno], DIOCSDINFO, Dlbl[diskno]) == -1)
		AskAbort("Couldn't set label: %s", strerror(errno));
	    if(ioctl(Dfd[diskno], DIOCWDINFO, Dlbl[diskno]) == -1)
		AskAbort("Couldn't write label: %s", strerror(errno));
	    flag=0;
	    disable_label(Dfd[diskno]);
	    changed=0;
	    
	    if (Dlbl[diskno]->d_partitions[OURPART].p_size) {
		WriteBootblock(Dfd[diskno], lbl, dp);
		grumble = "Wrote MBR and disklabel to disk";
	    } else {
		grumble = "Wrote MBR to disk";
	    }

	    break;

	case 'q': case 'Q':
	    return;
	    break;
	default:
	    beep();
	    break;
	}
    }
}

