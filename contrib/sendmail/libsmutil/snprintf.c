/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: snprintf.c,v 8.27.16.4 2001/07/20 04:19:37 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>

/*
**  SNPRINTF, VSNPRINT -- counted versions of printf
**
**	These versions have been grabbed off the net.  They have been
**	cleaned up to compile properly and support for .precision and
**	%lx has been added.
*/

/**************************************************************
 * Original:
 * Patrick Powell Tue Apr 11 09:48:21 PDT 1995
 * A bombproof version of doprnt (sm_dopr) included.
 * Sigh.  This sort of thing is always nasty do deal with.  Note that
 * the version here does not include floating point...
 *
 * snprintf() is used instead of sprintf() as it does limit checks
 * for string length.  This covers a nasty loophole.
 *
 * The other functions are there to prevent NULL pointers from
 * causing nast effects.
 **************************************************************/

/*static char _id[] = "$OrigId: snprintf.c,v 1.2 1995/10/09 11:19:47 roberto Exp $";*/
void	sm_dopr();
char	*DoprEnd;
int	SnprfOverflow;

#if !HASSNPRINTF && !SNPRINTF_IS_BROKEN
# define sm_snprintf	snprintf
# ifndef luna2
#  define sm_vsnprintf	vsnprintf
extern int	vsnprintf __P((char *, size_t, const char *, va_list));
# endif /* ! luna2 */
#endif /* !HASSNPRINTF && !SNPRINTF_IS_BROKEN */

/* VARARGS3 */
int
# ifdef __STDC__
sm_snprintf(char *str, size_t count, const char *fmt, ...)
# else /* __STDC__ */
sm_snprintf(str, count, fmt, va_alist)
	char *str;
	size_t count;
	const char *fmt;
	va_dcl
# endif /* __STDC__ */
{
	int len;
	VA_LOCAL_DECL

	VA_START(fmt);
	len = sm_vsnprintf(str, count, fmt, ap);
	VA_END;
	return len;
}

int
sm_vsnprintf(str, count, fmt, args)
	char *str;
	size_t count;
	const char *fmt;
	va_list args;
{
	str[0] = 0;
	DoprEnd = str + count - 1;
	SnprfOverflow = 0;
	sm_dopr( str, fmt, args );
	if (count > 0)
		DoprEnd[0] = 0;
	if (SnprfOverflow > 0 && tTd(57, 2))
		dprintf("\nvsnprintf overflow, len = %ld, str = %s",
			(long) count, shortenstring(str, MAXSHORTSTR));
	return strlen(str) + SnprfOverflow;
}

/*
 * sm_dopr(): poor man's version of doprintf
 */

void fmtstr __P((char *value, int ljust, int len, int zpad, int maxwidth));
void fmtnum __P((long value, int base, int dosign, int ljust, int len, int zpad));
void dostr __P(( char * , int ));
char *output;
void dopr_outch __P(( int c ));
int	SyslogErrno;

