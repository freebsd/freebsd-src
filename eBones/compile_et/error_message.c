/*
 * Copyright 1987 by the Student Information Processing Board
 * of the Massachusetts Institute of Technology
 * For copyright info, see "Copyright.SIPB".
 *
 *	from: error_message.c,v 1.1 86/11/10 21:34:34 spook Exp $
 *	$Id: error_message.c,v 1.3 1994/09/09 21:43:22 g89r4222 Exp $
 */

#include <stdio.h>
#include "error_table.h"
extern int sys_nerr;

static char buffer[25];

char *
error_message(code)
     int code;
{
     register int offset;
     register error_table **et;
     register int table_num;
     register int div;
     register char *cp;

     offset = code & ((1<<ERRCODE_RANGE)-1);
     table_num = code - offset;
     if ((_et_list == (error_table **)NULL) && table_num)
	  goto oops;
     if (!table_num) {
	  if (offset < sys_nerr)
	       return(sys_errlist[offset]);
	  else
	       goto oops;
     }
     for (et = _et_list; *et != (error_table *)NULL; et++) {
	  if ((*et)->base == table_num) {
	       /* This is the right table */
	       if ((*et)->n_msgs <= offset)
		    goto oops;
	       return((*et)->msgs[offset]);
	  }
     }
 oops:
     cp = buffer;
     {
	  register char *cp1;
	  for (cp1 = "Unknown code "; *cp1; cp1++, cp++)
	       *cp = *cp1;
	  if (table_num) {
	       for (cp1 = error_table_name(table_num); *cp1; cp1++, cp++)
		    *cp = *cp1;
	       *cp++ = ' ';
	       *cp = '\0';
	  }
     }
     div = 1000000000;
     if (offset == 0) {
	  *cp++ = '0';
	  *cp = '\0';
	  return(buffer);
     }
     while (div > offset)
	  div /= 10;
     do {
	  register int n = offset / div;
	  *cp++ = '0' + n;
	  offset -= n * div;
	  div /= 10;
     } while (offset && div);
     while (div) {
	  *cp++ = '0';
	  div /= 10;
     }
     *cp = '\0';
     return(buffer);
}
