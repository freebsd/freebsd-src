/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: tst01.c,v 1.5 1995/04/29 07:21:13 phk Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>
#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif
#include <sys/types.h>
#include "libdisk.h"

CHAR_N;

int
main(int argc, char **argv)
{
	struct disk *d,*db;
	char myprompt[BUFSIZ];
#ifndef READLINE
	char input[BUFSIZ];
#endif
	char *p,*q=0;
	char **cp,*cmds[200];
	int ncmd,i;

	if (argc < 2) {
		fprintf(stderr,"Usage:\n\t%s diskname\n",argv[0]);
		exit(1);
	}
	d = Open_Disk(argv[1]);
	if (!d) 
		err(1,"Couldn't open disk %s",argv[1]);

	sprintf(myprompt,"%s %s> ",argv[0],argv[1]);
	while(1) {
		printf("\n\n\n\n");
		Debug_Disk(d);
		p = CheckRules(d);
		if (p) {
			printf("%s",p);
			free(p);
		}
#ifdef READLINE
		if (q)
			free(q);
		q = p = readline(myprompt);
#else
		printf(myprompt);
		fflush(stdout);
		q = p = fgets(input,sizeof(input),stdin);
#endif
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
			All_FreeBSD(d);
			continue;
		}
		if (!strcasecmp(*cmds,"bios") && ncmd == 4) { 
			Set_Bios_Geom(d,
				strtol(cmds[1],0,0),
				strtol(cmds[2],0,0),
				strtol(cmds[3],0,0));
			continue;
		}
		if (!strcasecmp(*cmds,"phys") && ncmd == 4) { 
			d = Set_Phys_Geom(d,
				strtol(cmds[1],0,0),
				strtol(cmds[2],0,0),
				strtol(cmds[3],0,0));
			continue;
		}
		if (!strcasecmp(*cmds,"collapse")) { 
			if (cmds[1])
				while (Collapse_Chunk(d,
				    (struct chunk *)strtol(cmds[1],0,0)))
					;
			else		
				Collapse_Disk(d);
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
				d = Open_Disk(argv[1]);
			if (d)
				Free_Disk(db);
			else
				d = db;
			continue;
		}
		if (!strcasecmp(*cmds,"boot")) {
			extern u_char boot1[],boot2[];
			Set_Boot_Blocks(d,boot1,boot2);
			continue;
		}
		if (!strcasecmp(*cmds,"write")) {
			printf("Write=%d\n",
				Write_Disk(d));
			Free_Disk(d);
			d = Open_Disk(d->name);
			continue;
		}
		if (strcasecmp(*cmds,"help"))
			printf("\007ERROR\n");
		printf("CMDS:\n");
		printf("\tallfreebsd\n");
		printf("\tbios cyl hd sect\n");
		printf("\tboot\n");
		printf("\tcollapse [pointer]\n");
		printf("\tcreate offset size enum subtype flags\n");
		printf("\tdelete pointer\n");
		printf("\tlist\n");
		printf("\tphys cyl hd sect\n");
		printf("\tquit\n");
		printf("\tread [disk]\n");
		printf("\twrite\n");
		printf("\nENUM:\n\t");
		for(i=0;chunk_n[i];i++)
			printf("%d = %s%s",i,chunk_n[i],i == 4 ? "\n\t" : "  ");
		printf("\n");
		
	}
	exit (0);
}
