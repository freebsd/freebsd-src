2001-09-26  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.ac (AM_INIT_AUTOMAKE): Version 1.13.25.

	* src/buffer.c (flush_read): Don't diagnose partial blocks before
	end of file; just ignore them silently.

	* src/list.c (read_header): Don't keep around extended name
	and link info indefinitely; keep it only for the next file.
	This fixes a bug introduced in 1.13.24, and removes the need
	for some static variables.  Set recent_long_name and
	recent_long_link to zero if there were no long links; this
	avoids a violation of ANSI C rules for pointers in delete.c.
	* THANKS: Add Christian Laubscher.

2001-09-26  Jim Meyering  <meyering@lucent.com>

	* doc/tar.texi (Remote Tape Server): is know -> is known

2001-09-25  Paul Eggert  <eggert@twinsun.com>

	* lib/unicodeio.c (EILSEQ): Include <iconv.h> first, since
	<iconv.h> may define EILSEQ (e.g. libiconv).  Define a
	replacement EILSEQ to be ENOENT, not EINVAL, since callers may
	want to distinguish EINVAL and EILSEQ.

2001-09-24  Christophe Kalt  <Christophe.Kalt@kbcfp.com>

	* src/extract.c (maybe_recoverable):
	Treat OVERWRITE_OLD_DIRS like DEFAULT_OLD_FILES.

2001-09-22  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.ac (AM_INIT_AUTOMAKE): Version 1.13.24.

	* ABOUT-NLS, intl/*: Update to gettext-0.10.40, replacing LGPL
	with GPL.

	* INSTALL, mkinstalldirs: Update to autoconf 2.52 version.
	* PORTS: Add copyright notice, 'star' reference.
	* README-alpha: Add copyright notice, autoconf 2.52 patch.
	* THANKS: Add Christophe Kalt.
	* config.sub: Upgrade to 2001-09-14 version.

	* configure.ac (ALL_LINGUAS): Add ko.
	* po/ko.po: Resurrected file.

	* doc/convtexi.pl: Add coding advice for Emacs.

	* doc/getdate.texi: Add copyright notice.

	* doc/mdate-sh: Upgrade to automake 1.5 version.

	* doc/tar.texi (extracting files): Mention --to-stdout.
	(Option Summary, Dealing with Old Files): New option --overwrite-dir.
	(Overwrite Old Files): Likewise.

	* lib/Makefile.am (noinst_HEADERS):
	Remove copysym.h.  Add print-copyr.h, unicodeio.h.
	(libtar_a_SOURCES): Remove copysym.c, Add print-copyr.c, unicodeio.c.

	* lib/copysym.c, lib/copysym.h: Remove.
	* lib/print-copyr.c, lib/print-copyr.h, lib/unicodeio.c,
	lib/unicodeio.h: New files.

	* lib/error.c, lib/getopt.c, lib/getopt.h, lib/getopt1.c,
	lib/mktime.c, lib/strtoll.c: Switch from LGPL to GPL.

	* lib/quotearg.c (HAVE_MBSINIT): Undef if !HAVE_MBRTOWC.
	(mbsinit): Define to 1 if !defined mbsinit && !HAVE_MBSINIT.

	* m4/Makefile.am (EXTRA_DIST): Remove isc-posix.m4.
	* m4/isc-posix.m4: Remove.

	* m4/prereq.m4 (jm_PREREQ_QUOTEARG): Check for mbsinit.

	* po/POTFILES.in: Add copyright notice.

	* src/Makefile.am (LDADD): Like libtar.a before @INTLLIBS@ as
	well as after.
	* tests/Makefile.am (LDADD): Likewise.

	* src/buffer.c (write_archive_buffer, close_archive):
	If an archive is a socket, treat it like a FIFO.
	(records_read, records_written): New vars.
	(write_archive_to_stdout): Now bool, not int.
	(open_archive, flush_write, flush_read): Keep records_read and
	records_written up to date.

	* src/common.h (enum old_files): New value OVERWRITE_OLD_DIRS.
	(write_archive_to_stdout): Now bool, not int.
	(enum read_header): New value HEADER_SUCCESS_EXTENDED.
	(read_header): Now takes bool arg.  Existing callers modified
	to pass 0, unless otherwise specified.

	* src/delete.c (records_read): Remove; now a global.
	(acting_as_filter): Now bool, not int.
	(recent_long_name, recent_long_link, recent_long_name_blocks,
	recent_long_link_blocks, records_read, records_written): New decls.
	(records_skipped): New var.
	(move_archive): Don't divide by zero if arg is 0.
	Use the above vars to compute how far to move.
	(write_recent_blocks): New function.
	(delete_archive_member): Pass 1 to read_header, so that it doesn't
	read more than 1 block.  Handle resulting HEADER_SUCCESS_EXTENDED code.
	Keep track of how many records have been skipped.
	Let the buffer code count records.
	When copying a header, copy any extended headers that came before it.

	* src/extract.c (extract_archive): When marking a directory to be
	updated after symlinks, stat all directories after it in the
	delayed-set-stat list too, since they will be checked after
	symlinks.  Add support for --overwrite-dir.

	* src/list.c (recent_long_name, recent_long_link,
	recent_long_name_blocks, recent_long_link_blocks): New vars.
	(read_and): Pass 0 to read_header.
	(read_header): New arg RAW_EXTENDED_HEADERS.  Store away extended
	headers into new vars.  Null-terminate incoming symbolic links.

	* src/rmt.c: Include print-copyr.h, not copysym.h.
	(main): Use print_copyright, not copyright_symbol.
	* src/tar.c (decode_options): Likewise.
	(OVERWRITE_DIR_OPTION): New constant.
	(long_options, usage, decode_options): Add --overwrite-dir.

	* src/tar.h: Put copyright notice into documentation.

	* tests/Makefile.am (TESTS): Add delete03.sh.
	* tests/delete03.sh: New file.

	* tests/genfile.c: Include print-copyr.h, not copysym.h.
	(main): Use print_copyright, not copyright_symbol.
	Include <argmatch.h>.
	(pattern_strings): Remove.
	(pattern_args, pattern_types): New constants.
	(main): Use XARGMATCH, not argmatch.

2001-09-20  Jim Meyering  <meyering@lucent.com>

	* lib/xstrtol.c (strtoimax): Guard declaration with
	`#if !HAVE_DECL_STRTOIMAX', rather than just `#ifndef strtoimax'.
	The latter fails because some systems (at least rs6000-ibm-aix4.3.3.0)
	have their own, conflicting declaration of strtoimax in sys/inttypes.h.
	(strtoumax): Likewise, for completeness (it wasn't necessary).
	* m4/xstrtoimax.m4 (jm_AC_PREREQ_XSTRTOIMAX):
	Check for declaration of strtoimax.
	* m4/xstrtoumax.m4 (jm_AC_PREREQ_XSTRTOUMAX):
	Check for declaration of strtoumax.

2001-09-16  Paul Eggert  <eggert@twinsun.com>

	* fnmatch.m4 (jm_FUNC_FNMATCH): Fix typo in previous patch: yes -> no.

2001-09-14  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.ac (AC_INIT_AUTOMAKE): Version 1.13.23.

	* README-alpha: Describe automake patch.

	* configure.ac (LIBOBJS):
	Remove automake 1.4 workaround, as we're using 1.5 now.
	(USE_INCLUDED_LIBINTL): New AC_DEFINE.

	* lib/copysym.c: Include stddef.h, for size_t.
	Include langinfo.h if needed.
	Use locale_charset only if USE_INCLUDED_LIBINTL;
	if not, use nl_langinfo (CODESET) if available.

2001-09-13  Paul Eggert  <eggert@twinsun.com>

	* config.guess, config.sub: Sync with canonical versions.

	* configure.ac (jm_PREREQ_XGETCWD): Add.

	* lib/Makefile.am (noinst_HEADERS): Add copysym.h.
	(libtar_a_SOURCES): Add copysym.c.
	* copysym.c, copysym.h: New files.

	* lib/error.c: Sync with fileutils version.

	* m4/Makefile.am (EXTRA_DIST): Add getcwd.m4; remove uintmax_t.m4.
	* m4/getcwd.m4: New file.
	* m4/uintmax_t.m4: Remove.

	* m4/gettext.m4 (AM_WITH_NLS):
	Fix bug with calculating version of Bison 1.29.
	Reported by Karl Berry.

	* src/Makefile.am (datadir): Remove.

	* src/rmt.c: Include copysym.h.
	(main): Use copyright_symbol to translate copyright notice,
	instead of gettext.
	* src/tar.c: Likewise.
	* tests/genfile.c: Likewise.

	* src/system.h (MB_LEN_MAX): New symbol.

2001-09-11  Paul Eggert  <eggert@twinsun.com>

	* src/extract.c (struct delayed_set_stat): New member
	'after_symlinks'.
	(delay_set_stat): Initialize it to 0.
	(set_mode): New arg current_stat_info.  Use it (if nonnull) to avoid
	taking an extra stat ourselves.  All callers changed.
	(set_stat): Likewise.
	(apply_nonancestor_delayed_set_stat): New arg 'after_symlinks'.
	If false, stop when encountering a struct whose 'after_symlinks'
	member is true.  Otherwise, go through all structures but check
	them more carefully.  All callers changed.
	(extract_archive): When extracting a deferred symlink, if its parent
	directory's status needs fixing, then mark the directory as needing
	to be fixed after symlinks.
	(extract_finish): Fix status of ordinary directories, then apply
	delayed symlinks, then fix the status of directories that are
	ancestors of delayed symlinks.

	* src/rtapelib.c (rexec):
	Remove declaration; it ran afoul of prototypes on Crays.
	Reported by Wendy Palm of Cray.

2001-09-06  Paul Eggert  <eggert@twinsun.com>

	* lib/strtoimax.c (HAVE_LONG_LONG):
	Redefine to HAVE_UNSIGNED_LONG_LONG if unsigned.
	(strtoimax): Use sizeof (long), not
	sizeof strtol (ptr, endptr, base),
	to work around bug in IBM C compiler.

2001-09-04  Paul Eggert  <eggert@twinsun.com>

	* lib/xgetcwd.c: Include "xalloc.h".
	(xgetcwd): Do not return NULL when memory is exhausted; instead,
	report an error and exit.

	* m4/prereq.m4 (jm_PREREQ_XREADLINK): New macro.
	(jm_PREREQ): Use it.

2001-09-03  Paul Eggert  <eggert@twinsun.com>

	* m4/prereq.m4 (jm_PREREQ): Add jm_PREREQ_XGETCWD.
	(jm_PREREQ_XGETCWD): New macro.

	* lib/exclude.c (fnmatch_no_wildcards):
	Fix typo that caused us to do case-folding
	search even when that was not desired.  This occurred only in the
	no-wildcard case.

	* lib/xgetcwd.c: Include pathmax.h if not HAVE_GETCWD.
	Do not include xalloc.h.
	(INITIAL_BUFFER_SIZE): New symbol.
	Do not use xmalloc / xrealloc, since the caller is responsible for
	handling errors.  Preserve errno around `free' during failure.
	Do not overrun buffer when using getwd.

	* lib/xgetcwd.c (xgetcwd):
	Use HAVE_GETCWD_NULL, not defined __GLIBC__ && __GLIBC__ >= 2,
	to decide whether to use getcwd (NULL, 0).

2001-09-02  Paul Eggert  <eggert@twinsun.com>

	* lib/xgetcwd.c: Fix typo in local var; from Jim Meyering.

2001-09-01  Jim Meyering  <meyering@lucent.com>

	* exclude.c: Use `""', not `<>' to #include non-system header files.
	(fnmatch_no_wildcards): Rewrite not to use function names, strcasecmp
	and strncasecmp as r-values.  Unixware didn't have declarations.

2001-08-31  Jim Meyering  <meyering@lucent.com>

	* lib/xgetcwd.c (xgetcwd): Reorganize to avoid some duplication.
	Use an initial, malloc'd, buffer of length 128 rather than
	a statically allocated one of length 1024.

2001-08-30  Paul Eggert  <eggert@twinsun.com>

	* lib/utime.c: Include full-write.h.
	* lib/xstrtol.c (strtoimax): New decl.

2001-08-29  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.ac (AC_INIT_AUTOMAKE): Version 1.13.22.

	* src/create.c (dump_file): Relativize link names before dumping.
	This fixes a bug reported by Jose Pedro Oliveira.

	* src/create.c (dump_file): Use offsetof when computing sizes for
	struct hack; this avoids wasted space in some cases.
	* src/incremen.c (note_directory, find_directory): Likewise.
	* src/names.c (name_gather, addname): Likewise.

	* src/extract.c (extract_archive): Use strcpy, not memcpy,
	for consistency with other code that does similar things.
	* src/names.c (name_gather): Likewise.

	* src/names.c (read_name_from_file, name_next, name_gather,
	add_hierarchy_to_namelist): Avoid quadratic behavior when
	reallocating buffers.  Check for buffer size overflow.
	(addname): Avoid unnecessary clearing of memory.

2001-08-29  "Jan D."  <Jan.Djarv@mbox200.swipnet.se>

	* src/extract.c (delay_set_stat): Fix off-by-one error in file
	name size allocation that caused core dumps.

2001-08-28  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.ac (AC_INIT_AUTOMAKE): Version 1.13.21.

	* configure.ac (GNU_SOURCE): Define to 1, not /**/.
	(major_t, minor_t, ssize_t): Use new-style AC_CHECK_TYPE.
	(daddr_t): Remove; no longer used.
	(jm_PREREQ_HUMAN): Add.

	* acconfig.h: Remove; no longer needed.

	* config.guess, config.sub:
	New files, from automake 1.5.  Gettext 0.10.39 needs them.
	* depcomp, missing, mkinstalldirs: Upgrade to automake 1.5.

	* Makefile.am (AUTOMAKE_OPTIONS): Add dist-bzip2.
	(SUBDIRS): Put intl before lib, as gettext requires.

	* ABOUT-NLS: Upgrade to gettext 0.10.39.
	* intl: Upgrade entire directory to gettext 0.10.39.
	* m4/codeset.m4, m4/glibc21.m4, m4/iconv.m4:
	New files, from gettext 0.10.39.
	* m4/gettext.m4, m4/isc-posix.m4, m4/lcmessage.m4, m4/progtest.m4,
	Upgrade to gettext 0.10.39,
	* po/Makefile.in.in: Likewise, except fix a typo in its copying
	permissions.
	* po/cat-id-tbl.c, po/stamp-cat-id:
	Remove; no longer used by gettext 0.10.39.
	* po/ChangeLog: New file.

	* doc/Makefile.am (EXTRA_DIST): Add freemanuals.texi.
	$(srcdir)/tar.texi: Likewise.
	* doc/freemanuals.texi: New file.
	* doc/tar.texi (Free Software Needs Free Documentation): New appendix.
	`fileds' -> `fields'
	* doc/texinfo.tex: Upgrade to version 2001-07-25.07.

	* lib/Makefile.am (EXTRA_DIST): Add strtoll.c, strtoimax.c.
	(noinst_HEADERS): Add quote.h.
	(libtar_a_SOURCES): Add quote.c, xstrtoimax.c.

	* lib/exclude.c: Fix typo in '#include <stdint.h>' directive.

	* lib/full-write.c, lib/savedir.c: Comment fix.

	* lib/pathmax.h: Remove.

	* lib/quote.c, lib/quote.h: New files.

	* lib/xgetcwd.c: Don't include pathmax.h.
	Include stdlib.h and unistd.h if available.
	Include xalloc.h.
	(xmalloc, xstrdup, free): Remove decls.
	(xgetcwd): Don't assume sizes fit in unsigned.
	Check for overflow when computing sizes.
	Simplify reallocation code.

	* lib/xmalloc.c: Quote failure tests.

	* lib/strtoumax.c, lib/xstrtoimax.c: New files.

	* lib/strtoimax.c: Renamed from strtouxmax.c.  Make it more
	similar to strtol.c.
	(UNSIGNED): Renamed from STRTOUXMAX_UNSIGNED.
	(verify): New macro.
	(strtoumax, uintmax_t, strtoull, strtol): Remove.
	(intmax_t, strtoimax, strtol, strtoll): New macros, if UNSIGNED.
	(strtoimax): Renamed from strtoumax.  All uses of unsigned values
	changed to signed values.  Check sizes at compile-time, not
	run-time.  Prefer strtol to strtoll if both work.
	(main): Remove.

	* lib/xstrtol.h (xstrtoimax): New decl.

	* m4/Makefile.am (EXTRA_DIST):
	Add codeset.m4, glibc21.m4, iconv.m4, inttypes.m4,
	longlong.m4, xstrtoimax.m4.

	* m4/inttypes.m4 (jm_AC_HEADER_INTTYPES_H):
	Remove; now done by autoconf.
	(jm_AC_TYPE_INTMAX_T, jm_AC_TYPE_UINTMAX_T): Replace with
	Use AC_CHECK_TYPE instead of merely looking for the header.

	* m4/uintmax_t.m4: Use shorter comment.

	* m4/xstrtoumax.m4 (jm_AC_PREREQ_XSTRTOUMAX):
	Quote first arg of AC_DEFUN.
	Require jm_AC_TYPE_INTMAX_T and jm_AC_TYPE_LONG_LONG since they
	is needed to parse the include file.
	Simplify logic behind the args to AC_REPLACE.

	* src/Makefile.am (OMIT_DEPENDENCIES): Remove.

	* src/ansi2knr.1, src/ansi2knr.c: Remove; wasn't being used.

	* src/rmt.c (main):
	Use "Copyright %d" to simplify the translator's job in the future.
	Advise translator about circle-C.
	* src/tar.c: (decode_options): Likewise.
	* tests/genfile.c (main): Likewise.

2001-08-28  Jim Meyering  <meyering@lucent.com>

	* lib/argmatch.c: Include "quote.h".
	(argmatch_invalid): Quote the context.

	* lib/dirname.c (dir_name): Fix typo on PC platforms.

	* lib/backupfile.c, lib/basename.c, lib/dirname.c, lib/strtoul.c:
	Use single-quote for local .h files.

	* lib/error.h (__attribute__): Don't depend on __STRICT_ANSI__.

	* lib/getopt.c, lib/getopt.h, lib/getopt1.c: Upgrade to recent
	glibc versions.

	* lib/getdate.y (get_date): Initialize tm_isdst to -1 before
	invoking mktime the last time.

	* lib/pathmax.h: Use #if rather than #ifdef for HAVE_UNISTD_H.

	* lib/rename.c: Major rewrite by Volker Borchert to use system
	rename function, but to work around problems with trailing
	slashes.

	* lib/strtoll.c: New file, from glibc.
	* lib/strtoul.c: Update from glibc.

	* lib/strtouxmax.c: Renamed from lib/strtoumax.c.
	Add support for signed numbers, too.
	(strtoul, strtoull): Do not declare if STRTOUXMAX_UNSIGNED
	is not defined.
	(strtol, strtoll): Declare as needed, if STRTOUXMAX_UNSIGNED is
	not defined.
	(strtoumax, uintmax_t, strtoull, strtoul): New macros.
	(main): Use generic names in debugging output.
	* lib/strtoimax.c: Plus add the following changes of my own:
	(main): Use accurate names in debugging output.

	* lib/xgetcwd.c (xgetcwd): Use getcwd if glibc 2 or later.
	Don't use PATH_MAX.

	* m4/c-bs-a.m4, m4/check-decl.m4, m4/d-ino.m4, m4/error.m4,
	m4/getline.m4, m4/jm-mktime.m4, m4/malloc.m4, m4/mbrtowc.m4,
	m4/mbstate_t.m4, m4/realloc.m4, m4/uintmax_t.m4, m4/utimbuf.m4,
	m4/utime.m4, m4/utimes.m4:
	Quote the first argument in each use of AC_DEFUN.

	* m4/getline.m4: Don't use string.h.

	* m4/inttypes.m4, m4/longlong.m4, m4/xstrtoimax.m4: New files.

	* m4/mbrtowc.m4 (jm_FUNC_MBRTOWC): @%:@ -> #.

