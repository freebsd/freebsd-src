.\" Copyright (c) 1988 The Regents of the University of California.
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
.\"	@(#)1.t	1.6 (Berkeley) 5/7/91
.\"
.ds lq ``
.ds rq ''
.ds LH "Installing/Operating \*(4B
.ds RH Introduction
.ds CF \*(DY
.LP
.\".nr H1 1
.bp
.LG
.B
.ce
.NH 1
INTRODUCTION
.sp 2
.R
.NL
.PP
This document explains how to install the Berkeley
version of \*(Ux for the \*(Th on your system.  While this is the first
release from Berkeley for the \*(Th, the version of
.UX
distributed by Computer Consoles Inc. (CCI) was derived from 4.2BSD.
Consequently, the filesystem
format is compatible and it will only be necessary for you to perform
a full bootstrap procedure if you are installing the release on a new
machine.
The System V \*(Ux systems distributed by CCI, Unisys (Sperry) and Harris
are also derived from 4.2BSD, but only the network and filesystem
remain compatible with \*(4B.
The object file formats are completely different in the System V
releases.
Thus, the most straightforward procedure for upgrading a System V
system is to perform a full bootstrap.
.PP
The full bootstrap procedure
is outlined in chapter 2; the process includes booting standalone
utilities from tape to format a disk, if necessary, and then copying
a small root filesystem image onto a swap area.  This filesystem is
then booted and used to extract a dump of a standard root filesystem.
Finally, that root filesystem is booted, and the remainder of the system
binaries and sources are read from the archives on the tape(s).
.PP
The technique for upgrading a 4.2 or beta-release 4.3BSD system is described
in chapter 3 of this document.
As \*(4B is completely compatible with the beta release,
and sufficiently compatible with the vendor-supplied 4.2BSD releases,
the upgrade procedure involves extracting a new set of system binaries
onto new root and /usr filesystems.
The sources are then extracted, and local configuration files are merged
into the new system.  User filesystems may be upgraded in place.
All 4.3BSD-beta binaries may be used with \*(4B in the course
of the conversion.
It is desirable to recompile local sources after the conversion,
as the compilers have had many fixes installed.
However, due to significant incompatibilities between
the vendor-derived versions of 4.2BSD and the Berkeley \*(4B release, many
4.2BSD binary images will not function properly.  For such programs it
will be both necessary and desirable to recompile this software after
the conversion.  Consult section 3 for a description of the differences
between \*(4B and the previous vendor-supplied systems for the \*(Th.
.NH 2
Hardware supported
.PP
This distribution can be booted on a CCI Power 6/32, Harris HCX-7,
Unisys (Sperry) 7000/40, or ICL Clan 7 with any disks supported on the \*(Vs
disk controllers sold by these vendors (SMD/E or VDDC).
The new CCI SMD/E controller with working scatter-gather I/O
is supported as well.
In particular, the following drives are supported:
.DS
.TS
l l.
FUJITSU 160M	CDC 9766 300M
FUJITSU 330M	CDC 340M
FUJITSU 2351 Eagle	CDC 515M
Maxtor 340M
.TE
.DE
The distribution can also be booted on a Harris HCX-9
using any disk on the HDC disk controller on the \*(Vm,
although the \*(Vm tapes are not currently supported.
.PP
The only tape drives supported by this distribution are 9-track tape drives
attached to the Ciprico Tapemaster tape controller.
.NH 2
Distribution format
.PP
The distribution comes in two formats:
.DS
(3)\0\0 1600bpi  2400'  9-track magnetic tapes, or
(1)\0\0 6250bpi  2400'  9-track magnetic tape
.DE
Installation from scratch on any machine requires a tape unit.  If your
machine is currently running 4.2 or 4.3BSD-beta and has a network connection
to a 4.2 or 4.3BSD machine with a tape drive, it is a simple matter to
install the software from a remote tape drive.
.PP
If you have the facilities, we \fBstrongly\fP recommend copying the
magnetic tape(s) in the distribution kit to guard against disaster.
The tapes contain some 1024-byte records followed by many
10240-byte records.  There are interspersed tape marks;
end-of-tape is signaled by a double end-of-file.  The first file
on the tape is a very small file system containing
preliminary bootstrapping programs.  This is followed by a binary image
of an approximately 2 megabyte ``mini root'' file system.  Following
the mini root file is a full dump of the root file system
(see \fIdump\fP\|(8)*).
.FS
\ * References of the form \fIX\fP(Y) mean the entry named
\fIX\fP in section Y of the
.UX
programmer's manual.
.FE
Additional files on the tape(s)
contain tape archive images of the system binaries and sources (see
\fItar\fP\|(1)).  See Appendix A for a description of the contents
and format of the tape(s).
One file contains software
contributed by the user community; refer to the accompanying
documentation for a description of its contents and an
explanation of how it should be installed.
.NH 2
Hardware terminology
.PP
This section gives a short discussion of hardware terminology
to help you get your bearings. 
.PP
The Power 6/32 (and most related machines being shipped) use a \*(Vs
for all I/O peripherals.
The console processor used for bootstrap and
diagnostic purposes is also located on the \*(Vs.
The Harris HCX-9 uses a \*(Vm instead of a \*(Vs; however, the architecture
is completely analogous, and the following discussion applies with the
exception of the name of the bus and the name of the disk controller.
The device naming
conventions described here apply to the console processor; under \*(Ux
device naming is considerably simpler.
.PP
The \*(Vs is a 32-bit bus that supports devices which
use 16-bit, 24-bit, or 32-bit addresses (or some combination).
The type of each address placed on the \*(Vs is indicated
by an accompanying \fIaddress modifier\fP.  In addition to the
width of the
address present on the bus, \*(Vs address modifiers
may be used to indicate the privileges of the requesting
program (.e.g the program is executing in supervisory mode).
The 6/32's \*(Vs adapter accepts device requests with either
16, 24, or 32-bit address modifiers.
16-bit addresses are used to access control registers
for \*(Vs devices.
24-bit addresses are used to access up to one megabyte of \*(Vs
local memory or device shared memory
as well as the first 15Mb of main memory.
24-bit addresses are used for DMA by some peripherals,
interpreting the address
as an absolute physical address in referencing main memory.
Other devices use 32-bit addressing, allowing them to access all
of main memory.
This means that the address space for 24-bit devices overlaps
that of 32-bit devices.
Devices which do not support full 32-bit
addressing can be difficult to work with as their limited addressing
restricts the placement of I/O buffers in main memory.  Unfortunately,
because the \*(Vs has had limited acceptance, there are
very few good \*(Vs device controllers available; this has
resulted in several non-\*(Vs devices being attached to the
\*(Vs through bus-adapter cards.  Devices of this sort often
support only 20-bit or 24-bit addressing.
.PP
From the \*(Th side of the \*(Vs adaptor,
the three address spaces are mapped so as to avoid
overlaps.  Physical addresses in the range 0xffff0000 to 0xfffffff are
used to access \*(Vs devices which use 16-bit addresses.  References
to this region of the \*(Th address space result in a \*(Vs
transfer with a 16-bit address generated from the lower order 16
bits of the memory address and a ``short addressing non-privileged I/O
access'' address modifier (0x10).  Addresses in the range 0xff000000 to
0xffff0000 are used to access 24-bit \*(Vs devices, generating a 24-bit
address and a ``standard addressing non-privileged data access''
address modifier (0x01).
Within this range, addresses from 0xfff00000 to 0xffff0000 refer
to \*(Vs local memory used by devices (such as the VIOC)
for shared communication areas.
Finally, any other address in the
the primary I/O adapter space, 0xc0000000 to 0xff000000, generates
a 32-bit \*(Vs address with an ``extended addressing non-privileged
data access'' address modifier (0xf1).  Note, however, that 32-bit
addresses generated by references to this region result in a \*(Vs
address with bits 31-30 set to 0.  Thus, for example, a reference to
a device located at 0xfe000000 would result in a \*(Vs transfer
with the address set to 0x3e000000.  A complete list of the characteristics
of the devices supported in the system may be found in Appendix A.
.PP
The console processor on most \*(Vs machines has a set of names for devices:
.DS
.TS
l l.
FUJITSU 160M disk drives	fsd
FUJITSU 330M disk drives	fuj
FUJITSU 450M disk drives	egl**
CDC 300M disk drives	smd
CDC 340M disk drives	xfd
CDC 515M disk drives	xsd
MXD Maxtor 340M disk drives	mxd
Cipher tape drives	cyp
.TE
.FS
**\|Eagle drives are not supported by the console processor on all tahoe
machines.
.FE
.DE
Devices are fully specified to the console processor with:
.DS
xxx(y,z)
.DE
where \fIxxx\fP is one of the above names (e.g. \fIxfd\fP).
The value \fIy\fP specifies a controller to use and also
the device; it is computed as
.DS
8 * \fIcontroller\fP + \fIdevice\fP
.DE
Thus, controller 0 (by convention the controller located at \*(Vs
address 0xfff2400), drive 0 would have a \fIy\fP value of 0
while controller 1 (address of 0xfff2800) drive 0 would have a \fIy\fP
value of 4*.
.FS
*\|Note that this means you can not reference drives 4-15 on a
controller; as a result we expect the console interface to
change soon.
.FE
The \fIz\fP value is interpreted differently for tapes and disks;
for disks it is a disk block, and for tapes it is a file number
on the tape.
.PP
The HCX-9 uses different controllers and terminology:
.DS
.TS
l l.
disks on HDC controller	dsk
Xylogics tapes	???
.TE
.DE
Devices are fully specified to the console processor with:
.DS
xxx(x,y,z)
.DE
where \fIxxx\fP is one of the above names (e.g. \fIdsk\fP).
The value \fIy\fP specifies the device unit number.
Thus, controller 0 (by convention the controller located at \*(Vs
address 0xfff2400), drive 0 would have a \fIy\fP value of 0
while controller 1 (address of 0xfff2800) drive 0 would have a \fIy\fP
value of 4*.
The \fIz\fP value is interpreted as on the other systems.
.PP
The console processor has the notion of a \fIdefault device\fP
to use with file related commands.  The default device is specified
according to the form shown above.  Further, the console processor,
by default, interprets certain system files on the default disk to discover
information about disk drives in the system.  As
.UX
device names are decidedly different from the names used by the
console processor this can lead to serious confusion.  We will
return to this problem later in section 4; for now you should
simply be aware of the difference in naming conventions.
.NH 2
\*(Ux device naming
.PP
\*(Ux has a set of names for devices which are different
from the CCI names for the devices, viz.:
.DS
.TS
l l.
\*(Vs (SMD/E, VDDC) disk drives	dk
\*(Vm (HDC) disk drives	hd
Cipher tape drives	cy
.TE
.DE
.PP
The standalone system, used to bootstrap the full \*(Ux system,
uses device names of the form:
.DS
xx(c,d,p)
.DE
where \fIxx\fP is the device type, normally \fIdk\fP or \fIcy\fP.  The
value \fIc\fP specifies the controller to use, and \fId\fP specifies
the device.  The \fIp\fP value is interpreted differently for tapes
and disks: for disks it is a disk \fIpartition\fP (in the range 0-7),
and for tapes it is a file number offset on the tape.  Thus, partition
1 of a ``dk'' type disk drive on controller vd0 at drive 0 would be
``dk(0,0,1)''.  Normally the controller will be controller 0; it
may therefore be omitted from the device specification, and most of
the examples in this document reflect this.  When not running
standalone, this partition would normally be available as ``/dev/dk0b''.
Here the prefix ``/dev'' is the name of the directory where all
``special files'' normally live, the ``dk'' serves the obvious purpose,
the ``0'' identifies this as a partition of dk drive number ``0'' and
the ``b'' identifies this as the second partition.
.PP
In all simple cases, where only a single controller is present, a drive
with unit number 0 (determined by its unit plug on the front of the drive)
will be called unit 0 in its \*(Ux file name.  This is not, however, strictly
necessary, since the system has a level of indirection in this naming.
If there are multiple controllers, the disk unit numbers will normally
be counted sequentially across controllers.  This can be taken
advantage of to make the system less dependent on the interconnect
topology, and to make reconfiguration after hardware failure extremely
easy.
.PP
Each \*(Ux physical disk is divided into at most 8 logical disk partitions,
each of which may occupy any consecutive cylinder range on the physical
device.  The cylinders occupied by the 8 partitions for each drive type
are specified initially in the disk description file /etc/disktab
(c.f. \fIdisktab\fP(5)).  The partition information and description of the
drive geometry are written in the first sector of each disk with the
\fIdisklabel\|\fP(8) program.  Each partition may be used for either a
raw data area such as a paging area or to store a \*(Ux file system.
It is conventional for the first partition on a disk to be used
to store a root file system, from which \*(Ux may be bootstrapped.
The second partition is traditionally used as a paging area, and the
rest of the disk is divided into spaces for additional ``mounted
file systems'' by use of one or more additional partitions.
.PP
Returning to the discussion of the standalone system, we recall
that tapes also took three integer parameters.  In the normal case
where the Cipher tape drive is unit 0 on the first controller
(the only unit supported by the standalone utilities), the
files on the tape have names ``cy(0,0,0)'' (or just ``cy(0,0)'',
``cy(0,1)'', etc.
Here ``file'' means a tape file containing a single data stream
terminated by a tape mark.*
.FS
* Note that while a tape file consists of a single data stream,
the distribution tape(s) have data structures in these files.
Although the tape(s) contain only a few tape files, they comprise
several thousand \*(Ux files.
.FE
.NH 2
\*(Ux devices: block and raw
.PP
\*(Ux makes a distinction between ``block'' and ``raw'' (character)
devices.  Each disk has a block device interface where
the system makes the device byte addressable and you can write
a single byte in the middle of the disk.  The system will read
out the data from the disk sector, insert the byte you gave it
and put the modified data back.  The disks with the names
``/dev/xx0[a-h]'', etc., are block devices.
There are also raw devices available.
These have names like ``/dev/rxx0[a-h]'', the
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
