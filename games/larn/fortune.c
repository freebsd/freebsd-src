/* fortune.c		 Larn is copyrighted 1986 by Noah Morgan. */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "header.h"
/*
 *	function to return a random fortune from the fortune file
 */
static char *base=0;	/* pointer to the fortune text */
static char **flines=0;	/* array of pointers to each fortune */
static int fd=0;		/* true if we have load the fortune info */
static int nlines=0;	/* # lines in fortune database */

char *fortune(file)
	char *file;
	{
	register char *p;
	register int lines,tmp;
	struct stat stat;
	char *malloc();
	if (fd==0)
		{
		if ((fd=open(file,O_RDONLY)) < 0)	/* open the file */
			return(0); /* can't find file */

	/* find out how big fortune file is and get memory for it */
		stat.st_size = 16384;
		if ((fstat(fd,&stat) < 0) || ((base=malloc(1+stat.st_size)) == 0))
			{
			close(fd); fd= -1; free((char*)base); return(0); 	/* can't stat file */
			}

	/* read in the entire fortune file */
		if (read(fd,base,stat.st_size) != stat.st_size)
			{
			close(fd); fd= -1; free((char*)base); return(0); 	/* can't read file */
			}
		close(fd);  base[stat.st_size]=0;	/* final NULL termination */

	/* count up all the lines (and NULL terminate) to know memory needs */
		for (p=base,lines=0; p<base+stat.st_size; p++) /* count lines */
			if (*p == '\n') *p=0,lines++;
		nlines = lines;

	/* get memory for array of pointers to each fortune */
		if ((flines=(char**)malloc(nlines*sizeof(char*))) == 0)
			{
			free((char*)base); fd= -1; return(0); /* malloc() failure */
			}

	/* now assign each pointer to a line */
		for (p=base,tmp=0; tmp<nlines; tmp++)
			{
			flines[tmp]=p;  while (*p++); /* advance to next line */
			}
		}

	if (fd > 2)	/* if we have a database to look at */
		return(flines[rund((nlines<=0)?1:nlines)]);
	else 
		return(0);
	}