2001-08-27  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.ac (AC_INIT_AUTOMAKE): Version 1.13.20.

	The biggest change is the new --exclude semantics and options.
	The basic idea was suggested by Gerhard Poul; thanks!

	* NEWS: Describe new --exclude semantics and options, and bug fixes.
	* README: ignfail.sh fails on some NFS hosts.
	* NEWS, README, lib/xstrtol.h: Add copyright notice.

	* Makefile.am (ACLOCAL_AMFLAGS): Add -I m4.
	(M4DIR, ACINCLUDE_INPUTS, $(srcdir)/acinclude.m4):
	Remove; the automake bug has been fixed.
	* acinclude.m4: Remove.

	* configure.ac: Renamed from configure.in.
	(AC_PREREQ): Bump from 2.13 to 2.52.
	(ALL_LINGUAS): Add id, tr.  Remove ko, as po/ko.po (dated
	1997-05-30) has an encoding error.
	(jm_AC_HEADER_INTTYPES_H): Remove; now done by autoconf.
	(AC_FUNC_FNMATCH): Use AC_CONFIG_LINKS, not AC_LINK_FILES.

	* doc/fdl.texi: Update to current GNU version.

	* doc/tar.texi: Put leading '*' in direntry.
	Accommodate new gfdl sectioning.
	New option --recursion (the default) that is the inverse of
	--no-recursion.

	New options --anchored, --ignore-case, --wildcards,
	--wildcards-match-slash, and their negations (e.g., --no-anchored).
	Along with --recursion and --no-recursion, these control how exclude
	patterns are interpreted.  The default interpretation of exclude
	patterns is now --no-anchored --no-ignore-case --recursion
	--wildcards --wildcards-match-slash.

	* lib/Makefile.am (OMIT_DEPENDENCIES): Remove.

	* lib/exclude.c (bool): Declare, perhaps by including stdbool.h.
	(<sys/types.h>): Include only if HAVE_SYS_TYPES_H.
	(<stdlib.h>, <string.h>, <strings.h>, <inttypes.h>, <stdint.h>):
	Include	if available.
	(<xalloc.h>): Include
	(SIZE_MAX): Define if <stdint.h> or <inttypes.h> doesn't.
	(verify): New macro.  Use it to verify that EXCLUDE macros do not
	collide with FNM macros.
	(struct patopts): New struct.
	(struct exclude): Use it, as exclude patterns now come with options.
	(new_exclude): Support above changes.
	(new_exclude, add_exclude_file):
	Initial size must now be a power of two to simplify overflow checking.
	(free_exclude, fnmatch_no_wildcards): New function.
	(excluded_filename): No longer requires options arg, as the options
	are determined by add_exclude.  Now returns bool, not int.
	(excluded_filename, add_exclude):
	Add support for the fancy new exclusion options.
	(add_exclude, add_exclude_file): Now takes int options arg.
	Check for arithmetic overflow when computing sizes.
	(add_exclude_file): xrealloc might modify errno, so don't
	realloc until after errno might be used.

	* lib/exclude.h (EXCLUDE_ANCHORED, EXCLUDE_INCLUDE,EXCLUDE_WILDCARDS):
	New macros.
	(free_exclude): New decl.
	(add_exclude, add_exclude_file): Now takes int options arg.
	(excluded_filename): No longer requires options arg, as the options
	are determined by add_exclude.  Now returns bool, not int.

	* lib/prepargs.c: Include <string.h>; required for C99 since
	we use strlen.

	* lib/quotearg.c:
	BSD/OS 4.1 wchar.h requires FILE and struct tm to be declared.

	* lib/xstrtol.h (_DECLARE_XSTRTOL): Improve quality of
	diagnostic for LONGINT_INVALID_SUFFIX_CHAR.

	* m4/Makefile.am (EXTRA_DIST): Add check-decl.m4, mbrtowc.m4.
	Remove inttypes_h.m4, largefile.m4, mktime.m4.

	* m4/inttypes_h.m4, m4/largefile.m4, m4/mktime.m4: Remove;
	subsumed by Autoconf 2.50.

	* m4/error.m4: Upgrade to serial 2.

	* m4/fnmatch.m4 (jm_FUNC_FNMATCH): Upgrade to serial 4, but
	remove test for GNU C library.  It's not correct, as some
	older glibcs are buggy.

	* m4/getline.m4, m4/malloc.m4: Upgrade to serial 4.

	* m4/prereq.m4: Upgrade to serial 20, but then:
	(jm_PREREQ): Add jm_PREREQ_EXCLUDE.
	(jm_PREREQ_EXCLUDE): New macro.
	(jm_PREREQ_HUMAN): Remove jm_AC_HEADER_INTTYPES_H, as it is subsumed
	by autoconf 2.5x.

	* m4/realloc.m4: Upgrade to serial 4.

	* m4/strerror_r.m4: Revert to serial 1002.

	* m4/uintmax_t.m4: Upgrade to autoconf 2.5x.

	* m4/utimes.m4: Upgrade to latest version (still "serial 3").

	* m4/xstrtoumax.m4: Upgrade to serial 3, but then:
	(jm_AC_PREREQ_XSTRTOUMAX): Remove jm_AC_HEADER_INTTYPES_H, as
	it is now subsumed by autoconf.  Add inttypes.h.

	* po/cs.po, po/da.po, po/de.po, po/es.po, po/et.po, po/fr.po,
	po/it.po, po/pl.po, po/sl.po, po/sv.po: Sync with translation project.

	* src/buffer.c (new_volume): Stop if the script exits with an error.

	* src/common.h (excluded_with_slash, excluded_without_slash):
	Remove, replacing by:
	(excluded): New decl.
	(link_error): New decl.
	(excluded_name): Now returns bool.

	* src/extract.c:
	(struct delayed_symlinks, extract_archive, apply_delayed_symlinks):
	Support hard links to symbolic links.

	(struct delayed_symlink): Remove 'names' member, replacing it with
	'sources' and 'target' member.  All uses changed.

	(struct string_list): New type.

	(delayed_set_stat, extract_archive): Use offsetof when computing sizes
	for struct hack; this avoids wasted space in some cases.

	(extract_archive): Fix test for absolute pathnames and/or "..".
	Use link_error to report errors for links.
	Remove redundant trailing '/' at "really_dir", for all uses, not
	just before invoking mkdir.
	If overwriting old files, do not worry so much about existing
	directories.
	Fix mode computation in the case where the directory exists.

	(apply_delayed_symlinks): If we can't make a hard link to a symbolic
	link, make a copy of the symbolic link.

	* src/incremen.c (get_directory_contents):
	If ignore_failed_read_option, only warn about
	stat failures.

	* src/list.c (from_header): Do not issue a diagnostic if TYPE is zero.
	However, check for error even for '-' or '+' case.

	(print_header): Try parsing uids and gids as unsigned integers first,
	and as a uid_t or gid_t only if that fails.  This adds support for
	listing positive uids and gids that are greater than UID_MAX and
	GID_MAX.

	* src/misc.c (link_error): New function.

	* src/names.c (collect_and_sort_names):
	If ignore_failed_read_option, only warn about
	stat errors.

	(excluded_name): Now returns bool.  Simplify, as the fancy
	features are now all in excluded_filename.

	* src/rtapelib.c (base_name): Remove decl, as system.h now
	declares it.

	* src/system.h: Include stddef.h if available.
	(offsetof): Declare if stddef.h doesn't.

	Include <dirname.h>.
	(FILESYSTEM_PREFIX_LEN, ISSLASH): Remove; now defined by dirname.h.

	* src/tar.c (ANCHORED_OPTION, IGNORE_CASE_OPTION,
	NO_ANCHORED_OPTION, NO_IGNORE_CASE_OPTION, NO_WILDCARDS_OPTION,
	NO_WILDCARDS_MATCH_SLASH_OPTION, WILDCARDS_OPTION,
	WILDCARDS_MATCH_SLASH_OPTION):
	New enum values.

	(long_options, usage, decode_options): Add support for --anchored,
	--ignore-case, --no-anchored, --no-ignore-case, --no-wildcards,
	--no-wildcards-match-slash, --recursion, --wildcards,
	--wildcards-match-slash.

	(decode_options): Implement the new way of interpreting exclude
	patterns.

	(usage): --newer-mtime takes a DATE operand.  DATE may be a file name.

	(OPTION_STRING, decode_options): Add -I, -y.  Currently these options
	just print error messages suggesting alternatives.

	(add_filtered_exclude): Remove.

	* tests/Makefile.am (TESTS): Alphabetize, except put version.sh first.

	* tests/extrac04.sh (out): Remove
	directory/subdirectory/file1, as the new semantics for
	--exclude exclude it.

	* tests/genfile.c (main): Don't use non-ASCII char in msgid.

2001-08-12  Paul Eggert  <eggert@twinsun.com>

	* lib/addext.c (<errno.h>): Include.
	(errno): Declare if not defined.
	(addext): Work correctly on the Hurd, where pathconf returns -1 and
	leaves errno alone, because there is no limit.  Also, work even if
	size_t is narrower than long.

2001-07-08  Paul Eggert  <eggert@twinsun.com>

	* lib/alloca.c (alloca): Arg is of type size_t, not unsigned.

2001-05-10  Paul Eggert  <eggert@twinsun.com>

	* lib/addext.c (ISSLASH, base_name): Remove decls; now in dirname.h.
	Include <backupfile.h> and <dirname.h> after size_t is defined.
	(addext): Use base_len to trim redundant trailing slashes instead of
	doing it ourselves.

	* lib/backupfile.c (ISSLASH, base_name):
	Remove decls; now in dirname.h.
	Include <argmatch.h>, <backupfile.h>, <dirname.h> after size_t
	is defined.
	(find_backup_file_name): Rename locals to avoid new functions.
	Use base_len instead of rolling it ourselves.
	Work even if dirlen is 0.
	Use a dir of '.' if given the empty string.

	* lib/basename.c:
	Do not include <stdio.h>, <assert.h>; no longer needed.
	(FILESYSTEM_PREFIX_LEN, PARAMS, ISSLASH): Remove; now in dirname.h.
	Include <string.h>, <dirname.h>.
	(base_name): Allow file names ending in slashes, other than names
	that are all slashes.  In this case, return the basename followed
	by the slashes.

	* lib/dirname.c: Include <string.h> instead of <stdlib.h>.
	(FILESYSTEM_PREFIX_LEN, ISSLASH): Remove; now in dirname.h.
	(dir_len): Renamed from dirlen.
	All callers changed.

	* lib/dirname.h (DIRECTORY_SEPARATOR, ISSLASH, FILESYSTEM_PREFIX_LEN):
	New macros.
	(base_name, base_len, dir_len, strip_trailing_slashes): New decls.

2001-02-16  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c (mbrtowc, mbrtowc, mbsinit):
	Do not declare or define if HAVE_MBRTOWC,
	since the test for HAVE_MBRTOWC now requires proper declarations.

	* lib/alloca.c (malloc): Undef before defining.

2001-02-13  Paul Eggert  <eggert@twinsun.com>

	* src/compare.c (read_and_process): Use off_t for size.
	From Maciej W. Rozycki.

2001-01-26  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c: Include stddef.h.  From Jim Meyering.

2001-01-12  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AC_INIT_AUTOMAKE): Version 1.13.19.

	* lib/savedir.h (savedir): Remove size arg.

	* doc/tar.texi: Add @setchapternewpage odd.
	Remove -I as an alias for -T, for now.
	Add @dircategory.
	Update copyright.  Remove "Published by".
	Dates beginning with / or . are taken to be file names.

	* src/tar.c (<time.h>): Do not include;
	(time): Do not declare.
	(usage): Remove -I as an alias for -T.
	(OPTION_STRING): Remove -I.
	(decode_options): Dates that look like an absolute path name,
	or that start with '.', are presumed to be file names whose
	dates are taken.
	Remove 'I' as an aliase for 'T'.
	Update copyright.

	* src/extract.c (<time.h>): Do not include; system.h now does this.
	(make_directories): Skip filesystem prefixes.
	Don't assume '/' is the only separator.
	(extract_sparse_file): Use new full_write semantics.
	On write error, return instead of invoking skip_file.
	Do not free sparsearray; caller does this now.
	(apply_nonancestor_delayed_set_stat): Do not assume '/' is the only
	separator.
	(extract_archive): Don't assume file name lengths fit in int.
	Report what got stripped from member name; it might be more than '/'.
	Use new full_write semantics.
	Do not pass redundant trailing "/" to mkdir, as POSIX does not allow
	mkdir to ignore it.
	Do not report mkdir error if old_files_option == KEEP_OLD_FILES.

	* src/buffer.c (<time.h>): Do not include; system.h now does this.
	(time): Remove decl; likewise.
	(child_open_for_uncompress): Use new full_write semantics.
	(flush_write): Use ISSLASH instead of testing for '/'.
	(flush_read): Likewise.

	* src/rmt.h (_remdev): Look for / anywhere in Path.

	* src/misc.c (contains_dot_dot): Skip filesystem prefix.
	Don't assume '/' is the only separator.
	(safer_rmdir): Don't assume '/' is the only separator.

	* src/compare.c (diff_archive): Don't assume '/' is the only separator.

	* lib/dirname.h (dirlen): New decl.

	* src/incremen.c (get_directory_contents):
	Remove path_size arg; all callers changed.
	Don't assume '/' is the only directory separator.
	(gnu_restore): Work even if file name length doesn't fit in int.

	* lib/addext.c (ISSLASH): New macro.
	(addext): Trim any redundant trailing slashes.

	* src/names.c (name_next):
	Don't assume '/' is the only directory separator.
	(namelist_match): Likewise.
	(add_hierarchy_to_namelist): Remove dirsize arg.
	Do not assume '/' is the only directory separator.
	(new_name): Likewise.

	* lib/Makefile.am (noinst_HEADERS): Add dirname.h, full-write.h.
	(libtar_a_SOURCES): Add dirname.c.

	* src/create.c (relativize):
	New function, with much of old start_header's guts.
	Handle filesystem prefixes.
	(start_header): Use this new function.
	(init_sparsearray): Don't bother to zero out the new array;
	it's not needed.
	(deal_with_sparse): Fix array allocation bug.
	(create_archive): Don't assume '/' is the only separator.
	(dump_file): Likewise.
	Don't worry about leading / in symlink targets.

	* lib/savedir.c (savedir):
	Remove size arg; it wasn't portable.  All callers changed.

	* lib/utime.c (utime_null): Adjust to new full_write convention.

	* configure.in (YACC): Avoid portability problem with Ultrix sh.

	* lib/backupfile.c: Include <dirname.h>.
	(ISSLASH): New macro.
	(find_backup_file_name): Use dirlen to calculate directory lengths.
	(max_backup_version): Strip redundant trailing slashes.

	* src/common.h: Include <full-write.h>.
	(get_directory_contents): No longer has size arg.
	(gnu_restore): Arg is size_t, not int.

	* src/system.h: Include <time.h>.
	(time): Declare if not defined.

	* lib/full-write.c: Include full-write.h, not safe-read.h.
	full_write returns size_t, with short writes meaning failure.
	All callers changed.

	* src/rtapelib.c: Include full-write.h.

	* src/rmt.c: Include full-write.h.
	(main): Update copyright.

	* doc/getdate.texi: Mention that only English is supported.
	Show how to use "date" so that the output is acceptable to getdate.
	Mention Z as an abbreviation for UTC.

	* lib/full-write.h: New file.

	* src/list.c: system.h now does time.h stuff.

	* lib/dirname.c:
	Use HAVE_STDLIB_H, not STDC_HEADERS, to decide whether to include
	stdlib.h.
	Do not include string.h, strings.h, or assert.h; no longer needed.
	(strrchr, memrchr, malloc): Remove decls; no longer needed.
	Include <xalloc.h>.
	(base_name): New decl.
	(BACKSLASH_IS_PATH_SEPARATOR): Remove.
	(dir_name_r): Remove.
	(dirlen): New function.
	(dir_name): Use dirlen instead of dir_name_r.
	(<string.h>, <strings.h>): Include only if test program.
	(main): Use "return 0", not "exit (0)".

2000-12-08  Paul Eggert  <eggert@twinsun.com>

	* lib/dirname.h: New file.

2000-11-02  Vesselin Atanasov  <vesselin@bgnet.bg>

	* lib/fnmatch.c: Do not comment out all the code if we are using
	the GNU C library, because in some cases we are replacing buggy
	code in the GNU C library itself.

2000-10-30  Paul Eggert  <eggert@twinsun.com>

	* lib/fnmatch.c (FOLD): Do not assume that characters are unsigned.

2000-10-29  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AC_INIT_AUTOMAKE): Version 1.13.18.

	* src/tar.c: Include <fnmatch.h>, for FNM_LEADING_DIR.

2000-10-28  Paul Eggert  <eggert@twinsun.com>

	* doc/tar.texi: --no-recursion now applies to extraction, too.
	* src/create.c (dump_file): no_recurse_option -> ! recursion_option
	* src/names.c (namelist_match, excluded_name):
	Do not match subfiles of a directory
	if --no-recursion is specified.
	* src/tar.c (NO_RECURSE_OPTION): Remove.
	(long_options): Have getopt set the --no-recursion flag.
	(decode_options): Initialize recursion_option to FNM_LEADING_DIR.
	Remove case for NO_RECURSE_OPTION.
	* src/common.h (recursion_option):
	Renamed from no_recurse_option, with sense
	negated, and with FNM_LEADING_DIR being the nonzero value.

	* names.c (namelist_match): New function.
	(name_match, name_scan): Use it to eliminate duplicate code.
	(names_notfound): Remove special case for Amiga.

2000-10-27  Paul Eggert  <eggert@twinsun.com>

	* src/misc.c (read_error_details, read_warn_details,
	read_fatal_details): Don't assume size_t is unsigned long.

	* src/buffer.c (flush_read): If read_full_records_option, try to
	fill the input buffer, as --delete -f - needs this.

2000-10-24  Paul Eggert  <eggert@twinsun.com>

	* m4/strerror_r.m4 (AC_FUNC_STRERROR_R): Port to autoconf 2.13.

	* src/buffer.c (check_label_pattern):
	Make sure header name is a string before
	passing it to fnmatch.
	(init_volume_number): Check for global_volno overflow.
	(new_volume): Check for global_volno overflow.

	* src/tar.c (decode_options):
	Check that volume label is not too long to overflow
	name in tar header block.

	* Makefile.am (EXTRA_DIST): Remove rebox.el.

	* configure.in (HAVE_DECL_STRERROR_R): Remove our handwritten code.
	(AC_FUNC_STRERROR_R): Use this instead.

