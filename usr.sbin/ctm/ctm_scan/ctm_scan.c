/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: ctm_scan.c,v 1.12 1995/07/13 15:33:42 phk Exp $
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <md5.h>

int barf[256];
int CheckMode = 0;

int
pstrcmp(const void *pp, const void *qq)
{
    return strcmp(*(char **)pp,*(char **)qq);
}

int
Do(char *path)
{
    DIR *d;
    struct dirent *de;
    struct stat st;
    int ret=0;
    u_char buf[BUFSIZ];
    u_char data[BUFSIZ],*q;
    int bufp;
    MD5_CTX ctx;
    int fd,i,j,k,l,npde,nde=0;
    char **pde, md5[33];

    npde = 1;
    pde = malloc(sizeof *pde * (npde+1));
    d = opendir(path);
    if(!d) { perror(path); return 2; }
    if(!strcmp(path,".")) {
	*buf = 0;
    } else {
	strcpy(buf,path);
	if(buf[strlen(buf)-1] != '/')
	    strcat(buf,"/");
    }
    bufp = strlen(buf);
    while((de=readdir(d))) {
	if(!strcmp(de->d_name,".")) continue;
	if(!strcmp(de->d_name,"..")) continue;
	if(nde >= npde) {
	    npde *= 2;
	    pde = realloc(pde,sizeof *pde * (npde+1));
	}
	strcpy(buf+bufp,de->d_name);
	if(stat(buf,&st)) {
	    ret |= 1;
	    continue;
	}
	pde[nde] = malloc(strlen(buf+bufp)+1);
        strcpy(pde[nde++],buf+bufp);
    }
    closedir(d);
    if(!nde) return 0;
    qsort(pde,nde,sizeof *pde, pstrcmp);
    for(k=0;k<nde;k++) {
	strcpy(buf+bufp,pde[k]);
        free(pde[k]);
	if(stat(buf,&st)) {
	    ret |= 1;
	    continue;
	}
	switch(st.st_mode & S_IFMT) {
	    case S_IFDIR:
		if(!CheckMode) {
		    i = printf("d %s %o %u %u - - -\n",
			buf,st.st_mode & (~S_IFMT),st.st_uid,st.st_gid);
		    if(!i)
			exit(-1);
		}
		ret |= Do(buf);
		break;
	    case S_IFREG:
		fd = open(buf,O_RDONLY);
		if(fd < 0) {
		    ret |= 1;
		    continue;
		}
		MD5Init(&ctx);
		l = 1;
		j = 0;
		while(0 < (i = read(fd,data,sizeof data))) {
		    l = (data[i-1] == '\n');
		    if(!CheckMode)
			MD5Update(&ctx,data,i);
		    for(q=data;i && !j;i--)
			if(barf[*q++])
			    j=1;
		}
		close(fd);
		if(CheckMode) {
		    if(j || !l) {
			i = printf("%s",buf);
			if(!i) exit(-1);
			if(j) printf("  Illegal characters.");
			if(!l) printf("  No final newline.");
			i = printf(".\n");
			if(!i) exit(-1);
			}
		} else {
		    if(!l)
			j=2;
		    i = printf("f %s %o %u %u %u %lu %s\n",
			buf,st.st_mode & (~S_IFMT),st.st_uid,st.st_gid,
			j,(u_long)st.st_size,MD5End(&ctx,md5));
		    if(!i) exit(-1);
		}
		break;
	    default:
		fprintf(stderr,"%s: type 0%o\n",buf, st.st_mode & S_IFMT);
		ret |= 4;
		break;
	}
    }
    free(pde);
    return ret;
}

int
main(int argc, char **argv)
{
    int i;

    /*
     * Initialize barf[], characters diff/patch will not appreciate.
     */

    barf[0x00] = 1;
    barf[0x7f] = 1;
    barf[0x80] = 1;
    barf[0xff] = 1;

    /*
     * -c is CheckMode
     */
    if (argc > 1 && !strcmp(argv[1],"-c")) {
	CheckMode=1;
	argc--;
	argv++;
    }

    /*
     * First argument, if any, is where to do the work.
     */
    if (argc > 1) {
	if(chdir(argv[1])) {
	    perror(argv[1]);
	    return 2;
	}
	argc--;
	argv++;
    }

    /*
     * Scan the directories recursively.
     */
    if (argc > 1) {
	while (argc > 1) {
		i = Do(argv[1]);
		argc--;
		argv++;
		if (i)
			return i;
	}
	return i;
    } else
	    return Do(".");
}
