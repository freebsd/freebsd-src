/*
 * pam_log.c -- PAM system logging
 *
 * $Id: pam_log.c,v 1.2 2000/11/19 23:54:02 agmorgan Exp $
 *
 */

#include "pam_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef __hpux
# include <stdio.h>
# include <syslog.h>
# ifdef __STDC__
#  ifndef __P
#   define __P(p)  p
#  endif /* __P */
#  include <stdarg.h>
#  define VA_LOCAL_DECL	va_list ap;
#  define VA_START(f)	va_start(ap, f)
#  define VA_END	va_end(ap)
# else /* __STDC__ */
#  ifndef __P
#   define __P(p)  ()
#  endif /* __P */
#  include <varargs.h>
#  define VA_LOCAL_DECL	va_list ap;
#  define VA_START(f)	va_start(ap)
#  define VA_END	va_end(ap)
# endif /* __STDC__ */
/**************************************************************
 * Patrick Powell Tue Apr 11 09:48:21 PDT 1995
 * A bombproof version of doprnt (dopr) included.
 * Sigh.  This sort of thing is always nasty do deal with.  Note that
 * the version here does not include floating point...
 *
 * snprintf() is used instead of sprintf() as it does limit checks
 * for string length.  This covers a nasty loophole.
 *
 * The other functions are there to prevent NULL pointers from
 * causing nast effects.
 **************************************************************/

static void dopr();
static char *end;
# ifndef _SCO_DS
/* VARARGS3 */
int
#  ifdef __STDC__
snprintf(char *str, size_t count, const char *fmt, ...)
#  else /* __STDC__ */
snprintf(str, count, fmt, va_alist)
	char *str;
	size_t count;
	const char *fmt;
	va_dcl
#  endif /* __STDC__ */
{
	int len;
	VA_LOCAL_DECL

	VA_START(fmt);
	len = vsnprintf(str, count, fmt, ap);
	VA_END;
	return len;
}
# endif /* _SCO_DS */

int
# ifdef __STDC__
vsnprintf(char *str, size_t count, const char *fmt, va_list args)
# else /* __STDC__ */
vsnprintf(str, count, fmt, args)
	char *str;
	int count;
	char *fmt;
	va_list args;
# endif /* __STDC__ */
{
	str[0] = 0;
	end = str + count - 1;
	dopr( str, fmt, args );
	if (count > 0)
		end[0] = 0;
	return strlen(str);
}

/*
 * dopr(): poor man's version of doprintf
 */

static void fmtstr __P((char *value, int ljust, int len, int zpad,
			int maxwidth));
static void fmtnum __P((long value, int base, int dosign, int ljust, int len,
			int zpad));
static void dostr __P(( char * , int ));
static char *output;
static void dopr_outch __P(( int c ));

static void
# ifdef __STDC__
dopr(char  * buffer, const char * format, va_list args )
# else /* __STDC__ */
dopr( buffer, format, args )
       char *buffer;
       char *format;
       va_list args;
# endif /* __STDC__ */
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

       output = buffer;
       while( (ch = *format++) ){
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

static void
fmtstr(  value, ljust, len, zpad, maxwidth )
       char *value;
       int ljust, len, zpad, maxwidth;
{
       int padlen, strlen;     /* amount to pad */

       if( value == 0 ){
               value = "<NULL>";
       }
       for( strlen = 0; value[strlen]; ++ strlen ); /* strlen */
       if (strlen > maxwidth && maxwidth)
	 strlen = maxwidth;
       padlen = len - strlen;
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

static void
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

static void
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

static void
dopr_outch( c )
       int c;
{
       if( end == 0 || output < end )
               *output++ = c;
}

int 
# ifdef __STDC__
vsyslog(int priority, const char *fmt, ...)
# else /* __STDC__ */
vsyslog(priority, fmt, va_alist)
  int priority;
  const char *fmt;
  va_dcl
# endif /* __STDC__ */
{
    VA_LOCAL_DECL
    char logbuf[BUFSIZ];
    
    VA_START(fmt);

    vsnprintf(logbuf, BUFSIZ, fmt, ap);
    syslog(priority, "%s", logbuf);

    VA_END;
}
#endif /* __hpux */

/* internal logging function */

void _pam_system_log(int priority, const char *format, ... )
{
    va_list args;
    char *eformat;

    D(("pam_system_log called"));

    if (format == NULL) {
	D(("NULL format to _pam_system_log() call"));
	return;
    }

    va_start(args, format);

    eformat = malloc(sizeof(_PAM_SYSTEM_LOG_PREFIX)+strlen(format));
    if (eformat != NULL) {
	strcpy(eformat, _PAM_SYSTEM_LOG_PREFIX);
	strcpy(eformat + sizeof(_PAM_SYSTEM_LOG_PREFIX) - 1, format);
	vsyslog(priority, eformat, args);
	_pam_overwrite(eformat);
	_pam_drop(eformat);
    } else {
	vsyslog(priority, format, args);
    }

    va_end(args);

    D(("done."));
}