void
sm_dopr( buffer, format, args )
       char *buffer;
       const char *format;
       va_list args;
{
       int ch;
       long value;
       int longflag  = 0;
       int pointflag = 0;
       int maxwidth  = 0;
       char *strvalue;
       int ljust;
       int len;
       int zpad;
#if !HASSTRERROR && !defined(ERRLIST_PREDEFINED)
	extern char *sys_errlist[];
	extern int sys_nerr;
#endif /* !HASSTRERROR && !defined(ERRLIST_PREDEFINED) */


       output = buffer;
       while( (ch = *format++) != '\0' ){
	       switch( ch ){
	       case '%':
		       ljust = len = zpad = maxwidth = 0;
		       longflag = pointflag = 0;
	       nextch:
		       ch = *format++;
		       switch( ch ){
		       case 0:
			       dostr( "**end of format**" , 0);
			       return;
		       case '-': ljust = 1; goto nextch;
		       case '0': /* set zero padding if len not set */
			       if(len==0 && !pointflag) zpad = '0';
				/* FALLTHROUGH */
		       case '1': case '2': case '3':
		       case '4': case '5': case '6':
		       case '7': case '8': case '9':
			       if (pointflag)
				 maxwidth = maxwidth*10 + ch - '0';
			       else
				 len = len*10 + ch - '0';
			       goto nextch;
		       case '*':
			       if (pointflag)
				 maxwidth = va_arg( args, int );
			       else
				 len = va_arg( args, int );
			       goto nextch;
		       case '.': pointflag = 1; goto nextch;
		       case 'l': longflag = 1; goto nextch;
		       case 'u': case 'U':
			       /*fmtnum(value,base,dosign,ljust,len,zpad) */
			       if( longflag ){
				       value = va_arg( args, long );
			       } else {
				       value = va_arg( args, int );
			       }
			       fmtnum( value, 10,0, ljust, len, zpad ); break;
		       case 'o': case 'O':
			       /*fmtnum(value,base,dosign,ljust,len,zpad) */
			       if( longflag ){
				       value = va_arg( args, long );
			       } else {
				       value = va_arg( args, int );
			       }
			       fmtnum( value, 8,0, ljust, len, zpad ); break;
		       case 'd': case 'D':
			       if( longflag ){
				       value = va_arg( args, long );
			       } else {
				       value = va_arg( args, int );
			       }
			       fmtnum( value, 10,1, ljust, len, zpad ); break;
		       case 'x':
			       if( longflag ){
				       value = va_arg( args, long );
			       } else {
				       value = va_arg( args, int );
			       }
			       fmtnum( value, 16,0, ljust, len, zpad ); break;
		       case 'X':
			       if( longflag ){
				       value = va_arg( args, long );
			       } else {
				       value = va_arg( args, int );
			       }
			       fmtnum( value,-16,0, ljust, len, zpad ); break;
		       case 's':
			       strvalue = va_arg( args, char *);
			       if (maxwidth > 0 || !pointflag) {
				 if (pointflag && len > maxwidth)
				   len = maxwidth; /* Adjust padding */
				 fmtstr( strvalue,ljust,len,zpad, maxwidth);
			       }
			       break;
		       case 'c':
			       ch = va_arg( args, int );
			       dopr_outch( ch ); break;
		       case 'm':
#if HASSTRERROR
			       dostr(strerror(SyslogErrno), 0);
#else /* HASSTRERROR */
			       if (SyslogErrno < 0 || SyslogErrno >= sys_nerr)
			       {
				   dostr("Error ", 0);
				   fmtnum(SyslogErrno, 10, 0, 0, 0, 0);
			       }
			       else
				   dostr((char *)sys_errlist[SyslogErrno], 0);
#endif /* HASSTRERROR */
			       break;

		       case '%': dopr_outch( ch ); continue;
		       default:
			       dostr(  "???????" , 0);
		       }
		       break;
	       default:
		       dopr_outch( ch );
		       break;
	       }
       }
       *output = 0;
}

void
fmtstr(  value, ljust, len, zpad, maxwidth )
       char *value;
       int ljust, len, zpad, maxwidth;
{
       int padlen, strleng;     /* amount to pad */

       if( value == 0 ){
	       value = "<NULL>";
       }
       for( strleng = 0; value[strleng]; ++ strleng ); /* strlen */
       if (strleng > maxwidth && maxwidth)
	 strleng = maxwidth;
       padlen = len - strleng;
       if( padlen < 0 ) padlen = 0;
       if( ljust ) padlen = -padlen;
       while( padlen > 0 ) {
	       dopr_outch( ' ' );
	       --padlen;
       }
       dostr( value, maxwidth );
       while( padlen < 0 ) {
	       dopr_outch( ' ' );
	       ++padlen;
       }
}

