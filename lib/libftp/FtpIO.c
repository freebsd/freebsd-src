/*		      Library for ftpd clients.(libftp)
			Copyright by Oleg Orel
			 All rights reserved.
			
This  library is desined  for  free,  non-commercial  software  creation. 
It is changeable and can be improved. The author would greatly appreciate 
any  advises, new  components  and  patches  of  the  exist
ing  programs.
Commercial  usage is  also  possible  with  participation of it's author.



*/

#include "FtpLibrary.h"
#include <unistd.h>

int FtpRead(FTP *con)
{
  int c;
  
  if ( con -> mode == 'I' )
    return FtpGetc(con,FTPDATA(con));
  
  if ( con->ch != EOF )
    {
      c=con->ch;
      con->ch=EOF;
      return c;
    }
  
  c=FtpGetc(con,FTPDATA(con));
  
  if ( c == Ctrl('M') )
    {
      c = FtpGetc(con,FTPDATA(con));
      
      if ( c == Ctrl('J') )
	return '\n';
      con->ch = c;
      return Ctrl('M');
    }
  return c;
}

int FtpWrite(FTP *ftp,char c)
{
  
  if ( ftp -> mode == 'I' || c != '\n' )
    return FtpPutc(ftp,FTPDATA(ftp),c);
  
  FtpPutc(ftp,FTPDATA(ftp),Ctrl('M'));
  return FtpPutc(ftp,FTPDATA(ftp),Ctrl('J'));
}

	  
int FtpGetc(FTP *ftp,FILE *fp)
{
  fd_set fds;
  char c;
  
  FD_ZERO(&fds);
  FD_SET(fileno(fp),&fds);

  if (select(getdtablesize(), &fds, 0, 0, &(ftp->timeout))<1)
    return EXIT(ftp,QUIT);

  if (read(fileno(fp),&c,1)<1)
    return EOF;
  
  if (ftp->hash!=NULL) (*ftp->hash)(ftp,1);
  
  return (int)c;
}


STATUS FtpPutc(FTP *ftp,FILE *fp,char c)
{
  fd_set fds;
  
  FD_ZERO(&fds);
  FD_SET(fileno(fp),&fds);
  
  if (select(getdtablesize(), 0, &fds, 0, &(ftp->timeout))<1)
    return EXIT(ftp,QUIT);
  
  if (write(fileno(fp),&c,1)!=1)
    return EXIT(ftp,QUIT);

  if (ftp->hash!=NULL) (*ftp->hash)(ftp,1);
  return 0;
}


STATUS FtpReadBlock(FTP *ftp, char *buffer, int size)
{
  fd_set fds;
  register int rsize;
  
  FD_ZERO(&fds);
  FD_SET(fileno(FTPDATA(ftp)),&fds);
  
  if (select(getdtablesize(), &fds,0, 0, &(ftp->timeout))<1)
    return EXIT(ftp,QUIT);
  
  
  if ((rsize=read(fileno(FTPDATA(ftp)),buffer,size))<0)
    return EXIT(ftp,QUIT);
  
  if (ftp->hash!=NULL && rsize!=0) (*ftp->hash)(ftp,rsize);
  
  if (ftp->mode == 'A')
    {
      char buffer2[size];
      register int i,ii;

      for (i=0,ii=0;i<rsize;i++,ii++)
	if (buffer[i]==Ctrl('M')&&buffer[i+1]==Ctrl('J'))
	  buffer2[ii]='\n',i++;
	else
	  buffer2[ii]=buffer[i];
      
      rsize=ii;
      bcopy(buffer2,buffer,rsize);
    }
  return rsize;
}


STATUS FtpWriteBlock(FTP *ftp, char *buffer, int size)
{
  fd_set fds;
  register int wsize;
  char buffer2[size*2];
  
  FD_ZERO(&fds);
  FD_SET(fileno(FTPDATA(ftp)),&fds);
  
  if (select(getdtablesize(), 0, &fds, 0, &(ftp->timeout))<1)
    return EXIT(ftp,QUIT);
  
  
  if (ftp->mode=='A')
    {
      register int i,ii;

      for(i=0,ii=0;i<size;i++,ii++)
	if (buffer[i]=='\n')
	  buffer2[ii++]=Ctrl('M'),buffer2[ii]=Ctrl('J');
	else
	  buffer2[ii]=buffer[i];
      buffer=buffer2;
      size=ii;
    }
  
  if ((wsize=write(fileno(FTPDATA(ftp)),buffer,size))!=size) 
    return EXIT(ftp,QUIT);
  
  if ( ftp->hash!=NULL && wsize!=0 ) (*ftp->hash)(ftp,wsize);
  
  return wsize;
}


    