2000-10-23  Paul Eggert  <eggert@twinsun.com>

	* src/extract.c: Include <time.h>, since we invoke "time".

	* lib/prepargs.c (prepend_default_options):
	Don't use NULL, for portability.

	* m4/fnmatch.m4: Add "working" to message.

	* src/names.c: (_GNU_SOURCE): Remove; autoconf now does this.
	Include <hash.h>.
	(getpwuid, getgrgid): Declare only if system headers don't.
	(gid_to_gname): Don't invoke setgrent.
	(namelist): Now static, not global.
	(nametail): New var.  All uses of namelast changed to use
	nametail, with one extra level of indirection.
	(name_gather): Use memcpy instead of strncpy + assignment of NUL.
	(name_match): Set nametail too, when setting namelist to null.
	(add_hierarchy_to_namelist): Change type of dir arg from char * to
	struct name *, so that we don't have to look up the name again
	here.  Get change_dir from dir rather than as a separate arg.  Add
	dirsize arg, and pass it along to get_directory_contents.  Remove
	unnecessary check of directory type.
	(new_name): Do not append a slash if PATH already ends in one.
	(avoided_names, struct avoided_name): Remove.
	(avoided_name_table): New var, replacing avoided_names.
	(hash_avoided_name, compare_avoided_names): New function.
	(add_avoided_name, is_avoided_name): Use hash table rather than
	linked list.

	* src/buffer.c (_GNU_SOURCE): Remove; autoconf now does this.
	(child_open_for_compress, child_open_for_uncompress,
	close_archive): Propagate any failure of the compression process
	back to "tar".
	(open_archive, flush_write, flush_read, close_archive): Do not
	allocate an array of size PATH_MAX, as PATH_MAX might be (size_t)
	-1.  Instead, allocate an array with the size that's needed.
	(open_archive): Don't bother checking S_ISCHR of /dev/null.
	(backspace_output): Don't try to backspace past start of archive.
	(close_archive): Remove special case for DELETE_SUBCOMMAND.

	* acconfig.h (_GNU_SOURCE, DEFAULT_ARCHIVE, DEFAULT_BLOCKING,
	DENSITY_LETTER, DEVICE_PREFIX, EMUL_OPEN3, HAVE_GETGRGID,
	HAVE_GETPWUID, HAVE_MKNOD, HAVE_RTAPELIB, HAVE_ST_FSTYPE_STRING,
	HAVE_UNION_WAIT, HAVE_UTIME_H, HAVE_VALLOC, MTIO_CHECK_FIELD, PACKAGE,
	PROTOTYPES, REMOTE_SHELL, STD_INC_PATH, VERSION, WITH_CATALOGS,
	WITH_DMALLOC, WITH_REGEX):
	Remove; now generated automatically.

	* configure.in (_GNU_SOURCE): Define to empty, not 1, for
	compatibility for glibc fragments.
	(_GNU_SOURCE, HAVE_UTIME_H, MTIO_CHECK_FIELD,
	HAVE_ST_FSTYPE_STRING, HAVE_MKNOD, REMOTE_SHELL, DENSITY_LETTER,
	DEVICE_PREFIX, DEFAULT_ARCHIVE, DEFAULT_BLOCKING): Add comment so
	that we needn't put an entry into acconfig.h.
	(ALL_LINGUAS): Add da.
	(AC_C_BACKSLASH_A): Remove; jm_PREREQ_QUOTEARG now does this.
	(AC_CHECK_HEADERS): Add stdbool.h (for hash.h users), wctype.h
	(for strtol.c).
	(AC_MBSTATE_T): Add.
	(RMT): Append $(EXEEXT).
	(HAVE_GETGRGID, HAVE_GETPWUID, pe_AC_TYPE_SIGNED_CHAR): Remove.
	(HAVE_DECL_FREE, HAVE_DECL_GETGRGID, HAVE_DECL_GETPWUID,
	HAVE_DECL_GETENV, HAVE_DECL_MALLOC, HAVE_DECL_STRTOUL,
	HAVE_DECL_STRTOULL, HAVE_DECL_STRERROR_R): New macros.
	(jm_PREREQ_ADDEXT, jm_PREREQ_ERROR, jm_PREREQ_QUOTEARG): Add.
	(AC_REPLACE_FUNCS): Remove execlp; no longer needed.
	(AC_CHECK_FUNCS): Add clock_gettime; AC_SEARCH_LIBS wasn't enough.
	Remove mbrtowc; jm_PREREQ_QUOTEARG now does this.
	(EMUL_OPEN3): Remove; no longer needed.
	(DENSITY_LETTER, DEVICE_PREFIX): Simplify m4 quoting.

	* m4/fnmatch.m4 (AC_FUNC_FNMATCH): Detect d*/*1 vs d/s/1 bug.

	* src/common.h: Do not include basename.h.
	* src/rtapelib.c (base_name): Do not include basename.h;
	declare base_name instead.

	* lib/basename.h, lib/execlp.c, lib/getpagesize.h, lib/mkdir.c:
	Remove these files.
	* lib/getstr.c, lib/getstr.h, lib/hash.h, lib/hash.h, lib/prepargs.c,
	lib/prepargs.h, lib/savedir.c, lib/savedir.h: New files.
	* lib/Makefile.am (EXTRA_DIST, noinst_HEADERS, libtar_a_SOURCES):
	Adjust to the above changes.

	* lib/Makefile.am (AUTOMAKE_OPTIONS): Remove ../src/ansi2knr.

	* src/open3.c: Remove.

	* src/Makefile.am (AUTOMAKE_OPTIONS): Remove ansi2knr.
	(tar_SOURCES): Remove open3.c.
	(INCLUDES): Remove -I.., as automake does that.
	(OMIT_DEPENDENCIES): ../lib/fnmatch.h -> fnmatch.h.  Add localedir.h.

	The following changes are to put LOCALEDIR into localedir.h instead
	of passing it on the command line.
	(DEFS): Remove.
	(DISTCLEANFILES): New macro.
	(localedir.h): New rule.
	(rmt.o tar.o): Now depend on localedir.h.

	* tests/delete02.sh, tests/extrac04.sh: New files.

	* tests/Makefile.am (AUTOMAKE_OPTIONS): Remove ansi2knr.
	(TESTS): Add extrac04.sh, and restore delete02.sh.
	(DEFS): Remove; LOCALEDIR is now done via localedir.h.
	(INCLUDES): Remove -I.. as automake does this now.

	* src/rtapelib.c (rexec): Don't declare unless using it.
	(do_command): Simplify signal-handling code slightly.

	* src/delete.c (blocks_needed): Remove.  All uses changed to use
	blocking_factor - new_blocks.
	(acting_as_filter): New var.
	(write_record, delete_archive_members): Use acting_as_filter
	rather than archive == STDIN_FILENO to detect whether we're acting
	as a filter, as open can return STDIN_FILENO in some cases.
	(delete_archive_members): Ignore zero blocks if
	ignore_zeros_option is nonzero.  Fix bug that messed up last
	output block: write_eot can't be used here, as it gets confused
	when the input is at end of file.

	* src/compare.c (diff_archive): Do not impose an arbitrary limit on
	symbolic link contents length.  Pass directory size to
	get_directory_contents.

	* m4/decl.m4, m4/error.m4, m4/mbstate_t.m4, m4/prereq.m4,
	m4/strerror_r.m4: New files.
	* m4/signedchar.m4: Remove this file.
	* Makefile.am (ACINCLUDE_INPUTS): Adjust to above changes.
	* m4/Makefile.am (EXTRA_DIST): Likewise.

	* Makefile.am (DISTCLEANFILES): Add intl/libintl.h.

	* po/da.po: New translation file.

	* src/mangle.c (extract_mangle):
	Fix diagnostic with wrong number of %s'es.

	* lib/fnmatch.c (fnmatch):
	Fix some FNM_FILE_NAME and FNM_LEADING_DIR bugs,
	e.g. fnmatch("d*/*1", "d/s/1", FNM_FILE_NAME) incorrectly yielded zero.

	* lib/full-write.c (full_write): Some buggy drivers return 0 when you
	fall off a device's end.  Detect this.

	* src/system.h (IN_CTYPE_DOMAIN): Renamed from CTYPE_DOMAIN.  All
	uses changed.
	(open): Remove macro; we no longer support EMUL_OPEN3.  Do not
	include <pathmax.h> and directory include files like <dirent.h>;
	no longer used.  Include <savedir.h> instead.
	(closedir, signed_char): remove macro; no longer used.
	(bool, false, true): Include <stdbool.h> if you have the include
	file, otherwise define.

	* src/misc.c:
	(is_dot_or_dotdot, closedir_error, closedir_warn, opendir_error,
	opendir_warn, readdir_error): Remove; no longer needed.
	(safer_rmdir): Strip leading ./ (or .// or ./// or ././ or etc.)
	before deciding whether we're trying to remove ".".
	(remove_any_file): Try unlink first if we are not root.  Use
	savedir when recursively removing directories, to avoid exhausting
	file descriptors.
	(savedir_error, savedir_warn, symlink_error): New functions.

	* src/list.c: (read_and): Do not invoke
	apply_nonancestor_delayed_set_stat; DO_SOMETHING is now
	responsible for that.  Do not invoke apply_delayed_set_stat; our
	caller is now responsible for that.
	(read_header): Use signed char instead of signed_char.  Prevent
	later references to current_header from mistakenly treating it as
	an old GNU header.
	(from_header): Quote invalid base-64 strings in diagnostics.
	(time_from_header): Do not warn about future timestamps in
	archive; check_time now does that.
	(print_header): Quote unknown file types.
	(skip_member): New function, replacing skip_extended_headers and
	now skipping the whole member instead of just the extended
	headers.  All callers changed.  This makes the code handle
	extended headers uniformly, and fixes some bugs.

	* src/update.c (update_archive): Use skip_member.

	* src/extract.c (we_are_root): Now global.
	(struct delayed_symlink): New type.
	(delayed_symlink_head): New var.
	(extr_init, fatal_exit): Invoke extract_finish on fatal errors,
	not apply_delayed_set_stat.
	(set_mode, set_stat): Pointer args are now const pointers.
	(check_time): New function.
	(set_stat): Warn if setting a file's timestamp to be the future.
	(make_directories): Do not save and restore errno.
	(maybe_recoverable): Set errno to ENOENT if we cannot make missing
	intermediate directories.
	(extract_archive): Invoke apply_nonancestor_delayed_set_stat here,
	not in caller.  Extract potentially dangerous symbolic links more
	carefully, deferring their creation until the end, and using a
	regular file placeholder in the meantime.  Do not remove trailing
	/ and /. from file names.  Do not bother checking for ".." when
	checking whether a directory loops back on itself, as loopbacks
	can occur with symlinks too.  Also, in that case, do not bother
	saving and restoring errno; just set it to EEXIST.
	(apply_nonancestor_delayed_set_stat): A prefix is a potential
	ancestor if it ends in slash too (as well as ending in a char just
	before slash).
	(apply_delayed_set_stat): Remove.
	(apply_delayed_symlinks, extract_finish): New functions.

	* doc/fdl.texi: New file.
	* doc/Makefile.am (EXTRA_DIST): Add fdl.texi.
	($(srcdir)/tar.info): Add fdl.texi.  Invoke makeinfo with --no-split.
	* doc/tar.texi: Add Free Documentation License.  New section
	"Overwrite Old Files", and revamp that section to make it easier to
	follow.  "tar" -> "GNU tar" where appropriate.  Migrate getdate
	documentation into getdate.texi.  Fix several minor typos.  Describe
	TAR_OPTIONS.  Describe incompatibility between incremental backups and
	--atime-preserve.  Describe incompatibility between --verify and other
	options.  Mention that tar normally removes symbolic links rather than
	following them, when extracting a file of the same name.

	* THANKS: Add gpoul.  Change skip's address.

	* po/POTFILES.in: Add lib/human.c.

	* src/common.h (namelist, namelast): Remove decls.
	(we_are_root, extract_finish, skip_member, savedir_error,
	savedir_warn, symlink_error, gnu_list_name): New decls.
	(apply_delayed_set_stat, apply_nonancestor_delayed_set_stat,
	skip_extended_headers, is_dot_or_dotdot, closedir_error,
	closedir_warn, opendir_error, opendir_warn, readdir_error,
	readdir_warn): Remove decls.
	(get_directory_contents): New off_t arg.
	(addname): Now returns struct name *.

	* src/tar.h, tests/genfile.c: Fix comments.

	* src/create.c: Include hash.h.
	(gnu_list_name): Remove decl.
	(struct link): Remove "next" member.
	(linklist): Remove.
	(start_header): Say "leading `FOO'" rather than "`FOO' prefix" for
	consistency with other diagnostics.
	(deal_with_sparse): Check for I/O error when closing the file.
	(create_archive): Do not allocate an array of size PATH_MAX, as
	PATH_MAX might be (size_t) -1.  Instead, allocate an array with
	the size that's needed.
	(hash_link, compare_links): New functions.
	(dump_file): Do not exhaust open file descriptors when descending
	deeply into a directory, by using savedir rather than
	opendir/readdir.  Do not zero-fill the name buffer unnecessarily.
	Hash the set of links already created, instead of using a linked
	list.  Fix some bugs in outputting sparse files which caused the
	sparse tables to be incorrect.  When a file unexpectedly shrinks,
	output zeros rather than garbage.  Do not allocate an array of
	size PATH_MAX, as PATH_MAX might be (size_t) -1.  Instead,
	allocate an array with the size that's needed.

	* src/incremen.c: Include hash.h.
	(struct directory): Remove "next", "dir_text".  Change "name" to
	be char[1] with struct hack, not const char *.  Add "found".
	(directory_list): Remove.  Replaced by directory_table.
	(directory_table): New var.
	(nfs_string): Renamed from nfs.
	(hash_directory, compare_directories): New functions.
	(note_directory): Now returns struct directory *.  First arg is
	now const pointer.  struct stat arg is now dev_t, ino_t, nfs.
	Remove text arg.  New "found" arg, basically corresponding to the
	old text arg not being null.  All callers changed.
	(note_directory, find_directory): Use hash table rather than
	linked list.
	(get_directory_contents): New arg "device".  Use savedir to do the
	hard work.  Save the nfs-ness of stat_data, since it might change
	under us.  Use note_directory instead of find_directory to save
	some work.  When adding an "A" record, do it with
	add_to_accumulator instead of cheating with strcat.
	(read_directory_file): Use "+" flag before device to indicate
	whether it was NFS.  Fix typo in checking for strtoul error.
	(write_directory_file_entry): New function.
	(write_directory_file): Use it, and use the hash routines to
	traverse the directory table.
	(gnu_restore): Use savedir rather than opendir/readdir.

	* src/tar.c: Include localedir.h, prepargs.h.
	(long_options): Now static.
	(long_options, usage, decode_options): -j is now short for
	--bzip2, and -I is now an alias for -T.
	(decode_options, main): argv is not const pointer now.
	(decode_options): Invoke prepend_default_options to support
	TAR_OPTIONS.  In diagnostic, mention the string that was the
	invalid blocking factor, tape length, group, owner, or record
	size.  --delete is no longer incompatible with -f -, undoing
	2000-01-07 change.
	(main): Invoke extract_finish at end of extraction.

	* src/rmt.c: Include localedir.h.
	(main): Update copyright date to 2000.

	* doc/getdate.texi: New file, taken from fileutils 4.0.27, with the
	following changes: Use @sc where appropriate.  Document the ranges of
	supported times more precisely.  Add Eggert to getdate authors.
	Document old Latin 12m/12pm tradition.  Remove list of alphabetic time
	zone names, as it wasn't correct and people shouldn't be relying on it
	anyway.  Relative items also account for non-DST adjustments.  Fix
	some misspellings.

	* lib/prepargs.c, lib/prepargs.h, tests/extrac04.sh: New file.

	* tests/ignfail.sh: opendir -> savedir in diagnostics.

	* tests/preset.in: Set LANGUAGE to the empty string, for some
	brain damaged host.

2000-10-20  Paul Eggert  <eggert@twinsun.com>

	* m4/fnmatch.m4: Mention the GNU C library.

2000-10-19  Paul Eggert  <eggert@twinsun.com>

	* m4/fnmatch.m4: Add a couple more test cases to catch bugs in
	glibc 2.1.95.

2000-10-17  Paul Eggert  <eggert@twinsun.com>

	* lib/human.c (<limits.h>): Do not include; human.h does it if needed.
	(CHAR_BIT): Remove.

	* lib/human.h (<limits.h>): Include if HAVE_LIMITS_H.
	(CHAR_BIT): Define if not defined.

2000-09-09  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c: From fileutils: rename ISASCII to IN_CTYPE_DOMAIN.

2000-08-07  Paul Eggert  <eggert@twinsun.com>

	* lib/xmalloc.c: Memory exhausted -> memory exhausted

	* lib/xalloc.h (xalloc_msg_memory_exhausted):
	change to array from char *.

2000-08-06  Paul Eggert  <eggert@twinsun.com>

	* m4/mbstate_t.m4: Define mbstate_t to be int, not char, for
	compatibility with glibc 2.1.3 strftime.c.

2000-07-31  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c (quotearg_n_options):
	Don't make the initial slot vector a constant,
	since it might get modified.

	* lib/quotearg.c: Add support for more than one preallocated slot.

2000-07-30  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c (quotearg_n_options):
	Preallocate a slot 0 buffer, so that the caller
	can always quote one small component of a "memory exhausted" message
	in slot 0.

2000-07-23  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c:
	Include <wchar.h> even if ! (HAVE_MBRTOWC && 1 < MB_LEN_MAX), so that
	mbstate_t is always defined.

	Do not inspect MB_LEN_MAX, since it's incorrectly defined to be 1 in
	some GCC installations, and this configuration error is likely to be
	common.

2000-07-22  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c:
	When the system forces us to redefine mbstate_t, shadow its mbsinit
	function.  From Bruno Haible.

2000-07-14  Paul Eggert  <eggert@twinsun.com>

	* lib/xmalloc.c: Simplify exhausted message.

	* lib/quotearg.h: Update copyright date; from Jim Meyering.

2000-07-13  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.h (enum quoting style):
	New constant clocale_quoting_style.

	* lib/quotearg.c:
	(quoting_style_args, quoting_style_vals, quotearg_buffer_restyled):
	Add support for clocale_quoting_style, undoing previous change to
	locale_quoting_style.

2000-07-10  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c:
	<wchar.h>: Include only if HAVE_MBRTOWC && 1 < MB_LEN_MAX,
	since otherwise we don't need it.
	(MB_CUR_MAX): Redefine to 1 if ! (HAVE_MBRTOWC && 1 < MB_LEN_MAX),
	since we don't do multibytes in that case.
	(quotearg_buffer_restyled): If a unibyte locale, don't bother to
	invoke multibyte primitives.

	* m4/mbstate_t.m4 (AC_MBSTATE_T):
	Renamed from AC_MBSTATE_T_OBJECT.  All uses changed.
	Change from a two-part test, which defines both HAVE_MBSTATE_T_OBJECT
	and mbstate_t, to a single-part test that simply defines mbstate_t.

	* lib/quotearg.c (mbrtowc): Do not use HAVE_WCHAR_H in the definition.
	Use defined mbstate_t, not HAVE_MBSTATE_T_OBJECT,
	to decide whether to define the BeOS workaround macro;
	this adjusts to the change to AC_MBSTATE_T.

	* m4/strerror_r.m4: New file.

2000-07-05  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c: Use double-quote to quote.

	* lib/quotearg.c (N_): New macro.
	(gettext_default): New function.
	(quotearg_buffer_restyled): Use gettext_default ("{LEFT QUOTATION MARK}",
	"\"") for left quote, and gettext_default ("{RIGHT QUOTATION MARK}", "\"")
	for right quote.

	* lib/quotearg.c (struct quoting_options):
	Simplify quote_these_too dimension.
	From Bruno Haible  <haible@clisp.cons.org>.

	* m4/mbstate_t.m4 (AC_MBSTATE_T_OBJECT):
	Test for mbstate_t only if the test
	for an object-type mbstate_t fails.

	* lib/quotearg.c (mbrtowc): Declare returned type, since BeOS doesn't.

2000-07-03  Paul Eggert  <eggert@twinsun.com>

	* m4/mbstate_t.m4 (AC_MBSTATE_T_OBJECT): Port to autoconf 2.13.
	Add AC_CHECK_HEADERS(stdlib.h), since we use HAVE_STDLIB_H.

	* lib/quotearg.c (mbrtowc):
	Assign to *pwc, and return 1 only if result is nonzero.
	(iswprint): Define to ISPRINT if we are substituting our own mbrtowc.

2000-07-02  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c (mbstate_t):
	Do not define; it should be defined with AC_CHECK_TYPE.

2000-06-26  Paul Eggert  <eggert@twinsun.com>

	* m4/mbstate_t.m4: Include stdio.h before wchar.h, to work around
	a bug in glibc 2.1.3.

	* lib/xmalloc.c: Fix inaccorate comment for xrealloc.

2000-06-19  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c (ISASCII): Add #undef and move definition to follow
	inclusion of wctype.h to work around solaris2.6 namespace pollution.
	(ISPRINT): Likewise.
	Reported by Tom Tromey.

2000-06-15  Paul Eggert  <eggert@twinsun.com>

	* lib/human.c (adjust_value): New function.
	(human_readable_inexact): Apply rounding style even when printing
	approximate values.

	* lib/human.c: Avoid shadowing warnings.
	From Jim Meyering.

2000-06-14  Paul Eggert  <eggert@twinsun.com>

	* lib/human.c (human_readable_inexact): Allow an input block size
	that is not a multiple of the output block size, and vice versa.

	* lib/getdate.y (get_date): Apply relative times after time zone
	indicator, not before.

2000-05-31  Paul Eggert  <eggert@twinsun.com>

	* m4/largefile.m4: Rewrite so that we don't need to run getconf,
	and thus don't need AC_CANONICAL_HOST.

	(AC_SYS_LARGEFILE_FLAGS, AC_SYS_LARGEFILE_SPACE_APPEND): Remove.
	(AC_SYS_LARGEFILE_TEST_INCLUDES): New macro.
	(AC_SYS_LARGEFILE_MACRO_VALUE): Change arguments from
	CODE-TO-SET-DEFAULT to VALUE, INCLUDES, FUNCTION-BODY.  All uses
	changed.  Instead of inspecting the output of getconf, try to
	compile the test program without and with the macro definition.
	(AC_SYS_LARGEFILE): Do not require AC_CANONICAL_HOST or check for
	getconf.  Instead, check for the needed flags by compiling test
	programs.

	* configure.in (AC_CANONICAL_HOST): Remove; the largefile stuff no
	longer needs it.
	* config.guess, config.sub: Remove these files, for similar reasons.

2000-05-03  Paul Eggert  <eggert@twinsun.com>

	* m4/largefile.m4 (AC_SYS_LARGEFILE): Define _XOPEN_SOURCE to be
	500, instead of _GNU_SOURCE to be 1, to work around glibc 2.1.3
	bug.  This avoids a clash when files like regex.c that define
	_GNU_SOURCE.

2000-05-02  Paul Eggert  <eggert@twinsun.com>

	* m4/largefile.m4 (AC_SYS_LARGEFILE):
	Define _GNU_SOURCE if this is needed to make
	ftello visible (e.g. glibc 2.1.3).  Use compile-time test, rather than
	inspecting host and OS, to decide whether to define _LARGEFILE_SOURCE.

	* lib/quotearg.c (mbrtowc, mbstat_t):
	Add definitions if !HAVE_MBSTATE_T_OBJECT.
	(<wctype.h>): Include if HAVE_WCTYPE_H.
	(iswprint): Define to 1 if we lack it

2000-04-18  Paul Eggert  <eggert@twinsun.com>

	* m4/mbstate_t.m4: New file.

2000-04-17  Bruno Haible  <haible@clisp.cons.org>

	* tests/ignfail.sh: Test for uid 0 along with user "root".

2000-04-05  Paul Eggert  <eggert@twinsun.com>

	* m4/largefile.m4 (AC_SYS_LARGEFILE_FLAGS):
	Don't use -n32 on IRIX if the installer said
	otherwise.

2000-02-28  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c (ALERT_CHAR): New macro.
	(quotearg_buffer_restyled): Use it.

2000-02-23  Rainer Orth  <ro@TechFak.Uni-Bielefeld.DE>

	* src/list.c (tartime): Fix off-by-one error when copying year if
	OLD_CTIME.

2000-02-18  Paul Eggert  <eggert@twinsun.com>

	* lib/getdate.y: Handle two-digit years with leading zeros correctly.
	(textint): New typedef.
	(parser_control): Changed from struct parser_control to typedef
	(for consistency).  Member year changed from int to textint.  All
	uses changed.
	(YYSTYPE): Removed; replaced by %union with int and textint
	members.
	(tID): Removed; not used.
	(tDAY, tDAY_UNIT, tDAYZONE, tHOUR_UNIT, tID, tLOCAL_ZONE,
	tMERIDIAN, tMINUTE_UNIT, tMONTH, tMONTH_UNIT tSEC_UNIT, tSNUMBER,
	tUNUMBER, tYEAR_UNIT, tZONE, o_merid): Now of type <intval>.
	(tSNUMBER, tUNUMBER): Now of type <textintval>.
	(date, number, to_year): Use width of number in digits, not its
	value, to determine whether it's a 2-digit year, or a 2-digit
	time.
	(yylex): Store number of digits of numeric tokens.  Return '?' for
	unknown identifiers, rather than (unused) tID.

2000-01-16  Paul Eggert  <eggert@twinsun.com>

	* lib/quotearg.c (quotearg_buffer_restyled):
	Do not quote alert, backslash, formfeed,
	and vertical tab unnecessarily in shell quoting style.

2000-01-15  Paul Eggert  <eggert@twinsun.com>

	* m4/c-bs-a.m4:
	Change quoting to be compatible with future autoconf versions.

2000-01-11  Paul Eggert  <eggert@twinsun.com>

	* lib/exclude.c (FILESYSTEM_PREFIX_LEN, ISSLASH): Remove unused macros.

2000-01-07  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AC_INIT_AUTOMAKE): Version 1.13.17.

	Fix bug with fnmatch.h dependency, as follows:
	* src/Makefile.am (OMIT_DEPENDENCIES): New macro.
	* lib/Makefile.am (OMIT_DEPENDENCIES): New macro.

	* src/common.h (apply_nonancestor_delayed_set_stat):
	Renamed from apply_delayed_set_stat.
	(apply_delayed_set_stat, decode_mode, chmod_error_details,
	chown_error_details, close_warn, closedir_warn, mkdir_error,
	read_error_details, read_fatal_details, read_warn_details,
	seek_error_details, seek_warn_details, utime_error,
	write_error_details, write_fatal_details): New decls.

	Make diagnostic messages more regular.
	* src/create.c (dump_file): Quote file names with colons if possible.
	* src/compare.c	(diff_archive): Likewise.
	* src/extract.c (repair_delayed_set_stat, extract_archive): Likewise.
	* src/incremen.c (get_directory_contents, gnu_restore): Likewise.
	* src/mangle.c (extract_mangle): Likewise.
	* src/misc.c (call_arg_error, call_arg_fatal, call_arg_warn):
	Likewise.
	* src/buffer.c (archive_write_error, flush_archive, close_archive,
	new_volume, xclose):
	Use error message functions to report errors consistently.
	* src/compare.c (diff_sparse_files, diff_archive): Likewise.
	* src/create.c (finish_sparse_file, dump_file): Likewise.
	* src/extract.c (set_mode, set_stat, extract_sparse_file,
	extract_archive): Likewise.
	* src/list.c (list_archive): Likewise.
	* src/update.c (append_file): Likewise.
	* src/compare.c (diff_init, diff_sparse_files):
	Use xalloc_die to report memory exhaustion.
	* src/incremen.c (gnu_restore): Likewise.
	* src/list.c (read_header): Likewise.
	* src/mangle.c (extract_mangle): Likewise.
	* src/misc.c (maybe_backup_file): Likewise.
	* src/tar.c (decode_options): Likewise.
	* src/compare.c (read_and_process, fill_in_sparse_array,
	diff_sparse_files):
	Use consistent terminology for unexpected-EOF message.
	* src/extract.c (extract_sparse_file, extract_archive): Likewise.
	* src/list.c (list_archive, read_header, skip_file,
	skip_extended_headers): Likewise.
	* src/buffer.c (archive_write_error): Add noreturn attribute to decl.
	(xdup2): Regularize messages with rest of tar.

	* src/buffer.c (flush_read): Don't read past EOF.

	* src/extract.c (extr_init):
	If we run out of memory, invoke apply_delayed_set_stat.
	(prepare_to_extract): Don't complain if we can't remove ".".
	(apply_delayed_set_stat): New function.
	(apply_nonancestor_delayed_set_stat):
	Renamed from apply_delayed_set_stat.  All uses changed.
	Don't remove head if it doesn't apply.

	* src/create.c (find_new_file_size):
	Return size instead of storing through pointer.
	All callers changed.
	(deal_with_sparse): Don't keep reading after read errors.
	(finish_sparse_file): Just abort if there is an internal error.
	(dump_file): Fix typo: stat_warn and stat_error were interchanged.
	Don't restore access times on directories during incremental dumps
	until after dealing with the directory.
	If ignoring failed reads, count closedir, read, and unknown
	file errors as warnings, not errors.
	Fix buffer overrun problem when dumping sparse files.

	* src/list.c (read_and):
	Invoke apply_nonancestor_delayed_set_stat on file names
	after handling them.
	(decode_mode): Remove; moved to misc.c.

	* src/misc.c (safer_rmdir): New function.
	(remove_any_file): Use it to avoid problems with rmdir(".").
	(maybe_backup_file): Regularize diagnostics.
	(undo_backup_file): Likewise.
	(decode_mode): Moved here from list.c.
	(chmod_error_details, chown_error_details, close_fatal,
	close_warn, closedir_warn, mkdir_error, read_error_details,
	read_warn_details, read_fatal_details, seek_error_details,
	seek_warn_details, utime_error, write_error_details,
	write_fatal_details): New functions.

	* src/delete.c (save_record): Remove static variable (now local).
	(move_archive): Don't position before start of archive.
	(write_record): Abort if count is zero at inopportune time.
	Plug memory leak.

	* src/tar.c (decode_options): --delete and -f - are now
	incompatible, since we didn't have time to fix their bugs.

	* tests/Makefile.am (TESTS): Remove delete02.sh.
	* tests/ignfail.sh: Adjust to new quoting scheme again.

