/*		      Library for ftpd clients.(libftp)
			Copyright by Oleg Orel
			 All rights reserved.

This  library is desined  for  free,  non-commercial  software  creation.
It is changeable and can be improved. The author would greatly appreciate
any  advises, new  components  and  patches  of  the  existing  programs.
Commercial  usage is  also  possible  with  participation of it's author.
*/

#include "FtpLibrary.h"

static char * simplename(char *s)
{
  char *p;

  if ( (p=(char *)strrchr(s,'/')) == NULL )
    return s;
  return p+1;
}




STATUS FtpFilenameChecker(char ** in, char ** out)
{
  struct stat st;

  if ( (stat(*out,&st) == 0) && S_ISDIR(st.st_mode))
    {
      char * sfn = simplename(*in);
      char * new = (char *) malloc ( strlen(*out)+ strlen(sfn) + 2 );

      strcpy(new,*out);
      strcat(new,"/");
      strcat(new,sfn);
      *out=new;
      return 0;
    };
   return 0;
}


