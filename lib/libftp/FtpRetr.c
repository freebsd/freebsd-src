/*
		      Library for ftpd clients.(libftp)
			Copyright by Oleg Orel
			 All rights reserved.
			
This  library is desined  for  free,  non-commercial  software  creation. 
It is changeable and can be improved. The author would greatly appreciate 
any  advises, new  components  and  patches  of  the  existing  programs.
Commercial  usage is  also  possible  with  participation of it's author.



*/

#include "FtpLibrary.h"
#include <unistd.h>

extern STATUS FtpFilenameChecker(char ** in, char ** out);

STATUS FtpRetr (FTP * con , char * command ,
		       char *in , char * out)
{
  FILE *o;
  struct stat st;
  char buffer[FTPBUFSIZ];
  register int size;
 
  FtpFilenameChecker(&in,&out);

  if ( FtpTestFlag(con,FTP_REST) && stat(out,&st)==0)
    {
      con -> seek = st.st_size;
      if ((o=Ftpfopen(out,"a+"))==NULL)
	return EXIT(con,LQUIT);
    }
  else
    {
      con -> seek = 0;
      if ((o=Ftpfopen(out,"w+"))==NULL)
	return EXIT(con,LQUIT);
    }
  

  if ( FtpError(FtpData(con,command,in,"r")))
    {

      if (con->seek==0) return EXIT(con,con->errno);
      
      con -> seek = 0;
      fclose(o);
      
      if ( FtpError(FtpData(con,command,in,"r")) )
	{
	  return EXIT(con,con->errno);
	}
      
      if ((o=Ftpfopen(out,"w+"))==NULL)
	return EXIT(con,LQUIT);
    }
  
  
  fseek(o,con->seek,0);
  
  while((size=FtpReadBlock(con,buffer,FTPBUFSIZ))>0)
    {
      if (write(fileno(o),buffer,size)!=size)
	return EXIT(con,LQUIT);
    }

  Ftpfclose(o);
  return FtpClose(con);
}












