## README for file(1) Command and the libmagic(3) library ##

    @(#) $File: README.md,v 1.5 2023/05/28 13:59:47 christos Exp $

- Bug Tracker: <https://bugs.astron.com/>
- Build Status: <https://travis-ci.org/file/file>
- Download link: <ftp://ftp.astron.com/pub/file/>
- E-mail: <christos@astron.com>
- Fuzzing link: <https://bugs.chromium.org/p/oss-fuzz/issues/list?sort=-opened&can=1&q=proj:file>
- Home page: https://www.darwinsys.com/file/
- Mailing List archives: <https://mailman.astron.com/pipermail/file/>
- Mailing List: <file@astron.com>
- Public repo: <https://github.com/file/file>
- Test framework: <https://github.com/file/file-tests>

Phone: Do not even think of telephoning me about this program. Send
cash first!

This is Release 5.x of Ian Darwin's (copyright but distributable)
file(1) command, an implementation of the Unix File(1) command.
It knows the 'magic number' of several thousands of file types.
This version is the standard "file" command for Linux, *BSD, and
other systems. (See "patchlevel.h" for the exact release number).

The major changes for 5.x are CDF file parsing, indirect magic,
name/use (recursion) and overhaul in mime and ascii encoding
handling.

The major feature of 4.x is the refactoring of the code into a
library, and the re-write of the file command in terms of that
library. The library itself, libmagic can be used by 3rd party
programs that wish to identify file types without having to fork()
and exec() file. The prime contributor for 4.0 was Mans Rullgard.

UNIX is a trademark of UNIX System Laboratories.

The prime contributor to Release 3.8 was Guy Harris, who put in
megachanges including byte-order independence.

The prime contributor to Release 3.0 was Christos Zoulas, who put
in hundreds of lines of source code changes, including his own
ANSIfication of the code (I liked my own ANSIfication better, but
his (__P()) is the "Berkeley standard" way of doing it, and I wanted
UCB to include the code...), his HP-like "indirection" (a feature
of the HP file command, I think), and his mods that finally got
the uncompress (-z) mode finished and working.

This release has compiled in numerous environments; see PORTING
for a list and problems.

This fine freeware file(1) follows the USG (System V) model of the
file command, rather than the Research (V7) version or the V7-derived
4.[23] Berkeley one. That is, the file /etc/magic contains much of
the ritual information that is the source of this program's power.
My version knows a little more magic (including tar archives) than
System V; the /etc/magic parsing seems to be compatible with the
(poorly documented) System V /etc/magic format (with one exception;
see the man page).

In addition, the /etc/magic file is built from a subdirectory
for easier(?) maintenance.  I will act as a clearinghouse for
magic numbers assigned to all sorts of data files that
are in reasonable circulation. Send your magic numbers,
in magic(5) format please, to the maintainer, Christos Zoulas.

COPYING - read this first.
* `README` - read this second (you are currently reading this file).
* `INSTALL` - read on how to install
* `src/apprentice.c` - parses /etc/magic to learn magic
* `src/apptype.c` - used for OS/2 specific application type magic
* `src/ascmagic.c` - third & last set of tests, based on hardwired assumptions.
* `src/asctime_r.c` - replacement for OS's that don't have it.
* `src/asprintf.c` - replacement for OS's that don't have it.
* `src/buffer.c` - buffer handling functions.
* `src/cdf.[ch]` - parser for Microsoft Compound Document Files
* `src/cdf_time.c` - time converter for CDF.
* `src/compress.c` - handles decompressing files to look inside.
* `src/ctime_r.c` - replacement for OS's that don't have it.
* `src/der.[ch]` - parser for Distinguished Encoding Rules
* `src/dprintf.c` - replacement for OS's that don't have it.
* `src/elfclass.h` - common code for elf 32/64.
* `src/encoding.c` - handles unicode encodings
* `src/file.c` - the main program
* `src/file.h` - header file
* `src/file_opts.h` - list of options
* `src/fmtcheck.c` - replacement for OS's that don't have it.
* `src/fsmagic.c` - first set of tests the program runs, based on filesystem info
* `src/funcs.c` - utilility functions
* `src/getline.c` - replacement for OS's that don't have it.
* `src/getopt_long.c` - replacement for OS's that don't have it.
* `src/gmtime_r.c` - replacement for OS's that don't have it.
* `src/is_csv.c` - knows about Comma Separated Value file format (RFC 4180).
* `src/is_json.c` - knows about JavaScript Object Notation format (RFC 8259).
* `src/is_simh.c` - knows about SIMH tape file format.
* `src/is_tar.c, tar.h` - knows about Tape ARchive format (courtesy John Gilmore).
* `src/localtime_r.c` - replacement for OS's that don't have it.
* `src/magic.h.in` - source file for magic.h
* `src/mygetopt.h` - replacement for OS's that don't have it.
* `src/magic.c` - the libmagic api
* `src/names.h` - header file for ascmagic.c
* `src/pread.c` - replacement for OS's that don't have it.
* `src/print.c` - print results, errors, warnings.
* `src/readcdf.c` - CDF wrapper.
* `src/readelf.[ch]` - Stand-alone elf parsing code.
* `src/softmagic.c` - 2nd set of tests, based on /etc/magic
* `src/mygetopt.h` - replacement for OS's that don't have it.
* `src/strcasestr.c` - replacement for OS's that don't have it.
* `src/strlcat.c` - replacement for OS's that don't have it.
* `src/strlcpy.c` - replacement for OS's that don't have it.
* `src/strndup.c` - replacement for OS's that don't have it.
* `src/tar.h` - tar file definitions
* `src/vasprintf.c` - for systems that don't have it.
* `doc/file.man` - man page for the command
* `doc/magic.man` - man page for the magic file, courtesy Guy Harris.
	Install as magic.4 on USG and magic.5 on V7 or Berkeley; cf Makefile.

Magdir - directory of /etc/magic pieces
------------------------------------------------------------------------------

If you submit a new magic entry please make sure you read the following
guidelines:

- Initial match is preferably at least 32 bits long, and is a _unique_ match
- If this is not feasible, use additional check
- Match of <= 16 bits are not accepted
- Delay printing string as much as possible, don't print output too early
- Avoid printf arbitrary byte as string, which can be a source of
  crash and buffer overflow

- Provide complete information with entry:
  * One line short summary
  * Optional long description
  * File extension, if applicable
  * Full name and contact method (for discussion when entry has problem)
  * Further reference, such as documentation of format

gpg for dummies:
------------------------------------------------------------------------------

```
$ gpg --verify file-X.YY.tar.gz.asc file-X.YY.tar.gz
gpg: assuming signed data in `file-X.YY.tar.gz'
gpg: Signature made WWW MMM DD HH:MM:SS YYYY ZZZ using DSA key ID KKKKKKKK
```

To download the key:

```
$ gpg --keyserver hkp://keys.gnupg.net --recv-keys KKKKKKKK
```
------------------------------------------------------------------------------


Parts of this software were developed at SoftQuad Inc., developers
of SGML/HTML/XML publishing software, in Toronto, Canada.
SoftQuad was swallowed up by Corel in 2002 and does not exist any longer.
