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
.\"	@(#)2.t	6.5 (Berkeley) 5/7/91
.\"
.ds lq ``
.ds rq ''
.ds LH "Installing/Operating \*(4B
.ds RH Bootstrapping
.ds CF \*(DY
.bp
.nr H1 2
.nr H2 0
.bp
.LG
.B
.ce
2. BOOTSTRAP PROCEDURE
.sp 2
.R
.NL
.PP
\fBNote\fP: The \*(4B release contains only Tahoe filesystems and executable
images.
The procedures in this section cannot be used on the VAX
with the \*(4B distribution tape supplied by Berkeley.
However, it is possible to make a boot tape that can be used in this way
by extracting the sources in the distribution tape on a VAX, compiling,
and making a tape using the procedures described in Appendix A.
.\" This section explains the bootstrap procedure that can be used
.\" to get the kernel supplied with this distribution running on your machine.
If you are not currently running 4.2BSD or 4.3BSD you will
have to do a full bootstrap using a 4.3BSD tape;
to install the \*(4B release, the new sources must then be loaded
and compiled.
Chapter 3 describes how to upgrade an existing 4.2BSD or 4.3BSD system.
programs.
An understanding of the operations used in a full bootstrap
is very helpful in performing an upgrade as well.
In either case, it is highly desirable to read and understand
the remainder of this document before proceeding.
.NH 2
Converting pre-4.2BSD Systems
.PP
The file system format was changed between 3BSD and 4.0BSD,
and again between 4.1BSD and 4.2BSD.
At a minimum you will have to dump any old file systems,
and then restore them onto the \*(4B file system.
Sites running 3BSD or 32/V may be able to modify the \fIrestore\fP
program to understand the old 512 byte block file system,
but this has never been tried.
The dump format used in 4.0BSD and 4.1BSD is backward-compatible
with that used in \*(4B (which is unchanged from 4.2BSD).
That is, the \*(4B \fIrestore\fP program understands
how to read 4.0BSD and 4.1BSD dump tapes, although \*(4B dump tapes cannot
be restored under 4.0BSD or 4.1BSD.
It is also desirable to make a convenient copy of system configuration
files for use as guides when setting up the new system;
the list of files to save from 4.2BSD systems in chapter 3
may be used as a guideline.
.PP
The first step is to dump your file systems with \fIdump\fP\|(8).
For the utmost of safety this should be done to magtape.
However, if you enjoy gambling with your life
(or you have a VERY friendly user community)
and you have enough disk space, you can try
converting your file systems while copying to a new disk partition
by piping the output of \fIdump\fP directly into \fIrestore\fP 
after bringing up \*(4B.
If you select the latter tack,
a version of the 4.1BSD dump program that runs under \*(4B is
provided in \fI/etc/dump.4.1\fP.
Beware that file systems created under \*(4B can
use about 5-10% more disk space for file system related information
than under 4.1BSD.  Thus, before dumping each file system it is
a good idea to remove any files that may be easily regenerated.
Since all programs should be recompiled under the new
system, your best bet is to remove any object files.  File
systems with at least 10% free space on them should restore into
an equivalently sized \*(4B file system without problem.
.NH 2
Booting from tape
.PP
The tape bootstrap procedure used to create a
working system involves the following major
steps:
.IP 1)
Format a disk pack with the \fIformat\fP program.
.IP 2)
Copy a ``mini root'' file system from the
tape onto the swap area of the disk.
.IP 3)
Boot the UNIX system on the ``mini root''.
.IP 4)
Restore the full root file system using \fIrestore\fP\|(8).
.IP 5)
Build a console floppy, cassette, or RL02 pack for bootstrapping.
.IP 6)
Reboot the completed root file system.
.IP 7)
Label the disks with the \fIdisklabel\fP(8) program.
.IP 8)
Build and restore the /usr file system from tape
with \fItar\fP\|(1).
.IP 9)
Extract the system and utility files and contributed software
as desired.
.PP
Certain of these steps are dependent on your hardware
configuration.  Formatting the disk pack used for the
root file system may require using the DEC standard
formatting programs.  Also, if you are bootstrapping
the system on an 11/750, no console cassette is created.
.PP
Bootstrapping an 8600 is a bit more difficult than bootstrapping
the other machines.  The procedures for loading the toggle program
and reading the tape bootstrap monitor described in Appendix B must be
used if you do not have access to a console RL02 pack
with a UNIX bootstrap.
Such a pack may be made on an 8600 already running UNIX,
or on another \*(4B system with an RL02 drive
using the procedures in 4.1.1.
One may be required to enter the toggle program more than once.
After the bootstrap monitor is loaded, device addresses will be the same
as if the machine were an 11/780.
UNIBUS and MASSBUS adaptors are numbered from zero across both SBIA's
(if present).
.PP
The following sections describe the above steps in detail.
In these sections references to disk drives are of the
form \fIxx\fP(\fIn\fP,\fIm\fP)
and references to files on tape drives are of the form
\fIyy\fP(\fIn\fP,\fIm\fP) where \fIxx\fP and \fIyy\fP
are names described in section 1.4 and \fIn\fP
and \fIm\fP are the unit and offset numbers described in
section 1.4.  Commands
you are expected to type are shown in italics, while that
information printed by the system is shown emboldened.
Throughout the installation steps the
reboot switch on a 780 or 730 should be set to
off; on an 8600 or 750 set the power-on action to halt. (In
normal operation a 780 or 730 will have the reboot
switch on and an 8600 or 750 will have the power-on action set
to reboot/restart.)
.PP
If you encounter problems while following the instructions in
this part of the document, refer to Appendix C for help in
troubleshooting.
.NH 3
Step 1: formatting the disk
.PP
All disks used with \*(4B should be formatted to insure
the proper handling of physically corrupted disk sectors.
If you have DEC disk drives, you should use the standard
DEC formatter to format your disks.  If not, the 
.I format
program included in the distribution, or a vendor supplied
formatting program, may be used to format disks.  The
.I format
program is capable of formatting
any of the following supported distribution devices:
.DS
.TS
lw(1.5i) l.
EMULEX MASSBUS:	AMPEX Capricorn, 9300, CDC 9766, 9775,
	FUJITSU 330M, 2351 Eagle
EMULEX SC-21V, SC-31	AMPEX 9300, Capricorn, CDC 9730, 9766,
    UNIBUS:	FUJITSU 160M, 330M
EMULEX SC-31 UNIBUS:	FUJITSU 2351 Eagle
.TE
.DE
.PP
If you have run a pre-4.1BSD version of UNIX on the packs you
are planning to use for bootstrapping it is likely that the
bad sector information on the packs has been destroyed, since
it was accessible as normal data in the last several tracks of
the disk.  You should therefore run the
formatter again to make sure the information is valid.
.PP
On an 11/750, to use a disk pack as a bootstrap device,
sectors 0 through 15, the disk sectors in the file
``/boot'' (the program that
loads the system image),
and the file system indices that lead to this file
must not have any errors.
On an 8600, 11/780, or 11/730, the ``boot'' program is loaded from
the console medium and includes device drivers for the
``hp'' and ``up'' disks that do
ECC correction and bad sector forwarding; consequently, on
these machines the system may be bootstrapped on these disks
even if the disk is not error free in critical locations. 
In general, if the first 15884 sectors of your disk are
clean you are safe; if not you can take your chances.
.PP
To load the
.I format
program, insert the distribution TU58 cassette or RX01 floppy
disk in the appropriate console device (on the 11/730 use 
cassette 0) and do the following steps.
.PP
If you have an 8600 start the bootstrap monitor using the
procedure described in Appendix B.  Then give the command:
.RT
.DS
\fB=\|\fIformat\fR
.DE
.PP
If you have an 11/780 give the commands:
.RT
.DS
\fB>>>\|\fIHALT\fR
\fB>>>\|\fIUNJAM\fR
\fB>>>\|\fIINIT\fR
\fB>>>\|\fILOAD FORMAT\fR
\fB>>>\|\fISTART 2\fR
.DE
.PP
If you have an 11/750 give the commands:
.DS
\fB>>>\|\fII\fR
\fB>>>\|\fIB DDA0\fR
\fB=\|\fIformat\fR
.DE
.PP
If you have an 11/730 give the commands:
.DS
\fB>>>\|\fIH\fR
\fB>>>\|\fII\fR
\fB>>>\|\fIL DD0:FORMAT\fR
\fB>>>\|\fIS 2\fR
.DE
.PP
The
.I format
program should now be running and awaiting your input:
.DS
\fBDisk format/check utility\fP

\fBEnable debugging (1=bse, 2=ecc, 3=bse+ecc)?\fP
.DE
.PP
If you made a mistake loading the program off the TU58 cassette
or using the bootstrap monitor loaded for the 8600
the ``='' prompt should reappear and you can retype the program
name.  If something else happened, you may have a bad distribution
cassette or floppy, or your hardware may be broken; refer to
Appendix C for help in troubleshooting.  If you are unable to
load programs off the distributed medium,
consult Appendix B for an alternate (more painful) approach.
.PP
.I Format
will create sector headers and verify the integrity of each
sector formatted.  Remember 
.I format
runs only on the 
.B up
and
.B hp
drives listed above. 
.I Format
will prompt for the information required as shown below.
Questions with default answers appear with the default in parentheses
at the prompt; a carriage return will take the default.
If you err in answering questions,
``Delete'' erases the last character typed, and ``^U'' erases
the current input line.
.DS
.ta 2i
\fBEnable debugging (0=none, 1=bse, 2=ecc, 3=bse+ecc)?\fP \fI0\fP
\fBDevice to format?\fP \fIxx(0,0)\fP
 ...(the old bad sector table is read; ignore any errors that occur here)...
\fBFormatting drive \fIxx\fP0 on adaptor 0: verify (yes/no)?\fP \fIyes\fP
\fBDevice data: #cylinders=842, #tracks=20, #sectors=48\fP
\fBStarting cylinder (0):\fP	(hit RETURN to accept the defaults)
\fBStarting track (0):\fP
\fBEnding cylinder (841):\fP
\fBEnding track (19):\fP
\fBAvailable test patterns are:\fP
.in +1.0i
\fB1 - (f00f) RH750 worst case\fP
\fB2 - (ec6d) media worst case\fP
\fB3 - (a5a5) alternating 1's and 0's\fP
\fB4 - (ffff) Severe burnin (up to 48 passes)\fP
.in -1.0i
\fBPattern (one of the above, other to restart)?\fP \fI2\fP
\fBMaximum number of bit errors to allow for soft ECC (3):\fP
\fBStart formatting...make sure the drive is online\fP
 ...(soft ecc's and other errors are reported as they occur)...
 ...(if 4 write check errors were found, the program terminates like this)...
\fBErrors:\fP
\fBBad sector: 0\fP
\fBWrite check: 4\fP
\fBHard ECC: 0\fP
\fBOther hard: 0\fP
\fBMarked bad: 0\fP
\fBSkipped: 0\fP
\fBTotal of 4 hard errors revectored.\fP
\fBWriting bad sector table at block 524256\fP
 ...(524256 is the block # of the first block in the bad sector table)...
\fBDone\fP
.DE
Once the root device has been formatted,
.I format
will prompt for another disk to format.  Halt the machine by
typing ``control-P'' and ``H'' (the ``H'' is necessary only on
the 780 and 8600, but does not hurt on the other machines).
.DS
\fBEnable debugging (1=bse, 2=ecc, 3=bse+ecc)?\fP\fI^P\fP
\fB>>>\|\fIH\fR
.DE
.PP
It may be necessary to format other drives before constructing
file systems on them; this can be done at a later time with the
steps just performed.
.I Format
can also be used in an extended test mode (pattern 4)
that uses numerous test patterns
in up to 48 passes to detect as many disk surface errors as possible;
this test may be run for many hours, depending on the CPU and controller.
On an 11/780, this can be sped up significantly by
setting the clock fast.
It may be run for some number of passes, then either terminated or continued
according to the errors found to that point.
.NH 3
Step 2: copying the mini-root file system
.PP
The second step is to run a simple program,
\fIcopy\fP, which copies a small root
file system into the second partition of the disk.
This file system will serve as the base for creating the actual root
file system to be restored.  The version of the operating
system maintained on the ``mini-root'' file system understands
that it should not swap on top of itself, thereby allowing double use
of the disk partition.  \fICopy\fP is loaded just as the
\fIformat\fP program was loaded; for example, on an 8600 or 8200,
one must enter the toggle and the bootstrap monitor
as described in Appendix B and then:
.DS
.TS
lw(2i) l.
(copy mini root file system)
\fB=\fP\|\fIcopy\fP
\fBFrom:\fP \fIyy(y,1)\fP	(unit \fIy\fP, second tape file)
\fBTo:\fP \fIxx(x,1)\fP	(mini root is on drive \fIx\fP; second partition)
\fBCopy completed: 205 records copied\fP
\fBFrom:\fP
.TE
.DE
while for an 11/780:
.DS
.TS
lw(2i) l.
(copy mini root file system)
\fB>>>\fP\|\fILOAD COPY\fP
\fB>>>\fP\|\fISTART 2\fP
\fBFrom:\fP \fIyy(y,1)\fP	(unit \fIy\fP, second tape file)
\fBTo:\fP \fIxx(x,1)\fP	(mini root is on drive \fIx\fP; second partition)
\fBCopy completed: 205 records copied\fP
\fBFrom:\fP
.TE
.DE
or for an 11/750:
.DS
.TS
lw(2i) l.
(copy mini root file system)
\fB>>>\fP\|\fIB DDA0\fP
\fB=\fP\|\fIcopy\fP
\fBFrom:\fP \fIyy(y,1)\fP	(unit \fIy\fP, second tape file)
\fBTo:\fP \fIxx(x,1)\fP	(mini root is on drive \fIx\fP; second partition)
\fBCopy completed: 205 records copied\fP
\fBFrom:\fP
.TE
.DE
and for an 11/730:
.DS
.TS
lw(2i) l.
(copy mini root file system)
\fB>>>\fP\|\fIL DD0:COPY\fP
\fB>>>\fP\|\fIS 2\fP
\fBFrom:\fP \fIyy(y,1)\fP	(unit \fIy\fP, second tape file)
\fBTo:\fP \fIxx(x,1)\fP	(mini root is on drive \fIx\fP; second partition)
\fBCopy completed: 205 records copied\fP
\fBFrom:\fP
.TE
(As above, `delete' erases characters and `^U' erases lines.)
.DE
.NH 3
Step 3: booting from the mini-root file system
.PP
You now have the minimal set of tools necessary to create a
root file system and restore the file system contents from tape.
To access this file system load the bootstrap program
and boot the version of unix that has been placed in the
``mini-root'':
.DS
(follow the procedure in Appendix B to load the bootstrap monitor)
.TS
lw(2i) l.
(load bootstrap program)
\fB=\fP\|\fIboot\fP
\fBBoot\fP
\fB:\fP \fIxx(x,1)vmunix\fP	(bring in \fIvmunix\fP off mini root)
.TE
.DE
or, on an 11/780:
.DS
.TS
lw(2i) l.
(load bootstrap program)
\fB>>>\fP\|\fIBOOT ANY\fP
\fBBoot\fP
\fB:\fP \fIxx(x,1)vmunix\fP	(bring in \fIvmunix\fP off mini root)
.TE
.DE
or, on an 11/750:
.DS
.TS
lw(2i) l.
(load bootstrap program)
\fB>>>\fP\|\fIB DDA0\fP
\fB=\fP\|\fIboot\fP
\fBBoot\fP
\fB:\fP \fIxx(x,1)vmunix\fP	(bring in \fIvmunix\fP off mini root)
.TE
.DE
or, on an 11/730:
.DS
.TS
lw(2i) l.
(load bootstrap program)
\fB>>>\fP\|\fIL DD0:BOOT\fP
\fB>>>\fP\|\fID RB 3\fP
\fB>>>\fP\|\fIS 2\fP
\fBBoot\fP
\fB:\fP \fIxx(x,1)vmunix\fP	(bring in \fIvmunix\fP off mini root)
.TE
(As above, `delete' erases characters and `^U' erases lines.)
.DE
.LP
The standalone boot program should then read the system from
the mini root file system you just created, and the system should boot:
.DS
.B
271944+78848+92812 start 0x12e8
4.3 BSD UNIX #1: Wed Apr  9 23:33:59 PST 1988
    karels@monet.berkeley.edu:/usr/src/sys/GENERIC
real mem  = \fIxxx\fP
avail mem = \fIyyy\fP
\fI\&... information about available devices ...\fP
root device? 
.R
.DE
.PP
The first three numbers are printed out by the bootstrap
programs and are the sizes of different
parts of the system (text, initialized and uninitialized data).  The
system also allocates several system data structures after it starts
running.  The sizes of these structures are based on the amount of available
memory and the maximum count of active users expected, as declared in a system
configuration description.  This will be discussed later.
.PP
UNIX itself then runs for the first time and begins by printing out a banner
identifying the release and
version of the system that is in use and the date that it was compiled.  
.PP
Next the
.I mem
messages give the
amount of real (physical) memory and the
memory available to user programs
in bytes.
For example, if your machine has 16 megabytes of memory,
\fIxxx\fP will be 16777216.
.PP
The messages that come out next show what devices were found on
the current processor.  These messages are described in
\fIautoconf\fP\|(4).
The distributed system may not have
found all the communications devices you have (dh's, dz's, etc.),
or all the mass storage peripherals you have especially
if you have more than
two of anything.  You will correct this soon, when you create
a description of your machine from which to configure UNIX.
The messages printed at boot here contain much of the information
that will be used in creating the configuration.
In a correctly configured system most of the information
present in the configuration description
is printed out at boot time as the system verifies that each device
is present.
.PP
The \*(lqroot device?\*(rq prompt was printed by the system 
and is now asking you for the name of the root file system to use.
This happens because the distribution system is a \fIgeneric\fP
system.  It can be bootstrapped on any VAX cpu and with its root device
and paging area on any available disk drive.  You should respond
to the root device question with \fIxx0*\fP.  This response
supplies two pieces of information:
first, \fIxx0\fP shows that the disk it is running on is drive
0 of type \fIxx\fP, secondly the \*(lq*\*(rq shows that the system is
running \*(lqatop\*(rq the paging area.  The latter is most important,
otherwise the system will attempt to page on top of itself and
chaos will ensue.
You will later build a system tailored to your configuration that
will not ask this question when it is bootstrapped.
.DS
\fBroot device?\fP \fIxx0*\fP
WARNING: preposterous time in file system \-\- CHECK AND RESET THE DATE!
\fBerase ^?, kill ^U, intr ^C\fP
\fB#\fP
.DE
.PP
The \*(lqerase ...\*(rq message is part of /.profile
that was executed by the root shell when it started.  This message
is present to remind you that the line character erase,
line erase, and interrupt characters are set to be what
is standard on DEC systems; this insures that things are
consistent with the DEC console interface characters.
.NH 3
Step 4: restoring the root file system
.PP
UNIX is now running,
and the `UNIX Programmer's manual' applies.
The `#' is the prompt from the shell,
and lets you know that you are the super-user,
whose login name is \*(lqroot\*(rq.  To complete installation
of the bootstrap system two steps remain.  First, the root
file system must be created, and second a boot floppy or
cassette must be constructed.
.PP
To create the root file system the shell script \*(lqxtr\*(rq
should be run as follows:
.DS
\fB# \fIdisk=xx0  type=tt  tape=yy  xtr\fR
.DE
where \fIxx0\fP is the name of the disk on which the root
file system is to be restored (unit 0), \fItt\fP is the type of
drive on which the root file system is to be restored
(see the table below),
and \fIyy\fP is the name
of the tape drive on which the distribution tape is mounted.
.PP
If the root file system is to reside on a disk other than
unit 0 (as the information printed out
during autoconfiguration shows), you will
have to create the necessary special files in /dev and use
the appropriate value. For example, if the root should be
placed on hp1, you must create /dev/rhp1a and /dev/hp1a using 
the MAKEDEV script in /dev as follows:
.DS
\fB# \fIcd /dev; MAKEDEV hp1\fR
.DE
The following table lists the various drive \fItype\fPs.
.DS
.TS
l l | l l.
Drive	Type	Drive	Type
_
DEC RM03	type=rm03	DEC RM05	type=rm05
DEC RM80	type=rm80	DEC RP06	type=rp06
DEC RP07	type=rp07	DEC RK07	type=rk07
DEC RA80	type=ra80	DEC RA60	type=ra60
DEC RA81	type=ra81	DEC R80	type=rb80
DEC RA70	type=ra70	DEC RA82	type=ra82
DEC RD53	type=rd53	DEC RD54	type=rd54
CDC 9766	type=9766	CDC 9775	type=9775
AMPEX 300M	type=9300	AMPEX 330M	type=capricorn
FUJITSU 160M	type=fuji160	FUJITSU 330M	type=capricorn
FUJITSU 404M	type=eagle
.TE
.DE
This will generate many messages regarding the construction
of the file system and the restoration of the tape contents,
but should eventually stop with the messages:
.DS
 ...
\fBRoot filesystem extracted\fP

\fBIf this is an 8600, update the console RL02\fP
\fBIf this is an 8200, update the floppy\fP
\fBIf this is a 780, update the floppy\fP
\fBIf this is a 730, update the cassette\fP
\fB#\fP
.DE
.NH 3
Step 5: creating a boot floppy or cassette
.PP
If the machine is an 8600, 8200, 11/780 or 11/730, a boot floppy,
cassette, or console RL02 should be constructed according to the
instructions in chapter 4.  For 11/750's, bootstrapping is performed by
using a boot prom and special code located in sectors 0-15 of the
root file system.  The 
.I disklabel
program installs the needed code.
.\" XXX needs thought:
Locate the disk name and type from the table in step 7, then
run the following command:
.DS
\fB#\fP \fIdisklabel -rw ${disk}0 $type "optional_pack_name"
.DE
On an 11/780 with old-style (MS780C) interleaved memory, or other
configurations that
require alteration of the standard boot files, this step may
be left for later.
.NH 3
Step 6: rebooting the completed root file system
.PP
With the above work completed, all that is left is to reboot:
.DS
.ta 3.5i
\fB#\|\fIsync\fR	(synchronize file system state)
\fB#\|\fI^P\fR	(halt machine)
\fB>>>\|\fIHALT\fR	(for 11/780's)
\fB>>>\|\fIUNJAM\fR	(for 8600's or 11/780's only)
\fB>>>\|\fII\fR	(initialize processor state)
\fB>>>\|\fIB xxS\fR	(on an 11/750, use \fIB/2\fP; see below for 8200)
\&...(boot program is eventually loaded)...
\fBBoot\fP
\fB: \fIxx(x,0)vmunix\fR	(\fIvmunix\fP brought in off root)
\fB271944+78848+92812 start 0x12e8
\fB4.3 BSD UNIX #1: Wed Apr  9 23:33:59 PST 1988
\fB    karels@monet.berkeley.edu:/usr/src/sys/GENERIC
\fBreal mem  = \fIxxx\fR
\fBavail mem = \fIyyy\fR
\fI\&... information about available devices ...\fP
\fBroot on xx0\fP
WARNING: preposterous time in file system \-\- CHECK AND RESET THE DATE!
\fBerase ^?, kill ^U, intr ^C\fP
\fB#\fP

.DE
.PP
On an 8200,
or if the root device selected by the kernel is not correct,
it is necessary to boot using the option to ask for the root
device.
On the 8200, use \fIB/R5:800\fP followed by \fI@ANYBOO.CMD\fP;
on the 11/750, use \fIB/3\fP; on the other processors, use \fIBOOT ANY\fP.
At the prompt from the bootstrap, use the same device specification
above: \fIxx(x,0)vmunix\fP.
Then, to the question ``root device?,''
respond with \fIxx\fP0.
See section 6.1 and appendix C if the system does not reboot properly.
.PP
The system is now running single user on the installed
root file system.  The next section tells how to complete
the installation of distributed software on the /usr file system.
.NH 3
Step 7: placing labels on the disks
.PP
First set up shell variables, so that
the commands we give will work regardless of the disk you have.
You might wish to review the disk configuration information in section 4.3
before continuing; the partitions used below are those most appropriate
in size.
Find the disk you have in the following table and execute
the commands in the right hand portion of the table:
.DS
.TS
l l.
DEC RM03	\fB#\fI disk=hp; name=hp0g; type=rm03\fR
DEC RM05	\fB#\fI disk=hp; name=hp0g; type=rm05\fR
DEC RM80	\fB#\fI disk=hp; name=hp0g; type=rm80\fR
DEC RP06	\fB#\fI disk=hp; name=hp0g; type=rp06\fR
DEC RP07	\fB#\fI disk=hp; name=hp0h; type=rp07\fR
DEC RK07	\fB#\fI disk=hk; name=hk0g; type=rk07\fR
DEC RA60	\fB#\fI disk=ra; name=ra0h; type=ra60\fR
DEC RA70	\fB#\fI disk=ra; name=ra0h; type=ra70\fR
DEC RA80	\fB#\fI disk=ra; name=ra0h; type=ra80\fR
DEC RA81	\fB#\fI disk=ra; name=ra0h; type=ra81\fR
DEC RA82	\fB#\fI disk=ra; name=ra0h; type=ra82\fR
DEC R80	\fB#\fI disk=rb; name=rb0h; type=rb80\fR
UNIBUS CDC 9766	\fB#\fI name=up0g; type=9766\fR
UNIBUS AMPEX 300M	\fB#\fI disk=up; name=up0g; type=9300\fR
UNIBUS AMPEX 330M	\fB#\fI disk=up; name=up0g; type=capricorn\fR
UNIBUS FUJITSU 160M	\fB#\fI disk=up; name=up0g; type=fuji160\fR
UNIBUS FUJITSU 330M	\fB#\fI disk=up; name=up0g; type=capricorn\fR
UNIBUS FUJITSU 404M	\fB#\fI disk=up; name=up0h; type=eagle\fR
MASSBUS CDC 9766	\fB#\fI disk=up; name=hp0g; type=9766\fR
MASSBUS AMPEX 300M	\fB#\fI disk=up; name=hp0g; type=9300\fR
MASSBUS AMPEX 330M	\fB#\fI disk=up; name=hp0g; type=capricorn\fR
MASSBUS FUJITSU 330M	\fB#\fI disk=up; name=hp0g; type=capricorn\fR
MASSBUS FUJITSU 404M	\fB#\fI disk=up; name=hp0h; type=eagle\fR
.TE
.DE
If you have a DEC RA disk, but it is on a KDB50, insert a `k':
.DS
\fB#\fP \fIdisk=k$disk; name=k$name
.DE
Next find the tape you have in the following table and execute the
commands in the right hand portion of the table:
.DS
.TS
l l.
DEC TE16/TU45/TU77	\fB#\fI cd /dev; MAKEDEV ht0; sync\fR
DEC TU78	\fB#\fI cd /dev; MAKEDEV mt0; sync\fR
DEC TS11	\fB#\fI cd /dev; MAKEDEV ts0; sync\fR
DEC TK50/TK70/TA80/TA81	\fB#\fI cd /dev; MAKEDEV tmscp0; sync\fR
EMULEX TC11	\fB#\fI cd /dev; MAKEDEV tm0; sync\fR
SI 9700	\fB#\fI cd /dev; MAKEDEV ut0; sync\fR
.TE
.DE
.PP
On hp and ra disks (excluding those on the KDB50),
\*(4B uses disk labels in the first sector of each disk to contain
information about the geometry of the drive and the partition layout.
This information is written with \fIdisklabel\fP\|(8).
To label the disk containing the root file system,
run the following command:
.DS
\fB#\fP \fIdisklabel -rw ${disk}0 $type "optional_pack_name"
.DE
This sets up the default partition table.  \fIType\fP can
be any name listed in /etc/disktab; if you want something other
than the default tables, you can edit /etc/disktab and add
a new name: e.g., ``ra81-local.''  Alternatively, you can
use the \fI\-e\fP option to edit the label;
you will have to set the ``EDITOR'' environment variable to /bin/ed:
.DS
\fB#\fP \fIEDITOR=/bin/ed; export EDITOR\fP
.DE
.PP
You should label all your disks as soon as possible, but you
\fImust\fP label the root pack on a VAX-11/750,
even if labels are not supported (e.g., on ``up'' disks),
as this also creates the boot block.
As a general rule, it is always safe to run \fIdisklabel\fP:
if labels are not supported on some disk,
nothing of consequence will happen.
.NH 3
Step 8: setting up the /usr file system
.PP
The next thing to do is to extract the rest of the data from
the tape:
.br
.ne 5
.sp
.DS
.TS
lw(2i) l.
\fB#\fI date \fRyymmddhhmm	(set date, see \fIdate\fP\|(1))
\&....
\fB#\fI passwd root\fR	(set password for super-user)
\fBNew password:\fP	(password will not echo)
\fBRetype new password:\fP
\fB#\fI hostname \fRmysitename	(set your hostname)
\fB#\fI newfs ${name} ${type}\fR	(create empty user file system)
(this takes a few minutes)
\fB#\fI mount /dev/${name} /usr\fR	(mount the usr file system)
\fB#\fI cd /usr\fR	(make /usr the current directory)
\fB#\fI mt fsf\fR
\fB#\fI tar xpbf 20 /dev/rmt12\fR	(extract all of usr except usr/src)
(this takes about 15-20 minutes)
.TE
.DE
If the tape had been rewound or positioned incorrectly before the \fItar\fP,
it may be repositioned by the following commands.
.DS
\fB#\fI mt rew\fR
\fB#\fI mt fsf 3\fR
.DE
The data on the fourth tape file has now been extracted.
If you are using 1600bpi tapes,
the first reel of the distribution is no longer needed;
the remainder of the installation procedure uses the second
reel of tape that should be mounted in place of the first.
The first instruction below is ignored if using 1600bpi tapes.
The installation procedure continues from this point on the 6250bpi tape.
.DS
.TS
lw(2i) l.
\fB#\fI mt fsf\fR	(do not do on 1600bpi tapes)
\fB#\fI mkdir src\fR	(make directory for source)
\fB#\fI mkdir src/sys\fR	(make directory for system source)
\fB#\fI cd src/sys\fR	(make /usr/sys the current directory)
\fB#\fI tar xpbf 20 /dev/rmt12 \fR	(extract the system source)
(this takes about 5-10 minutes)
\fB#\fI cd /\fR	(back to root)
\fB#\fI chmod 755  /  /usr  /usr/src /usr/src/sys\fR
\fB#\fI rm \-f sys\fR
\fB#\fI ln \-s usr/src/sys sys\fR	(make a symbolic link to the system source)
\fB#\fI umount /dev/${name}\fR	(unmount /usr)
.TE
.DE
.PP
You can check the consistency of the /usr file system by doing
.DS
\fB#\fI fsck /dev/r${name}\fR
.DE
The output from
.I fsck
should look something like:
.DS
.B
** /dev/r\fIxx\fP0h
** Last Mounted on /usr
** Phase 1 - Check Blocks and Sizes
** Phase 2 - Check Pathnames
** Phase 3 - Check Connectivity
** Phase 4 - Check Reference Counts
** Phase 5 - Check Cyl groups
671 files, 3497 used, 137067 free (75 frags, 34248 blocks)
.R
.DE
.PP
If there are inconsistencies in the file system, you may be prompted
to apply corrective action; see the document describing
.I fsck
for information.
.PP
To use the /usr file system, you should now remount it by
saying
.DS
\fB#\fI /etc/mount /dev/${name} /usr\fR
.DE
You can then extract the source code for the commands
(except on RK07's and RM03's this will fit in the /usr file system):
.DS
\fB#\fI cd /usr/src\fR
\fB#\fI mt fsf\fR
\fB#\fI tar xpb 20\fR
.DE
If you get an error at this point, most likely it was
a problem with tape positioning.
You can reposition the tape by rewinding it and
then skipping over the files already read (see \fImt\fP\|(1)).
.NH 3
Additional software
.PP
There is one additional tape file on the distribution tape(s)
which has not been installed to this point;
it contains user contributed software in \fItar\fP\|(1) format.
On the 1600bpi tape set, this file is the sole file on the third tape.
It can be installed by positioning the tape 
using \fImt\fP\|(1) and reading
in the files as was done for /usr/src above.
As distributed, the user contributed software should be placed in /usr/src/new.
It may be extracted by mounting the appropriate tape (if not already mounted),
positioning the tape at the beginning of this file (for 6250bpi),
and extracting with
.IR tar :
.DS
\fB#\fP \fIcd /usr/src\fP
\fB#\fP \fImkdir new\fP
\fB#\fP \fIchmod 755 new\fP
\fB#\fP \fIcd new\fP
\fB#\fP \fItar xpb 20\fP
.DE
Several of the directories for large contributed software subsystems
have been placed in a single archive file and compressed to allow
.NH 2
Additional conversion information
.PP
After setting up the new \*(4B filesystems,
you may restore any user files that were saved on tape before beginning
the conversion.
Note that the \*(4B \fIrestore\fP program does
its work on a mounted file system using normal system operations
(unlike the 4.1BSD \fIrestor\fP that accessed the raw file
system device and deposited inodes in the appropriate locations
on disk).  This means that file system dumps may be restored even
if the characteristics of the file system changed.  To restore
a dump tape for, say, the /a file system something like the following
would be used:
.DS
\fB#\fI mkdir /a\fR
\fB#\fI disklabel -rw hp1 eagle\fR
\fB#\fI newfs hp1g\fR
\fB#\fI mount /dev/hp1g /a\fR
\fB#\fI cd /a\fR
\fB#\fI restore r\fR
.DE
If you chose to convert 4.1BSD filesystems while copying to a new disk area,
do so by piping the output of \fIdump.4.1\fP directly into \fIrestore\fP 
after bringing up \*(4B.
.PP
If \fItar\fP images were written instead of doing a dump, you should
be sure to use the `p' option when reading the files back.
No matter how you restore a file system, be sure and check its
integrity with \fIfsck\fP when the job is complete.
.PP
To convert a compiler from 4.1BSD
to \*(4B you should simply have to recompile and relink the
various parts.  If the processor is written in itself, for instance
a P\s-2ASCAL\s0 compiler written in P\s-2ASCAL\s0, the important step in
converting is to save a working copy of the 4.1BSD binary before
converting to \*(4B.  Then, once the system has been changed over,
the 4.1BSD binary should be used in the rebuilding process. 
To do this, you should enable the 4.1 compatibility
option when you configure the kernel (see section 4.3).
.PP
If no working 4.1BSD binary exists, or the language processor
uses some nonstandard system call, you will likely have to compile
the language processor into an intermediate form, such as assembly
language, on a 4.1BSD system, then bring the intermediate form
to \*(4B for assembly and loading.
