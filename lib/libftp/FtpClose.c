/*		      Library for ftpd clients.(libftp)
			Copyright by Oleg Orel
			 All rights reserved.
			
This  library is desined  for  free,  non-commercial  software  creation. 
It is changeable and can be improved. The author would greatly appreciate 
any  advises, new  components  and  patches  of  the  existing  programs.
Commercial  usage is  also  possible  with  participation of it's author.
*/

#include "FtpLibrary.h"

STATUS FtpClose(FTP *ftp)
{
  int i;
  String S1;

  fflush(FTPDATA(ftp));
  shutdown(fileno(FTPDATA(ftp)),2);
  fclose(FTPDATA(ftp));
  
  FtpAssert(ftp,i=FtpGetMessage(ftp,S1));
  if ( i != 226 )
    return EXIT(ftp,-i);
  return EXIT(ftp,i);
}
