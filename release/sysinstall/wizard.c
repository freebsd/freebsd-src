/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: wizard.c,v 1.8 1996/04/28 20:54:11 jkh Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include "sysinstall.h"

u_char mbr[] = {
    250,51,192,142,208,188,0,124,139,244,80,7,80,31,251,252,191,0,6,185,0,1,
    242,165,234,29,6,0,0,190,190,7,179,4,128,60,128,116,14,128,60,0,117,28,
    131,198,16,254,203,117,239,205,24,139,20,139,76,2,139,238,131,198,16,254,
    203,116,26,128,60,0,116,244,190,139,6,172,60,0,116,11,86,187,7,0,180,14,
    205,16,94,235,240,235,254,191,5,0,187,0,124,184,1,2,87,205,19,95,115,12,
    51,192,205,19,79,117,237,190,163,6,235,211,190,194,6,191,254,125,129,61,
    85,170,117,199,139,245,234,0,124,0,0,73,110,118,97,108,105,100,32,112,97,
    114,116,105,116,105,111,110,32,116,97,98,108,101,0,69,114,114,111,114,32,
    108,111,97,100,105,110,103,32,111,112,101,114,97,116,105,110,103,32,115,
    121,115,116,101,109,0,77,105,115,115,105,110,103,32,111,112,101,114,97,
    116,105,110,103,32,115,121,115,116,101,109,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128,
    1,1,0,4,15,63,60,63,0,0,0,241,239,0,0,0,0,1,61,5,15,63,243,48,240,0,0,144,
    208,2,0,0,0,1,244,165,15,63,170,192,192,3,0,144,208,2,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,85,170
};

u_char bteasy17[] = {
    51,192,142,192,142,216,142,208,188,0,124,252,139,244,191,0,6,185,0,1,242,
    165,234,96,6,0,0,139,213,88,162,72,7,60,53,116,28,180,16,246,228,5,174,
    4,150,246,68,4,255,116,62,198,4,128,232,218,0,138,116,1,139,76,2,235,8,
    232,207,0,185,1,0,50,209,187,0,124,184,1,2,205,19,114,30,129,191,254,1,
    85,170,117,22,234,0,124,0,0,128,250,129,116,2,178,128,139,234,66,128,242,
    179,136,22,58,7,191,190,7,185,4,0,198,6,45,7,49,50,246,136,45,138,69,4,
    60,0,116,35,60,5,116,31,254,198,190,42,7,232,113,0,190,72,7,70,70,139,28,
    10,255,116,5,50,125,4,117,243,141,183,114,7,232,90,0,131,199,16,254,6,45,
    7,226,203,128,62,117,4,2,116,11,190,59,7,10,246,117,10,205,24,235,172,190,
    42,7,232,57,0,232,54,0,50,228,205,26,139,218,131,195,96,180,1,205,22,180,
    0,117,11,205,26,59,211,114,242,160,72,7,235,10,205,22,138,196,60,28,116,
    243,4,246,60,49,114,214,60,53,119,210,80,190,40,7,187,27,6,83,252,172,80,
    36,127,180,14,205,16,88,168,128,116,242,195,86,184,1,3,187,0,6,185,1,0,
    50,246,205,19,94,198,6,72,7,63,195,13,138,13,10,70,48,32,46,32,46,32,46,
    160,100,105,115,107,32,49,13,10,10,68,101,102,97,117,108,116,58,32,70,63,
    160,0,1,0,4,0,6,3,7,7,10,10,99,14,100,14,101,20,128,20,129,25,130,30,147,
    36,165,39,159,43,117,47,82,47,219,50,64,55,242,61,0,100,111,243,72,80,70,
    211,79,115,178,85,110,105,248,78,111,118,101,108,236,77,105,110,105,248,
    76,105,110,117,248,65,109,111,101,98,225,66,83,196,66,83,68,233,80,67,73,
    216,67,80,205,86,101,110,105,248,68,111,115,115,101,227,63,191,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,85,170
};

int
scan_block(int fd, daddr_t block)
{
    u_char foo[512];

    if (-1 == lseek(fd,block * 512,SEEK_SET))
	err(1,"lseek");
    if (512 != read(fd,foo, 512))
	return 1;
    return 0;
}

