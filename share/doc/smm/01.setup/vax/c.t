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
.\"	@(#)c.t	6.4 (Berkeley) 5/7/91
.\"
.de IR
\fI\\$1\fP\|\\$2
..
.ds LH "Installing/Operating \*(4B
.\".nr H1 6
.\".nr H2 0
.ds RH "Appendix C \- installation troubleshooting
.ds CF \*(DY
.bp
.LG
.B
.ce
APPENDIX C \- INSTALLATION TROUBLESHOOTING
.sp 2
.R
.NL
.PP
This appendix lists and explains certain problems
that might be encountered while trying to install the \*(4B
distribution.  The information provided here is
limited to the early steps in the installation process;
i.e. up to the point where the root file system is installed.
If you have a problem installing
the release consult this section before contacting our group.
.SH
\fBUsing the distribution console medium.\fP
.LP
This section describes problems that may occur when using
the programs provided on the distributed console medium:
TU58 cassette or RX01 floppy disk.
.LP
\fIprogram can not be loaded\fP.
.LP
Check to make
sure the correct floppy or cassette is being used.
If using a floppy, be sure it is not in upside down.  If using
a cassette on an 11/730, be certain drive 0 is being used.
If a hard I/O error occurred while reading a floppy, try resetting
the console LSI-11 by powering it on and off.  If you can not
boot the cassette's bootstrap monitor, verify that the standard
DEC console cassette can be read; if it can not, your cassette
drive is probably broken.
.LP
\fIprogram halts without warning\fP.
.LP
Check to make sure you have specified
the correct disk to format; consult sections 1.3 and 1.4 for a
discussion of
the VAX and UNIX device naming conventions.  On 11/750's,
specifying a non-existent MASSBUS device will cause the
program to halt as it receives an interrupt (standalone
programs operate by polling devices).
.LP
If using a floppy, try reading the floppy under
your current system.  If this works, copy the floppy to a new 
one and begin again.  If using a cassette on an 11/730,
do likewise.
.LP
\fIformat prints ``Known devices are ...''\fP.
.LP
You have requested
.I format
to work on a device for which it has no driver, or that does not exist;
only the listed devices are supported.
.LP
\fIformat, boot, or copy prints ``unknown drive type''\fP.
.LP
A MASSBUS disk was specified, but the associated MASSBUS
drive type register indicates a drive of unknown type.
This probably means you typed something wrong or your
hardware is incorrectly configured.
.LP
\fIformat, boot, or copy prints ``unknown device''\fP.
.LP
The device specified is probably not one of those supported
by the distribution; consult section 1.1.  If the device
is listed in section 1.1, the drive may be dual-ported, or
for some other reason the driver was unable to decipher
its characteristics.  If this is a MASSBUS drive, try
powering the MASSBUS adapter and/or controller on and
off to clear the drive type register.
.LP
\fIcopy does not copy 205 records\fP
.LP
If a tape read error occurred,
clean your tape drive heads.  If a disk write error occurred,
the disk formatting may have failed.  If the disk pack is
removable, try another one.  If you are currently running
UNIX, you can reboot your old system and use \fIdd\fP to
copy the mini-root file system into a disk partition
(assuming the destination is not in use by the running
system).
.LP
\fIboot prints ``not a directory''\fP
.LP
The
.I boot
program was unable to find the requested program because
it encountered something other than a directory while
searching the file system.
This usually suggests that
no file system is present on the disk partition supplied,
or the file system has been corrupted.  First check to
make sure you typed the correct line to boot.  If this
is the case and you are booting from the mini-root file
system, the mini-root was probably not copied correctly off the
tape (perhaps it was not placed in the correct disk partition).
Try reinstalling the mini-root file system or, if trying
to boot the true root file system, try booting from the
mini-root file system and run \fIfsck\fP on the restored
root file system to insure its integrity.  Finally, as
a last resort, copy the \fIboot\fP program from the 
mini-root file system to the newly installed root file system.
.LP
\fIboot prints ``bad format''\fP
.LP
The program you requested 
.I boot
to load did not have a 407, 410, or 413 magic number in its
header.  This should never happen on a distribution system.
If you were trying to boot off the root file system, reboot
the system on the mini-root file system and look at the
program on the root file system.  Try copying the copy
of vmunix on the mini-root to the root file system also.
.LP
\fIboot prints ``Short read''\fP
.LP
The file header for the program contained a size larger
than the actual size of the file located on disk.  This
is probably the result of file system corruption (or a 
disk I/O error).  Try booting again or creating a new
copy of the program to be loaded (see above).
.SH
\fBBooting the generic system\fP
.LP
This section contains common problems encountered when booting
the generic version of the system.
.LP
\fIsystem panics with ``panic: iinit''\fP
.LP
This occurred because the system was unable to mount
the root file system.
The root file system supplied at the ``root device?''
prompt was probably incorrect.  Remember that when running on the
mini-root file system, this question must be answered
with something of the form ``hp0*''.  If the answer
had been ``hp0'', the system would have used the ``a''
partition on unit 0 of the ``hp'' drive, where presumably 
no file system exists.
.LP
Alternatively, the file system on which you were trying
to run is corrupted.  Try
reinstalling the appropriate file system.
.LP
\fIsystem selects incorrect root device\fP
.LP
That is, you try to boot the system single user with
``B/2'' or ``B xxS'' but do not get the root file system
in the expected location.  This is most likely caused
by your having many disks available more suited to be
a root file system than the one you wanted.  For example,
if you have a ``up'' disk and an ``hk'' disk and install
the system on the ``hk'', then try to boot the system 
to single-user mode, the heuristic used by the generic
system to select the root file system will choose the
``up'' disk.  The following list gives, in descending
order, those disks thought most suitable to be a root
file system: ``hp'', ``up'', ``ra'', ``rb'', ``rl'', ``hk''
(the position of ``rl'' is subject to argument). 
To get the root device you want you must boot
using ``B/3'' or ``B ANY'', then supply the root device
at the prompt.
.LP
\fIsystem crashes during autoconfiguration\fP
.LP
This is almost always caused by an
unsupported UNIBUS device being present at a
location where a supported device was expected.
You must disable the device in some way, either
by pulling it off the bus, or by moving the location
of the console status register (consult Appendix A
for a complete list of UNIBUS csr's used in the generic system).
.LP
\fIsystem does not find device(s)\fP
.LP
The UNIBUS device is not at a standard location.  Consult
the list of control status register addresses in Appendix
A, or wait to configure a system to your hardware.
.LP
Alternatively, certain devices are difficult to locate
during autoconfiguration.  A classic example is the TS11
tape drive that does not autoconfigure properly if it is
rewinding when the system is rebooted.  Tape drives should
configure properly if they are off-line, or are not performing
a tape movement.  Disks that are dual-ported should
autoconfigure properly if the drive is not being simultaneously
accessed through the alternate port.
.SH
\fBBuilding console cassettes\fP
.LP
This sections describes common problems encountered
while constructing a console bootstrap cassette.
.LP
\fIsystem crashes\fP
.LP
You are trying to build a cassette for an 11/750.
On an 11/750 the system is booted by using a bootstrap
prom and sector 0 of the root file system.  Refer
to section 2.1.5 or 
.IR tu (4)
for the appropriate reprimand.
.LP
\fIsystem hangs\fP
.LP
You are using an MRSP prom on an 11/750 and think you
can ignore the instructions in this document.  The
problem here is that the generic system only supports
the MRSP prom on an 11/730.  Using it on an 11/750 requires
a special system configuration; consult
.IR tu (4)
for more information.
