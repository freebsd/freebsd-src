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
 *	$Id: daemon.c,v 1.1.1.1.2.1 1999/02/05 12:21:57 abial Exp $
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char *logo[]={
"    [31m)\\_)\\[37m\n",
"    [31m([37m[1mo,o[m[31m)[37m\n",
" [32m__  [31m\\~/[37m\n",
" [32m-->=[41m===[40m[31m\\[37m\n",
" [32m~~[37m  [31md d[37m\n",
"   [35mpicoBSD[37m\n",
NULL};


void
printit(int c, char **v)
{
	int len=0,i,j;
	char **line,**txt;
	char buf[100];
	FILE *fd=NULL;

	if(c>0 && strcmp(v[0],"-f")==0) {
		fd=fopen(v[1],"r");
		if(fd==NULL) c=0;
	}
	line=logo;
	if(c==0) {
		while(*line) {
			printf("%s",*line);
			line++;
		}
		return;
	}
	if(fd!=NULL) {
		txt=malloc(sizeof(char *)*6);
		j=0;
		while(j<6 && !feof(fd)) {
			if(fgets(buf,99,fd)==NULL) continue;
			buf[strlen(buf)-1]='\0';
			*(txt+j)=strdup(buf);
			j++;
		}
		if(j<6) {
			for(i=j;i<5;i++) {
				*(txt+i)=strdup("");
			}
		}
		c=6;
	} else txt=v;
	for (j=0;j<c;j++) {
		if(len<strlen(txt[j])) len=strlen(txt[j]);
	}
	for(j=0;j<len+11;j++) {
		printf("=");
	}
	printf("\n");
	i=0;
	while(*line) {
		if(i<c) {
			printf("%s",txt[i]);
			for(j=0;j<(len-strlen(txt[i]));j++) {
				printf(" ");
			}
			i++;
		} else for(j=0;j<len;j++) printf(" ");
		printf("%s",*line);
		line++;
	}
	while (i<c) {
		printf("%s\n",txt[i]);
		i++;
	}
	if(fd!=NULL) {
		while(!feof(fd)) {
			buf[0]='\0';
			if(fgets(buf,99,fd)==NULL) continue;
			printf("%s",buf);
		}
		fclose(fd);
	} else {
		for(j=0;j<len+10;j++) printf("=");
		printf("\n");
	}
	return;
}

int
main(int argc, char *argv[])
{
	int c;
	char **v;

	if(argc<2) printit(0,NULL);
	else printit(--argc,++argv);
	exit(0);
}
