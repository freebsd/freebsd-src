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
.\"	@(#)2.t	1.8 (Berkeley) 5/7/91
.\"
.ds lq ``
.ds rq ''
.ds LH "Installing/Operating \*(4B
.ds RH Bootstrapping
.ds CF \*(DY
.bp
.\".nr H1 2
.\".nr H2 0
.bp
.LG
.B
.ce
.NH 1
BOOTSTRAP PROCEDURE
.sp 2
.R
.NL
.PP
This section explains the bootstrap procedure that can be used
to get the kernel supplied with this distribution running on your machine.
If you are not currently running 4.2BSD you will
have to do a full bootstrap.
Chapter 3 describes how to upgrade a 4.2BSD system.
An understanding of the operations used in a full bootstrap
is very helpful in performing an upgrade as well.
In either case, it is highly desirable to read and understand
the remainder of this document before proceeding.
.NH 2
Booting from tape
.PP
The tape bootstrap procedure used to create a
working system involves the following major
steps:
.IP 1)
Format a disk pack with the \fIvdformat\fP program, if necessary.
.IP 2)
Copy a ``mini root'' file system from the
tape onto the swap area of the disk.
.IP 3)
Boot the UNIX system on the ``mini root.''
.IP 4)
Restore the full root file system using \fIrestore\fP\|(8).
.IP 5)
Reboot the completed root file system.
.IP 6)
Label the disks with the \fIdisklabel\fP\|(8) program.
.IP 7)
Build and restore the /usr file system from tape
with \fItar\fP\|(1).
.IP 8)
Extract the system and utility files and contributed software
as desired.
.PP
The following sections describe the above steps in detail.  In these
sections references to disk drives are of the form \fIxx\fP\|(\fId\fP,
\fIp\fP) and references to files on tape drives are of the form
\fIxx\fP\|(\fIc\fP,\fId\fP, \fIp\fP)
where \fIxx\fP are device types described in section 1.4,
\fIc\fP is the (optional) controller unit number,
\fId\fP is the drive unit number, and \fIp\fP is a disk partition
or tape file offset numbers as described in section 1.4.
For the sake of simplicity, all disk examples will use the disk type
``dk'' and all tape examples will similarly use ``cy'';
the examples assume drive 0, partition 0.
Commands you
are expected to type are shown in italics, while that information
printed by the system is shown emboldened.
.PP
If you encounter problems while following the instructions in
this part of the document, refer to Appendix B for help in
troubleshooting.
.NH 3
Step 1: formatting the disk
.PP
All disks used with \*(4B should be formatted to insure
the proper handling of physically corrupted disk sectors.
The
.I vdformat
program included in the distribution, or a vendor supplied
formatting program, may be used to format disks if this has not
already been done.  The \fIvdformat\fP program is capable of formatting
any of the disk drives listed in section 1.1, when booting from tape;
when booting from disk, it supports any drive listed in
\fI/etc/disktab\fP.
.PP
To load the \fIvdformat\fP program, perform the following steps.
.DS
.TS
lw(2i) l.
(machine powered up)
\fBMIB POC\fP
\fBType '#' to cancel boot\fP
\fI#\fP	(cancel automatic reboot)
\fBCP [a10.h0]#>\fP\fI\|h\fP	(halt the cpu)
\fB#>\|\fP\fIfd cyp(0,0)\fP	(make cypher default device)
\fB#>\|\fP\fIp23 3.\fP \fB00000000\fP	(set boot flags)
\fB#>\|\fP\fIy.\fP	(initialize the machine)
\fB#>\|\fP\fIfb\fP	(boot machine)
\fBcyp(0,0)/etc/fstab\fP
\fBCP cold boot\fP
\fB4 way interleave set\fP
\fBCPU memory test\fP
\fBECC CPU memory test\fP
\fBcyp(0,0)/.\fP
\fBCPU POC1\fP
\fBcyp(0,0)/poc1\fP
\fBCPU POC2\fP
\fBcyp(0,0)/poc2\fP
\fBFPP POC\fR	(only if floating point processor present)
\fBcyp(0,0)/fppoc\fP
\fBFPP WCS\fR	(only if floating point processor present)
\fBcyp(0,0)/fppwcs\fP
\fBBOOT SYSTEM cyp(0,0)/boot\fP

\fBBoot\fP
\fB:\fIcy(0,0)stand/vdformat\fR	(load and run from first tape file)
\fB52224+17408+1177716 start 0x1000\fP
\fBVDFORMAT     Berkeley Version 1.6\fP
.TE

\fBcontroller 0: smd\fP
\fBcontroller 1: smd-e\fP

\fBType `Help' for help, `Start' to execute operations.\fP

\fBvdformat>\fP
.DE
.PP
The \fIvdformat\fP program should now be running and awaiting your input.
If you made a mistake loading the program off the tape
you should get either the ``:'' prompt again (from the
boot program) or the ``#>'' prompt from the console
processor.  In either case you can retype the appropriate
command to try again.
If something else happened, you may have a bad distribution
tape, or your hardware may be broken; refer to
Appendix B for help in troubleshooting.
.PP
\fIVdformat\fP will create sector headers and verify
the integrity of each sector formatted.  
The program starts up by identifying the disk controllers
installed in the machine.  Old VDDC controllers which 
support only SMD drives are indicated
as ``smd'' while newer controllers capable of supporting both
SMD and extended-SMD drives are tagged as ``smd-e''. 
\fIVdformat\fP
will prompt for the information required as shown below.
If you err in answering questions,
``Delete'' or backspace erase the last character typed, and ``^U'' erases
the current input line.  At any point you can ask for
assistance by typing ``help''; \fIvdformat\fP will list
the possible answers to the current question.
.DS
\fBvdformat>\fP\|\fIformat\fP
  \fBFormat on which controllers?\fP\|\fI1\fP
    \fBDrives on controller 1?\fP\|\fI0\fP
      \fBNumber of patterns to use while verifying?\fP\|\fI1\fP
      \fBDrive type for controller 1, drive 0?\fP\|\fIegl\fP
        \fBModule serial number for controller 1, drive 0?\fP\|\fI1\fP
\fBvdformat>\fP\|\fIlist\fP
  \fBThe following operations will occur when Start is issued:\fP
    \fBFormat: Controller 1, drive 0, type EGL.\fP
\fBvdformat>\fP\|\fIstart\fP
\fBStarting format on controller 1, drive 0, type EGL.\fP
(\fIbad sectors will be indicated\fP)
\fBvdformat>\fP
.DE
Once the root device has been formatted, \fIvdformat\fP
will prompt for another command.
Return to the bootstrap by typing
.DS
\fBvdformat>\fP\|\fIexit\fP
.DE
or halt the machine by
typing ``~h''.
.DS
\fBvdformat>\fP \fI~h\fP
\fB#>\|\fP
.DE
.PP
It may be necessary to format other drives before constructing
file systems on them; this can be done at a later time with the
steps just performed, or \fIvdformat\fP may be brought in
off a disk drive as described in \(sc6.1.
.NH 3
Step 2: copying the mini-root file system
.PP
The second step is to run a simple program, \fIcopy\fP, to copy a
small root file system into the \fBsecond\fP partition of the disk.  (Note
that the disk partitions used by \*(4B may not correspond to those
used by vendor supplied software.)  This file system will serve as the
base for creating the actual root file system to be restored.  The
generic version of the operating system maintained on the ``mini-root''
file system understands that it should not swap on top of itself, thereby
allowing double use of the disk partition.  Disk 0 is normally used for
this operation; this is reflected in the example procedure.  Another disk
may be substituted if necessary, although several modifications will
be necessary to create special files for the alternate disk.  \fICopy\fP
is loaded just as the \fIvdformat\fP program was loaded; if you don't
have the bootstrap running, repeat the previous instructions until you
see the prompt from boot (a colon), and then:
.DS
.TS
lw(2i) l.
\fB:\|\fP\fIcy(0,0)copy\fP	(load and run copy program)
\fBFrom:\fP \fIcy(0,1)\fP	(tape drive unit 0, second tape file)
\fBTo:\fP \fIdk(0,1)\fP	(disk drive unit 0, second disk partition)
\fBCopy completed: 205 records copied\fP
\fBBoot\fP
\fB:\fP
.TE
.DE
As before, `delete' or backspace erase characters and `^U' erases lines.
.NH 3
Step 3: booting from the mini-root file system
.PP
You now have the minimal set of tools necessary to create a
root file system and restore the file system contents from tape.
To access this file system load the bootstrap program
and boot the version of unix that has been placed in the
``mini-root.''
As before, load the bootstrap if you do not already have
it running.  At the colon prompt:
.DS
.TS
lw(2i) l.
\fB: \fP\fIdk(0,1)vmunix\fP	(get \fIvmunix\fP from disk drive 0, second partition)
.TE
.DE
The standalone boot program should then read the system from
the mini root file system you just created, and the system should boot:
.DS
.B
271944+78848+92812 start 0x12e8
4.3 BSD #1: Sat Jun  4 17:11:42 PDT 1988
	(karels@okeeffe.Berkeley.EDU:/usr/src/sys/GENERIC)
real mem  = xxx
avail mem = ###
using ### buffers containing ### bytes of memory
(... information about available devices ...)
root device? 
.R
.DE
.PP
The first three numbers are printed out by the bootstrap programs and
are the sizes of different parts of the system (text, initialized and
uninitialized data).  The system also allocates several system data
structures after it starts running.  The sizes of these structures are
based on the amount of available memory and the maximum count of active
users expected, as declared in a system configuration description.  This
will be discussed later.
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
For example, if your machine has 16Mb bytes of memory, then
\fBxxx\fP will be 16777216.
.PP
The messages that come out next show what devices were found on
the current processor.  These messages are described in
\fIautoconf\fP\|(4).
The distributed system may not have
found all the communications devices you have (VIOC's or MPCC's),
or all the mass storage peripherals you have, especially
if you have more than
two of anything.  You will correct this when you create
a description of your machine from which to configure a site-dependent
version of UNIX.
The messages printed at boot here contain much of the information
that will be used in creating the configuration.
In a correctly configured system most of the information
present in the configuration description
is printed out at boot time as the system verifies that each device
is present.
.PP
The \*(lqroot device?\*(rq prompt was printed by the system 
to ask you for the name of the root file system to use.
This happens because the distribution system is a \fIgeneric\fP
system, i.e. it can be bootstrapped on a Tahoe cpu with its root device
and paging area on any available disk drive.  You should respond to the
root device question with ``dk0*''.  This response supplies two pieces
of information: first, ``dk0'' shows that the disk it is running on is
drive 0 of type ``dk'', and, secondly, the \*(lq*\*(rq shows that the
system is running \*(lqatop\*(rq the paging area.  The latter is
extremely important, otherwise the system will attempt to page on top
of itself and chaos will ensue.  You will later build a system tailored
to your configuration that will not ask this question when it is
bootstrapped.
.DS
\fBroot device?\fP \fIdk0*\fP
WARNING: preposterous time in file system \-\- CHECK AND RESET THE DATE!
\fBerase ^?, kill ^U, intr ^C\fP
\fB#\fP
.DE
.PP
The \*(lqerase ...\*(rq message is part of the /.profile
that was executed by the root shell when it started.  This message
is present to inform you as to what values the character erase,
line erase, and interrupt characters have been set.
.NH 3
Step 4: restoring the root file system
.PP
UNIX is now running,
and the \fIUNIX Programmer's manual\fP applies.  The ``#'' is the prompt
from the Bourne shell, and lets you know that you are the super-user,
whose login name is \*(lqroot\*(rq.
.PP
To complete installation of the bootstrap system one step remains: the
root file system must be created.  If the root file system is to reside
on a disk other than unit 0, you will have to create the necessary special
files in /dev and use the appropriate value in the following example
procedures.
.PP
For example, if the root must be placed on dk1, you should
create /dev/rdk1a and /dev/dk1a using the MAKEDEV script in /dev
as follows:
.DS
\fB#\fP\|\fIcd /dev; MAKEDEV dk1\fP
.DE
.PP
To actually create the root file system the shell script \*(lqxtr\*(rq
should be run:
.DS
\fB#\fP\|\fIdisk=dk0 tape=cy xtr\fP
(Note, ``dk0'' specifies both the disk type and the unit number.  Modify
as necessary.)
.DE
.PP
This will generate many messages regarding the construction
of the file system and the restoration of the tape contents,
but should eventually stop with the message:
.DS
 ...
\fBRoot filesystem extracted\fP
\fB#\fP
.DE
.NH 3
Step 5: rebooting the completed root file system
.PP
With the above work completed, all that is left is to reboot:
.DS
.ta 3.5i
\fB#\|\fP\fIsync\fP	(synchronize file system state)
\fB#\|\fP\fI~h\fP	(halt cpu)
\fB#>\|\fP\fIy.\fP	(initialize machine)
\fB#>\|\fP\fIp23 2.\fP	(set boot flags)
\fB#>\|\fP\fIfr boot\fP
\fB\&...(boot program is eventually loaded)...\fP
\fBBoot\fP
\fB:\fP \fIdk(0,0)vmunix\fP	(\fIvmunix\fP from disk drive 0, partition 0)
(Modify unit number as necessary.)
.B
.nf
271944+78848+92812 start 0x12e8
4.3 BSD #1: Sat Jun  4 17:11:42 PDT 1988
        (karels@okeeffe.Berkeley.EDU:/usr/src/sys/GENERIC)
real mem  = ###
avail mem = ###
using ### buffers containing ### bytes of memory
(... information about available devices ...)
root on dk0
WARNING: preposterous time in file system -- CHECK AND RESET THE DATE!
erase ^?, kill ^U, intr ^C
#
.fi
.DE
.R
.PP
If the root device selected by the kernel is not correct, it is necessary
to reboot again using the option to ask for the root device.  On the Tahoe
use ``\fIp23 3.\fP''.  At the prompt from the bootstrap, use the same
disk driver unit specification as used above: ``\fIdk(0,0)vmunix\fP''.
Then, to the question ``root device?,'' respond with ``\fIdk0\fP''.
See section 6.1 and appendix C if the system does not reboot properly.
.PP
The system is now running single user on the installed root file system.
The next section tells how to complete the installation of distributed
software on the /usr file system.
.NH 3
Step 6: placing labels on the disks
.PP
\*(4B uses disk labels in the first sector of each disk to contain
information about the geometry of the drive and the partition layout.
This information is written with \fIdisklabel\fP\|(8).
Note that recent CCI releases, and apparently Harris releases,
may use a different form of disk label, also in the first sector.
As the formats of these labels are incompatible,
skip this step if your machine is using disk labels already.
Recent firmware for the console processor (CP) may use these labels,
and thus the labels must be retained.
Eventually, it will be possible to use both formats simultaneously.
You may wish to experiment on a spare disk once the system is running.
.PP
For each disk that you wish to label, run the following command:
.DS
\fB#\|\fP\fIdisklabel  -rw  dk\fP\fB#\fP  \fBtype\fP  \fI"optional_pack_name"\fP
.DE
The \fB#\fP is the unit number; the \fBtype\fP is the CCI disk device
name as listed in section 1.4 or any other name listed in /etc/disktab.
The optional information may contain any descriptive name for the
contents of a disk, and may be up to 16 characters long.  This procedure
will place the label on the disk using the information found in /etc/disktab
for the disk type named.  The default disk partitions in \*(4B are the mostly
the same as those in the CCI 1.21 release, except for CDC 340Mb xfd drives;
see section 4.2 for details.  If you have changed the disk partition sizes,
you may wish to add entries for the modified configuration in /etc/disktab
before labeling the affected disks.
.PP
Note that the partition sizes and sectors per track in /etc/disktab
are now specified in sectors, not units of kilobytes as in the vendors'
4.2BSD and System V systems.
For most SMD disks, the sector size is 512 bytes, and is listed explicitly.
ESDI disks on a Power 6/32SX use a sector size of 1024 bytes.
.NH 3
Step 7: setting up the /usr file system
.PP
The next thing to do is to extract the rest of the data from
the tape.
You might wish to review the disk configuration information in section
4.2 before continuing; the partitions used below are those most appropriate
in size.
.PP
For the Cipher tape drive, execute the following commands:
.DS
\fB#\fP \fIcd /dev; MAKEDEV cy0\fP
.DE
Then perform the following:
.br
.ne 5
.sp
.DS
.TS
lw(2i) l.
\fB#\fP \fIdate yymmddhhmm\fP	(set date, see \fIdate\fP\|(1))
\&....
\fB#\fP \fIpasswd root\fP	(set password for super-user)
\fBNew password:\fP	(password will not echo)
\fBRetype new password:\fP
\fB#\fP \fIhostname mysitename\fP	(set your hostname)
\fB#\fP \fInewfs dk#c\fP	(create empty user file system)
(\fIdk\fP is the disk type, \fI#\fP is the unit number, \fIc\fP
is the partition; this takes a few minutes)
\fB#\fP \fImount /dev/dk#c /usr\fP	(mount the usr file system)
\fB#\fP \fIcd /usr\fP	(make /usr the current directory)
\fB#\fP \fImt -t /dev/rmt12 fsf\fP	(space to end of previous tape file)
\fB#\fP \fItar xbpf 40 /dev/rmt12\fP	(extract all of usr except usr/src)
(this takes about 15-20 minutes)
.TE
.DE
If no disk label has been installed on the disk, the \fInewfs\fP
command will require a third argument to specify the disk type,
using one of the names in /etc/disktab.
If the tape had been rewound or positioned incorrectly before the \fItar\fP,
it may be repositioned by the following commands.
.DS
\fB#\fP \fImt -t /dev/rmt12 rew\fP
\fB#\fP \fImt -t /dev/rmt12 fsf 3\fP
.DE
The data on the fourth tape file has now been extracted.
If you are using 1600bpi tapes, the first reel of the
distribution is no longer needed; you should now mount the second
reel instead.  The installation procedure continues from this
point on the 6250bpi tape.
.DS
.TS
lw(2i) l.
\fB#\fP \fImkdir src\fP	(make directory for source)
\fB#\fP \fIcd src\fP	(make source directory the current directory)
\fB#\fP \fImt -t /dev/rmt12 fsf\fP	(space to end of previous tape file)
\fB#\fP \fItar xpbf 40 /dev/rmt12\fP 	(extract the system source)
(this takes about 5-10 minutes)
\fB#\fP \fIcd /\fP	(change directory, back to the root)
\fB#\fP \fIchmod 755  /usr/src\fP
\fB#\fP \fIumount /dev/dk#c\fP	(unmount /usr)
.TE
.DE
.PP
You can check the consistency of the /usr file system by doing
.DS
\fB#\fP \fIfsck /dev/rdk#c\fP
.DE
The output from
.I fsck
should look something like:
.DS
.B
** /dev/rdk#c
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
to apply corrective action; see the \fIfsck\fP(8) or \fIFsck -- The UNIX
File System Check Program\fP for more details.
.PP
To use the /usr file system, you should now remount it with:
.DS
\fB#\fP \fI/etc/mount /dev/dk#c /usr\fP
.DE
.PP
If you are using 1600bpi tapes, the second reel of the
distribution is no longer needed; you should now mount the third
reel instead.  The installation procedure continues from this
point on the 6250bpi tape.
.DS
\fB#\fP \fImkdir /usr/src/sys\fP
\fB#\fP \fIchmod 755 /usr/src/sys\fP
\fB#\fP \fIcd /usr/src/sys\fP
\fB#\fP \fImt -t /dev/rmt12 fsf\fP
\fB#\fP \fItar xpbf 40 /dev/rmt12\fP
.DE
.PP
There is one additional tape file on the distribution tape(s)
which has not been installed to this point; it contains user
contributed software in \fItar\fP\|(1) format.  As distributed,
the user contributed software should be placed in /usr/src/new.
.DS
\fB#\fP \fImkdir /usr/src/new\fP
\fB#\fP \fIchmod 755 /usr/src/new\fP
\fB#\fP \fIcd /usr/src/new\fP
\fB#\fP \fImt -t /dev/rmt12 fsf\fP
\fB#\fP \fItar xpbf 40 /dev/rmt12\fP
.DE
Several of the directories for large contributed software subsystems
have been placed in a single archive file and compressed due to space
constraints within the distribution.
.NH 2
Additional conversion information
.PP
After setting up the new \*(4B filesystems, you may restore the user
files that were saved on tape before beginning the conversion.
Note that the \*(4B \fIrestore\fP program does its work on a mounted
file system using normal system operations.  This means that file
system dumps may be restored even if the characteristics of the file
system changed.  To restore a dump tape for, say, the /a file system
something like the following would be used:
.DS
\fB#\fP \fImkdir /a\fP
\fB#\fP \fInewfs dk#c\fI
\fB#\fP \fImount /dev/dk#c /a\fP
\fB#\fP \fIcd /a\fP
\fB#\fP \fIrestore r\fP
.DE
.PP
If \fItar\fP images were written instead of doing a dump, you should
be sure to use its `-p' option when reading the files back.  No matter
how you restore a file system, be sure to unmount it and and check its
integrity with \fIfsck\fP(8) when the job is complete.




