/*-
 * Copyright (c) 1998 Andrzej Bialecki
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: help.c,v 1.1.1.1 1998/07/14 07:30:53 abial Exp $
 *
 */


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

void
display(char *fname)
{
	FILE *fd;
	DIR *dirp;
	struct dirent *d;
	char buf[100],junk[5],c, *dot;
	int i;

	snprintf(buf,99,"/help/%s.hlp",fname);
	if((fd=fopen(buf,"r"))==NULL) {
		printf("No help available for '%s'.\n",fname);
		exit(1);
	}
	printf("\n");
	i=0;
	while(!feof(fd)) {
		if(fgets(buf,99,fd)==NULL) continue;
		if(i<23) {
			printf("%s",buf);
			i++;
		} else {
			printf("[7mPress Enter to continue[m");
			fgets(junk,5,stdin);
			printf("%s",buf);
			i=0;
		}
	}
	printf("\n");
	i=0;
	if(strcmp(fname,"help")==0) {
		printf("The following help items are available:\n\n");
		dirp=opendir("/help/.");
		while((d=readdir(dirp))!=NULL) {
			if(d->d_name[0]=='.') continue;
			if((dot=strchr(d->d_name,'.'))!=NULL) {
				*dot='\0';
			}
			printf("%-13s",d->d_name);
			i++;
			if(i>5) {
				printf("\n");
				i=0;
			}
		}
		closedir(dirp);
		printf("\n");
	}
	return;
}


int
main(int argc, char *argv[])
{
	if(argc==1) {
		display("help");
	} else {
		display(argv[1]);
	}
	exit(0);
}
