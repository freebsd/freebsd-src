/* Wrapper code for Taylor UUCP on Amiga Unix (SVR4) for cron invoked UUCP */
/* processes.                                                              */

/* The problem:  Cron is not a "licensed" process. any process that grabs a 
   controlling terminal needs to be licensed.  Taylor UUCP needs controlling
   terminals.  Taylor UUCP does relinquish the controlling terminal before 
   fork(), so the "UUCP" license is appropriate. 
   This simple program does the "right" thing, but *MUST* be SETUID ROOT */

/* Written by: Lawrence E. Rosenman <ler@lerami.lerctr.org> */

#include <sys/sysm68k.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>

int main(int argc,char *argv[],char *envp)
{
  struct passwd *pw;
  char   name[256];

  strcpy(name,"/usr/local/lib/uucp/uucico");
  if (sysm68k(_m68k_LIMUSER,EUA_GET_LIC) == 0 ) { /* are we unlicensed? */
	  if (sysm68k(_m68k_LIMUSER,EUA_UUCP) == -1) { /* yes, get a "uucp" license */
		 fprintf(stderr,"sysm68k failed, errno=%d\n",errno); /* we didn't? crab it */
		 exit(errno);
      }
  }

  pw = getpwnam("uucp"); /* get the Password Entry for uucp */
  if (pw == NULL)
  {
	 fprintf(stderr,"User ID \"uucp\" doesn't exist.\n");
	 exit(1);
  }
  setgid(pw->pw_gid); /* set gid to uucp */
  setuid(pw->pw_uid); /* set uid to uucp */ 
  argv[0]=name; /* have PS not lie... */
  execv("/usr/local/lib/uucp/uucico",argv); /* go to the real program */
  exit(errno);
}