void
Scan_Disk(Disk *d)
{
    char device[64];
    u_long l;
    int i,j,fd;

    strcpy(device,"/dev/r");
    strcat(device,d->name);

    fd = open(device,O_RDWR);
    if (fd < 0) {
	msgWarn("open(%s) failed", device);
	return;
    }
    for(i=-1,l=0;;l++) {
	j = scan_block(fd,l);
	if (j != i) {
	    if (i == -1) {
		printf("%c: %lu.",j ? 'B' : 'G', l);
		fflush(stdout);
	    } else if (i == 0) {
		printf(".%lu\nB: %lu.",l-1,l);
		fflush(stdout);
	    } else {
		printf(".%lu\nG: %lu.",l-1,l);
		fflush(stdout);
	    }
	    i = j;
	}
    }
    close(fd);
}

void
slice_wizard(Disk *d)
{
    Disk *db;
    char myprompt[BUFSIZ];
    char input[BUFSIZ];
    char *p,*q=0;
    char **cp,*cmds[200];
    int ncmd,i;

    sprintf(myprompt,"%s> ", d->name);
    while(1) {
	printf("--==##==--\n");
	Debug_Disk(d);
	p = CheckRules(d);
	if (p) {
	    printf("%s",p);
	    free(p);
	}
	printf(myprompt);
	fflush(stdout);
	q = p = fgets(input,sizeof(input),stdin);
	if(!p)
	    break;
	for(cp = cmds; (*cp = strsep(&p, " \t\n")) != NULL;)
	    if (**cp != '\0')
		cp++;
	ncmd = cp - cmds;
	if(!ncmd)
	    continue;
	if (!strcasecmp(*cmds,"quit")) { break; }
	if (!strcasecmp(*cmds,"exit")) { break; }
	if (!strcasecmp(*cmds,"q")) { break; }
	if (!strcasecmp(*cmds,"x")) { break; }
	if (!strcasecmp(*cmds,"delete") && ncmd == 2) {
	    printf("delete = %d\n",
		   Delete_Chunk(d,
				(struct chunk *)strtol(cmds[1],0,0)));
	    continue;
	}
	if (!strcasecmp(*cmds,"allfreebsd")) {
	    All_FreeBSD(d, 0);
	    continue;
	}
	if (!strcasecmp(*cmds,"dedicate")) {
	    All_FreeBSD(d, 1);
	    continue;
	}
	if (!strcasecmp(*cmds,"bios") && ncmd == 4) {
	    Set_Bios_Geom(d,
			  strtol(cmds[1],0,0),
			  strtol(cmds[2],0,0),
			  strtol(cmds[3],0,0));
	    continue;
	}
	if (!strcasecmp(*cmds,"list")) {
	    cp = Disk_Names();
	    printf("Disks:");
	    for(i=0;cp[i];i++) {
		printf(" %s",cp[i]);
		free(cp[i]);
	    }
	    free(cp);
	    continue;
	}
	if (!strcasecmp(*cmds,"create") && ncmd == 6) {

	    printf("Create=%d\n",
		   Create_Chunk(d,
				strtol(cmds[1],0,0),
				strtol(cmds[2],0,0),
				strtol(cmds[3],0,0),
				strtol(cmds[4],0,0),
				strtol(cmds[5],0,0)));
	    continue;
	}
	if (!strcasecmp(*cmds,"read")) {
	    db = d;
	    if (ncmd > 1)
		d = Open_Disk(cmds[1]);
	    else
		d = Open_Disk(d->name);
	    if (d)
		Free_Disk(db);
	    else
		d = db;
	    continue;
	}
	if (!strcasecmp(*cmds,"scan")) {
	    Scan_Disk(d);
	    continue;
	}
	if (!strcasecmp(*cmds,"write")) {
	    printf("Write=%d\n",
		   Fake ? 0 : Write_Disk(d));
	    Free_Disk(d);
	    d = Open_Disk(d->name);
	    continue;
	}
	if (strcasecmp(*cmds,"help"))
	    printf("\007ERROR\n");
	printf("CMDS:\n");
	printf("allfreebsd\t\t");
	printf("dedicate\t\t");
	printf("bios cyl hd sect\n");
	printf("collapse [pointer]\t\t");
	printf("create offset size enum subtype flags\n");
	printf("subtype(part): swap=1, ffs=7\t\t");
	printf("delete pointer\n");
	printf("list\t\t");
	printf("quit\n");
	printf("read [disk]\t\t");
	printf("scan\n");
	printf("write\t\t");
	printf("ENUM:\n\t");
	for(i=0;chunk_n[i];i++)
	    printf("%d = %s%s",i,chunk_n[i],i == 4 ? "\n\t" : "  ");
	printf("\n");

    }
}
