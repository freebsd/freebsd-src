/****************************************************************/
/*								*/
/*	putenv(3)						*/
/*								*/
/*		Change or add an environment entry		*/
/*								*/
/****************************************************************/
/*   origination        1987-Oct-7               T. Holm	*/
/****************************************************************/

/*
Path: hoptoad!pacbell!ames!ll-xn!mit-eddie!uw-beaver!ssc-vax!uvicctr!tholm
From: tholm@uvicctr.UUCP (Terrence W. Holm)
Newsgroups: comp.os.minix
Subject: putenv(3)
Message-ID: <395@uvicctr.UUCP>
Date: 5 May 88 06:40:52 GMT
Organization: University of Victoria, Victoria B.C. Canada

EFTH Minix report #2  - May 1988 -  putenv(3)

This is an implementation of putenv(3) that we
wrote for Minix. Please consider this a public
domain program.
*/

#include <stdio.h>

#define  PSIZE  sizeof(char *)

extern  char  **environ;

char  *strchr();
char  *malloc();

/****************************************************************/
/*								*/
/*      int							*/
/*	putenv( entry )						*/
/*								*/
/*		The "entry" should follow the form 		*/
/*		"NAME=VALUE". This routine will search the 	*/
/*		user environment for "NAME" and replace its 	*/
/*		value with "VALUE".				*/
/*								*/
/*		Note that "entry" is not copied, it is used 	*/
/*		as the environment entry. This means that it 	*/
/*		must not be unallocated or otherwise modifed 	*/
/*		by the caller, unless it is replaced by a 	*/
/*		subsequent putenv().				*/
/*								*/
/*		If the name is not found in the environment, 	*/
/*		then a new vector of pointers is allocated, 	*/
/*		"entry" is put at the end and the global 	*/
/*		variable "environ" is updated.			*/
/*								*/
/*		This function normally returns NULL, but -1	*/
/*		is returned if it can not allocate enough 	*/
/*		space using malloc(3), or "entry" does not	*/
/*		contain a '='.					*/
/*								*/
/****************************************************************/


int
putenv( entry )
  char *entry;
{
  unsigned length;
  unsigned size;
  char     *temp;
  char     **p;
  char     **new_environ;

  /*  Find the length of the "NAME="  */

  temp = strchr(entry,'=');
  if ( temp == 0 )
    return( -1 );

  length = (unsigned) (temp - entry + 1);


  /*  Scan through the environment looking for "NAME="  */

  for ( p=environ; *p != 0 ; p++ )
    if ( strncmp( entry, *p, length ) == 0 )
      {
      *p = entry;
      return( 0 );
      }


  /*  The name was not found, build a bigger environment  */

  size = p - environ;

  new_environ = (char **) malloc( (size+2)*PSIZE );

  if ( new_environ == (char **) NULL )
    return( -1 );

  memcpy ((char *) new_environ, (char *) environ, size*PSIZE );

  new_environ[size]   = entry;
  new_environ[size+1] = NULL;

  environ = new_environ;

  return(0);
}
