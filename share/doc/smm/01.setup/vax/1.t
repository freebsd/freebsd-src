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
.\"	@(#)1.t	6.5 (Berkeley) 5/7/91
.\"
.ds lq ``
.ds rq ''
.ds LH "Installing/Operating \*(4B
.ds RH Introduction
.ds CF \*(DY
.LP
.nr H1 1
.bp
.LG
.B
.ce
1. INTRODUCTION
.sp 2
.R
.NL
.PP
This document explains how to install the \*(4B release of the Berkeley
version of UNIX for the VAX on your system.  Because of the file system
organization used in \*(4B, if you are not currently running 4.2BSD
or 4.3BSD
you will have to do a full bootstrap from the distribution tape.
The procedure for performing a full bootstrap is outlined in chapter 2.
The process includes booting standalone utilities from tape
to format a disk if necessary, then to copy a small root filesystem
image onto a swap area.
This filesystem is then booted and used to extract a dump of a standard root
filesystem.
Finally, that root filesystem is booted, and the remainder of the system
binaries and sources are read from the archives on the tape(s).
.PP
The technique for upgrading a 4.2BSD or 4.3BSD system is described
in chapter 3 of this document.
As \*(4B is upward-compatible with 4.2BSD,
the upgrade procedure involves extracting a new set of system binaries
onto new root and /usr filesystems.
The sources are then extracted, and local configuration files are merged
into the new system.
4.2BSD and 4.3BSD user filesystems may up upgraded in place,
and 4.2BSD and 4.3BSD
binaries may be used with \*(4B in the course of the conversion.
It is desirable to recompile most local software after the conversion,
as there are many changes and performance improvements in the standard
libraries.
.NH 2
Hardware supported
.PP
Note that some VAX models are identical
to others in all respects except speed.
The VAX 8650 will be hereafter referred to as a VAX 8600;
likewise, the VAX 8250 will be referred to as a VAX 8200,
the VAX-11/785 as an 11/780, and the 11/725 as an 11/730.
These names are sometimes shortened to ``8600,'' ``8200,''
``780,'' ``750,'' and ``730,''
and the MicroVAX II is sometimes called the ``630.''
.PP
This distribution can be booted on a VAX 8600,
VAX 8200, VAX-11/780, VAX-11/750, VAX-11/730, or MicroVAX II
cpu with at least 2 megabytes of memory, and
any of the following disks:
.DS
.TS
lw(1.5i) l.
DEC MASSBUS:	RM03, RM05, RM80, RP06, RP07
EMULEX MASSBUS:	AMPEX Capricorn, 9300, CDC 9766, 9775,
	FUJITSU 2351 Eagle, 2361\(dg
DEC UNIBUS:	RK07, RL02, RAxx\(dg, RC25
EMULEX SC-21V, SC-31	AMPEX DM980, Capricorn, 9300,
   UNIBUS\(dg:	CDC 9762, 9766, FUJITSU 160M, 330M
EMULEX SC-31 UNIBUS\(dg:	FUJITSU 2351 Eagle
DEC IDC:	R80, RL02
DEC BI:	RAxx\(dg
DEC QBUS:	RD53, RD54, RAxx\(dg
.TE
.DE
.FS
\(dg Other compatible UNIBUS controllers and drives
may be easily usable with the system,
but may require minor modifications to the system
to allow bootstrapping.
The EMULEX disk and SI tape controllers, and
the drives shown here are known
to work as bootstrap devices.
RAxx includes the RA60, RA70, RA80, RA81, and RA82,
as well as the RX50 floppy drives on the MicroVAX II.
Other SMD and MSCP drives can be added with minor or no modifications.
.FE
.PP
The tape drives supported by this distribution are:
.DS
.TS
lw(1.5i) l.
DEC MASSBUS:	TE16, TU45, TU77, TU78
EMULEX MASSBUS:	TC-7000
DEC UNIBUS:	TS11, TU80, TU81\(dg
EMULEX TC-11, AVIV UNIBUS:	KENNEDY 9300, STC, CIPHER
TU45 UNIBUS:	SI 9700
DEC QBUS:	TK50\(dd
.TE
.DE
.FS
\(dg The TU81 support is untested but is identical to
the TK50 code.
.FE
.FS
\(dd No TK50 media are included in the distribution,
hence a machine with only a TK50
must already be running some version of UNIX
that can be used to load the software over a network.
.FE
.PP
The tapes and disks may be on any available UNIBUS or MASSBUS adapter
at any slot.
.PP
This distribution does not support the DEC CI780 or the HSC50 disk controller.
As such, this
distribution will not boot on the standard VAX 8600
cluster configurations.
You will need to configure your system to use only UNIBUS,
MASSBUS, and BI bus disk and tape devices.
In addition,
BI Ethernet, tape, and terminal controllers are unsupported.  You
cannot boot this distribution on a VAX 8200 without a UNIBUS.
.NH 2
Distribution format
.PP
The basic distribution contains the following items:
.DS
(3)\0\0 1600bpi 9-track 2400' magnetic tapes, or
(1)\0\0 6250bpi 9-track 2400' magnetic tape, and
(1)\0\0 TU58 console cassette, and
(1)\0\0 RX01 console floppy disk.
.DE
Installation on any machine requires a tape unit. 
Since certain standard VAX packages
do not include a tape drive, this means one must either
borrow one from another VAX system or one must be purchased
separately.  The console media distributed with the system
are not suitable for use as the standard console media,
as they do not contain microcode needed upon power-up; their
intended use is only for installation.
.PP
If you have the facilities, it is a good idea to copy the
magnetic tape(s) in the distribution kit to guard against disaster.
The tapes contain some
512-byte records followed by many 10240-byte records.
There are interspersed tape marks; end-of-tape is signaled
by a double end-of-file.
The first file on the tape contains preliminary bootstrapping programs.
This is followed by a binary image
of a 2 megabyte ``mini root''
file system.  Following the mini root
file is a full dump of the root file system (see \fIdump\fP\|(8)*).
.FS
* References of the form X(Y) mean the subsection named
X in section Y of the 
.UX
programmer's manual.
.FE
Additional files on the tape(s)
contain tape archive images (see
\fItar\fP\|(1)).  See Appendix A for a description of the contents
and format of the tape(s).
One file contains software
contributed by the user community; refer to the accompanying
documentation for a description of its contents and an
explanation of how it should be installed.
.NH 2
VAX hardware terminology
.PP
This section gives a short discussion of VAX hardware terminology
to help you get your bearings.
.PP
If you have MASSBUS disks and tapes it is necessary to know the
MASSBUS that they are attached to, at least for the purposes of bootstrapping
and system description.  The MASSBUSes can have up to 8 devices attached
to them.  A disk counts as a device.  A tape \fIformatter\fP counts
as a device, and several tape drives may be attached to a formatter.
If you have a separate MASSBUS adapter for a disk and one for a tape
then it is conventional to put the disk as unit 0 on the MASSBUS with
the lowest ``TR'' number, and the tape formatter as unit 0 on the next
MASSBUS.  On a 11/780 this would correspond to having the disk on
``mba0'' at ``tr8'' and the tape on ``mba1'' at ``tr9''.  Here the
MASSBUS adapter with the lowest TR number has been called ``mba0''
and the one with the next lowest number is called ``mba1''.
.PP
To find out the MASSBUS that your tape and disk are on you can examine
the cabling and the unit numbers or your site maintenance guide.
Do not be fooled into thinking that the number on the front of the
tape drive is a device number; it is a \fIslave\fP number,
one of several possible
tapes on the single tape formatter.
For bootstrapping, the slave number \fBmust\fP be 0.  The formatter
unit number may be anything distinct from the other numbers on the
same MASSBUS, but you must know what it is.
.PP
The MASSBUS devices are known by several different names by DEC software
and by UNIX.  At various times it is necessary to know both
names.  There is, of course, the name of the device like ``RM03''
or ``RM80''; these are easy to remember because they are printed
on the front of the device.  DEC also names devices based on the
driver name in the system using a convention that reflects
the interconnect topology of the machine.  The first letter of such
a name is a ``D'' for a disk, the second letter depends on the type
of the drive, ``DR'' for RM03, RM05, and RM80's, ``DB'' for RP06's.
The next letter is related to the interconnect; DEC calls the first
MASSBUS or UNIBUS adapter ``A'', the second ``B'', etc.  Thus, ``DRA'' is
an RM drive on the first MASSBUS adapter.  Finally, the name ends
in a digit corresponding to the unit number for the device on the
MASSBUS: e.g., ``DRA0'' is a disk at the first device slot on the
first MASSBUS adapter and is an RM disk.
.NH 2
UNIX device naming
.PP
UNIX has a set of names for devices which are different
from the DEC software names for the devices.  The following table lists
both the DEC and UNIX names for the supported devices:
.DS
.TS
l l l.
Hardware	UNIX	DEC
_
RM disks	hp	DR
RP disks	hp	DB
MASSBUS TE/TU tapes	ht	MT
TU78 tape	mt	MF
RK disks	hk	DM
RL disks	rl	DL
TS tapes	ts	MS
UDA disks	ra	DU
RC25 disks	ra	DU
IDC disks	rb	DQ
UNIBUS SMD disks	up
TM tapes	tm
TMSCP tapes	tms	MU
UNIBUS TU tapes	ut
BI KDB disks	kra	DU
.TE
.DE
Here UNIBUS SMD disks are disks on an RM-emulating controller on the UNIBUS,
and TM tapes are tapes on a controller that emulates the DEC TM11.
UNIBUS TU tapes are tapes on a UNIBUS controller that emulates the DEC TU45.
IDC disks are disks on an 11/730 Integral Disk Controller.
TS tapes are tapes on a controller compatible with the DEC TS11 (e.g.
a TU80).
TMSCP tapes include the TU81 and TK50.
.PP
The normal standalone system, used to bootstrap the full UNIX system,
uses device names:
.DS
xx(a,c,d,p)
.DE
where \fIxx\fP is any of the UNIX device names in the table above.
The parameters \fIa\fP, \fIc\fP, and \fId\fP
are the \fIadapter\fP, \fIcontroller\fP, and \fIdrive\fP
numbers respectively.
The adapter is the index number of the MASSBUS or UNIBUS
(with the first one found as number 0,
and others numbered sequentially as found).
The controller (or ``device'') number is the index number of
the controller on that adapter.
On the MASSBUS, the controller number is ignored for disk,
and is used for the formatter number for tape.
The drive number is
the index of the disk or tape drive on that controller or formatter.
The \fIp\fP value is interpreted differently for tapes and disks:
for disks it is a disk \fIpartition\fP (in the range 0-7);
for tapes it is a file number on the tape.
Here ``file'' means a tape file containing a single data stream
terminated by a tape mark.*
.FS
* Note that while a tape file consists of a single data stream,
the distribution tape(s) have data structures in these files.
Although the tape(s) contain only a few tape files, they comprise
several thousand UNIX files.
.FE
For example, partition 7 of drive 2 on an RA81 connected to
the only UDA50 on UNIBUS 1 would be ``ra(1,0,2,7)''.
Normally, the adapter and controller will both be 0; they
may therefore be omitted from the device specification,
and most of the examples in this document do so.
When not running standalone, this partition would normally
be available as ``/dev/ra2g''.
Here the prefix ``/dev'' is the name of the directory where all
``special files'' normally live, the ``ra'' serves the obvious purpose,
the ``2'' identifies this as a partition of ra drive number ``2,''
and the ``g'' identifies this as the seventh partition.
.PP
On the VAX 8200, the adapter numbering is controlled by the
ordering of the nodes on the BI; the BI is probed from low
node numbers towards high.  Hence if there are two KDB50 adapters,
one at node 4, and one at node 7, the one at node 4 is kdb0,
and the one at node 7 is kdb1.
The numbering for UNIBUS adapters works similarly.
Usually, the first UNIBUS on an 8200 is at node 0; you will need
this node number to boot from tape.
Other VAX models do not permit such chaotic ordering of adapters.
.PP
In all simple cases, where only a single controller is present,
a drive with unit number 0 (in its unit
plug on the front of the drive) will be called unit 0 in its UNIX
file name.  This is not, however, strictly necessary, since the system
has a level of indirection in this naming.
If there are multiple controllers, the disk unit numbers
will normally be counted sequentially across controllers.
This can be taken
advantage of to make the system less dependent on the interconnect
topology, and to make reconfiguration after hardware
failure extremely easy.
.PP
Each UNIX physical disk is divided into at most 8 logical disk partitions,
each of which may occupy any consecutive cylinder range on the
physical device.  The cylinders occupied
by the 8 partitions for each drive type
are specified initially
.\" in section 4 of the programmers manual and
in the disk description file /etc/disktab (c.f.
\fIdisktab\fP(5)).
The partition information and description of the drive geometry
are written in the first sector of each disk with the
\fIdisklabel\fP(8) program;
currently, this is possible on hp and ra disks, but not on the other
types of disks on the VAX.
Each partition may be used
for either a raw data area such as a paging area or to store a
UNIX file system.
It is conventional for the first partition on a disk to be used
to store a root file system, from which UNIX may be bootstrapped.
The second partition is traditionally used as a paging area, and the
rest of the disk is divided into spaces for additional ``mounted
file systems'' by use of one or more additional partitions.
.PP
The third logical partition of each physical disk also has a conventional
usage on the \*(Mc: it allows access to the entire physical device, in many
cases including bad
sector forwarding information recorded at the end of the disk (one track
plus 126 sectors).  It is
occasionally used to store a single large file system or to access
the entire pack when making a copy of it on another.
Care must be taken if
using this partition not to overwrite the last few tracks and thereby
clobber the bad sector information.
Note that the sector containing the disk label is normally write-protected
so that it is not accidentally overwritten.
Pack-to-pack copies should normally skip the first 16 sectors of a pack,
which contain the label and the initial bootstrap for some processors.
.NH 2
UNIX devices: block and raw
.PP
UNIX makes a distinction between ``block'' and ``raw'' (character)
devices.  Each disk has a block device interface where
the system makes the device byte addressable and you can write
a single byte in the middle of the disk.  The system will read
out the data from the disk sector, insert the byte you gave it
and put the modified data back.  The disks with the names
``/dev/xx0a'', etc are block devices.
There are also raw devices available.
These have names like ``/dev/rxx0a'', the
``r'' here standing for ``raw''.
Raw devices bypass the buffer cache and use DMA directly to/from
the program's I/O buffers;
they are normally restricted to full-sector transfers.
In the bootstrap procedures we
will often suggest using the raw devices, because these tend
to work faster.
Raw devices are used when making new filesystems,
when checking unmounted filesystems,
or for copying quiescent filesystems.
The block devices are used to mount file systems,
or when operating on a mounted filesystem such as the root.
.PP
You should be aware that it is sometimes important whether to use
the character device (for efficiency) or not (because it wouldn't
work, e.g. to write a single byte in the middle of a sector).
Don't change the instructions by using the wrong type of device
indiscriminately.