void
fmtnum(  value, base, dosign, ljust, len, zpad )
       long value;
       int base, dosign, ljust, len, zpad;
{
       int signvalue = 0;
       unsigned long uvalue;
       char convert[20];
       int place = 0;
       int padlen = 0; /* amount to pad */
       int caps = 0;

       /* DEBUGP(("value 0x%x, base %d, dosign %d, ljust %d, len %d, zpad %d\n",
	       value, base, dosign, ljust, len, zpad )); */
       uvalue = value;
       if( dosign ){
	       if( value < 0 ) {
		       signvalue = '-';
		       uvalue = -value;
	       }
       }
       if( base < 0 ){
	       caps = 1;
	       base = -base;
       }
       do{
	       convert[place++] =
		       (caps? "0123456789ABCDEF":"0123456789abcdef")
			[uvalue % (unsigned)base  ];
	       uvalue = (uvalue / (unsigned)base );
       }while(uvalue);
       convert[place] = 0;
       padlen = len - place;
       if( padlen < 0 ) padlen = 0;
       if( ljust ) padlen = -padlen;
       /* DEBUGP(( "str '%s', place %d, sign %c, padlen %d\n",
	       convert,place,signvalue,padlen)); */
       if( zpad && padlen > 0 ){
	       if( signvalue ){
		       dopr_outch( signvalue );
		       --padlen;
		       signvalue = 0;
	       }
	       while( padlen > 0 ){
		       dopr_outch( zpad );
		       --padlen;
	       }
       }
       while( padlen > 0 ) {
	       dopr_outch( ' ' );
	       --padlen;
       }
       if( signvalue ) dopr_outch( signvalue );
       while( place > 0 ) dopr_outch( convert[--place] );
       while( padlen < 0 ){
	       dopr_outch( ' ' );
	       ++padlen;
       }
}

void
dostr( str , cut)
     char *str;
     int cut;
{
  if (cut) {
    while(*str && cut-- > 0) dopr_outch(*str++);
  } else {
    while(*str) dopr_outch(*str++);
  }
}

void
dopr_outch( c )
       int c;
{
#if 0
       if( iscntrl(c) && c != '\n' && c != '\t' ){
	       c = '@' + (c & 0x1F);
	       if( DoprEnd == 0 || output < DoprEnd )
		       *output++ = '^';
       }
#endif /* 0 */
       if( DoprEnd == 0 || output < DoprEnd )
	       *output++ = c;
       else
		SnprfOverflow++;
}

/*
**  QUAD_TO_STRING -- Convert a quad type to a string.
**
**	Convert a quad type to a string.  This must be done
**	separately as %lld/%qd are not supported by snprint()
**	and adding support would slow down systems which only
**	emulate the data type.
**
**	Parameters:
**		value -- number to convert to a string.
**
**	Returns:
**		pointer to a string.
*/

char *
quad_to_string(value)
	QUAD_T value;
{
	char *formatstr;
	static char buf[64];

	/*
	**  Use sprintf() instead of snprintf() since snprintf()
	**  does not support %qu or %llu.  The buffer is large enough
	**  to hold the string so there is no danger of buffer
	**  overflow.
	*/

#if NEED_PERCENTQ
	formatstr = "%qu";
#else /* NEED_PERCENTQ */
	formatstr = "%llu";
#endif /* NEED_PERCENTQ */
	sprintf(buf, formatstr, value);
	return buf;
}
/*
**  SHORTENSTRING -- return short version of a string
**
**	If the string is already short, just return it.  If it is too
**	long, return the head and tail of the string.
**
**	Parameters:
**		s -- the string to shorten.
**		m -- the max length of the string (strlen()).
**
**	Returns:
**		Either s or a short version of s.
*/

char *
shortenstring(s, m)
	register const char *s;
	int m;
{
	int l;
	static char buf[MAXSHORTSTR + 1];

	l = strlen(s);
	if (l < m)
		return (char *) s;
	if (m > MAXSHORTSTR)
		m = MAXSHORTSTR;
	else if (m < 10)
	{
		if (m < 5)
		{
			(void) strlcpy(buf, s, m + 1);
			return buf;
		}
		(void) strlcpy(buf, s, m - 2);
		(void) strlcat(buf, "...", sizeof buf);
		return buf;
	}
	m = (m - 3) / 2;
	(void) strlcpy(buf, s, m + 1);
	(void) strlcat(buf, "...", sizeof buf);
	(void) strlcat(buf, s + l - m, sizeof buf);
	return buf;
}