2000-01-06  Paul Eggert  <eggert@twinsun.com>

	* lib/getdate.y: Sync tm_diff with the GNU C Library.
	(TM_YEAR_BASE): Renamed from TM_YEAR_ORIGIN.  All uses changed.
	(tm_diff): Renamed from difftm.  All uses changed.
	Replace body with that taken from GNU C Library 2.1.3pre1.
	(get_date): Prefer tm_gmtoff to tm_diff if available.

1999-12-29  "Melissa O'Neill"  <oneill@cs.sfu.ca>

	* tests/incremen.sh: Invoke stat on newly created file so that its
	ctime is updated on Nextstep.

1999-12-21  Machael Stone  <mstone@cs.loyola.edu>

	* lib/getdate.y (get_date):
	Fix typo when checking for time_t overflow in time zone calculations.

1999-12-13  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AC_INIT_AUTOMAKE): Version 1.13.16.

	* README-alpha: New file.
	* README: New sections for gzip and bzip2, Solaris.
	Remove mention of BACKLOG.

	* configure.in (AC_C_BACKSLASH_A): Add.
	(AC_CHECK_HEADERS): Add wchar.h.
	(AC_CHECK_FUNCS): Add mbrtowc.
	(AC_FUNC_CLOSEDIR_VOID): Add.

	* tests/Makefile.am (TESTS): Add delete02.sh.
	(POSTPONED_TESTS): Remove.
	(EXTRA_DIST): Remove $(POSTPONED_TESTS).

	* tests/preset.in:
	Set LC_ALL rather than LANGUAGE, LANG, and LC_MESSAGES.

	* tests/ignfail.sh (err): Adjust to new quoting scheme.

	* tests/delete02.sh: Fix typo: need to list archive2, not archive.

	* tests/extrac03.sh: Use -P option, so that .. doesn't get diagnosed.

	* src/tar.c ("quotearg.h"): New include.
	(usage): Now has __attribute__ ((noreturn)).
	(confirm): Report errno if we can't open tty.
	(confirm, decode_options):
	Quote arbitrary strings in diagnostics.
	(OVERWRITE_OPTION): New constant.
	(long_options, usage, decode_options): New --overwrite option.
	(decode_options): --keep-old-files, --overwrite, and --unlink-first
	are now mutually exclusive.
	Don't assume that gettext preserves errno.
	(main): Set default quoting style to escape_quoting_style.

	* src/update.c (<quotearg.h>): New include.
	(append_file):
	Don't assume that gettext preserves errno.
	Quote arbitrary strings in diagnostics.
	Check for close error.

	* src/names.c (<quotearg.h>): New include.
	(name_init, name_next, name_close, names_notfound,
	collect_and_sort_names): Don't assume that gettext preserves
	errno.  Quote arbitrary strings in diagnostics.
	(excluded_name): Fix typo that caused empty patterns to be
	mishandled.

	* src/misc.c (<quotearg.h>): New include.
	(quote_copy_string): Quote only newline and backslash; the output is no
	longer meant for humans, and is locale-independent.
	(contains_dot_dot): New function.
	(remove_any_file): Don't use lstat; just rmdir the file and then use
	unlink if the rmdir fails because the file isn't a directory.
	Check for readdir and closedir errors.
	(maybe_backup_file): Report "stat" for stat errors.
	(maybe_backup_file, chdir_do):
	Quote arbitrary strings in diagnostics.
	(maybe_backup_file, undo_last_backup):
	Don't assume that gettext preserves errno.
	(call_arg_error, call_arg_fatal, call_arg_warn,
	chdir_fatal, close_error, closedir_error, exec_fatal, mkfifo_error,
	mknod_error, open_error, open_fatal, open_warn, opendir_error,
	opendir_warn, read_error, read_fatal, readdir_error, readdir_warn,
	readlink_error, readlink_warn, seek_error, seek_warn, stat_error,
	stat_warn, truncate_error, truncate_warn, unlink_error, waitpid_error,
	write_error, write_fatal, xfork, xpipe, quote_n, quote): New functions.

	* src/system.h (__attribute__): New macro.
	(O_NDELAY, O_NONBLOCK, O_APPEND): Remove.
	(S_ISDOOR): New macro.
	(closedir): New macro, if CLOSEDIR_VOID.

	* src/rmt.c, src/rtapelib.c (decode_oflag):
	O_APPEND might not be defined.

	* src/list.c: (read_and, list_archive):
	Quote arbitrary strings in diagnostics.
	(from_header): Use locale_quoting_style to quote diagnostics.
	(print_header, print_for_mkdir): Quote with quotearg, not quote_copy_string.

	* src/rmt.h (REM_BIAS): Increase from 128 to (1 << 30).

	* src/Makefile.am: Use ## for copyright comments.

	* src/extract.c (<quotearg.h>): New include.
	(enum permstatus): New enum.
	(struct delayed_set_stat): file_name is now at end of buffer, to avoid
	two mallocs.  New members file_name_len, invert_permissions, permstatus.
	(extr_init): Remove hack that silently adjusted newdir_umask.
	(set_mode, set_stat): New args invert_permissions, permstatus, typeflag.
	Use these args to decide whether and how to set modes.
	(set_mode, set_stat, prepare_to_extract, extract_sparse_file, extract_archive):
	Don't assume that gettext preserves errno.
	(set_stat): Remove arg symlink_flag; subsumed by typeflag.
	(delay_set_stat, repair_delayed_set_stat): New functions.
	(make_directories): Avoid mkdir where last part of path is "..".
	Create a struct delayed_set_stat for each directory made.
	(prepare_to_extract): Renamed from unlink_destination, and
	return 0 immediately if to_stdout_option; all callers changed.
	(maybe_recoverable): New parameter interdir_made.
	Add support for --overwrite.
	(extract_sparse_file, extract_archive):
	Quote arbitrary strings in diagnostics.
	(extract_archive): By default, warn about ".." in member names, and skip them.
	Don't open files with O_NONBLOCK or O_APPEND.
	Open with O_TRUNC only if --overwrite; otherwise, use O_EXCL to avoid
	overwriting them.  Pass only rwxrwxrwx permissions to `open' and `mkdir',
	minus the current umask.  Keep track of intermediate directories made,
	to avoid looping when making x/../x when x doesn't exist; the
	earlier code solved this in a different way that didn't fit well
	into the new scheme.  Don't extract permissions onto existing
	directories unless --overwrite is given.  Do not add -wx------
	permissions to new directories permanently; just do it temporarily.
	Remove no-longer-needed hack with MSDOS and directory time stamps.
	(apply_delayed_set_stat): New argument specifies which directories to
	fix statuses of.  Do not wait until the end of extraction to fix
	statuses; instead, fix a directory's status once we exit that directory.
	This requires less memory and does the right thing in some cases
	where the old method didn't.
	(fatal_exit): New function.

	* src/incremen.c (<quotearg.h>): New include.
	(get_directory_contents, gnu_restore):
	Check for readdir and closedir errors.
	(get_directory_contents, read_directory_file, gnu_restore):
	Quote arbitrary strings in diagnostics.
	(get_directory_contents, read_directory_file, write_directory_file):
	Don't assume that gettext preserves errno.

	* src/create.c (<quotearg.h>): New include.
	(start_header): Use `member names' to refer to archive member names, not
	`archive names'.  Warn about `..' in member names.
	(finish_sparse_file, dump_file):
	Quote arbitrary strings in diagnostics.
	(finish_sparse_file, dump_file):
	Don't assume that gettext preserves errno.
	(dump_file): Don't use `access' to determine whether a directory is readable;
	this isn't reliable if tar is setuid.  Use `opendir' instead.
	Check for readdir and closedir failures.
	Don't dump sockets as if they were fifos; just warn and skip.

	* src/delete.c (move_archive):
	Don't report fatal error merely because sizes don't fit
	into struct mtop values; fall back on lseek instead.
	Say `Cannot' uniformly, instead of `Could not' sometimes and `Cannot' others.
	Say `reposition' instead of `re-position'.
	(delete_archive_members):
	Set archive to STDOUT_FILENO before outputting trailing buffer.

	* src/compare.c (<quotearg.h>): New include.
	(diff_init): Use `Cannot' uniformly, instead of `Could not' sometimes
	and `Cannot' others.
	(report_difference, diff_archive):
	Quote arbitrary strings in diagnostics.
	(process_rawdata, diff_sparse_files, get_stat_data, diff_archive, seek_warn):
	Don't assume that gettext preserves errno.
	(diff_archive): Don't open regular files with O_NONBLOCK.
	Preserve access times of files if --atime.

	* src/common.h (FATAL_ERROR): Use new fatal_exit function to exit.
	(FATAL_ERROR, USAGE): Don't return 0.
	(enum old files): New enum.
	(old_files_option): New variable, replacing keep_old_files_option and
	unlink_first_option.
	(apply_delayed_set_stat): Now takes char const * param.
	(fatal_exit, contains_dot_dot, chdir_fatal, close_error,
	closedir_error, exec_fatal, mkfifo_error, mknod_error, open_error,
	open_fatal, open_warn, opendir_error, opendir_warn, read_error,
	read_fatal, readdir_error, readdir_warn, readlink_error,
	readlink_warn, seek_error, seek_warn, stat_error, stat_warn,
	truncate_error, truncate_warn, unlink_error, waitpid_error,
	write_error, write_fatal, xfork, xpipe, quote, quote_n): New decls.

	* src/buffer.c:
	(xclose, xdup2, child_open_for_compress, child_open_for_uncompress,
	archive_write_error, archive_read_error, flush_archive, close_archive,
	init_volume_number, new_volume):
	Don't assume that gettext preserves errno.

	(xdup2): Don't report errno if dup returns an unexpected nonnegative value.
	(open_archive): Reject multivolume verify attempts a bit earlier.
	Rename local variable `access', in case it's defined by system header.

	(open_archive, backspace_output): Use `Cannot' uniformly, instead of
	`Could not' sometimes and `Cannot' others.

	(open_archive, flush_read, flush_archive, close_archive, new_volume):
	Quote arbitrary strings in diagnostics.

	(read_error): Set archive to STDOUT_FILENO temporarily when writing
	archive buffer.

	(init_volume_number): Check for input and output errors in volno_file.

	(new_volume): Use new fatal_exit function to exit, and new xfork
	function to fork.

	* m4/Makefile.am (EXTRA_DIST): Add c-bs-a.m4.

	* Makefile.am (ACINCLUDE_INPUTS): Add $(M4DIR)/c-bs-a.m4.

	* doc/tar.texi: Add --overwrite.
	--absolute-names rejects ".." in names.

	* lib/quotearg.c: Add support for multibyte characters.
	(ISGRAPH): Remove.
	(ISPRINT): New macro.
	(<wchar.h>): Include if HAVE_MBRTOWC && HAVE_WCHAR_H.
	(isprint, mbrtowc, mbsinit, mbstate_t): New macros,
	defined if ! (HAVE_MBRTOWC && HAVE_WCHAR_H).
	(quotearg_buffer_restyled): New function, with most of the old
	quotearg_buffer's contents.
	Major rewrite to support multibyte characters.
	(quotearg_buffer): Now just calls quotearg_buffer_restyled.

	* m4/c-bs-a.m4: New file.

	* lib/Makefile.am: Use ## for copyright notice.

	* scripts/Makefile.am: Use ## on copyright notice.

	* doc/Makefile.am:
	($(srcdir)/tar.info, tar.dvi): We now use texinfo 4.0.

