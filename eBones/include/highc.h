/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Known breakage in the version of Metaware's High C compiler that
 * we've got available....
 *
 *	from: highc.h,v 4.0 89/01/23 09:59:15 jtkohl Exp $
 *	$FreeBSD$
 */

#define const
/*#define volatile*/

/*
 * Some builtin functions we can take advantage of for inlining....
 */

#define abs			_abs
/* the _max and _min builtins accept any number of arguments */
#undef MAX
#define MAX(x,y)		_max(x,y)
#undef MIN
#define MIN(x,y)		_min(x,y)
/*
 * I'm not sure if 65535 is a limit for this builtin, but it's
 * reasonable for a string length.  Or is it?
 */
/*#define strlen(s)		_find_char(s,65535,0)*/
#define bzero(ptr,len)		_fill_char(ptr,len,'\0')
#define bcmp(b1,b2,len)		_compare(b1,b2,len)
