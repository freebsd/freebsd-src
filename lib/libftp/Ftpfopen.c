/*
		      Library for ftpd clients.(libftp)
			Copyright by Oleg Orel
			 All rights reserved.
			
This  library is desined  for  free,  non-commercial  software  creation. 
It is changeable and can be improved. The author would greatly appreciate 
any  advises, new  components  and  patches  of  the  existing  programs.
Commercial  usage is  also  possible  with  participation of it's author.



*/

#include <FtpLibrary.h>

#define NFSD 256

static int fds_types[NFDS];
static int init=0;

enum {T_EMPTY=0,T_FILE,T_STREAM,T_PIPE,T_FULL};

FILE *Ftpfopen(char *filename,char *mode)
{
  FILE *fp;
  
  if (!init)
    { 
      bzero(fds_types,NFDS*sizeof(fds_types[0]));
      init=1;
    }

  if (!strcmp(filename,"*STDIN*") || (!strcmp(filename,"-") && (mode[0]=='r')) )
    {
      fds_types[fileno(stdin)]=T_STREAM;
      return stdin;
    }
  
  if (!strcmp(filename,"*STDOUT*") || (!strcmp(filename,"-") && (mode[0]=='w')))
    {
      fds_types[fileno(stdout)]=T_STREAM;
      return stdout;
    }
  
  if (strcmp(filename,"*STDERR*")==0)
    {
      fds_types[fileno(stderr)]=T_STREAM;
      return stderr;
    }
  


  if (*filename=='|') 
    {
      fp=popen(filename+1,mode);
      if (fp==NULL) return fp;
      fds_types[fileno(fp)]=T_PIPE;
      return fp;
    }

  fp=FtpFullOpen(filename,mode);
  if (fp==NULL) return fp;
  fds_types[fileno(fp)]=T_FILE;
  return fp;
  
}

int Ftpfclose(FILE *fp)
{

  if (!init)
    { 
      bzero(fds_types,NFDS*sizeof(fds_types[0]));
      init=1;
    }

  switch (fds_types[fileno(fp)])
    {
    
    case T_FILE:
      
      return FtpFullClose(fp);
      
    case T_STREAM:

      return fflush(fp);
      
    case T_PIPE:
      
      return pclose(fp);
      
    default:

      return -1;
    }

}