1999-12-05  Paul Eggert  <eggert@twinsun.com>

	* doc/ChangeLog, lib/ChangeLog, scripts/ChangeLog,
	src/ChangeLog, tests/ChangeLog: Remove these files.
	* ChangeLog.1: New file, incorporating the above files, plus old
	ChangeLog entries.
	* Makefile.am (EXTRA_DIST): Add ChangeLog.1.

1999-12-05  Dale Worley  <worley@ariadne.com>

	* src/compare.c (<utime.h>, struct utimbuf): Add.
	(diff_archive): Restore access times if --atime.
	* doc/tar.texi: Explain that --atime also preserves modification time.

1999-12-04  Gerhard Poul  <gpoul@gnu.org>

	* ABOUT-NLS: Update to latest version from ftp.gnu.org.
	* BACKLOG, TODO: Remove.
	* Makefile.am (all-local, BABYL, dist-zoo, id, ID): Remove.
	* README: Bring up to date.

1999-12-03  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.13.15.

	* src/compare.c (diff_archive):
	Do not set errno to EPIPE; we no longer use perror.

	* src/create.c (dump_file):
	If a parent directory said that a file should be there but it is
	absent, diagnose it as being removed in the meantime.
	Do not pass meaningless errno to ERROR when reporting that the
	file changed as we read it.
	Report that a file changed if its ctime changes; this is more
	sensitive than mtime+size, and more accurate.

	* src/incremen.c (enum children): New type.
	(struct directory): Change old char allnew member to new enum children
	children member.
	All uses changed.
	(get_directory_contents): When doing an incremental dump that does
	not cross filesystem boundaries, dump the mount points, even though
	they are in a different filesystem.  This is for convenience when
	restoring, and for consistency with non-incremental dumps.
	This requires a 3-way flag for keeping track of which children we want,
	so we use enum children rather than boolean.

	* src/open3.c (modes): Remove.
	(open3): Remove unportable assumptions about flag encodings.
	Use `stat' instead of `access' for testing file existence,
	to avoid problems with setuid programs.

	* src/names.c (name_next): If file names are given both in the
	command line (e.g. via -C) and in a file (via -T), do not
	ignore the command-line names.

	* m4/uintmax_t.m4: Backport to autoconf 2.13.

	* doc/tar.texi: Clarify getdate authorship.

1999-11-23  Paul Eggert  <eggert@twinsun.com>

	* lib/Makefile.am (DISTCLEANFILES): New macro.

	* configure.in (tar_fnmatch_hin):
	Remove; it runs afoul of a bug in autoconf 2.13.
	Instead, always link fnmatch.h to some file, even if it's a throwaway.

1999-11-19  Paul Eggert  <eggert@twinsun.com>

	* m4/largefile.m4: Update serial.

1999-11-18  Paul Eggert  <eggert@twinsun.com>

	* m4/largefile.m4 (AC_SYS_LARGEFILE_FLAGS): Work around a bug in
	the QNX shell, which doesn't propagate exit status of failed
	commands inside shell assignments.

1999-11-07  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.13.14.

	* configure.in (AC_PREREQ): Bump to 2.13.
	(ALL_LINGUAS): Add pt_BR, ja.
	(AC_FUNC_FNMATCH): Remove lib/funmatch.h before invoking, not after.
	(tar_cv_path_RSH): Prefer a non-symlink rsh to a symlink one,
	for AIX crossbuilds.

	* doc/tar.texi: New node create options for --ignore-failed-read.
	Remove unused version control symbols.
	Modernize texinfo usage.

	* src/tar.c (usage): Add examples.

	* m4/fnmatch.m4 (AC_FUNC_FNMATCH):
	Include fnmatch.h when testing fnmatch.

	* src/common.h (collect_and_sort_names): New decl.

	* src/list.c (from_header):
	Handle 32-bit two's complement negative time stamps
	even if the leading octal digit is 2 or 3.

	* src/extract.c (set_stat): Remove duplicate code.

	* src/create.c (to_chars): Remove trailing newline from warning.
	(dump_file): Ignore doors.
	(finish_header): Report block numbers with origin 0, not origin 1.

	* src/rmt.c: Include getopt.h.
	(long_opts): New constant.
	(usage): New function.
	(main): Implement --help and --version.
	Output usage message if arguments are bad.

1999-10-10  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.13.13.

	* README: Remove --with-dmalloc.
	Add --disable-largefile.
	Remove old NeXT dirent problems, or AIX valloc problems.
	Remove old union wait advice, and old %lld advice.
	Remove advice about FreeBSD 2.1.7, ISC 4.1mu, Ultrix `make'.

	* doc/tar.texi: Clarify documentation for portable file names.

	* configure.in (AM_WITH_DMALLOC): Remove.
	(ALL_LINGUAS): Add ja.

	* src/tar.c (decode_options):
	Invalid dates are now treated as (time_t) -1.
	Redo version message to conform to GNU standards.

	* src/create.c (dump_file):
	Fix typo: last two args to dump_file were interchanged.
	* src/update.c (update_archive): Likewise.

	* src/common.h (tartime): New decl.

	* src/list.c (tartime): Now extern.
	(read_and): Invalid headers cause errors, not warnings.

1999-10-03  Paul Eggert  <eggert@twinsun.com>

	* lib/getdate.y (__attribute__):
	Don't use if GCC claims to be before 2.8; this is
	needed for OPENStep 4.2 cc.  Also, don't use if strict ANSI.

1999-09-25  Paul Eggert  <eggert@twinsun.com>

	* lib/fnmatch.c, lib/fnmatch.hin: Merge changes from latest glibc.
	* lib/getopt.c, lib/getopt.h, lib/getopt1.c: Likewise.

	* tests/incremen.sh: Add yet another sleep.

1999-09-24  Paul Eggert  <eggert@twinsun.com>

	* NEWS: A read error now causes a nonzero exit status.

	* src/create.c (to_chars): Fix base-256 output.

	* src/buffer.c (write_error):
	Read error is an error, not just a warning.

1999-09-24  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.13.12.

	* src/tar.c (<time.h>): Include.
	(time): Declare if not defined.
	(confirm): Don't read past EOF.
	(long_options, usage): Add --no-same-owner, --no-same-permissions.
	(main): Use clock_gettime if available.

	* tests/Makefile.am (TESTS): Add incremen.sh
	(INCLUDES): Add -I../lib, for fnmatch.h.

	* src/update.c (update_archive):
	Remove call to name_expand; had no effect.
	Use chdir_do to change into directory.
	Use deref_stat instead of stat.
	Use add_avoided_name to mark names to be avoided; the old method of
	setting a bit with the name caused all descendants of that name to
	be avoided, in some circumstances.

	* tests/incremen.sh: Remove unnecessary sleeps.

	* src/names.c (name_next): Go back to using plain chdir.
	(name_gather): Use chdir_arg to keep track of arguments to chdir.
	(addname): Likewise.
	(name_match): Use chdir_do to act on chdir args.
	(merge_sort): Moved here from incremen.c.
	(compare_names, add_hierarchy_to_namelist, collect_and_sort_names):
	Likewise.
	(name_expand): Remove.
	(name_from_list): Skip fake names.
	Use chdir_do to act on chdir args.
	(struct avoided_name): New struct.
	(avoided_names): New var.
	(add_avoided_name, is_avoided_name): New functions.

	* src/system.h (stat, lstat): Define in terms of statx on
	STX_HIDDEN && !_LARGE_FILES /* AIX */ hosts.
	(UCHAR_MAX): New macro.
	(TYPE_MAXIMUM): Cast to arg type, for types narrow than int.

	* m4/largefile.m4: Work around GCC 2.95.1 bug with HP-UX 10.20.

	* src/incremen.c (<time.h>): Remove include; no longer used.
	(time): Remove decl.
	(time_now): Remove.
	(get_directory_contents): Use deref_stat.
	Consider a subdirectory to be all new only if
	listed_incremental_option or if it its timestamp is newer than the
	cutoff.
	(add_hierarchy_to_namelist, merge_sort): Move to names.c.
	(read_directory_file): Now extern.  Do not set time_now.
	(write_directory_file): Renamed from write_dir_file.
	Use start_time instead of time_now.
	(compare_names, collect_and_sort_names): Move to names.c.

	* src/mangle.c (<time.h>): Remove; not used.
	(time): Do not declare.

	* src/misc.c (chdir_from_initial_wd): Remove.
	(deref_stat): New function.
	(struct wd): New struct.
	(wd, wds, wd_alloc): New variables.
	(chdir_arg, chdir_do): New function.

	* src/compare.c (get_stat_data): Use deref_stat.

	* src/common.h (name_expand): Remove.

	* src/list.c (time): Declare if not defined.
	(base_64_digits): Moved here from create.c.
	(base64_map): Use UCHAR_MAX for size, not less-clear (unsigned char)
	-1.
	(read_and): Don't get time from header unless we need it now;
	as getting time can cause duplicate diagnostics if bogus.
	Remove "Hmm, " from diagnostic.
	Use "Skipping to next header" uniformly.
	(from_header): Renamed from from_chars.  All uses changed.
	Allow different forms for unportable 2's complement numbers.
	Don't check for extended forms when parsing checksums.
	Parse base-256 output.
	(gid_from_header): Renamed from gid_from_chars.  All uses changed.
	(major_from_header): Renamed from major_from_chars.  All uses changed.
	(minor_from_header): Renamed from minor_from_chars.  All uses changed.
	(mode_from_header): Renamed from mode_from_chars.  All uses changed.
	(off_from_header): Renamed from off_from_chars.  All uses changed.
	(size_from_header): Renamed from size_from_chars.  All uses changed.
	(time_from_header): Renamed from time_from_chars.  All uses changed.
	Warn about future timestamps.
	(uid_from_header): Renamed from uid_from_chars.  All uses changed.
	(uintmax_from_header): Renamed from uintmax_from_chars.
	All uses changed.
	(tartime): New function, incorporating isotime.
	(isotime): Delete.
	(print_header): Use tartime.

	* src/create.c (to_chars): Fix typo in decl.
	Don't assign through char const *.
	Rename name_expand back to collect_and_sort_names.

	* src/extract.c (<time.h>): No need to include.
	(time): No need to declare.
	(now): Remove variable.
	(extr_init): Don't initialize `now'.
	Increment same_permissions_option and same_owner_option if we_are_root
	is nonzero; this supports the new --no-same-owner option.
	(set_stat): Use start_time instead of `now'.

	* src/create.c (struct link): Remove unused linkcount member.
	(base_64_digits): Move to list.c.
	(base_8_digits): Remove.
	(to_octal): New function, with some of old contents of to_base.
	(to_base): Remove.
	(to_base256): New function.
	(to_chars): Use base 256, not base 64, for huge values.
	(mode_to_chars): Don't use two's complement in GNU format or POSIX
	format.
	(dump_file): Interchange last two arguments. If TOP_LEVEL is negative,
	it means we have an incremental dump where we don't know whether this
	is a top-level call.
	Use deref_stat instead of statx / stat / lstat.
	Cast result of alloca.
	Check for dates if 0 < top_level, not if listed_incremental_option.
	Move multiple-link check after directory check.
	Do not dump avoided names.
	Dump hard links to symbolic names as links, not as separate
	symbolic links.
	start_header cannot return a null pointer, so don't test for it.
	Likewise for find_next_block.

	* src/buffer.c, src/common.h (<human.h>): Include.
	(read_error): Read error is an error, not just a warning.
	(print_total_written): Also print human-readable byte count, and
	bytes/s.
	(open_archive, flush_write): Use start_time, not current time.
	(flush_read): Report about garbage bytes ignored at end of archive,
	but act on non-garbage bytes (instead of ignoring them).
	(new_volume): Use WARN for warnings.

	* doc/Makefile.am:
	($(srcdir)/tar.info): Add -I$(srcdir) so that subdir builds work.

	* Makefile.am (ACINCLUDE_INPUTS): Add $(M4DIR)/fnmatch.m4.

	* m4/Makefile.am (EXTRA_DIST): Add fnmatch.m4.

	* lib/Makefile.am (noinst_HEADERS):
	Rename fnmatch.h to fnmatch.hin; add human.h.
	(libtar_a_SOURCES): Add human.c, xstrtoul.c.
	(INCLUDES): Remove -I.. -I$(srcdir) -- automake adds this for us.

	* src/Makefile.am (rmt_LDADD, tar_LDADD): New macros.

	* lib/fnmatch.c (strchrnul):
	Define to __strchrnul if _LIBC, to our own replacement otherwise.
	Do not define if !_LIBC and if it already exists.
	(internal_fnmatch): Use it.

	* configure.in (tar_LDADD): New variable, used only when linking tar.
	(rmt_LDADD): Similarly, for rmt.
	(AC_FUNC_FNMATCH): Link fnnmatch.hin to fnmatch.h if we're using our
	fnmatch.c; otherwise, use the system fnmatch.h.

	* doc/tar.texi: Add --no-same-owner, --no-same-permissions.
	Modernize sample backup script.

	* THANKS: Martin Goik's email address has changed.

	* m4/fnmatch.m4: New file.

1999-09-03  Paul Eggert  <eggert@twinsun.com>

	* lib/lchown.h (ENOSYS): Don't use ENOMSG; it's not in NeXTStep3.3.
	Use EINVAL instead.

1999-08-29  Paul Eggert  <eggert@twinsun.com>

	* lib/getdate.y (get_date):
	Rename outermost local `probe' to `quarter'.
	Rename latter local `tm' to probe_tm.
	From: Jim Meyering <meyering@ascend.com>
	Message-ID: <uryn1vafyyc.fsf@ixi.eng.ascend.com>

1999-08-28  Paul Eggert  <eggert@twinsun.com>

	* lib/getdate.y (PC): New macro; use it when possible.
	(number): Handle `Nov 11 1996' example correctly.
	See Risks Digest 20.55 (1999-08-27)
	http://catless.ncl.ac.uk/Risks/20.55.html#subj18

1999-08-23  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.13.11.

	Remove minor cases of lint from many source files: this includes
	unnecessary casts, uses of NULL, etc.

	* configure.in (AC_PROG_YACC): Remove.
	(YACC): Always use bison.
	(AC_STRUCT_TIMEZONE): Add.
	(AC_REPLACE_FUNCS): Add strcasecmp, strncasecmp.

	* doc/tar.texi: --bzip2 is now -I.  Remove obsolete time zone info.
	Fix spelling.

	* lib/Makefile.am (EXTRA_DIST): Add strcasecmp.c, strncasecmp.c.
	($(srcdir)/getdate.c): Rename y.tab.c to getdate.c only if successful.

	* lib/strcasecmp.c, lib/strncasecmp.c: New files.

	* src/common.h (merge_sort): Remove decl; no longer exported.

	* src/system.h (voidstar): Remove.
	(memcpy, memcmp): Cast args.
	("xalloc.h"): Add include.
	(xmalloc, xrealloc): Remove decl.

	* src/mangle.c (time): Do not declare if defined.
	(first_mangle, mangled_num): Remove.

	* src/list.c (from_chars): Report out-of-range values more precisely.
	(off_from_chars): Do not allow negative offsets.
	(uid_from_chars): Allow negative uids.

	* src/create.c (linklist): Now static.
	(to_chars): Fix wording of message to match from_chars.

	* src/misc.c (merge_sort): Move to incremen.c.
	* src/incremen.c (merge_sort): Move here from misc.c; now static.
	It's too painful to make it both generic and portable.
	(read_directory_file): "timestamp" -> "time stamp" in messages.

	* src/tar.c (long_options, usage, main): -y is now -I (for --bzip).
	(usage): Fix misspelling.
	(OPTION_STRING): -y is now -I.
	(decode_options): Use -1, not EOF, for getopt_long result.
	Fix typo when invoking xstrtoumax: look for LONGINT_OK, not LONG_MAX.
	Handle operands after any "--" argument.
	(main): Report any output errors.

	* src/rmt.c (main): status is ssize_t, not long.

	* src/names.c (name_gather): Handle trailing -C option correctly.
	(addname): use memcpy, not strncpy, to copy a string of known length.
	(name_match): Handle trailing -C option correctly.
	Propagate -C option to following files.
	(name_match, name_scan): Remove redundant matching code.

	* src/buffer.c (open_archive): Use American spelling in diagnostic.

	* lib/getdate.y: Major rewrite.  Add copyright notice.
	(<stdio.h>): Include only if testing.
	(ISUPPER): Remove.
	(ISLOWER): New macro.
	(<string.h>): Include if HAVE_STRING_H, not USG.
	(bcopy): Remove.
	(yymaxdepth, ..., yycheck): Don't bother to redefine, since we assume
	bison.
	(EPOCH_YEAR): Renamed from EPOCH.
	(table): Renamed from TABLE.
	(meridian): Now an anonymous enum.
	(struct parser_control): New type.
	(YYLEX_PARAM, YYPARSE_PARAM, YYSTYPE): New macros.
	(yyInput, ..., yyRelYear): Migrated into struct parser_control.
	(%pure_parser): Added, so that the parser is pure.
	(%union): Removed; the type is now just plain int.
	All %type directives removed.
	(tLOCAL_ZONE): New %token.
	(month_day_table): Renamed from MonthDayTable.
	(gmtime, localtime, mktime, time): Declare only if not defined.
	(meridian_table): New table.
	(dst_table): New table.
	(units_table): renamed from UnitsTable.
	(relative_time_table): Renamed from OtherTable.
	(time_zone_table): Renamed from TimezoneTable.  Modernized.
	(military_table): Renamed from MilitaryTable.
	(to_hour): Renamed from ToHour.
	(to_year): Renamed from ToYear.
	(lookup_zone): New function.
	(LookupWord): Renamed from lookup_word.  Use lookup_zone for time
	zones.
	(yylex): Now reentrant.  All callers changed.
	(get_date): Add support for local time zone abbreviations.
	Make it reentrant.

