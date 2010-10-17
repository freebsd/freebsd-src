$!
$! This file configures the bfd library for use with openVMS (Alpha and Vax)
$!
$! We do not use the configure script, since we do not have /bin/sh
$! to execute it.
$!
$! Written by Klaus K"ampf (kkaempf@rmi.de)
$!
$arch_indx = 1 + ((f$getsyi("CPU").ge.128).and.1)      ! vax==1, alpha==2
$arch = f$element(arch_indx,"|","|VAX|Alpha|")
$!
$if arch .eqs. "Alpha"
$then
$ write sys$output "Configuring for Alpha target"
$ target = "alpha"
$!
$! copy bfd-in2.h to bfd.h, replacing @ macros
$!
$ edit/tpu/nojournal/nosection/nodisplay/command=sys$input -
        []bfd-in2.h /output=[]bfd.h
$DECK
!
!  Copy file, changing lines with macros (@@)
!
!
   vfile := CREATE_BUFFER("vfile", "CONFIGURE.IN");
   rang := CREATE_RANGE(BEGINNING_OF(vfile), END_OF(vfile));
   match_pos := SEARCH_QUIETLY('AM_INIT_AUTOMAKE(bfd, ', FORWARD, EXACT, rang);
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
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT(vers);
   ENDIF;
   match_pos := SEARCH_QUIETLY('@wordsize@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('64');
   ENDIF;
   match_pos := SEARCH_QUIETLY('@BFD_HOST_64BIT_LONG@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('0');
   ENDIF;
   match_pos := SEARCH_QUIETLY('@BFD_HOST_64_BIT_DEFINED@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('__DECC');
      SPLIT_LINE;
      COPY_TEXT('#include <ints.h>');
   ENDIF;
   match_pos := SEARCH_QUIETLY('@BFD_HOST_64_BIT@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('int64');
   ENDIF;
   match_pos := SEARCH_QUIETLY('@BFD_HOST_U_64_BIT@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('uint64');
   ENDIF;
   WRITE_FILE(file, GET_INFO(COMMAND_LINE, "output_file"));
   QUIT
$  EOD
$
$else
$
$ write sys$output "Configuring for Vax target"
$ target = "vax"
$!
$! copy bfd-in2.h to bfd.h, replacing @ macros
$!
$ edit/tpu/nojournal/nosection/nodisplay/command=sys$input -
        []bfd-in2.h /output=[]bfd.h
$DECK
!
!  Copy file, changing lines with macros (@@)
!
!
   vfile := CREATE_BUFFER("vfile", "CONFIGURE.IN");
   rang := CREATE_RANGE(BEGINNING_OF(vfile), END_OF(vfile));
   match_pos := SEARCH_QUIETLY('AM_INIT_AUTOMAKE(bfd, ', FORWARD, EXACT, rang);
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
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT(vers);
   ENDIF;
   match_pos := SEARCH_QUIETLY('@wordsize@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('32');
   ENDIF;
   match_pos := SEARCH_QUIETLY('@BFD_HOST_64BIT_LONG@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('0');
   ENDIF;
   match_pos := SEARCH_QUIETLY('@BFD_HOST_64_BIT_DEFINED@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('__DECC');
      SPLIT_LINE;
      COPY_TEXT('#include <ints.h>');
   ENDIF;
   match_pos := SEARCH_QUIETLY('@BFD_HOST_64_BIT@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('int64');
   ENDIF;
   match_pos := SEARCH_QUIETLY('@BFD_HOST_U_64_BIT@', FORWARD, EXACT, rang);
   IF match_pos <> 0 THEN;
      POSITION(BEGINNING_OF(match_pos));
      ERASE(match_pos);
      COPY_TEXT('uint64');
   ENDIF;
   WRITE_FILE(file, GET_INFO(COMMAND_LINE, "output_file"));
   QUIT
$  EOD
$endif
$
$ write sys$output "Generated `bfd.h' from `bfd-in2.h'."
$!
$!
$! create targmatch.h
$!
$ open/write tfile []targmatch.h
$ write tfile "{ """ + target + "-*-*vms*""" + ","
$ write tfile "#if defined (SELECT_VECS)"
$ write tfile "SELECT_VECS"
$ write tfile "#else"
$ write tfile "UNSUPPORTED_TARGET"
$ write tfile "#endif"
$ write tfile "},"
$ close tfile
$ write sys$output "Generated `targmatch.h'"
$!
$!
$! create config.h
$!
$ create []config.h
/* config.h-vms.  Generated by hand by Klaus Kämpf, kkaempf@didymus.rmi.de.  */
/* config.in.  Generated automatically from configure.in by autoheader.  */
/* Whether malloc must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_MALLOC */
/* Whether free must be declared even if <stdlib.h> is included.  */
/* #undef NEED_DECLARATION_FREE */
/* Define if you have a working `mmap' system call.  */
/* #define HAVE_MMAP 1 */
/* Do we need to use the b modifier when opening binary files?  */
/* #undef USE_BINARY_FOPEN */
/* Name of host specific header file to include in trad-core.c.  */
/* #undef TRAD_HEADER */
/* Define only if <sys/procfs.h> is available *and* it defines prstatus_t.  */
/* #undef HAVE_SYS_PROCFS_H */
/* Do we really want to use mmap if it's available?  */
/* #undef USE_MMAP */
/* Define if you have the fcntl function.  */
#define HAVE_FCNTL 1
/* Define if you have the getpagesize function.  */
#define HAVE_GETPAGESIZE 1
/* Define if you have the madvise function.  */
#define HAVE_MADVISE 1
/* Define if you have the mprotect function.  */
#define HAVE_MPROTECT 1
/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1
/* Define if you have the <stddef.h> header file.  */
#define HAVE_STDDEF_H 1
/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1
/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1
/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1
/* Define if you have the <sys/file.h> header file.  */
#define HAVE_SYS_FILE_H 1
/* Define if you have the <time.h> header file.  */
#define HAVE_TIME_H 1
/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1
$!
$ write sys$output "Generated `config.h'"

