$!
$! This file configures binutils for use with openVMS/Alpha
$! We do not use the configure script, since we do not have /bin/sh
$! to execute it.
$!
$! Written by Klaus K"ampf (kkaempf@rmi.de)
$!
$arch_indx = 1 + ((f$getsyi("CPU").ge.128).and.1)      ! vax==1, alpha==2
$arch = f$element(arch_indx,"|","|VAX|Alpha|")
$!
$!
$! Generate config.h
$!
$ create []config.h
/* config.h.  Generated automatically by configure.  */
/* config.in.  Generated automatically from configure.in by autoheader.  */
/* Is the type time_t defined in <time.h>?  */
#define HAVE_TIME_T_IN_TIME_H 1
/* Is the type time_t defined in <sys/types.h>?  */
#define HAVE_TIME_T_IN_TYPES_H 1
/* Does <utime.h> define struct utimbuf?  */
#define HAVE_GOOD_UTIME_H 1
/* Whether fprintf must be declared even if <stdio.h> is included.  */
#define NEED_DECLARATION_FPRINTF 1
/* Whether sbrk must be declared even if <unistd.h> is included.  */
#undef NEED_DECLARATION_SBRK
/* Do we need to use the b modifier when opening binary files?  */
/* #undef USE_BINARY_FOPEN */
/* Define if you have the sbrk function.  */
/* #undef HAVE_SBRK 1 */
/* Define if you have the utimes function.  */
#define HAVE_UTIMES 1
/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1
/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1
/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1
/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1
/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1
/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1
$ write sys$output "Generated `config.h'"
$!
$!
$! Edit VERSION in makefile.vms-in
$!
$ edit/tpu/nojournal/nosection/nodisplay/command=sys$input -
        []makefile.vms-in /output=[]makefile.vms
$DECK
!
! Get VERSION from configure.in
!
   mfile := CREATE_BUFFER("mfile", "CONFIGURE.IN");
   rang := CREATE_RANGE(BEGINNING_OF(mfile), END_OF(mfile));
   match_pos := SEARCH_QUIETLY('AM_INIT_AUTOMAKE(binutils, ', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
     POSITION(BEGINNING_OF(match_pos));
     ERASE(match_pos);
     vers := CURRENT_LINE-")";
   ELSE;
     vers := "unknown";
   ENDIF;

   file := CREATE_BUFFER("file", GET_INFO(COMMAND_LINE, "file_name"));
   rang := CREATE_RANGE(BEGINNING_OF(file), END_OF(file));
   match_pos := SEARCH_QUIETLY('@VERSION@', FORWARD, EXACT, rang);
   POSITION(BEGINNING_OF(match_pos));
   ERASE(match_pos);
   COPY_TEXT(vers);
   WRITE_FILE(file, GET_INFO(COMMAND_LINE, "output_file"));
   QUIT
$  EOD
$ write sys$output "Created `makefile.vms'"