1999-08-20  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.13.10.

	* src/create.c (to_chars): Generate GNU base-64 representation
	if we are generating an old or new GNU format tar file for a
	number that can't be represented with the POSIX format.

	* configure.in (AC_CHECK_FUNCS): Add fchdir.
	(AM_FUNC_GETLINE): Add.
	(LIBOBJS): Add getline.o to workaround comment.
	* Makefile.am (ACINCLUDE_INPUTS): Add $(M4DIR)/getline.m4.
	* m4/Makefile.am (EXTRA_DIST): Add getline.m4.
	* lib/Makefile.am (noinst_HEADERS): Add getline.h, save-cwd.h.
	(libtar_a_SOURCES): Add save-cwd.c, xgetcwd.c.
	* lib/getline.c, lib/getline.h, lib/save-cwd.c,
	lib/save-cwd.h, m4/getline.m4: New files.

	* src/misc.c (<save-cwd.h>): Include.
	(chdir_from_initial_wd): New function.

	* src/names.c (name_next): Use chdir_from_initial_wd, not chdir.
	(name_gather): Handle `-C x -C y' correctly.
	Do not rely on addname to handle -C.
	(addname): New CHANGE_DIR parameter.  All callers changed.
	Remove ugly calls to getcwd; no longer needed.
	(name_match, name_from_list): Use chdir_from_initial_wd, not chdir.

	* src/incremen.c (listed_incremental_stream): New var.
	(read_directory_file): Remove arbitrary limits on file name length.
	Do not attempt to get the working directory; we can bypass this
	on fchdir hosts.  Open the listed_incremental_option file for both
	read and write instead of opening it twice.  Check for I/O errors
	when doing I/O to this file.  Check for invalid data in the file,
	and report line numbers of invalid data.
	(write_dir_file): Likewise.
	(collect_and_sort_names): Use chdir_from_initial_wd, not chdir.
	Do not invoke write_dir_file; that's our caller's responsibility.

	* src/list.c (max): New macro.
	(isotime): Now takes time_t, not time_t *.  Report the decimal values
	of times that can't be broken down.
	(print_header): Don't assume that major and minor device numbers can
	fit into uintmax_t.

	* src/common.h (struct name): change_dir is now char const *.
	(write_directory_file): Remove unused decl.
	(STRINGIFY_BIGINT): Assume b always points to UINTMAX_STRSIZE_BOUND
	chars; the old `sizeof (b)' broke when b was a pointer not an array.
	(chdir_from_initial_wd): New decl.
	(addname): New 2nd arg.

	* THANKS: Torsten Lull -> Catrin Urbanneck

1999-08-18  Paul Eggert  <eggert@twinsun.com>

	* configure.in (HAVE_GETHOSTENT, HAVE_SETSOCKOPT):
	Don't depend on ac_cv_func variables.
	From Albert Chin-A-Young <china@thewrittenword.com>.

1999-08-18  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.13.9

	* m4/signedchar.m4: New file.
	* configure.in (pe_AC_TYPE_SIGNED_CHAR): Add.
	* src/system.h (signed_char): New macro.
	* Makefile.am (ACINCLUDE_INPUTS): Add $(M4DIR)/signedchar.m4.
	* m4/Makefile.am (EXTRA_DIST): Add signedchar.m4.

	* src/create.c (write_eot): Write at least two zero blocks.

	* src/extract.c (extract_archive): Fix sparse array bug:
	we did not find end of array correctly.

	* src/compare.c: (fill_in_sparse_array, diff_sparse_files):
	Don't assume find_next_block yields nonnull.
	* src/extract.c (extract_sparse_file, extract_archive): Likewise.
	* src/list.c (skip_extended_headers): Likewise.

	* src/list.c (read_and, list_archive): Simplify code.
	(read_header): Fix computation of signed checksums on machines where
	char is unsigned.
	Do not consider a block to be zero unless all its bytes are zero,
	even the checksum bytes.  Do not attempt to parse the checksum of
	a zero block.  Fix memory leak with long names and links.
	(from_chars): Accommodate a buggy tar that outputs leading NUL
	if the previous field overflows.

	* src/misc.c (quote_copy_string): Generate \177 for '\177', not
	\?, for portability to non-ASCII hosts.

1999-08-16  Paul Eggert  <eggert@twinsun.com>

	* configure.in (AM_INIT_AUTOMAKE), NEWS: Version 1.13.8.

	* src/extract.c (make_directories): Do not chown intermediate
	directories, even if we are root.

	* src/list.c (read_header): Fix bugs when interpreting
	POSIX-compliant headers that do not contain null bytes in the
	header or link names.

1999-08-14  Paul Eggert  <eggert@twinsun.com>

	* configure.in (AM_INIT_AUTOMAKE), NEWS: Version 1.13.7.

	* configure.in (AC_CHECK_HEADERS): Remove sys/wait.h.
	(AC_HEADER_SYS_WAIT): Add.
	(AC_REPLACE_FUNCS): Add waitpid.
	(tar_cv_header_union_wait, HAVE_UNION_WAIT): Remove.
	* lib/waitpid.c: New file.
	* lib/Makefile.am (EXTRA_DIST): Add waitpid.c.
	* src/system.h (WCOREDUMP): Remove; no longer used.
	(WIFSTOPPED): Likewise.
	(WEXITSTATUS, WIFSIGNALED): Default to Solaris 7 versions.
	* src/buffer.c (child_open_for_compress): Undo previous change.
	(close_archive): Use waitpid, POSIX-style, instead of old BSD style.
	(new_volume): Likewise.

	* src/buffer.c, src/extract.c, src/incremen.c (time):
	Don't declare if defined.
	* src/extract.c (extr_init): Remove unneeded cast around 0 arg to time.
	* src/incremen.c (read_directory_file):
	Invoke `time' the same way everyone else does.
	Check validity of --listed-incremental file contents a bit better.
	Do not worry about --after-date-option; tar.c now checks this.
	* src/list.c (isotime): Report ??? if localtime returns null.
	Don't assume years fit into four digits.
	Don't append trailing newline.
	(print_header): Report ??? if localtime returns null;
	Don't assume years fit into four digits.

	* src/compare.c (diff_archive): Do not fall back on absolute name
	when --absolute-names is not specified.

	* src/create.c (start_header):
	Include text of ignored filesystem prefix in warning.
	(create_archive): Check for excluded names when doing incremental
	pass through directory.
	(dump_file): Do not dump old files explicitly given on command line
	when using --listed-incremental.  Do not strip ./ prefix from names.

	* src/tar.c: -g now implies after_date_option = 1.
	-g and -N are now incompatible options.

	* doc/tar.texi: Explain --exclude better.  Don't strip leading `./'.

1999-08-11  Jeff Dairiki  <dairiki@dairiki.org>

	* src/list.c (read_header): Don't parse OLDGNU_FORMAT
	incremental headers as POSIX prefixes.

1999-08-11  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in: Version 1.13.6.

	* configure.in (ALL_LINGUAS): Add pt_BR.
	* po/pt_BR.po: New file.

	* doc/Makefile.am ($(srcdir)/tar.info, $(srcdir)/header.texi):
	Renamed from tar.info and header.texi; adjust actions so that
	they work in other directories.

	* doc/tar.texi: Add -y and --bzip2.
	Patterns containing / now exclude only file names whose prefix match.

	* lib/exclude.h (excluded_filename): New option parameter.
	(add_exclude_file): New ADD_FUNC parameter.
	(excluded_pathname): Remove decl.
	* lib/exclude.c (_GNU_SOURCE):
	Remove; no longer needed since we don't use FNM_ macros.
	(excluded_filename): Renamed from excluded_filename_opts.
	(excluded_filename, excluded_pathname): Remove.
	(add_exclude_file): New ADD_FUNC parameter.

	* po/POTFILES.in: Add lib/quotearg.c.

	* src/buffer.c (_GNU_SOURCE): Define.
	(<fnmatch.h>): Include unconditionally.
	(child_open_for_compress): Dup after closing, to avoid possible file
	descriptor exhaustion.
	(flush_write): Use FILESYSTEM_PREFIX_LEN instead of MSDOS ifdef.
	(flush_read): Likewise.

	* src/common.h (LG_8, LG_64): New macros.
	(excluded_with_slash, excluded_without_slash): New vars.
	(excluded): Remove.
	(base_64_digits): New decl.
	(gid_to_chars, major_to_chars, minor_to_chars, mode_to_chars,
	off_to_chars, size_to_chars, time_to_chars, uid_to_chars,
	uintmax_to_chars,
	GID_TO_CHARS, MAJOR_TO_CHARS, MINOR_TO_CHARS, MODE_TO_CHARS,
	OFF_TO_CHARS, SIZE_TO_CHARS, TIME_TO_CHARS, UID_TO_CHARS,
	UINTMAX_TO_CHARS):
	Renamed from gid_to_oct, major_to_oct, minor_to_oct, mode_to_oct,
	off_to_oct, size_to_oct, time_to_oct, uid_to_oct, uintmax_to_oct,
	GID_TO_OCT, MAJOR_TO_OCT, MINOR_TO_OCT, MODE_TO_OCT, OFF_TO_OCT,
	SIZE_TO_OCT, TIME_TO_OCT, UID_TO_OCT, UINTMAX_TO_OCT,
	respectively.  All definitions and uses changed.
	(excluded_name): New decl.

	* src/compare.c (diff_archive):
	Open files with O_NONBLOCK instead of O_NDELAY.

	* src/create.c (base_64_digits): New constant.
	(base_8_digits): New macro.
	(MAX_VAL_WITH_DIGITS): New macro.
	(to_base): First half of old to_oct.  Support base 64 too.
	(to_chars): Other half of old to_oct, for 64-bit support.
	(GID_NOBODY, UID_NOBODY): Don't define if the headers don't.
	(gid_substitute, uid_substitute): Look up names dynamically if
	GID_NOBODY and UID_NOBODY aren't defined; use -2 if all else fails.
	(mode_to_chars): Renamed from mode_to_oct.
	Support negative values in all the _to_chars functions.
	(start_header): Use FILESYSTEM_PREFIX_LEN instead of MSDOS ifdef.
	Abort if archive format is DEFAULT_FORMAT when it shouldn't be.
	(dump_file): Inspect entire pathname, not just new file name
	component, when deciding whether to exclude it.

	* src/extract.c (extract_archive):
	Open files with O_NONBLOCK instead of O_NDELAY.

	* src/incremen.c (get_directory_contents):
	Inspect entire pathname, not just new file name
	component, when deciding whether to exclude it.

	* src/list.c (<fnmatch.h>): Do not include.
	(from_chars): Renamed from from_oct.  New parameter specifing
	the negative of the minimum allowed value.  Support negative
	and base-64 values.
	(base64_map): New var.
	(base64_init): New function.
	(print_header): Output numeric uids and gids if numeric_owner_option.

	* src/misc.c (quote_copy_string): Use LG_8 instead of constants.

	* src/names.c (_GNU_SOURCE): Define.
	(<fnmatch.h>): Include unconditionally.
	(excluded_name): New function, taking over duties of excluded_pathname.
	All uses changed.

	* src/rmt.c (decode_oflag): New function.
	(main): Use it to support symbolic open flags.

	* src/rtapelib.c (encode_oflag): New function.
	(rmt_open__): Do not allow newlines in the path.
	Propagate errno correctly.
	Decode symbolic open flags, if present.

	* src/system.h (FILESYSTEM_PREFIX_LEN, ISSLASH, O_ACCMODE, O_NONBLOCK):
	New macros.

	* src/tar.c: (long_options, usage, OPTION_STRING, decode_options):
	New -y or --bzip2 option.
	(add_filtered_exclude): New function.
	(decode_options): Put excluded patterns with / into
	excluded_with_slash, and without / into excluded_without_slash.
	Compare newer_mtime_option to its new initial value
	TYPE_MINIMUM (time_t) when deciding whether more than one
	threshold date was specified.

