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

STATUS FtpStor (FTP * con , char * command ,
		       char *in , char * out)
{
  FILE *i;
  char buffer[FTPBUFSIZ];
  int size;

  con->seek=0;

  if ( (i=Ftpfopen(in,"rb")) == NULL )
    return EXIT(con,LQUIT);

  if ( FtpTestFlag(con,FTP_REST) &&
      (con->seek=FtpSize(con,out))<0 )
    con->seek=0;


  if ( FtpError(FtpData(con,command,out,"w")))
    {
      if (con->seek==0) return EXIT(con,con->errno);

      con -> seek =0;
      if ( FtpError(FtpData(con,command,out,"w")) )
	return EXIT(con,con->errno);
    }

  if (con->seek) fseek(i,con->seek,0);

  while ( (size=read ( fileno(i) , buffer, FTPBUFSIZ ))>0)
    FtpWriteBlock(con,buffer,size);

  Ftpfclose(i);
  return FtpClose(con);
}







