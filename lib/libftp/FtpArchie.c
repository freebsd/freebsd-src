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

#define C2I(n) ((int)((n)-'0'))

int FtpArchie ( char *what, ARCHIE *result, int len)
{
  FILE *archie;
  String cmd,tmp;
  int i;

  bzero(result,sizeof(result[0])*len);

  sprintf(cmd,"archie -l -m %d %s",len,what);

  if ((archie = popen(cmd,"r"))==NULL)
    return 0;

  for(i=0;i<len;i++)
    {
      char *p, *pp;

      if (fgets(tmp,sizeof (tmp), archie)==NULL)
	break;


      result[i].createtime.tm_year = C2I (tmp[2 ])*10 - 70 + C2I(tmp[3]);
      result[i].createtime.tm_mday = C2I (tmp[4 ])*10 + C2I(tmp[5]);
      result[i].createtime.tm_hour = C2I (tmp[6 ])*10 + C2I(tmp[7]);
      result[i].createtime.tm_min  = C2I (tmp[8 ])*10 + C2I(tmp[9]);
      result[i].createtime.tm_sec  = C2I (tmp[10])*10 + C2I(tmp[11]);

      for(p=tmp; *p!=' '; p++);
      for(; *p==' '; p++);

      result[i].size  = atoi(p);

      for(; *p!=' '; p++);
      for(; *p==' '; p++);

      for (pp=result[i].host;*p!=' ';p++,pp++) *pp=*p;
      *pp=0;
      for(; *p==' '; p++);
      for (pp=result[i].file;*p!='\n';p++,pp++) *pp=*p;
      *pp=0;

    }

  return i;
}