1999-07-20  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in: Version 1.13.5.

	* src/common.h (FATAL_ERROR): Invoke apply_delayed_set_stat
	before exiting.
	* src/buffer.c (new_volume): Likewise.
	* src/incremen.c (read_directory_file): Likewise.
	* src/tar.c (decode_options):
	ERROR ((TAREXIT_FAILURE, ... -> FATAL_ERROR ((0,
	for consistency.

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.13.4.
	* configure.in (AC_CHECK_FUNCS): Add lstat, readlink, symlink.

	* src/system.h (lstat): Define only if !HAVE_LSTAT && !defined lstat.
	(S_ISMPB, S_ISMPC, S_ISNWK): Remove unused macros.
	(S_ISBLK, S_ISCHR, S_ISCTG, S_ISFIFO, S_ISLNK, S_ISSOCK):
	Define to 0 if the corresponding S_IF* macro is not defined.
	(mkfifo): Do not define if already defined, or if S_IFIFO
	is not defined.

	* src/compare.c (diff_archive): Use HAVE_READLINK, not
	S_ISLNK, to determine whether to invoke readlink.
	* src/create.c (dump_file): Likewise.

	* src/extract.c (set_mode):
	Do not chmod unless we are root or the -p option was given;
	this matches historical practice.
	(unlink_destination): New function, which checks for unlink failures.
	(maybe_recoverable): Stay quiet if -U.
	(extract_archive): Use O_EXCL if unlink_first_option.
	Report unlink failures.
	Use HAVE_SYMLINK, not S_ISLNK, to determine whether symlink exists.
	Use HAVE_MKFIFO || defined mkfifo, not S_ISFIFO, to determine whether
	mkfifo exists.

	* src/incremen.c (get_directory_contents): Depend on
	S_ISHIDDEN, not AIX, to determine whether to invoke S_ISHIDDEN.

	* src/list.c: Remove S_IS* ifdefs.
	* src/misc.c (maybe_backup_file): Likewise.

	* src/misc.c (maybe_backup_file):
	"Virtual memory exhausted" -> "Memory exhausted",
	to conform to the other places this message is issued.

	* src/mangle.c (extract_mangle):
	Replace #ifdef S_ISLNK with #ifdef HAVE_SYMLINK.

	* src/rtapelib.c (rmt_open__):
	Remove typo that caused us to omit the first char
	of the basename.

1999-07-16  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): version 1.13.3.

	* doc/tar.texi: A path name is excluded if any of its file name
	components matches an excluded pattern, even if the path name was
	specified on the command line.
	* src/create.c (create_archive): Likewise.
	* src/list.c (read_and): Likewise.
	* src/update.c (update_archive): Likewise.
	* lib/exclude.h (excluded_pathname): New decl.
	* lib/exclude.c (_GNU_SOURCE): Define.
	(FILESYSTEM_PREFIX_LEN, ISSLASH): New macros.
	(excluded_filename_opts): New function.
	(excluded_pathname): New function.

	* lib/Makefile.am (EXTRA_DIST):
	xstrtol.c moved here from libtar_a_SOURCES.
	(libtar_a_SOURCES): Move xstrtol.c to EXTRA_DIST.
	Remove xstrtoul.c; no longer needed.
	* lib/xstrtol.c: Remove.

	* src/tar.c (decode_options):
	Set newer_time_option to TYPE_MINIMUM, so that
	negative timestamps are handled correctly.
	Replace invocations of xstrtol and xstrtoul with xstrtoumax, for
	uniformity (and so that we don't need to have the other fns).
	(main): Remove call to init_total_written; no longer needed.

	* configure.in (AC_CHECK_SIZEOF): Remove no-longer-needed
	checks for unsigned long and long long.
	* src/arith.c: Remove.
	* src/Makefile.am (tar_SOURCES): Remove arith.c.
	* po/POTFILES.in: Remove src/arith.c.
	* src/arith.h: Use double, to simplify configuration gotchas.
	(tarlong): Now double.
	(TARLONG_FORMAT): New macro.
	(BITS_PER_BYTE, BITS_PER_TARLONG, SUPERDIGIT, BITS_PER_SUPERDIGIT,
	LONGS_PER_TARLONG, SIZEOF_TARLONG, struct tarlong,
	zerop_tarlong_helper, lessp_tarlong_helper, clear_tarlong_helper,
	add_to_tarlong_helper, mult_tarlong_helper, print_tarlong_helper,
	zerop_tarlong, lessp_tarlong, clear_tarlong, add_to_tarlong,
	mult_tarlong, print_tarlong): Remove.  All callers replaced with
	arithmetic ops.

	* src/common.h (init_total_written): Remove decl.

	* src/buffer.c (total_written):
	Remove; replaced with prev_written + bytes_written.
	(prev_written): New var.
	(init_total_written): Remove.
	(print_total_written): Use TARLONG_FORMAT instead of print_tarlong.

	* m4/ulonglong.m4 (jm_AC_TYPE_UNSIGNED_LONG_LONG):
	Make sure that we can shift, multiply
	and divide unsigned long long values; Ultrix cc can't do it.

	* lib/modechange.c (mode_compile): Use uintmax_t, not unsigned long.
	Check for any unknown bits, not just unknown bits left of the leftmost
	known bit.

	* lib/quotearg.c (quotearg_buffer):
	Don't quote spaces if C quoting style.
	* src/list.c (from_oct):
	Use C quoting style for error; omit trailing NULs.

1999-07-14  Paul Eggert  <eggert@twinsun.com>

	* configure.in (AM_INIT_AUTOMAKE), NEWS: Version 1.13.2.

	* m4/xstrtoumax.m4 (jm_AC_PREREQ_XSTRTOUMAX): Check whether
	<inttypes.h> defines strtoumax as a macro (and not as a function).
	HP-UX 10.20 does this.

	* src/tar.c (usage): tar-bugs@gnu.org -> bug-tar@gnu.org
	* PORTS, README, TODO, doc/tar.texi: Likewise.

1999-07-12  Paul Eggert  <eggert@twinsun.com>

	* configure.in (AM_INIT_AUTOMAKE): Version 1.13.1.
	(LIBOBJS): Add mktime.o to automake 1.4 bug workaround.

	* src/list.c (decode_header):
	Do not assume that S_IFBLK and S_IFCHR are defined.

	* src/create.c (start_header): Do not assume S_IFMT is defined.
	(dump_file): Remove unnecessary check for screwy apollo lossage.
	Do not assume S_IFBLK and S_IFCHR are defined.

	* src/extract.c (extract_archive):
	Test whether S_IFCHR and S_IFBLK are nonzero,
	not whether they are defined, for consistency with other tests.

	* src/buffer.c (is_regular_file):
	Don't succeed on files that we can't access due to
	permissions problems.
	(open_archive): Fix wording on fatal error message.
	Don't bother to stat /dev/null if the archive is not a character
	special device.

	* src/compare.c (process_rawdata, diff_sparse_files, diff_archive):
	Report an error, not a warning, for I/O errors.
	(process_rawdata, process_dumpdir, diff_sparse_files):
	Change ungrammatical "Data differs" to "Contents differ".
	(get_stat_data): Find hidden files on AIX.
	Accept file name as argument; all uses changed.
	(get_stat_data, diff_archive): Use system error message for
	nonexistent files rather than rolling our own.
	(diff_archive): Unknown file types are errors, not warnings.
	Normalize spelling of message to "File type differs".
	Use get_stat_data to get link status, for consistency.
	Do not inspect st_rdev for fifos.
	Do not assume st_mode values contain only file types and mode bits.
	Check for mode changes and device number changes separately.

	* src/update.c (append_file):
	Open the file before statting it, to avoid a race.
	Complain about file shrinkage only when we reach EOF.

1999-07-08  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.13 released.

	* configure.in (AC_EXEEXT): Add.

	* lib/Makefile.am (noinst_HEADERS):
	Add basename.h, exclude.h.  Remove full-write.h.
	(libtar_a_SOURCES): Add exclude.c.

	* lib/basename.h, lib/exclude.c, lib/exclude.h, lib/safe-read.h:
	New files.
	* lib/full-write.c: Include safe-read.h instead of full-write.h.
	* lib/safe-read.h (safe_read): New decl.
	* src/rmt.c: Include safe-read.h.
	* src/rtapelib.c: Include basename.h, save-read.h.
	(rmt_open__): Use base_name to compute base name.

	* src/common.h:
	Include basename.h, exclude.h; don't include full-write.h.
	(exclude_option): Remove decl.
	(excluded): New decl.
	(add_exclude, add_exclude_file, check_exclude): Remove decls.

	* src/list.c (read_and):
	Use excluded_filename instead of check_exclude.
	Check base name of incoming file name, not entire file name, when
	deciding whether to exclude it.

	* src/create.c (finish_sparse_file):
	Use excluded_filename instead of check_exclude.
	Don't bother to stat excluded file names.
	* src/incremen.c (get_directory_contents): Likewise.

	* src/names.c (exclude_pool, exclude_pool_size,
	allocated_exclude_pool_size, simple_exclude_array,
	simple_excludes, allocated_simple_excludes,
	pattern_exclude_array, pattern_excludes,
	allocated_pattern_excludes, add_exclude, add_exclude_file,
	check_exclude):
	Remove; now done in ../lib/exclude.c.

	* src/tar.c (decode_options): Initialize `excluded'.
	Use new add_exclude_file and add_exclude functions.

1999-07-05  Paul Eggert  <eggert@twinsun.com>

	* m4/gettext.m4: Use changequote rather than [[ ]].

	* lib/safe-read.c: Renamed from lib/full-read.c.
	(safe_read): Renamed from full_read.  All uses changed.
	* lib/safe-read.h, lib/full-write.h: New files.
	* lib/Makefile.am (noinst_HEADERS): Add full-write.h, safe-read.h.
	(libtar_a_SOURCES): Rename full-read.c to safe-read.c.
	* lib/full-write.c: Include full-write.h.
	* src/common.h: Include full-write.h, safe-read.h.
	* src/system.h: (full_read, full_write): Remove decls.

	* src/Makefile.am (datadir): New var; needed for Solaris gettext.

	* src/system.h (bindtextdomain, textdomain): undef before
	defining, to avoid preprocessor warnings with --disable-nls
	on hosts whose locale.h includes libintl.h.

	* lib/xstrtol.c (__strtol): Remove decl; it doesn't work if __strtol
	expands to a macro, which occurs in HP-UX 10.20 with strtoumax.
	(strtol, strtoul): New decls (for pre-ANSI hosts), to replace
	the above decl.

1999-07-02  Paul Eggert  <eggert@twinsun.com>

	* Makefile.am (ACINCLUDE_INPUTS): Add $(M4DIR)/mktime.m4.
	* m4/mktime.m4: New file.
	* m4/Makefile.am.in, m4/README: Remove these files.
	* m4/Makefile.am (EXTRA_DIST): Add mktime.m4;
	remove README, Makefile.am.in.
	(Makefile.am): Remove rule; it didn't work in BSD/OS 4.0.
	* m4/jm-mktime.m4 (jm_FUNC_MKTIME): Invoke AC_FUNC_MKTIME,
	not AM_FUNC_MKTIME.

	* src/tar.c: Include signal.h.
	(SIGCHLD): Define to SIGCLD if SIGCLD is defined but SIGCHLD is not.
	(main): Ensure SIGCHLD is not ignored.

	(BACKUP_OPTION, DELETE_OPTION, EXCLUDE_OPTION, GROUP_OPTION,
	MODE_OPTION, NEWER_MTIME_OPTION, NO_RECURSE_OPTION, NULL_OPTION,
	OWNER_OPTION, POSIX_OPTION, PRESERVE_OPTION, RECORD_SIZE_OPTION,
	RSH_COMMAND_OPTION, SUFFIX_OPTION, USE_COMPRESS_PROGRAM_OPTION,
	VOLNO_FILE_OPTION, OBSOLETE_ABSOLUTE_NAMES,
	OBSOLETE_BLOCK_COMPRESS, OBSOLETE_BLOCKING_FACTOR,
	OBSOLETE_BLOCK_NUMBER, OBSOLETE_READ_FULL_RECORDS, OBSOLETE_TOUCH,
	OBSOLETE_VERSION_CONTROL): Make sure they can't be valid chars, so
	they don't overlap with char codes.  Use an enum instead of a lot
	of #defines.

	* src/system.h (ISASCII): Remove.
	(CTYPE_DOMAIN, ISDIGIT, ISODIGIT, ISPRINT, ISSPACE, S_ISUID,
	S_ISGID, S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP,
	S_IROTH, S_IWOTH, S_IXOTH, MODE_WXUSR, MODE_R, MODE_RW,
	MODE_RWX, MODE_ALL, SEEK_SET, SEEK_CUR, SEEK_END, CHAR_MAX,
	LONG_MAX): New macros.

	* src/incremen.c (ISDIGIT, ISSPACE): Remove; now in system.h.
	(read_directory_file): Cast ISSPACE arg to unsigned char.
	* src/misc.c (ISPRINT): Remove; now in system.h.
	(remove_any_file): Add brackets to pacify gcc -Wall.
	* src/list.c: Don't include <ctype.h>; system.h already does this.
	(ISODIGIT, ISSPACE): Remove; now in system.h.
	(decode_header): No need to AND mode with 07777; MODE_FROM_OCT
	does this now.
	(from_oct): Cast ISSPACE arg to unsigned char.

	* src/create.c (mode_to_oct): Translate modes from internal to
	external form.
	* src/list.c (mode_from_oct): Translate modes from external to
	internal form.  Do not complain about unrecognized mode bits.
	* src/common.h (TSUID, TSGID, TSVTX, TUREAD, TUWRITE, TUEXEC,
	TGREAD, TGWRITE, TGEXEC, TOREAD, TOWRITE, TOEXEC): Remove undefs.

	* src/extract.c: (extr_init, make_directories, extract_archive):
	Do not assume mode bits have traditional Unix values.
	* src/list.c (decode_mode): Likewise.
	* src/create.c (start_header, dump_file): Likewise.
	* src/buffer.c (child_open_for_compress,
	child_open_for_uncompress, open_archive, (close_archive): Likewise.
	* src/compare.c (diff_archive): Likewise.

	* src/extract.c (set_mode): Use %04 not %0.4 format.
	(extract_sparse_file): Do not use data_block uninitialized.
	Check for lseek failures.

	* src/rtapelib.c (rmt_lseek__):
	Convert lseek whence values to portable integers on the wire.
	* src/rmt.c (main): Likewise.  Check for whence values out of range.

	* src/create.c (finish_sparse_file): Use lseek whence macros
	instead of integers.
	* src/buffer.c (backspace_output): Likewise.
	* src/compare.c (diff_archive, verify_volume): Likewise.
	* src/delete.c (move_archive): Likewise.
	* src/extract.c (extract_sparse_file): Likewise.

	* src/create.c (dump_file): Do not invoke finish_sparse_file
	on a negative file descriptor.

	* src/buffer.c: Add braces to pacify gcc -Wall.

	* src/compare.c (diff_sparse_files): Report lseek errors.

	* configure.in (ALL_LINGUAS): Add cs, es, ru.

	* PORTS, TODO: gnu.ai.mit.edu -> gnu.org

	* src/arith.c, src/buffer.c (new_volume): Don't put ^G in
	message to be internationalized; \a doesn't work with msgfmt.

	* src/tar.c (long_options, main, usage, OPTION_STRING):
	Remove -E or --ending-file.
	* src/list.c (read_and): Likewise.
	* src/common.h (ending_file_option): Likewise.
	* src/buffer.c (close_archive): Likewise.

	* tests/after: Don't run two commands together in a pipeline,
	as some old shells mishandle pipeline exit status.

1999-06-28  Paul Eggert  <eggert@twinsun.com>

	* configure.in (AM_INIT_AUTOMAKE): version 1.12.64015.
	* NEWS: Describe changes since 1.12.
	* README: Update bug reporting address; move paxutils ref to NEWS.

	Handle EINTR correctly.
	* lib/Makefile.am (libtar_a_SOURCES): Add full-read.c, full-write.c.
	* lib/full-read.c, lib/full-write.c: New files.
	* src/buffer.c (child_open_for_compress, child_open_for_uncompress):
	Prefer full_read to read and full_write to write.
	* src/compare.c (process_rawdata, diff_sparse_files): Likewise.
	* src/create.c (deal_with_sparse, finish_sparse_file, dump_file):
	Likewise.
	* src/extract.c (extract_sparse_file): Likewise.
	* src/rmt.c (get_string, main, report_error_message,
	report_numbered_error): Likewise.
	* src/rmt.h (rmtread, rmtwrite): Likewise.
	* src/rtapelib.c (do_command, get_status_string, rmt_read__,
	rmt_write__, rmt_ioctl__): Likewise.
	* src/update.c (append_file): Likewise.
	* src/system.h (full_read, full_write): New decls.

	* po/POTFILES.in: Add lib/argmatch.c, lib/error.c lib/getopt.c,
	lib/xmalloc.c, src/arith.c, src/misc.c.

	* src/system.h (STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO):
	New macros.  All uses of STDIN and STDOUT changed.
	* src/rmt.c (prepare_record_buffer, main): Use STDIN_FILENO
	instead of 0 and STDOUT_FILENO instead of 1.
	* src/rtapelib.c (_rmt_rexec): Use STDIN_FILENO and STDOUT_FILENO
	instead of fileno (stdin) and fileno (stdout) or 0 and 1.

	* src/rmt.c (private_strerror): Avoid const.  Translate results.

	* tests/Makefile.am (TESTS): Remove incremen.sh; it doesn't work
	in the presence of NFS clock skew.

1999-06-25  Paul Eggert  <eggert@twinsun.com>

	* configure.in (AM_INIT_AUTOMAKE): version 1.12.64014.

	* src/buffer.c (write_archive_buffer): New function.
	(child_open_for_compress, flush_write, flush_read): Use it to write
	buffers.
	(open_archive): Report error if fstat of archive fails.
	Improve efficiency of check for /dev/null.
	Also, fix some corner cases with remote archives and /dev/null checking.
	(close_archive): Test for input fifo only if not remote.
	Truncate output archive only if it's not remote.

	* src/misc.c (remove_any_file):
	Don't terminate if you see . or ..; just skip them.

1999-06-18  Paul Eggert  <eggert@twinsun.com>

	* configure.in (AM_INIT_AUTOMAKE): version 1.12.64013.

	Output sizes using a format that's more compatible with
	traditional tar (and with GNU Emacs).
	* src/common.h (GID_TO_OCT, MAJOR_TO_OCT, MINOR_TO_OCT,
	MODE_TO_OCT, SIZE_TO_OCT, UID_TO_OCT, UINTMAX_TO_OCT):
	Don't subtract 1 from size.
	* src/create.c (to_oct): Prepend leading zeros, not spaces.
	Output a trailing NUL unless the value won't fit without it.
	(finish_header): No need to append NUL to chksum, now that
	to_oct is doing it.

1999-06-16  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): version 1.12.64012.

	* src/Makefile.am (LDADD): Link libtar.a after @INTLLIBS@, since
	@INTLLIBS@ might invoke rpl_realloc.

	* src/tar.c (backup_type): Remove decl; backupfile.h now has it.
	(intconv): Remove; use xstrto* fns instead.
	("xstrtol.h"): Include.
	(check_decimal): Remove.
	(long_options, usage, OPTION_STRING, decode_options):
	Remove -y, --bzip2, --unbzip2.
	(decode_options): Use xget_version instead of get_version.
	Check for overflow with -b and -L and RECORD_SIZE_OPTION.
	Replace invocations of check_decimal  with xstrtoumax.

	* tests/preset.in (echo_n, echo_c): Remove.

	* tests/after: Don't rely on $echo_c and $echo_n.

	* lib/addext.c, lib/dirname.c, lib/lchown.c, lib/lchown.h,
	lib/malloc.c, lib/mktime.c, lib/realloc.c, lib/strtol.c, lib/strtoul.c,
	lib/strtoull.c, lib/strtoumax.c, lib/utime.c, lib/xstrtol.c,
	lib/xstrtol.h, lib/xstrtoul.c, lib/xstrtoumax.c,
	m4/Makefile.am.in, m4/README, m4/ccstdc.m4, m4/d-ino.m4,
	m4/gettext.m4, m4/inttypes_h.m4, m4/isc-posix.m4,
	m4/jm-mktime.m4, m4/largefile.m4, m4/lcmessage.m4,
	m4/malloc.m4, m4/progtest.m4, m4/realloc.m4, m4/uintmax_t.m4,
	m4/ulonglong.m4, m4/utimbuf.m4, m4/utime.m4, m4/utimes.m4,
	m4/xstrtoumax.m4: New files.

	* configure.in(fp_PROG_ECHO): Remove; no longer needed.
	(AC_SYS_LARGEFILE): Renamed from AC_LFS.
	(jm_AC_HEADER_INTTYPES_H): Replaces inline code.
	(jm_STRUCT_DIRENT_D_INO, jm_AC_TYPE_UINTMAX_T, jm_AC_PREREQ_XSTRTOUMAX): Add.
	(AC_CHECK_FUNCS): Remove lchown.
	(AC_REPLACE_FUNCS): Remove basename, dirname.
	Add lchown, strtol, strtoul.
	(jm_FUNC_MKTIME): Add.
	(LIBOBJS): Replace .o with $U.o, so that the .o files in LIBOBJS
	are also built via the ANSI2KNR-filtering rules.
	Use a no-op line to work around bug in automake 1.4 with malloc and
	realloc.
	(AC_OUTPUT): Add m4/Makefile.

	* lib/Makefile.am (EXTRA_DIST):
	Add lchown.c, malloc.c, mktime.c, realloc.c,
	strtol.c, strtoul.c, strtoull.c, strtoumax.c, utime.c.
	(noinst_HEADERS): Add lchown.h, modechange.h, xstrtol.h.
	(libtar_a_SOURCES): Add addext.c, basename.c, xstrtol.c,
	xstrtoul.c, xstrtoumax.c.  Remove getversion.c.
	($(srcdir)/getdate.c:): Remove `expect conflicts' line.

	* src/system.h (uintmax_t): Don't declare; configure now does this.

	* src/common.h (backup_type): New decl.
	* src/common.h, src/misc.c, src/tar.c:
	Move include of backupfile.h to common.h.

	* src/misc.c (maybe_backup_file):
	Pass backup_type to find_backup_file_name.

	* src/list.c (print_header): Change sizes of uform and gform from 11 to
	UINTMAX_STRSIZE_BOUND.

	* doc/tar.texi: Remove --bzip2.
	Fix @xref typos reported by latest makeinfo.

	* Makefile.am (ACLOCAL_AMFLAGS): New macro.
	(SUBDIRS): Add m4.
	(M4DIR, ACINCLUDE_INPUTS): New macros.
	($(srcdir)/acinclude.m4): New rule.

	* acconfig.h (ENABLE_NLS, HAVE_CATGETS, HAVE_GETTEXT,
	HAVE_INTTYPES_H, HAVE_LC_MESSAGES, HAVE_STPCPY): Remve #undefs;
	now generated automatically by autoconf.

1999-05-15  Paul Eggert  <eggert@twinsun.com>

	* doc/tar.texi: Remove -y.

1999-04-09  Paul Eggert  <eggert@twinsun.com>

	* src/system.h (INT_STRLEN_BOUND): Fix off-by-factor-of-10 typo
	(we were allocating too much storage).
	(uintmax_t): Don't declare; configure now does this.

	* ABOUT-NLS: Update to gettext 0.10.35 edition.

1999-03-22  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): version 1.12.64010

	* acinclude.m4 (AC_LFS_FLAGS):
	Don't use -mabi=n32 with GCC on IRIX 6.2; it's the default.
	(AC_LFS): -n32, -o32, and -n64 are CPPFLAGS, not CFLAGS.
	(jm_FUNC_MALLOC, jm_FUNC_REALLOC): New macros.

	* configure.in (jm_FUNC_MALLOC, jm_FUNC_REALLOC):
	New macros; needed for latest GNU xmalloc.c.

	* Makefile.am (noinst_HEADERS): Add quotearg.h, xalloc.h.
	(libtar_a_SOURCES): Add quotearg.c.
	* list.c: Include <quotearg.h>.
	(from_oct): Add forward decl.
	(read_header): Return HEADER_FAILURE if we can't parse the checksum.
	(from_oct): Report an error only if TYPE is nonzero.
	Quote any funny characters in bad header.

1999-03-20  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): version 1.12.64009

	* acinclude.m4 (AC_LFS_FLAGS): Add support for IRIX 6.2 and later.
	(AC_LFS_SPACE_APPEND): Assume $2 is quoted properly; all callers
	changed.
	(AC_LFS): Simplify AIX revision number test.

1999-03-17  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): version 1.12.64008

	* configure.in (AC_VALIDATE_CACHED_SYSTEM_TUPLE):
	Remove; it doesn't work that well
	with AC_CANONICAL_HOST.
	(fp_WITH_INCLUDED_MALLOC): Remove; we'll just use the system malloc.

	* Makefile.am (EXTRA_DIST): Remove AC-PATCHES, AM-PATCHES, BI-PATCHES.

	* Makefile.am (EXTRA_DIST): Remove gmalloc.c.

	* acinclude.m4 (fp_WITH_INCLUDED_MALLOC): Remove.

	* tar.texi: Fix bug-report addr.

	* README: Remove --with-included-malloc.
	Upgrade version numbers of build software.

1999-03-07  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.12.64007.

	* acinclude.m4 (AM_WITH_NLS): Port to Solaris 2.5.1,
	where bindtextdomain and gettext require -lintl.
	(AC_LFS_FLAGS): Simplify so that it only gets the flags;
	`no' means it failed.
	(AC_LFS_SPACE_APPEND, AC_LFS_MACRO_VALUE): New macros.
	(AC_LFS): Use them.  Set _FILE_OFFSET_BITS, _LARGEFILE_SOURCE, and
	_LARGE_FILES from LFS_CFLAGS, so that in the normal case we don't need
	to add anything to the command line (it's all in config.h).
	Put any extra -D and -I options into CPPFLAGS, the rest into CFLAGS.

1999-03-01  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.12.64006.

	* acinclude.m4 (AC_LFS_FLAGS): Port to AIX 4.2.

	* src/list.c: (gid_from_oct, major_from_oct, minor_from_oct,
	mode_from_oct, off_from_oct, size_from_oct, time_from_oct,
	uid_from_oct, uintmax_from_oct): Use TYPE_MAXIMUM instead of macros
	like OFF_MAX, which are not reliable
	(e.g. OFF_MAX in AIX 4.2 is incorrect).
	* src/system.h (GID_MAX, MAJOR_MAX, MINOR_MAX, MODE_MAX, OFF_MAX,
	SIZE_MAX, TIME_MAX,UID_MAX, UINTMAX_MAX):  Remove; no longer used.

	* src/incremen.c (get_directory_contents):
	Don't use statx if _LARGE_FILES; it doesn't work under AIX 4.2.
	Have statx depend on STX_HIDDEN, not AIX.

	* src/create.c (to_oct):
	New parameter substitute, giving a substitute value to use
	when the original value is out of range.  Do not append a space to the
	output; modern tars don't.  When a value is out of range, specify the
	maximum value, not the number of bits.
	(GID_NOBODY, UID_NOBODY): New macros.
	(gid_to_oct, uid_to_oct): Use them as substitutes.
	(finish_header): Do not assume that UINTMAX_TO_OCT appends a space.
	(dump_file): Check whether the file changed as we read it.

	* src/rmt.c (main): Remove suspicious AIX/386 code.

1999-02-19  Paul Eggert  <eggert@twinsun.com>

	* intl/localealias.c (read_alias_file): Don't assume that memcpy
	returns a type compatible with char *; it doesn't on SunOS
	4.1.4 with Sun cc, since <string.h> doesn't declare memcpy.

	* NEWS, configure.in (AM_INIT_AUTOMAKE): Version 1.12.64005.

	* src/tar.c (long_options, usage): Prefer --unbzip2 to --bunzip2.
	* doc/tar.texi: Add --bzip2, --unbzip2 options.

	* configure.in (AC_CANONICAL_HOST, AC_VALIDATE_CACHED_SYSTEM_TUPLE):
	Add.
	(AC_LINK_FILES): Omit; AM_GNU_GETTEXT now does this.
	(AC_OUTPUT): Omit munging of po/Makefile; AM_GNU_GETTEXT now does this.
	* acinclude.m4 (AM_WITH_NLS):
	Update to latest gettext version (serial 5).
	(AC_LFS_FLAGS): New macro
	(AC_LFS): Use it.  Append to CFLAGS, LDFLAGS, LDLIBS instead of
	working only with unset variables.  Append to CFLAGS, not CPPFLAGS.
	Work properly in cross-compilation scenario, by checking for getconf
	with AC_CHECK_TOOL and by ditching uname in favor of
	AC_CANONICAL_HOST and $host_os.  Add --disable-lfs option.

	* lib/getdate.y: Update to fileutils 4.0 getdate.y, with one patch:
	replace FORCE_ALLOCA_H with HAVE_ALLOCA_H.
	* lib/Makefile.am (AUTOMAKE_OPTIONS): Append ../src/ansi2knr,
	since getdate.y now uses ANSI code.

	* config.guess, config.sub: New files; taken from automake 1.4.

	* intl/Makefile.in, intl/VERSION, intl/bindtextdom.c,
	intl/cat-compat.c, intl/dcgettext.c, intl/dgettext.c,
	intl/explodename.c, intl/finddomain.c, intl/gettext.c,
	intl/gettext.h, intl/gettextP.h, intl/hash-string.h,
	intl/l10nflist.c, intl/libgettext.h, intl/loadinfo.h,
	intl/loadmsgcat.c, intl/localealias.c, intl/textdomain.c:
	Update to GNU gettext 0.10.35, with patches as per GCC snapshot 990109.

1999-02-01  Paul Eggert  <eggert@twinsun.com>

	* src/tar.c: Update copyright.

	* NEWS: 1.12.64004

1999-02-01  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in: Version 1.12.64004

	* configure.in (AC_LFS): Use this macro, instead of open-coding it.

	* acinclude.m4 (AC_LFS, AM_PROG_CC_STDC): New macros.

	* src/extract.c (extract_archive): Fix bug when extracting sparse
	files: they were trashing the tar file header.

	* src/tar.c: (long_options, usage, OPTION_STRING, decode_options):
	Add -y or --bzip2 or --bunzip2 option.

1999-01-30  Paul Eggert  <eggert@twinsun.com>

	* src/names.c (cached_no_such_uname, cached_no_such_gname,
	cached_no_such_uid, cached_no_such_gid): New vars.
	(uid_to_uname, gid_to_gname, uname_to_uid, gname_to_gid):
	Cache failures, too.

	* src/tar.c (decode_options):
	Don't pass names longer than UNAME_FIELD_SIZE to
	uname_to_uid, as it messes up the cache.  Similarly for gname_to_uid.

1999-01-27  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in: Version 1.12.64003

	* src/buffer.c (backspace_output, close_archive):  Cast
	rmtlseek position arg to off_t, for benefit of K&R compilers
	with long long.
	* src/compare.c (verify_volume): Likewise.

	* NEWS, configure.in: Version 1.12.64002

	* src/create.c (gid_to_oct, major_to_oct, minor_to_oct, mode_to_oct,
	off_to_oct, size_to_oct, time_to_oct, uid_to_oct):
	Cast arg to uintmax_t for benefit of pre-ANSI compilers with long long.
	* src/list.c: (gid_from_oct, major_from_oct, minor_from_oct,
	mode_from_oct, off_from_oct, size_from_oct, time_from_oct,
	uid_from_oct): Likewise.

1999-01-25  Paul Eggert  <eggert@twinsun.com>

	* incremen.sh: Fix timing bug in regression test.

1999-01-22  Paul Eggert  <eggert@twinsun.com>

	* NEWS, configure.in: Update version

	* Makefile.am (localedir): Change to $(datadir)/locale.
	(DEFS): New macro, defining LOCALEDIR.
	(tar.o, tar._o, rmt.o, rmt._o): Remove.
	(INCLUDES): Add -I..

	* Makefile.am (localedir): Change to $(datadir)/locale.

1999-01-21  Paul Eggert  <eggert@twinsun.com>

	* NEWS, README, configure.in: Unofficial version 1.12.64001.

	* tests/Makefile.am (localedir): Change to $(datadir)/locale.
	* src/Makefile.am (localedir): Likewise.
	(DEFS): New macro, defining LOCALEDIR.
	(tar.o, tar._o, rmt.o, rmt._o): Remove.
	(INCLUDES): Add `-I..'.

	* tests/incremen.sh: Fix timing bug.

1999-01-20  Paul Eggert  <eggert@twinsun.com>

	* NEWS, README, configure.in: Unofficial version 1.12.64000.
	`lfs.7' changed to `64000' in version number
	to conform to gnits standards.

	* COPYING, INSTALL, doc/texinfo.tex, install-sh, missing,
	mkinstalldirs, ansi2knr.c: Update to latest public versions.

	Rebuild with automake 1.4 and autoconf 2.13, to work around some
	porting problems.

1998-12-07  Paul Eggert  <eggert@twinsun.com>

	* NEWS, README, configure.in: Unofficial version 1.12.lfs.6.

	* src/list.c (read_header):
	Accept file names as specified by POSIX.1-1996 section 10.1.1.

1998-11-30  Paul Eggert  <eggert@twinsun.com>

	* configure.in: Quote the output of uname.

	* src/extract.c (set_stat): chmod after chown even when not root;
	if we are using --same-owner this is needed e.g. on Solaris 2.5.1.

1998-11-15  Paul Eggert  <eggert@twinsun.com>

	* NEWS, README, configure.in: Unofficial version 1.12.lfs.5.

	* configure.in (ac_test_CPPFLAGS, ac_test_LDFLAGS, ac_test_LIBS,
	ac_getconfs, ac_result): Special case for HP-UX 10.20 or later.

1998-10-28  Paul Eggert  <eggert@twinsun.com>

	* NEWS, README, configure.in: Unofficial version 1.12.lfs.4.

	* src/system.h (voidstar): Use void * if __STDC__ is defined,
	not merely nonzero.

	* src/rtapelib.c: Don't use rexec code unless compiled with WITH_REXEC.
	On many installations, rexec is disabled.

1998-08-07  Paul Eggert  <eggert@twinsun.com>

	* NEWS, README, configure.in: Unofficial version 1.12.lfs.3.

	* src/names.c (uid_to_uname, gid_to_gname): Don't used cached name
	for nameless users and groups.

1998-02-17  Paul Eggert  <eggert@twinsun.com>

	* NEWS, README, configure.in: Unofficial version 1.12.lfs.2.
	* NEWS, README: Add explanation of why this isn't an official version.

1998-02-02  Paul Eggert  <eggert@twinsun.com>

	* NEWS, README, configure.in: Unofficial version 1.12.lfs.1.
	This is an unofficial version.

1997-12-17  Paul Eggert  <eggert@twinsun.com>

	* src/incremen.c (ST_DEV_MSB): New macro.
	(NFS_FILE_STAT): Use most significant bit of st_dev,
	even if it's unsigned.

1997-12-08  Paul Eggert  <eggert@twinsun.com>

	* src/system.h (ST_NBLOCKS): Fix typo in definition.

1997-11-19  Paul Eggert  <eggert@twinsun.com>

	* configure.in (HAVE_INTTYPES_H):
	Don't ignore cache variable if it's already set.

1997-11-10  Paul Eggert  <eggert@twinsun.com>

	* src/rmt.c (main): Don't assume mt_count is of type daddr_t.
	* src/delete.c (records_read): Now off_t.
	(move_archive): Don't assume mt_count is of type daddr_t.

1997-10-30  Paul Eggert  <eggert@twinsun.com>

	* configure.in (CPPFLAGS, LDFLAGS, LIBS):
	Set to appropriate values if large file support
	needs explicit enabling.
	(HAVE_INTTYPES_H, HAVE_ST_FSTYPE_STRING, daddr_t, major_t, minor_t,
	ssize_t):
	New macros to configure.
	(AC_TYPE_MODE_T, AC_TYPE_PID_T, AC_TYPE_OFF_T): Add.

	* acconfig.h (daddr_t, HAVE_INTTYPES_H, HAVE_ST_FSTYPE_STRING,
	major_t, minor_t, ssize_t): New macros.

	* src/arith.h (TARLONG_FORMAT):
	Fix typo: %uld -> %lu.  Use unsigned when long long
	(%lld -> %llu).
	(add_to_tarlong_helper, mult_tarlong_helper): 2nd arg is now unsigned long.
	(add_to_tarlong, mult_tarlong): Cast 2nd arg to unsigned long.

	* src/arith.c (add_to_tarlong_helper, mult_tarlong_helper):
	2nd arg is now unsigned long.

	* src/rmt.c (allocated_size): Now size_t, and now initialized to 0.
	(prepare_record_buffer): Arg is now size_t.
	Remove now-useless casts.

	(main): Use `long' for status, so that it can store ssize_t.
	Use daddr_t, mode_t, size_t, off_t when appropriate.
	Convert daddr_t and off_t values ourselves, since they might be longer
	than long.  Convert other types using `long' primitives.
	When processing MTIOCTOP, do not try to pass resulting
	count back, since it won't work (it could be too large) and it's
	not expected anyway.

	* src/update.c:
	(append_file) Use off_t, size_t, ssize_t when appropriate.  Remove
	now-useless casts.  Use unsigned long to print *_t types, except use
	STRINGIFY_BIGINT for off_t.
	(update_archive): Cast -1 to dev_t when necessary.

	* src/tar.c (check_decimal):
	Now returns 1 if successful, 0 otherwise, and returns
	uintmax_t value into new arg.  Check for arithmetic overflow.
	(decode_options): Avoid overflow if record_size fits in size_t but not int.
	Check for overflow on user or group ids.

	* src/compare.c (diff_init, process_rawdata, read_and_process,
	diff_sparse_files, diff_archive):
	Use off_t, pid_t, size_t, ssize_t when appropriate.
	Remove now-useless casts.  Use unsigned long to print *_t types,
	except use STRINGIFY_BIGINT for off_t.

	(process_noop, process_rawdata, process_dumpdir, read_and_process):
	Size arg is now size_t.

	(diff_sparse_files): Arg is now off_t.  Check for size_t overflow
	when allocating buffer.

	* src/rtapelib.c:
	(do_command, rmt_open__, rmt_read__, rmt_lseek__, rmt_ioctl__):
	Use pid_t, size_t, ssize_t when appropriate.  Remove now-useless casts.
	Use unsigned long to print *_t types, except use STRINGIFY_BIGINT for
	off_t.

	(get_status_string, get_status_off): New function.
	(get_status): Now returns long, so that it can store ssize_t.
	Invoke get_status_string to do the real work.
	(rmt_read__, rmt_write__): Now returns ssize_t. Size arg is now size_t.
	(rmt_lseek__): Now returns off_t, using new get_status_off function.
	(rmt_ioctl__): Convert mt_count by hand,
	since it might be longer than long.

	* src/mangle.c (extract_mangle):
	Check for overflow when converting off_t to size_t.
	Use off_t, size_t when appropriate.  Remove now-useless casts.

	* src/system.h (mode_t): Remove; now done by autoconf.
	(ST_NBLOCKS): Do not overflow if st_size is near maximum.
	Return number of ST_NBLOCKSIZE-byte blocks,
	not number of 512-byte blocks;
	this also helps to avoid overflow.
	(st_blocks): Declare if needed.
	(ST_NBLOCKSIZE): New macro.
	(<limits.h>, <inttypes.h>): Include if available.
	(CHAR_BIT): New macro.
	(uintmax_t): New typedef.
	(TYPE_SIGNED, TYPE_MINIMUM, TYPE_MAXIMUM, INT_STRLEN_BOUND,
	UINTMAX_STRSIZE_BOUND, GID_MAX, MAJOR_MAX, MINOR_MAX, MODE_MAX,
	OFF_MAX, SIZE_MAX, TIME_MAX, UID_MAX, UINTMAX_MAX): New macros.

	* src/names.c (name_init):
	Fix typo in error message: FILE* was passed, but char*
	was wanted.

	(read_name_from_file, name_gather, addname, name_match, name_scan,
	add_exclude): Use size_t when appropriate.  Remove now-useless casts.

	(exclude_pool_size, allocated_exclude_pool_size): Now size_t.

	* src/extract.c (newdir_umask, current_umask): Now mode_t.
	(extract_sparse_file): Args now use off_t.

	(set_mode, set_stat, make_directories, extract_sparse_file,
	extract_archive): Use off_t, size_t, ssize_t when appropriate.  Remove
	now-useless casts.  Use unsigned long to print *_t types, except use
	STRINGIFY_BIGINT for off_t.

	* src/misc.c (quote_copy_string):
	Use size_t when appropriate.  Remove now-useless casts.

	* src/list.c (read_and, list_archive, read_header, decode_mode,
	print_header, print_for_mkdir):
	Use mode_t, off_t, size_t when appropriate.  Remove
	now-useless casts.  Use unsigned long to print *_t types, except use
	STRINGIFY_BIGINT for off_t.

	(read_header): Check for overflow when converting header size.

	(from_oct): Now static.  Now returns uintmax_t.  `where' arg is now
	const char *.  Size arg is now size_t.  Now takes new type and maxval
	args.  Compute result using uintmax_t, not long.  Report error if
	field does not contain octal number in range.
	(gid_from_oct, major_from_oct, minor_from_oct, mode_from_oct,
	off_from_oct, size_from_oct, time_from_oct, uid_from_oct,
	uintmax_from_oct): New functions.

	(stringify_uintmax_t_backwards): New function.

	(decode_mode, print_for_mkdir): Mode arg is now mode_t.
	(skip_file): Offset arg is now off_t.

	* src/buffer.c (record_start_block, save_totsize, save_sizeleft,
	real_s_totsize, real_s_sizeleft, current_block_ordinal):
	Now off_t.
	(write_error): Arg is now ssize_t.
	(child_pid): Now pid_t.
	(available_space_after): Now size_t.

	(child_open_for_compress, child_open_for_uncompress, flush_write,
	open_archive, flush_write, write_error, flush_read, close_archive):
	Use pid_t, ssize_t, size_t when appropriate.  Remove now-useless
	casts.  Use unsigned long to print *_t types, except use
	STRINGIFY_BIGINT for off_t.

	* src/delete.c (records_read): Now daddr_t.
	(move_archive): Arg is now daddr_t.  Check for overflow when
	computing offset.
	(move_archive, delete_archive_members): Use daddr_t, off_t when
	appropriate.  Remove now-useless casts.

	* src/rmt.h (rmt_read__, rmt_write__): Now returns ssize_t.
	(rmt_lseek): Now returns off_t.

	* src/create.c (to_oct):
	Now static.  Value arg is now uintmax_t.  Accept new args
	giving name of type of octal field, for error messages.  Report an
	error if the value is too large to fit in the field.
	(gid_to_oct, major_to_oct, minor_to_oct, mode_to_oct, off_to_oct,
	size_to_oct, time_to_oct, uid_to_oct, uintmax_to_oct): New functions.

	(write_eot, write_long, finish_header, deal_with_sparse,
	finish_sparse_file, dump_file): Use dev_t, off_t, ssize_t, size_t when
	appropriate.  Remove now-useless casts.  Use unsigned long to print
	*_t types, except use STRINGIFY_BIGINT for off_t.

	(find_new_file_size): 1st arg is now off_t*.
	(finish_sparse_file): Args now use off_t, not long.
	Check for lseek error.
	(create_archive, dump_file): Cast -1 to dev_t when necessary.
	(dump_file): Device arg is now dev_t.
	Avoid overflow when testing whether file has holes
	by using the new ST_NBLOCKSIZE macro.

	* src/incremen.c (struct accumulator, add_to_accumulator,
	get_directory_contents, add_hierarchy_to_namelist, gnu_restore):
	Use size_t for sizes.
	(struct directory, get_directory_contents, add_hierarchy_to_namelist):
	Use dev_t, ino_t for devices and inodes.
	(gnu_restore): Use off_t for file offsets.
	(struct directory): Use char for flags.  Add new flag `nfs'.
	(nfs): New constant
	(NFS_FILE_STAT): New macro.
	(note_directory): Accept struct stat * instead of
	device and inode number.  All callers changed.
	(note_directory, get_directory_contents):
	Use NFS_FILE_STAT to determine whether directory is an NFS directory.
	(write_dir_file): Cast time_t to unsigned long before printing as %lu.

	* src/common.h (record_size, struct name, struct sp_array,
	available_space_after):
	Use size_t for sizes.
	(save_sizeleft, save_totsize, current_block_ordinal, skip_file):
	Use off_t for file offsets.
	(struct name): dir_contents is now const char *, not char *.
	(dump_file, get_directory_contents): Use dev_t for devices.
	(to_oct): Remove decl.
	(GID_TO_OCT, MAJOR_TO_OCT, MINOR_TO_OCT, MODE_TO_OCT, SIZE_TO_OCT,
	UID_TO_OCT, UINTMAX_TO_OCT, OFF_TO_OCT, TIME_TO_OCT, STRINGIFY_BIGINT,
	GID_FROM_OCT, MAJOR_FROM_OCT, MINOR_FROM_OCT, MODE_FROM_OCT,
	OFF_FROM_OCT, SIZE_FROM_OCT, TIME_FROM_OCT, UID_FROM_OCT,
	UINTMAX_FROM_OCT): New macros.
	(gid_to_oct, major_to_oct, minor_to_oct, mode_to_oct, off_to_oct,
	size_to_oct, time_to_oct, uid_to_oct, uintmax_to_oct,
	stringify_uintmax_t_backwards, gid_from_oct, major_from_oct,
	minor_from_oct, mode_from_oct, off_from_oct, size_from_oct,
	time_from_oct, uid_from_oct, uintmax_from_oct): New decls.
	(print_for_mkdir): 2nd arg is now mode_t.

See ChangeLog.1 for earlier changes.


Copyright (C) 1997, 1998, 1999, 2000, 2001 Free Software Foundation, Inc.

This file is part of GNU tar.

GNU tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU tar; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
