zlib 1.0.4 is a general purpose data compression library.  All the code
is reentrant (thread safe).  The data format used by the zlib library
is described by RFCs (Request for Comments) 1950 to 1952 in the files 
ftp://ds.internic.net/rfc/rfc1950.txt (zlib format), rfc1951.txt (deflate
format) and rfc1952.txt (gzip format). These documents are also available in
other formats from ftp://ftp.uu.net/graphics/png/documents/zlib/zdoc-index.html

All functions of the compression library are documented in the file
zlib.h. A usage example of the library is given in the file example.c
which also tests that the library is working correctly. Another
example is given in the file minigzip.c. The compression library itself
is composed of all source files except example.c and minigzip.c.

To compile all files and run the test program, follow the instructions
given at the top of Makefile. In short "make test; make install"
should work for most machines.  For MSDOS, use one of the special
makefiles such as Makefile.msc; for VMS, use Make_vms.com or descrip.mms.

Questions about zlib should be sent to <zlib@quest.jpl.nasa.gov> or,
if this fails, to the addresses given below in the Copyright section.
The zlib home page is http://quest.jpl.nasa.gov/zlib/

The changes made in version 1.0.4 are documented in the file ChangeLog.
The main changes since 1.0.3 are:

- In very rare conditions, deflate(s, Z_FINISH) could fail to produce an EOF
  bit, so the decompressor could decompress all the correct data but went
  on to attempt decompressing extra garbage data. This affected minigzip too.
- zlibVersion and gzerror return const char* (needed for DLL)
- port to RISCOS (no fdopen, no multiple dots, no unlink, no fileno)


A Perl interface to zlib written by Paul Marquess <pmarquess@bfsec.bt.co.uk>
is in the CPAN (Comprehensive Perl Archive Network) sites, such as:
ftp://ftp.cis.ufl.edu/pub/perl/CPAN/modules/by-module/Compress/Compress-Zlib*


Notes for some targets:

- For Turbo C the small model is supported only with reduced performance to
  avoid any far allocation; it was tested with -DMAX_WBITS=11 -DMAX_MEM_LEVEL=3

- For 64-bit Iris, deflate.c must be compiled without any optimization.
  With -O, one libpng test fails. The test works in 32 bit mode (with
  the -32 compiler flag). The compiler bug has been reported to SGI.

- zlib doesn't work with gcc 2.6.3 on a DEC 3000/300LX under OSF/1 2.1   
  it works when compiled with cc.

- zlib doesn't work on HP-UX 9.05 with one cc compiler (the one not
  accepting the -O option). It works with the other cc compiler.

- To build a Windows DLL version, include in a DLL project zlib.def, zlib.rc
  and all .c files except example.c and minigzip.c; compile with -DZLIB_DLL
  For help on building a zlib DLL, contact Alessandro Iacopetti
  <iaco@email.alessandria.alpcom.it>  http://lisa.unial.it/iaco ,
  or contact Brad Clarke <bclarke@cyberus.ca>.

- gzdopen is not supported on RISCOS


Acknowledgments:

  The deflate format used by zlib was defined by Phil Katz. The deflate
  and zlib specifications were written by Peter Deutsch. Thanks to all the
  people who reported problems and suggested various improvements in zlib;
  they are too numerous to cite here.

Copyright notice:

 (C) 1995-1996 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  gzip@prep.ai.mit.edu    madler@alumni.caltech.edu

If you use the zlib library in a product, we would appreciate *not*
receiving lengthy legal documents to sign. The sources are provided
for free but without warranty of any kind.  The library has been
entirely written by Jean-loup Gailly and Mark Adler; it does not
include third-party code.

If you redistribute modified sources, we would appreciate that you include
in the file ChangeLog history information documenting your changes.
