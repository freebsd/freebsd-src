This is the beta-test version of the GNU assembler.  (Probably
around Version 1.35, but check version.c which gets updated more
often than this readme.)

The assembler has been modified to support a feature that is
potentially useful when assembling compiler output, but which may
confuse assembly language programmers.  If assembler encounters a
.word pseudo-op of the form symbol1-symbol2 (the difference of two
symbols), and the difference of those two symbols will not fit in 16
bits, the assembler will create a branch around a long jump to
symbol1, and insert this into the output directly before the next
label:  The .word will (instead of containing garbage, or giving an
error message) contain (the address of the long jump)-symbol2.  This
allows the assembler to assemble jump tables that jump to locations
very far away into code that works properly.  If the next label is
more than 32K away from the .word, you lose (silently) RMS claims
this will never happen.  If the -k option is given, you will get a
warning message when this happens.

These files are currently set up to allow you to compile all of the
versions of the assembler (68020, VAX, ns32k, and i386) on the same
machine.  To compile the 68020 version, type 'make a68'.  To compile
the VAX version, type 'make avax'.  To compile the ns32k version,
type 'make a32k'.  To compile the Intel 80386 version, type 'make
a386'.  The Makefile contains instructions on how to make one of the
assemblers compile as the default.

Before you can compile the 68020 version of the assembler, you must
make m68k.h be a link to m-sun3.h , m-hpux.h or m-generic.h .  If
you are on a SUN-3 (or other machine that uses a magic number of
(2 << 16) | OMAGIC type 'ln -s m-sun3.h m68k.h' else if you are on a
machine running HP-UX, type 'ln m-hpux.h m689k.h' else type
'ln -s m-generic.h m68k.h' If your machine does not support symbolic
links, omit the '-s'.

See the instructions in the Makefile for compiling gas for the Sequent
Symmetry (dynix 3.0.12 + others?) or for the HP 9000/300

If your machine does not have both varargs.h and vfprintf(), but does have
_doprnt() add -DNO_VARARGS to the CFLAGS line in the makefile.  If your
machine has neither vfprintf() or _doprnt(), you will have to change
messages.c in order to get readable error messages from the assembler.


	REPORTING BUGS IN GAS

Bugs in gas should be reported to bug-gnu-utils@prep.ai.mit.edu  If you can't
get through to prep, try hack@gnu.ai.mit.edu or hack@media-lab.media.mit.edu

If you report a bug in GAS, please remember to include:

A description of exactly what went wrong.

The type of machine GAS was running on (VAX, 68020, etc),

The Operating System GAS was running under.

The options given to GAS.

The actual input file that caused the problem.

It is silly to report a bug in GAS without including an input file for
GAS.  Don't ask us to generate the file just because you made it from
files you think we have access to.

1. You might be mistaken.
2. It might take us a lot of time to install things to regenerate that file.
3. We might get a different file from the one you got, and might not see any
bug.

To save us these delays and uncertainties, always send the input file
for the program that failed.

If the input file is very large, and you are on the internet, you may
want to make it avaliable for anonymous FTP instead of mailing it.  If you
do, include instructions for FTP'ing it in your bug report.

------------------------------ README.APOLLO ---------------------------------

The changes required to get the GNU C compiler running on
Apollo 68K platforms are available via anonymous ftp from
labrea.stanford.edu (36.8.0.47) in the form of a compressed
tar file named "/pub/gnu/apollo-gcc-1.37.tar.Z".
The size of the file is 84145 bytes.

To build GCC for the Apollo you'll need the virgin FSF
distributions of bison-1.03, gas-1.34, and gcc-1.37. They
are also on labrea.stanford.edu as well as prep.ai.mit.edu.
My changes are to enable gas to produce Apollo COFF object
files and allow gcc to parse some of the syntax extensions
which appear in Apollo C header files. Note that the
COFF encapsulation technique cannot be used on the Apollo.
                                                             
The tar file should be unpacked in the directory containing
the gas-1.34 and gcc-1.37 directories; a few files will be overlaid,
and an APOLLO-GCC-README file will appear in the top directory.
This file contains detailed instructions on how to proceed.

These changes will only work for SR10.1 or later systems, using
the 6.6 or later version of the Apollo C compiler.

If you do not have ftp access, I can mail you the changes in the
form of diffs; they are approximately 40K in length. If you request
them, be sure to give me a voice phone number so I can contact you
in case I can't send you mail; I've had several requests in the
past from people I can't contact.

By the way, I'm working on getting the GNU C++ compiler running;
there are a couple problems to solve. I hope to be able to announce
the Apollo version shortly after the 1.37 version is released.

John Vasta                Hewlett-Packard Apollo Systems Division
vasta@apollo.hp.com       M.S. CHA-01-LT
(508) 256-6600 x6362      300 Apollo Drive, Chelmsford, MA 01824
UUCP: {decwrl!decvax, mit-eddie, attunix}!apollo!vasta

------------------------------------

You might refer others who are interested in a similar thing.

Kevin Buchs    buchs@mayo.edu


------------------------------ README.COFF -----------------------------------

If you have a COFF system, you may wish to aquire

	UUCP: osu-cis!~/gnu/coff/gnu-coff.tar.Z
	or
	FTP:  tut.cis.ohio-state.edu:/pub/gnu/coff/gnu-coff.tar.Z

These contain patches for gas that will make it produce COFF output.
I have never seen these patches, so I don't know how well they work.
