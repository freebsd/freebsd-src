.\" tbl | readme.ms | [tn]roff -ms | ...
.\" note the "C" (courier) and "CB" fonts: you will probably have to
.\" change these.
.\" $Id: readme.ms,v 1.1 90/12/13 13:09:15 oz Exp Locker: oz $

.de P1
.br
.nr dT 4
.nf
.ft C
.sp .5
.nr t \\n(dT*\\w'x'u
.ta 1u*\\ntu 2u*\\ntu 3u*\\ntu 4u*\\ntu 5u*\\ntu 6u*\\ntu 7u*\\ntu 8u*\\ntu 9u*\\ntu 10u*\\ntu 11u*\\ntu 12u*\\ntu 13u*\\ntu 14u*\\ntu
..
.de P2
.br
.ft 1
.br
.sp .5
.br
.fi
..
.\" CW uses the typewriter/courier font.
.de CW
\fC\\$1\\fP\\$2
..

.\" Footnote numbering [by Henry Spencer]
.\" <text>\*f for a footnote number..
.\" .FS
.\" \*F <footnote text>
.\" .FE
.\"
.ds f \\u\\s-2\\n+f\\s+2\\d
.nr f 0 1
.ds F \\n+F.
.nr F 0 1

.ND
.LP
.TL
\fIsdbm\fP \(em Substitute DBM
.br
or
.br
Berkeley \fIndbm\fP for Every UN*X\** Made Simple
.AU
Ozan (oz) Yigit
.AI
The Guild of PD Software Toolmakers
Toronto - Canada
.sp
oz@nexus.yorku.ca
.LP
.FS
UN*X is not a trademark of any (dis)organization.
.FE
.sp 2
\fIImplementation is the sincerest form of flattery. \(em L. Peter Deutsch\fP
.SH
A The Clone of the \fIndbm\fP library
.PP
The sources accompanying this notice \(em \fIsdbm\fP \(em constitute
the first public release (Dec. 1990) of a complete clone of
the Berkeley UN*X \fIndbm\fP library. The \fIsdbm\fP library is meant to
clone the proven functionality of \fIndbm\fP as closely as possible,
including a few improvements. It is practical, easy to understand, and
compatible.
The \fIsdbm\fP library is not derived from any licensed, proprietary or
copyrighted software.
.PP
The \fIsdbm\fP implementation is based on a 1978 algorithm
[Lar78] by P.-A. (Paul) Larson known as ``Dynamic Hashing''.
In the course of searching for a substitute for \fIndbm\fP, I
prototyped three different external-hashing algorithms [Lar78, Fag79, Lit80]
and ultimately chose Larson's algorithm as a basis of the \fIsdbm\fP
implementation. The Bell Labs
\fIdbm\fP (and therefore \fIndbm\fP) is based on an algorithm invented by
Ken Thompson, [Tho90, Tor87] and predates Larson's work.
.PP
The \fIsdbm\fR programming interface is totally compatible
with \fIndbm\fP and includes a slight improvement in database initialization.
It is also expected to be binary-compatible under most UN*X versions that
support the \fIndbm\fP library.
.PP
The \fIsdbm\fP implementation shares the shortcomings of the \fIndbm\fP
library, as a side effect of various simplifications to the original Larson
algorithm. It does produce \fIholes\fP in the page file as it writes
pages past the end of file. (Larson's paper include a clever solution to
this problem that is a result of using the hash value directly as a block
address.) On the other hand, extensive tests seem to indicate that \fIsdbm\fP
creates fewer holes in general, and the resulting pagefiles are
smaller. The \fIsdbm\fP implementation is also faster than \fIndbm\fP
in database creation.
Unlike the \fIndbm\fP, the \fIsdbm\fP
.CW store
operation will not ``wander away'' trying to split its
data pages to insert a datum that \fIcannot\fP (due to elaborate worst-case
situations) be inserted. (It will fail after a pre-defined number of attempts.)
.SH
Important Compatibility Warning
.PP
The \fIsdbm\fP and \fIndbm\fP
libraries \fIcannot\fP share databases: one cannot read the (dir/pag)
database created by the other. This is due to the differences
between the \fIndbm\fP and \fIsdbm\fP algorithms\**, 
.FS
Torek's discussion [Tor87]
indicates that \fIdbm/ndbm\fP implementations use the hash
value to traverse the radix trie differently than \fIsdbm\fP
and as a result, the page indexes are generated in \fIdifferent\fP order.
For more information, send e-mail to the author.
.FE
and the hash functions
used.
It is easy to convert between the \fIdbm/ndbm\fP databases and \fIsdbm\fP
by ignoring the index completely: see
.CW dbd ,
.CW dbu
etc.
.R
.LP
.SH
Notice of Intellectual Property
.LP
\fIThe entire\fP sdbm  \fIlibrary package, as authored by me,\fP Ozan S. Yigit,
\fIis hereby placed in the public domain.\fP As such, the author is not
responsible for the consequences of use of this software, no matter how
awful, even if they arise from defects in it. There is no expressed or
implied warranty for the \fIsdbm\fP library.
.PP
Since the \fIsdbm\fP
library package is in the public domain, this \fIoriginal\fP
release or any additional public-domain releases of the modified original
cannot possibly (by definition) be withheld from you. Also by definition,
You (singular) have all the rights to this code (including the right to
sell without permission, the right to hoard\**
.FS
You cannot really hoard something that is available to the public at
large, but try if it makes you feel any better.
.FE
and the right to do other icky things as
you see fit) but those rights are also granted to everyone else.
.PP
Please note that all previous distributions of this software contained
a copyright (which is now dropped) to protect its
origins and its current public domain status against any possible claims
and/or challenges.
.SH
Acknowledgments
.PP
Many people have been very helpful and supportive.  A partial list would
necessarily include Rayan Zacherissen (who contributed the man page,
and also hacked a MMAP version of \fIsdbm\fP),
Arnold Robbins, Chris Lewis,
Bill Davidsen, Henry Spencer, Geoff Collyer, Rich Salz (who got me started
in the first place), Johannes Ruschein
(who did the minix port) and David Tilbrook. I thank you all.
.SH
Distribution Manifest and Notes
.LP
This distribution of \fIsdbm\fP includes (at least) the following:
.P1
	CHANGES		change log
	README		this file.
	biblio		a small bibliography on external hashing
	dba.c		a crude (n/s)dbm page file analyzer
	dbd.c		a crude (n/s)dbm page file dumper (for conversion)
	dbe.1		man page for dbe.c
	dbe.c		Janick's database editor
	dbm.c		a dbm library emulation wrapper for ndbm/sdbm
	dbm.h		header file for the above
	dbu.c		a crude db management utility
	hash.c		hashing function
	makefile	guess.
	pair.c		page-level routines (posted earlier)
	pair.h		header file for the above
	readme.ms	troff source for the README file
	sdbm.3		man page
	sdbm.c		the real thing
	sdbm.h		header file for the above
	tune.h		place for tuning & portability thingies
	util.c		miscellaneous
.P2
.PP
.CW dbu
is a simple database manipulation program\** that tries to look
.FS
The 
.CW dbd ,
.CW dba ,
.CW dbu
utilities are quick hacks and are not fit for production use. They were
developed late one night, just to test out \fIsdbm\fP, and convert some
databases.
.FE
like Bell Labs'
.CW cbt
utility. It is currently incomplete in functionality.
I use
.CW dbu
to test out the routines: it takes (from stdin) tab separated
key/value pairs for commands like
.CW build
or
.CW insert
or takes keys for
commands like
.CW delete
or
.CW look .
.P1
	dbu <build|creat|look|insert|cat|delete> dbmfile
.P2
.PP
.CW dba
is a crude analyzer of \fIdbm/sdbm/ndbm\fP
page files. It scans the entire
page file, reporting page level statistics, and totals at the end.
.PP
.CW dbd
is a crude dump program for \fIdbm/ndbm/sdbm\fP
databases. It ignores the
bitmap, and dumps the data pages in sequence. It can be used to create
input for the
.CW dbu 
utility.
Note that
.CW dbd
will skip any NULLs in the key and data
fields, thus is unsuitable to convert some peculiar databases that
insist in including the terminating null.
.PP
I have also included a copy of the
.CW dbe
(\fIndbm\fP DataBase Editor) by Janick Bergeron [janick@bnr.ca] for
your pleasure. You may find it more useful than the little
.CW dbu
utility.
.PP
.CW dbm.[ch]
is a \fIdbm\fP library emulation on top of \fIndbm\fP
(and hence suitable for \fIsdbm\fP). Written by Robert Elz.
.PP
The \fIsdbm\fP
library has been around in beta test for quite a long time, and from whatever
little feedback I received (maybe no news is good news), I believe it has been
functioning without any significant problems. I would, of course, appreciate
all fixes and/or improvements. Portability enhancements would especially be
useful.
.SH
Implementation Issues
.PP
Hash functions:
The algorithm behind \fIsdbm\fP implementation needs a good bit-scrambling
hash function to be effective. I ran into a set of constants for a simple
hash function that seem to help \fIsdbm\fP perform better than \fIndbm\fP
for various inputs:
.P1
	/*
	 * polynomial conversion ignoring overflows
	 * 65599 nice. 65587 even better.
	 */
	long
	dbm_hash(char *str, int len) {
		register unsigned long n = 0;
	
		while (len--)
			n = n * 65599 + *str++;
		return n;
	}
.P2
.PP
There may be better hash functions for the purposes of dynamic hashing.
Try your favorite, and check the pagefile. If it contains too many pages
with too many holes, (in relation to this one for example) or if
\fIsdbm\fP
simply stops working (fails after 
.CW SPLTMAX
attempts to split) when you feed your
NEWS 
.CW history
file to it, you probably do not have a good hashing function.
If you do better (for different types of input), I would like to know
about the function you use.
.PP
Block sizes: It seems (from various tests on a few machines) that a page
file block size
.CW PBLKSIZ
of 1024 is by far the best for performance, but
this also happens to limit the size of a key/value pair. Depending on your
needs, you may wish to increase the page size, and also adjust
.CW PAIRMAX
(the maximum size of a key/value pair allowed: should always be at least
three words smaller than
.CW PBLKSIZ .)
accordingly. The system-wide version of the library
should probably be
configured with 1024 (distribution default), as this appears to be sufficient
for most common uses of \fIsdbm\fP.
.SH
Portability
.PP
This package has been tested in many different UN*Xes even including minix,
and appears to be reasonably portable. This does not mean it will port
easily to non-UN*X systems.
.SH
Notes and Miscellaneous
.PP
The \fIsdbm\fP is not a very complicated package, at least not after you
familiarize yourself with the literature on external hashing. There are
other interesting algorithms in existence that ensure (approximately)
single-read access to a data value associated with any key. These are
directory-less schemes such as \fIlinear hashing\fP [Lit80] (+ Larson
variations), \fIspiral storage\fP [Mar79] or directory schemes such as
\fIextensible hashing\fP [Fag79] by Fagin et al. I do hope these sources
provide a reasonable playground for experimentation with other algorithms.
See the June 1988 issue of ACM Computing Surveys [Enb88] for an
excellent overview of the field. 
.PG
.SH
References
.LP
.IP [Lar78] 4m
P.-A. Larson,
``Dynamic Hashing'', \fIBIT\fP, vol.  18,  pp. 184-201, 1978.
.IP [Tho90] 4m
Ken Thompson, \fIprivate communication\fP, Nov. 1990
.IP [Lit80] 4m
W. Litwin,
`` Linear Hashing: A new tool  for  file  and table addressing'',
\fIProceedings of the 6th Conference on Very Large  Dabatases  (Montreal)\fP,
pp.  212-223,  Very Large Database Foundation, Saratoga, Calif., 1980.
.IP [Fag79] 4m
R. Fagin, J.  Nievergelt,  N.  Pippinger,  and  H.  R. Strong,
``Extendible Hashing - A Fast Access Method for Dynamic Files'',
\fIACM Trans. Database Syst.\fP, vol. 4,  no.3, pp. 315-344, Sept. 1979.
.IP [Wal84] 4m
Rich Wales,
``Discussion of "dbm" data base system'', \fIUSENET newsgroup unix.wizards\fP,
Jan. 1984.
.IP [Tor87] 4m
Chris Torek,
``Re:  dbm.a  and  ndbm.a  archives'', \fIUSENET newsgroup comp.unix\fP,
1987.
.IP [Mar79] 4m
G. N. Martin,
``Spiral Storage: Incrementally  Augmentable  Hash  Addressed  Storage'',
\fITechnical Report #27\fP, University of Varwick, Coventry, U.K., 1979.
.IP [Enb88] 4m
R. J. Enbody and H. C. Du,
``Dynamic Hashing  Schemes'',\fIACM Computing Surveys\fP,
vol. 20, no. 2, pp. 85-113, June 1988.
