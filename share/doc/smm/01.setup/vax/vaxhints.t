.\" Copyright (c) 1980, 1986, 1988 The Regents of the University of California.
.\" All rights reserved.
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistributions of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistributions in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in the
.\"    documentation and/or other materials provided with the distribution.
.\" 3. All advertising materials mentioning features or use of this software
.\"    must display the following acknowledgement:
.\"	This product includes software developed by the University of
.\"	California, Berkeley and its contributors.
.\" 4. Neither the name of the University nor the names of its contributors
.\"    may be used to endorse or promote products derived from this software
.\"    without specific prior written permission.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
.\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
.\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\" SUCH DAMAGE.
.\"
.\"	@(#)vaxhints.t	1.4 (Berkeley) 5/7/91
.\"
.ds lq ``
.ds rq ''
.ds 4B 4.3BSD-tahoe
.nr Vx 1		\" VAX version
.ds Mc VAX
.ds mC vax
.ds Dk hp
.ds Dn RM80
.ds Pa g
.ds Ps 4.3BSD
.bd S B 3
.TL
Hints on Upgrading a 4.3BSD VAX System to 4.3BSD-Tahoe
.br
July 14, 1988
.AU
Michael J. Karels
.AI
Computer Systems Research Group
Department of Electrical Engineering and Computer Science
University of California, Berkeley
Berkeley, California  94720
(415) 642-7780
.de IR
\\fI\\$1\|\\fP\\$2
..
.de UX
UNIX\\$1
..
.PP
This set of notes is extracted from 
\fIInstalling and Operating \*(4B UNIX* on the VAX.\(dg\fP
.FS
*\s-2UNIX\s0 is a register trademark of AT&T in the USA and other countries.
.FE
.FS
\(dgDEC, VAX, IDC, SBI, UNIBUS and MASSBUS are trademarks of
Digital Equipment Corporation.
.FE
It is intended to highlight changes in \*(4B
that will affect installation on existing VAX systems,
and to point out areas of the documentation that should be examined
before installing this system.
It is \fInot\fP intended to substitute for the standard documentation,
but only to point out areas that have changed and that should be examined.
Not all of the documentation is provided in printed form,
but all of it is in the /usr/doc and /usr/src/man directories on
the distribution tape.
.PP
\fBNote that the \*(4B release contains only Tahoe filesystems and executable
images.\fP
The \*(4B distribution tape supplied by Berkeley
cannot be used to bootstrap a VAX without a running 4.2BSD or 4.3BSD system.
If you are not currently running 4.2BSD or 4.3BSD you will
have to do a full bootstrap using a 4.3BSD tape;
to install the \*(4B release, the new sources must then be loaded
and compiled.
It is possible to make a boot tape that can be used with additional machines
by extracting the sources in the distribution tape on a VAX, compiling,
and making a tape using the procedures described in Appendix A
of \fIInstalling and Operating \*(4B UNIX on the VAX.\fP
.PP
If you are running 4.2BSD or \*(Ps, upgrading your system
involves replacing your kernel and system utilities.
Binaries compiled under \*(Ps will work without recompilation
under \*(4B, though they may run faster if they are recompiled.
Binaries compiled under 4.2BSD will probably work without recompilation,
but it is a good idea to recompile and relink because of the many changes
in header files and libraries since 4.2BSD.
Conversion from 4.2BSD is probably done most easily by booting from a 4.3BSD
distribution tape, then using that system to load and compile the sources
from \*(4B.
Sites not running 4.3BSD should read
\fIInstalling and Operating 4.3BSD on the VAX\fP
as well as the \*(4B version of that document.
.PP
The easiest upgrade path from 4.2BSD or \*(Ps
(depending on your file system configuration)
is to build
new root and \fI/usr\fP file systems on unused partitions,
then copy or merge site specific files
into their corresponding files on the new system.
All user file systems can be retained unmodified,
except that the new \fIfsck\fP should be run
before they are mounted (see below).
If there is insufficient space to load the new root and \fI/usr\fP
filesystems before reusing the existing partitions,
it is \fBSTRONGLY\fP advised that you make full dumps of each filesystem
on magtape before beginning.
It is also desirable to run file system checks
of all filesystems to be converted to \*(4B before shutting down.
If you are running a system older than 4.2BSD, you will have to
dump and restore your file systems.
In either case, this is an excellent time to review your disk configuration
for possible tuning of the layout.
.PP
If converting from 4.2BSD or \*(Ps, your old
file systems must be converted.
The standard disk partitions in \*(4B are the same as those
in 4.2BSD and \*(Ps,
except for those on the DEC UDA50.
If you've modified the partition
sizes from the original BSD or CCI ones, and are not already using the
\*(4B disk labels, you will have to modify the default disk partion
tables in the kernel.  Make the necessary table changes and boot
your custom kernel \fBBEFORE\fP trying to access any of your old
file systems!  After doing this, if necessary, the remaining filesystems
may be converted in place by running the \*(4B version of
.IR fsck (8)
on each filesystem and allowing it to make the necessary corrections.
The new version of \fIfsck\fP is more
strict about the size of directories than the version supplied with 4.2BSD.
Thus the first time that it is run on a 4.2BSD file system,
it will produce messages of the form:
.DS
.if \n(Vx \{\
\fBDIRECTORY ...: LENGTH\fP xx \fBNOT MULTIPLE OF 512 (ADJUSTED)\fP
.\}
.if \n(Th \{\
\fBDIRECTORY ...: LENGTH\fP xx \fBNOT MULTIPLE OF 1024 (ADJUSTED)\fP
.\}
.DE
Length ``xx'' will be the size of the directory;
it will be expanded to the next multiple of
.if \n(Vx \{\
512
.\}
.if \n(Th \{\
1024
.\}
bytes.
The new \fIfsck\fP will also set default \fIinterleave\fP and
\fInpsect\fP (number of physical sectors per track) values on older
file systems, in which these fields were unused spares; this correction
will produce messages of the form:
.DS
\fBIMPOSSIBLE INTERLEAVE=0 IN SUPERBLOCK (SET TO DEFAULT)\fP*
\fBIMPOSSIBLE NPSECT=0 IN SUPERBLOCK (SET TO DEFAULT)\fP
.DE
.FS
* The defaults are to set \fIinterleave\fP to 1 and
\fInpsect\fP to \fInsect\fP;
.if \n(Vx \{\
this is correct on many drives.
Notable exceptions are the RM80 and RA81,
where npsect should be set to
one more than nsect.
This affects only performance (and in the case
of the RA81, at least, virtually unmeasurably).
.\}
.if \n(Th \{\
this is correct on all drives supported on the CCI.
.\}
.FE
File systems that have had their interleave and npsect values
set will be diagnosed by the old \fIfsck\fP as having a bad superblock;
the old \fIfsck\fP will run only if given an alternate superblock
.if \n(Vx \{\
(\fIfsck \-b32\fP),
.\}
.if \n(Th \{\
(\fIfsck \-b16\fP),
.\}
in which case it will re-zero these fields.
The \*(4B kernel will internally set these fields to their defaults
if fsck has not done so; again, the
.if \n(Vx \{\
\fI\-b32\fP
.\}
.if \n(Th \{\
\fI\-b16\fP
.\}
option may be
necessary for running the old \fIfsck\fP.
.PP
In addition, \*(4B removes several limits on file system sizes
that were present in both 4.2BSD and 4.3BSD.
The limited file systems
continue to work in \*(4B, but should be converted
as soon as it is convenient
by running \fIfsck\fP with the \fI\-c\fP option.
If no file systems have been so converted,
the sequence \fIfsck \-p \-c\fP will update all of them,
fix the interleave and npsect fields,
and fix any incorrect directory lengths
all at once.
The new unlimited file system formats are treated as read-only
by older systems.
A second \fIfsck \-c\fP, however, will
reconvert the new format to the old if none of the static limits
of the old file system format have been exceeded.
The new file systems are otherwise
compatible between 4.2BSD, \*(Ps, and \*(4B,
though running a \*(4B file system under older systems
may cause more of the above
messages to be generated the next time it is \fIfsck\fP'ed on \*(4B.
.NH 2
Hints on converting from 4.3BSD to \*(4B
.PP
The largest visible change between 4.3BSD to \*(4B
(other than the addition of support for the Tahoe processor)
is the addition of support for disk labels.
This facility allows each disk or disk pack to contain all geometry
information about the disk and the partition layout for the disk.
Disk labels are supported on all disk types on the Tahoe machines,
and on hp and ra/rd disks on the VAX.
See
.IR disklabel (8)
and
.IR disklabel (5).
Installation of this facility requires use of the new kernel and device
drivers, bootstraps and other standalone programs,
/etc/disktab,
.if \n(Vx \{\
.IR bad144 (8V),
.\}
.IR newfs (8),
and probably other programs.
.if \n(Vx \{\
.PP
\*(4B includes support for the VAX 8200 and 8250,
with support for the KDB-50 disk controller on the VAX BI bus,
contributed by Chris Torek.
.PP
The bootstrap programs have been fixed to work on MicroVAX IIs
and VAXstation II's with QVSS (VS II) or QDSS (GPX) displays;
the kernel includes support for these displays, courtesy of Digital
Equipment Corp.
In order to install the bootstrap on RD52/53/54 disks with
.IR disklabel (8),
the new /etc/disktab must be used,
or the block 0 bootstrap must be explictly listed as /usr/mdec/rdboot
(\fInot\fP raboot).
.\}
.PP
The order in which daemons are started by /etc/rc and /etc/rc.local
has changed, and network initialization has been split into /etc/netstart.
Look at the prototype files, and modify /etc/rc.local as necessary.
.PP
\*(4B includes the Olson
timezone implementation, which uses timezone and daylight-savings-time
rules loaded from files in /etc/zoneinfo; see
.IR ctime (3)
and
.IR tzfile (5).
.PP
The type of the
.IR sprintf (3S)
function has been changed from \fIchar *\fP in 4.2BSD and 4.3BSD
to \fIint\fP as in the proposed ANSI C standard and in System V.
Programers are discouraged from using the return value from
.I sprintf
until this change is ubiquitous.
Fortunately, the previous return value from
.I sprintf
was essentially useless.
.PP
The ownership and modes of some directories have changed.
The \fIat\fP programs now run set-user-id ``root'' instead of ``daemon.''
Also, the uucp directory no longer needs to be publicly writable,
as \fItip\fP reverts to privileged status to remove its lock files.
After copying your version of /usr/spool, you should do:
.DS
\fB#\fP \fIchown \-R root /usr/spool/at\fP
\fB#\fP \fIchown \-R uucp.daemon /usr/spool/uucp\fP
\fB#\fP \fIchmod \-R o\-w /usr/spool/uucp\fP
.DE
.PP
The MAKEHOSTS file has moved from /usr/hosts to /usr.
.PP
The source versions of the manual pages have been moved from
/usr/man/man[1-8] to /usr/src/man, /usr/src/new/man, and /usr/src/local/man.
Local manual pages should be moved into their respective source code
directories, or into /usr/src/local/man/man[1-8], and Makefiles changed to
install the formatted manual pages into /usr/local/man/cat[1-8].  The shell
script /usr/man/manroff calls nroff with the standard manual arguments.  An 
example of installing a manual page might be:
.DS
\fB#\fP \fI/usr/man/manroff example.2 > example.0\fP
\fB#\fP \fIinstall -o bin -g bin -m 444 example.0 /usr/local/man/cat2\fP
.DE
.PP
If you are using the name server, your \fIsendmail\fP configuration
file will need some minor updates to accommodate it.
See the ``Sendmail Installation and Operation Guide'' and the sample
\fIsendmail\fP configuration files in /usr/src/usr.lib/sendmail/cf.
The sendmail.cf's supplied with this release are alleged to be
``generic'', but have only really seen use at Berkeley.  In particular
there are two points to watch out for.  First, all host names in the
sendmail.cf itself must be fully qualified names.  Second, the
sendmail.cf's assume you have a /usr/lib/sendmail that was compiled
with the resolver library (i.e., not hosttables). This is necessary
to canonicalize unqualified names into fully-qualified names (e.g.,
foo -> foo.bar.com).  Using these .cf files with a host table can
probably be done, but it will be difficult.
Be sure to regenerate your sendmail frozen configuration file after
installation of your updated configuration file with the command
\fI/usr/lib/sendmail -bz\fP.
The aliases file,
/usr/lib/aliases has also been changed to add certain well-known addresses.

