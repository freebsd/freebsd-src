#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 *	A simple filter for the templates
 */

int main(int argc, char *argv[])
{
	char buf[1024];
	char *vec[8192];
	char *fvec[200];
	char **svec;
	char type[64];
	int i;
	int vp=2;
	int ret=0;
	pid_t pid;


	if(chdir(getenv("TOPDIR")))
	{
		perror("chdir");
		exit(1);
	}
	
	/*
	 *	Build the exec array ahead of time.
	 */
	vec[0]="kernel-doc";
	vec[1]="-docbook";
	for(i=1;vp<8189;i++)
	{
		if(argv[i]==NULL)
			break;
		vec[vp++]=type;
		vec[vp++]=argv[i];
	}
	vec[vp++]=buf+2;
	vec[vp++]=NULL;
	
	/*
	 *	Now process the template
	 */
	 
	while(fgets(buf, 1024, stdin))
	{
		if(*buf!='!') {
			printf("%s", buf);
			continue;
		}

		fflush(stdout);
		svec = vec;
		if(buf[1]=='E')
			strcpy(type, "-function");
		else if(buf[1]=='I')
			strcpy(type, "-nofunction");	
		else if(buf[1]=='F') {
			int snarf = 0;
			fvec[0] = "kernel-doc";
			fvec[1] = "-docbook";
			strcpy (type, "-function");
			vp = 2;
			for (i = 2; buf[i]; i++) {
				if (buf[i] == ' ' || buf[i] == '\n') {
					buf[i] = '\0';
					snarf = 1;
					continue;
				}

				if (snarf) {
					snarf = 0;
					fvec[vp++] = type;
					fvec[vp++] = &buf[i];
				}
			}
			fvec[vp++] = &buf[2];
			fvec[vp] = NULL;
			svec = fvec;
		} else
		{
			fprintf(stderr, "Unknown ! escape.\n");
			exit(1);
		}
		switch(pid=fork())
		{
		case -1:
			perror("fork");
			exit(1);
		case  0:
			execvp("scripts/kernel-doc", svec);
			perror("exec scripts/kernel-doc");
			exit(1);
		default:
			waitpid(pid, &ret ,0);
		}
	}
	exit(ret);
}
