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

FTP FtpInit = {
  NULL,     /*sock*/
  NULL,  /*data*/
  'A',   /*mode*/
  0,     /*errno*/
  0,     /*ch*/
  NULL,NULL,NULL, NULL, /*funcs*/
  0, /*seek*/
  0, /*flags*/
  {120,0}, /*timeout 2 min*/
  21, /*Port*/
  "johndoe", /* Title */
  0, /*Counter*/
};

FTP *FtpCreateObject()
{
  FTP *new = (FTP *) malloc (sizeof(FTP));

  bcopy(&FtpInit,new,sizeof(FTP));

  return new;
}


