/*    util.h
 *
 *    Copyright (c) 1991-2000, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#ifdef VMS
#  define PERL_FILE_IS_ABSOLUTE(f) \
	(*(f) == '/'							\
	 || (strchr(f,':')						\
	     || ((*(f) == '[' || *(f) == '<')				\
		 && (isALNUM((f)[1]) || strchr("$-_]>",(f)[1])))))

#else		/* !VMS */
#  ifdef WIN32
#    define PERL_FILE_IS_ABSOLUTE(f) \
	(*(f) == '/'							\
	 || ((f)[0] && (f)[1] == ':')		/* drive name */	\
	 || ((f)[0] == '\\' && (f)[1] == '\\'))	/* UNC path */
#  else		/* !WIN32 */
#    ifdef DOSISH
#      define PERL_FILE_IS_ABSOLUTE(f) \
	(*(f) == '/'							\
	 || ((f)[0] && (f)[1] == ':'))		/* drive name */
#    else	/* !DOSISH */
#      define PERL_FILE_IS_ABSOLUTE(f)	(*(f) == '/')
#    endif	/* DOSISH */
#  endif	/* WIN32 */
#endif		/* VMS */
