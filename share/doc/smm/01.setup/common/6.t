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
.\"	@(#)6.t	6.5 (Berkeley) 4/17/91
.\"
.de IR
\fI\\$1\fP\|\\$2
..
.ds LH "Installing/Operating \*(4B
.\".nr H1 6
.\".nr H2 0
.ds RH "System Operation
.ds CF \*(DY
.bp
.LG
.B
.ce
.NH 1
6. SYSTEM OPERATION
.sp 2
.R
.NL
.PP
This section describes procedures used to operate a \*(4B UNIX system.
Procedures described here are used periodically, to reboot the system,
analyze error messages from devices, do disk backups, monitor
system performance, recompile system software and control local changes.
.NH 2
Bootstrap and shutdown procedures
.PP
In a normal reboot, the system checks the disks and comes up multi-user
without intervention at the console.
Such a reboot
can be stopped (after it prints the date) with a ^C (interrupt).
This will leave the system in single-user mode, with only the console
terminal active.
It is also possible to allow the filesystem checks to complete
and then to return to single-user mode by signaling \fIfsck\fP(8)
with a QUIT signal (^\\).
.if \n(Th \{\
.PP
If booting from the console command level is needed, then the command
.DS
\fB#>\fP\|fb
.DE
will boot from the default device.
.PP
You can boot a system up single user by doing
.DS
\fB#>\fP\fI\|p23 2.\fP\fB#>\fP\fIy.\fP\fB#>\fP\fI\|fb\fP
.DE
.PP
Other possibilities are:
.DS
\fB#>\fP\fI\|p23 3.\fP\fB#>\fP\fIy.\fP\fB#>\fP\fI\|fb\fP
.DE
to do a full bootstrap, or
.DS
\fB#>\fP\fI\|p23 3.\fP\fB#>\fP\fIy.\fP\fB#>\fP\fI\|fr /boot\fP
.DE
to run the bootstrap without performing self-tests and
reloading microcode; it can be used after a full bootstrap has been done
once.
.\}
.if \n(Vx \{\
.PP
If booting from the console command level is needed, then the command
.DS
\fB>>>\fP\fIB\fP
.DE
will boot from the default device.
On an 8600, 8200, 11/780, or 11/730 the default device is
determined by a ``DEPOSIT''
command stored on the console boot device in the file ``DEFBOO.CMD''
(``DEFBOO.COM'' on an 8600);
on an 11/750 the default device is determined by the setting of a switch
on the front panel.
.PP
You can boot a system up single user
on an 8600, 780, or 730 by doing
.DS
\fB>>>\fP\fIB xxS\fP
.DE
where \fIxx\fP is one of HP, HK, UP, RA, or RB.
The corresponding command on an 11/750 is
.DS
\fB>>>\fP\fIB/2\fP
.DE
On an 8200, use
.DS
\fB>>>\fP\fIB/R5:800\fP
(node and memory test values)
\fBBOOT58>\fP \fI@\fPXX\fISBOO.CMD\fP
.DE
.PP
For second vendor storage modules on the
UNIBUS or MASSBUS of an 11/750 you will need to
have a boot prom.  Most vendors will sell you
such proms for their controllers; contact your vendor
if you don't have one.
.PP
Other possibilities are:
.DS
\fB>>>\fP\fIB ANY\fP
.DE
or, on an 8200,
.DS
\fB>>>\fP\fIB/R5:800\fP
\fBBOOT58>\fP\fI@ANYBOO.CMD\fP
.DE
or, on an 11/750
.DS
\fB>>>\fP\fIB/3\fP
.DE
.\}
These commands boot and ask for the name of the system to be booted.
They can be used after building a new test system to give the
boot program the name of the test version of the system.*
.FS
* Additional bootflags are used when a system is configured with
the kernel debugger; consult \fIkdb\fP(4) for details.
.FE
.PP
To bring the system up to a multi-user configuration from the single-user
status,
all you have to do is hit ^D on the console.  The system
will then execute /etc/rc,
a multi-user restart script (and /etc/rc.local),
and come up on the terminals listed as
active in the file /etc/ttys.
See
\fIinit\fP\|(8)
and
\fIttys\fP\|(5) for more details.
Note, however, that this does not cause a file system check to be performed.
Unless the system was taken down cleanly, you should run
``fsck \-p'' or force a reboot with
\fIreboot\fP\|(8)
to have the disks checked.
.PP
To take the system down to a single user state you can use
.DS
\fB#\fP \fIkill 1\fP
.DE
or use the
\fIshutdown\fP\|(8)
command (which is much more polite, if there are other users logged in)
when you are running multi-user.
Either command will kill all processes and give you a shell on the console,
as if you had just booted.  File systems remain mounted after the
system is taken single-user.  If you wish to come up multi-user again, you
should do this by:
.DS
\fB#\fP \fIcd /\fP
\fB#\fP \fI/etc/umount -a\fP
\fB#\fP \fI^D\fP
.DE
.PP
Each system shutdown, crash, processor halt and reboot
is recorded in the system log
with its cause.
.NH 2
Device errors and diagnostics
.PP
When serious errors occur on peripherals or in the system, the system
prints a warning diagnostic on the console.
These messages are collected
by the system error logging process
.IR syslogd (8)
and written into a system error log file
\fI/usr/adm/messages\fP.
Less serious errors are sent directly to \fIsyslogd\fP,
which may log them on the console.
The error priorities that are logged and the locations to which they are logged
are controlled by \fI/etc/syslog.conf\fP.  See
.IR syslogd (8)
for further details.
.PP
Error messages printed by the devices in the system are described with the
drivers for the devices in section 4 of the programmer's manual.
If errors occur suggesting hardware problems, you should contact
your hardware support group or field service.  It is a good idea to
examine the error log file regularly
(e.g. with the command \fItail \-r /usr/adm/messages\fP).
.NH 2
File system checks, backups and disaster recovery
.PP
Periodically (say every week or so in the absence of any problems)
and always (usually automatically) after a crash,
all the file systems should be checked for consistency
by
\fIfsck\fP\|(1).
The procedures of
\fIreboot\fP\|(8)
should be used to get the system to a state where a file system
check can be performed manually or automatically.
.PP
Dumping of the file systems should be done regularly,
since once the system is going it is easy to
become complacent.
Complete and incremental dumps are easily done with
\fIdump\fP\|(8).
You should arrange to do a towers-of-hanoi dump sequence; we tune
ours so that almost all files are dumped on two tapes and kept for at
least a week in most every case.  We take full dumps every month (and keep
these indefinitely).
Operators can execute ``dump w'' at login that will tell them what needs
to be dumped
(based on the /etc/fstab
information).
Be sure to create a group
.B operator
in the file /etc/group
so that dump can notify logged-in operators when it needs help.
.PP
More precisely, we have three sets of dump tapes: 10 daily tapes,
5 weekly sets of 2 tapes, and fresh sets of three tapes monthly.
We do daily dumps circularly on the daily tapes with sequence
`3 2 5 4 7 6 9 8 9 9 9 ...'.
Each weekly is a level 1 and the daily dump sequence level
restarts after each weekly dump.
Full dumps are level 0 and the daily sequence restarts after each full dump
also.
.PP
Thus a typical dump sequence would be:
.br
.ne 6
.KS
.TS
center;
c c c c c
n n n l l.
tape name	level number	date	opr	size
_
FULL	0	Nov 24, 1979	jkf	137K
D1	3	Nov 28, 1979	jkf	29K
D2	2	Nov 29, 1979	rrh	34K
D3	5	Nov 30, 1979	rrh	19K
D4	4	Dec 1, 1979	rrh	22K
W1	1	Dec 2, 1979	etc	40K
D5	3	Dec 4, 1979	rrh	15K
D6	2	Dec 5, 1979	jkf	25K
D7	5	Dec 6, 1979	jkf	15K
D8	4	Dec 7, 1979	rrh	19K
W2	1	Dec 9, 1979	etc	118K
D9	3	Dec 11, 1979	rrh	15K
D10	2	Dec 12, 1979	rrh	26K
D1	5	Dec 15, 1979	rrh	14K
W3	1	Dec 17, 1979	etc	71K
D2	3	Dec 18, 1979	etc	13K
FULL	0	Dec 22, 1979	etc	135K
.TE
.KE
We do weekly dumps often enough that daily dumps always fit on one tape.
.PP
Dumping of files by name is best done by
\fItar\fP\|(1)
but the amount of data that can be moved in this way is limited
to a single tape.
Finally if there are enough drives entire
disks can be copied with
\fIdd\fP\|(1)
using the raw special files and an appropriate
blocking factor; the number of sectors per track is usually
a good value to use, consult \fI/etc/disktab\fP.
.PP
It is desirable that full dumps of the root file system be
made regularly.
This is especially true when only one disk is available.
Then, if the
root file system is damaged by a hardware or software failure, you
can rebuild a workable disk doing a restore in the
same way that the initial root file system was created.
.PP
Exhaustion of user-file space is certain to occur
now and then; disk quotas may be imposed, or if you
prefer a less fascist approach, try using the programs
\fIdu\fP\|(1),
\fIdf\fP\|(1), and
\fIquot\fP\|(8),
combined with threatening
messages of the day, and personal letters.
.NH 2
Moving file system data
.PP
If you have the resources,
the best way to move a file system
is to dump it to a spare disk partition, or magtape, using
\fIdump\fP\|(8), use \fInewfs\fP\|(8) to create the new file system,
and restore the file system using \fIrestore\fP\|(8).
Filesystems may also be moved by piping the output of \fIdump\fP
to \fIrestore\fP.
The \fIrestore\fP program uses an ``in-place'' algorithm that
allows file system dumps to be restored without concern for the
original size of the file system.  Further, portions of a
file system may be selectively restored using a method similar
to the tape archive program.
.PP
If you have to merge a file system into another, existing one,
the best bet is to
use \fItar\fP\|(1).
If you must shrink a file system, the best bet is to dump
the original and restore it onto the new file system.
If you
are playing with the root file system and only have one drive,
the procedure is more complicated.
If the only drive is a Winchester disk, this procedure may not be used
without overwriting the existing root or another partition.
What you do is the following:
.IP 1.
GET A SECOND PACK, OR USE ANOTHER DISK DRIVE!!!!
.IP 2.
Dump the root file system to tape using
\fIdump\fP\|(8).
.IP 3.
Bring the system down.
.IP 4.
Mount the new pack in the correct disk drive, if
using removable media.
.IP 5.
Load the distribution tape and install the new
root file system as you did when first installing the system.
Boot normally
using the newly created disk file system.
.PP
Note that if you change the disk partition tables or add new disk
drivers they should also be added to the standalone system in
\fI/sys/\*(mCstand\fP,
and the default disk partition tables in \fI/etc/disktab\fP
should be modified.
.NH 2
Monitoring System Performance
.PP
The
.I systat
program provided with the system is designed to be an aid to monitoring
systemwide activity.  The default ``pigs'' mode shows a dynamic ``ps''.
By running in the ``vmstat'' mode
when the system is active you can judge the system activity in several
dimensions: job distribution, virtual memory load, paging and swapping
activity, device interrupts, and disk and cpu utilization.
Ideally, there should be few blocked (b) jobs,
there should be little paging or swapping activity, there should
be available bandwidth on the disk devices (most single arms peak
out at 20-30 tps in practice), and the user cpu utilization (us) should
be high (above 50%).
.PP
If the system is busy, then the count of active jobs may be large,
and several of these jobs may often be blocked (b).  If the virtual
memory is active, then the paging demon will be running (sr will
be non-zero).  It is healthy for the paging demon to free pages when
the virtual memory gets active; it is triggered by the amount of free
memory dropping below a threshold and increases its pace as free memory
goes to zero.
.PP
If you run in the ``vmstat'' mode
when the system is busy, you can find
imbalances by noting abnormal job distributions.  If many
processes are blocked (b), then the disk subsystem
is overloaded or imbalanced.  If you have several non-dma
devices or open teletype lines that are ``ringing'', or user programs
that are doing high-speed non-buffered input/output, then the system
time may go high (60-70% or higher).
It is often possible to pin down the cause of high system time by
looking to see if there is excessive context switching (cs), interrupt
activity (in) and per-device interrupt counts,
or system call activity (sy).  Cumulatively on one of
our large machines we average about 60-100 context switches and interrupts
per second and about 70-120 system calls per second.
.PP
If the system is heavily loaded, or if you have little memory
for your load (2M is little in most any case), then the system
may be forced to swap.  This is likely to be accompanied by a noticeable
reduction in system performance and pregnant pauses when interactive
jobs such as editors swap out.
If you expect to be in a memory-poor environment
for an extended period you might consider administratively
limiting system load.
.NH 2
Recompiling and reinstalling system software
.PP
It is easy to regenerate the system, and it is a good
idea to try rebuilding pieces of the system to build confidence
in the procedures.
The system consists of two major parts:
the kernel itself (/sys) and the user programs
(/usr/src and subdirectories).
The major part of this is /usr/src.
.PP
The three major libraries are the C library in /usr/src/lib/libc
and the \s-2FORTRAN\s0 libraries /usr/src/usr.lib/libI77 and
/usr/src/usr.lib/libF77.  In each
case the library is remade by changing into the corresponding directory
and doing
.DS
\fB#\fP \fImake\fP
.DE
and then installed by
.DS
\fB#\fP \fImake install\fP
.DE
Similar to the system,
.DS
\fB#\fP \fImake clean\fP
.DE
cleans up.
.PP
The source for all other libraries is kept in subdirectories of
/usr/src/usr.lib; each has a makefile and can be recompiled by the above
recipe.
.PP
If you look at /usr/src/Makefile, you will see that
you can recompile the entire system source with one command.
To recompile a specific program, find
out where the source resides with the \fIwhereis\fP\|(1)
command, then change to that directory and remake it
with the Makefile present in the directory.
For instance, to recompile ``date'', 
all one has to do is
.DS
\fB#\fP \fIwhereis date\fP
\fBdate: /usr/src/bin/date.c /bin/date\fP
\fB#\fP \fIcd /usr/src/bin\fP
\fB#\fP \fImake date\fP
.DE
this will create an unstripped version of the binary of ``date''
in the current directory.  To install the binary image, use the
install command as in
.DS
\fB#\fP \fIinstall \-s date -o bin -g bin -m 755 /bin/date\fP
.DE
The \-s option will insure the installed version of date has
its symbol table stripped.  The install command should be used
instead of mv or cp as it understands how to install programs
even when the program is currently in use.
.PP
If you wish to recompile and install all programs in a particular
target area you can override the default target by doing:
.DS
\fB#\fP \fImake\fP
\fB#\fP \fImake DESTDIR=\fPpathname \fIinstall\fP
.DE
.PP
To regenerate all the system source you can do
.DS
\fB#\fP \fIcd /usr/src\fP
\fB#\fP \fImake clean; make depend; make\fP
.DE
.PP
If you modify the C library, say to change a system call,
and want to rebuild and install everything from scratch you
have to be a little careful.
You must insure that the libraries are installed before the
remainder of the source, otherwise the loaded images will not
contain the new routine from the library.  The following
sequence will accomplish this.
.DS
\fB#\fP \fIcd /usr/src\fP
\fB#\fP \fImake clean\fP
\fB#\fP \fImake depend\fP
\fB#\fP \fImake build\fP
\fB#\fP \fImake installsrc\fP
.DE
The \fImake clean\fP removes any existing binary or object files in the source
trees to insure that everything will be recompiled and reloaded.  The \fImake
depend\fP recreates all of the dependencies.  See \fImkdep\fP(1) for
further details. The \fImake build\fP compiles and installs the libraries
and compilers, then recompiles the libraries and compilers and the remainder
of the sources.  The \fImake installsrc\fP installs all of the commands not
installed as part of the \fImake build\fP.
.if \n(Th \{\
This will take approximately 10
hours on a reasonably configured Tahoe.
.\}
.NH 2
Making local modifications
.PP
Locally written commands that aren't distributed are kept in /usr/src/local
and their binaries are kept in /usr/local.  This allows /usr/bin, /usr/ucb,
and /bin to correspond to the distribution tape (and to the manuals that
people can buy).  People using local commands should be made aware that
they aren't in the base manual.  Manual pages for local commands should be
installed in /usr/src/local/man and installed in /usr/local/man/cat[1-8].
The \fIman\fP(1) command automatically finds manual pages placed in
/usr/local/man/cat[1-8] to facilitate this practice.
.NH 2
Accounting
.PP
UNIX optionally records two kinds of accounting information:
connect time accounting and process resource accounting.  The connect
time accounting information is stored in the file \fI/usr/adm/wtmp\fP, which
is summarized by the program
.IR ac (8).
The process time accounting information is stored in the file
\fI/usr/adm/acct\fP after it is enabled by
.IR accton (8),
and is analyzed and summarized by the program
.IR sa (8).
.PP
If you need to recharge for computing time, you can develop
procedures based on the information provided by these commands.
A convenient way to do this is to give commands to the clock daemon
.I /etc/cron
to be executed every day at a specified time.  This is done by adding
lines to \fI/usr/adm/crontab\fP; see
.IR cron (8)
for details.
.NH 2
Resource control
.PP
Resource control in the current version of UNIX is more
elaborate than in most UNIX systems.  The disk quota
facilities developed at the University of Melbourne have
been incorporated in the system and allow control over the
number of files and amount of disk space each user may use
on each file system.  In addition, the resources consumed
by any single process can be limited by the mechanisms of
\fIsetrlimit\fP\|(2).  As distributed, the latter mechanism
is voluntary, though sites may choose to modify the login
mechanism to impose limits not covered with disk quotas.
.PP
To use the disk quota facilities, the system must be
configured with ``options QUOTA''.  File systems may then
be placed under the quota mechanism by creating a null file
.I quotas
at the root of the file system, running
.IR quotacheck (8),
and modifying \fI/etc/fstab\fP to show that the file system is read-write
with disk quotas (an ``rq'' type field).  The
.IR quotaon (8)
program may then be run to enable quotas.
.PP
Individual quotas are applied by using the quota editor
.IR edquota (8).
Users may view their quotas (but not those of other users) with the
.IR quota (1)
program.  The 
.IR repquota (8)
program may be used to summarize the quotas and current
space usage on a particular file system or file systems.
.PP
Quotas are enforced with 
.I soft
and
.I hard
limits.  When a user first reaches a soft limit on a resource, a
message is generated on his/her terminal.  If the user fails to
lower the resource usage below the soft limit the next time
they log in to the system the
.I login
program will generate a warning about excessive usage.  Should
three login sessions go by with the soft limit breached the
system then treats the soft limit as a
.I hard
limit and disallows any allocations until enough space is
reclaimed to bring the user back below the soft limit.  Hard
limits are enforced strictly resulting in errors when a user
tries to create or write a file.  Each time a hard limit is
exceeded the system will generate a message on the user's 
terminal.
.PP
Consult the auxiliary document, ``Disc Quotas in a UNIX Environment''
and the appropriate manual entries for more information.
.NH 2
Network troubleshooting
.PP
If you have anything more than a trivial network configuration,
from time to time you are bound to run into problems.  Before
blaming the software, first check your network connections.  On
networks such as the Ethernet a
loose cable tap or misplaced power cable can result in severely
deteriorated service.  The \fInetstat\fP\|(1) program may be of
aid in tracking down hardware malfunctions.  In particular, look
at the \fB\-i\fP and \fB\-s\fP options in the manual page.
.PP
Should you believe a communication protocol problem exists,
consult the protocol specifications and attempt to isolate the
problem in a packet trace.  The SO_DEBUG option may be supplied
before establishing a connection on a socket, in which case the
system will trace all traffic and internal actions (such as timers
expiring) in a circular trace buffer.  This buffer may then
be printed out with the \fItrpt\fP\|(8C) program.  Most of the
servers distributed with the system accept a \fB\-d\fP option forcing
all sockets to be created with debugging turned on.  Consult the
appropriate manual pages for more information.
.NH 2
Files that need periodic attention
.PP
We conclude the discussion of system operations by listing
the files that require periodic attention or are system specific:
.de BP
.IP \fB\\$1\fP
.br
..
.TS
center;
lb a.
/etc/fstab	how disk partitions are used
/etc/disktab	default disk partition sizes/labels
/etc/printcap	printer data base
/etc/gettytab	terminal type definitions
/etc/remote	names and phone numbers of remote machines for \fItip\fP(1)
/etc/group	group memberships
/etc/motd	message of the day
/etc/passwd	password file; each account has a line
/etc/rc.local	local system restart script; runs reboot; starts daemons
/etc/inetd.conf	local internet servers
/etc/hosts	host name data base
/etc/networks	network name data base
/etc/services	network services data base
/etc/hosts.equiv	hosts under same administrative control
/etc/syslog.conf	error log configuration for \fIsyslogd\fP\|(8)
/etc/ttys	enables/disables ports
/usr/lib/crontab	commands that are run periodically
/usr/lib/aliases	mail forwarding and distribution groups
/usr/adm/acct	raw process account data
/usr/adm/messages	system error log
/usr/adm/wtmp	login session accounting
.TE
