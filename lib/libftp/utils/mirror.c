
/* Mirror directrory structure to another host */


#include <stdio.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <syslog.h>
#include <FtpLibrary.h>

/* Usage: mirror <local_dir> <host> <user> <passwd> <remote_dir> */
FTP *ftp;

main(int a,char **b)
{

#define LOCAL_DIR  b[1]
#define HOST       b[2]
#define USER       b[3]
#define PASSWD     b[4]
#define REMOTE_DIR b[5]

  if ( a < 5 )
    quit("Usage: mirror <local_dir> <host> <user> <passwd> <remote_dir>");


  FtplibDebug(yes);
  FtpLogin(&ftp,HOST,USER,PASSWD,NULL);
  FtpChdir(ftp,REMOTE_DIR);
  FtpBinary(ftp);
  doit(LOCAL_DIR);
  exit(0);
}

doit(char *dirname)
{
  DIR *dp;
  struct direct *de;
  char n[256],fn[256];
  struct stat st;


  if ( (dp=opendir(dirname)) == NULL )
    {
      log(dirname);
      return;
    }

  while ( (de = readdir(dp)) != NULL )
    {
      if ( de -> d_name[0] == '.' )
	continue;

      sprintf(fn,"%s/%s",dirname,de->d_name);

      if ( stat(fn,&st) != 0 ) {
	log(fn);
	continue;
      }

      if ( S_ISDIR (st.st_mode) )
	{
	  FtpCommand(ftp,"MKD %s",fn,0,EOF); /* Ignore errors (0,EOF) */
	  doit(fn);
	  continue;
	}

      if ( st.st_size != FtpSize(ftp,fn))

      FtpPut(ftp,fn,fn);
    }

  closedir(dp);

}



quit(char *s)
{
  log(s);
  exit(1);
}

log(char *s)
{
  perror(s);
}



