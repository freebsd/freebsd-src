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
.\"	@(#)3.t	6.8 (Berkeley) 4/17/91
.\"
.ds lq ``
.ds rq ''
.ds RH "Upgrading a 4.2BSD or \*(Ps System
.ds CF \*(DY
.LP
.nr H1 3
.nr H2 0
.bp
.LG
.B
.ce
3. UPGRADING A 4.2BSD OR \*(Ps SYSTEM
.sp 2
.R
.NL
.PP
This section describes the procedure for upgrading a 4.2 or \*(Ps
system to \*(4B.  This procedure may vary according to the version of
the system running before conversion.
If you are upgrading from 4.2BSD,
begin by reading the ``Bugs Fixes and Changes in \*(4B'' document to
see what has changed since the last time you bootstrapped the system.
If you have local system modifications to the
kernel to install, look at the document
``Changes to the Kernel in \*(4B'' to get an idea of how
the system changes will affect your local modifications.
.if \n(Th \{\
If you are converting from a
System V system, some of this section will still apply (in particular,
the filesystem conversion).  However, many of the system configuration
files are different, and the executable file formats are completely
incompatible.
.\}
.PP
If you are running 4.2BSD or \*(Ps, upgrading your system
involves replacing your kernel and system utilities.
Binaries compiled under \*(Ps will work without recompilation
under \*(4B, though they may run faster if they are recompiled.
.if \n(Th \{\
When converting from 4.2BSD, most local programs will have to be recompiled,
as there are a number of incompatibilities between 4.3BSD
and the vendor-supplied 4.2BSD.
.\}
.if \n(Vx \{\
Binaries compiled under 4.2BSD will probably work without recompilation,
but it is a good idea to recompile and relink because of the many changes
in header files and libraries since 4.2BSD.
4.1BSD binary images can also run unchanged under \*(4B
but only when the system is configured to include the
``4.1BSD compatibility mode.''*
.FS
* With ``4.1BSD compatibility mode''
system calls from 4.1BSD are either emulated or safely ignored.
There are only two exceptions: programs that read directories or use
the old jobs library will not operate properly.  However, while 4.1BSD
binaries will execute under \*(4B
it is \fBSTRONGLY RECOMMENDED\fP that the programs be recompiled under
the new system.
.FE
.\}
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
.PP
Section 3.1 lists the files to be saved as part of the conversion process.
Section 3.2 describes the bootstrap process.
Section 3.3 discusses the merger of the saved files back into the new system.
Section 3.4 provides general hints on possible problems to be
aware of when converting from 4.2BSD to \*(4B.
.NH 2
Files to save
.PP
The following list enumerates the standard set of files you will want to
save and suggests directories in which site-specific files should be present.
This list will likely be augmented with non-standard files you
have added to your system.
If you do not have enough space to create parallel
file systems, you should create a \fItar\fP image of the
following files before the new file systems are created.
In addition, it is
\fBSTRONGLY\fP advised that you do a full dump before rebuilding the file
system to guard against missing something the first time around.
.DS
.TS
l c l.
/.cshrc	\(dg	root csh startup script
/.login	\(dg	root csh login script
/.profile	\(dg	root sh startup script
/.rhosts	\(dg	for trusted machines and users
/dev/MAKEDEV	\(dd	in case you added anything here
/dev/MAKEDEV.local	*	for making local devices
/etc/disktab	\(dd	in case you changed disk partition sizes
/etc/fstab	\(dg	disk configuration data
/etc/ftpusers	\(dg	for local additions
/etc/gateways	\(dg	routing daemon database
/etc/gettytab	\(dd	getty database
/etc/group	*	group data base
/etc/hosts	\(dg	for local host information
/etc/hosts.equiv	\(dg	for local host equivalence information
/etc/networks	\(dg	for local network information
/etc/passwd	*	user data base
/etc/printcap	\(dg	line printer database
/etc/protocols	\(dd	in case you added any local protocols
/etc/rc	*	for any local additions
/etc/rc.local	*	site specific system startup commands
/etc/remote	\(dg	auto-dialer configuration
/etc/services	\(dd	for local additions
/etc/syslog.conf	*	system logger configuration
/etc/securettys	*	for restricted list of ttys where root can log in
/etc/ttys	*	terminal line configuration data
/etc/ttytype	*	terminal line to terminal type mapping data
/etc/termcap	\(dd	for any local entries that may have been added
/lib	\(dd	for any locally developed language processors
/usr/dict/*	\(dd	for local additions to words and papers
/usr/hosts/MAKEHOSTS	*	for local changes
/usr/include/*	\(dd	for local additions
/usr/lib/aliases	\(dd	mail forwarding data base
/usr/lib/crontab	*	cron daemon data base
/usr/lib/font/*	\(dd	for locally developed font libraries
/usr/lib/lib*.a	\(dg	for locally libraries
/usr/lib/lint/*	\(dd	for locally developed lint libraries
/usr/lib/sendmail.cf	*	sendmail configuration
/usr/lib/tabset/*	\(dd	for locally developed tab setting files
/usr/lib/term/*	\(dd	for locally developed nroff drive tables
/usr/lib/tmac/*	\(dd	for locally developed troff/nroff macros
/usr/lib/uucp/*	\(dg	for local uucp configuration files
/usr/man/manl	*	for manual pages for locally developed programs
/usr/msgs	\(dg	for current msgs
/usr/spool/*	\(dg	for current mail, news, uucp files, etc.
/usr/src/local	\(dg	for source for locally developed programs
/sys/conf/HOST	\(dg	configuration file for your machine
/sys/conf/files.HOST	\(dg	list of special files in your kernel
/*/quotas	\(dg	file system quota files
.TE
.sp
\(dg\|Files that can be used from 4.2BSD or \*(Ps without change.
\(dd\|Files that need local modifications merged into \*(4B files.
*\|Files that require special work to merge and are discussed
in section 3.3.
.DE
.NH 2
Installing \*(4B
.PP
.if \n(Vx \{\
\fBNote\fP: The \*(4B release contains only Tahoe filesystems and executable
images.
In order to bring up \*(4B on a VAX, it is necessary to extract the sources
on a VAX, compile and install.
Most of the files listed above are found in /usr/src/sys/vaxdist
as well as in their standard locations on the distribution tape
so that the root and /usr images need not be extracted from the tape.
The following sections describe the procedure for installing \*(4B
on the Tahoe.
For a VAX system, the starting root and /usr filesystems can be created
by building and installing executables using alternate filesystems.
.\}
The next step is to build a working \*(4B system.
This can be done by following the steps in section 2 of
this document for extracting the root and /usr file systems
from the distribution tape onto unused disk partitions.
If you have a running 4.2BSD or \*(Ps system,
you can also do this by using
.IR dd (1)
to copy the \*(lqmini root\*(rq filesystem onto one disk partition,
then use it to load the \*(4B root filesystem as in chapter 2.
The root filesystem dump on the tape could also be extracted directly,
although this will require an additional file system check after booting \*(4B
to convert the new root filesystem.
The exact procedure chosen will depend on the disk configuration
and the number of suitable disk partitions that may be used.
If there is insufficient space to load the new root and \fI/usr\fP
filesystems before reusing the existing partitions,
it is \fBSTRONGLY\fP advised that you make full dumps of each filesystem
on magtape before beginning.
It is also desirable to run file system checks
of all filesystems to be converted to \*(4B before shutting down.
If you are running a system older than 4.2BSD, you will have to
dump and restore your file systems; see section 2.1 for some hints.
In either case, this is an excellent time to review your disk configuration
for possible tuning of the layout.
Section 4.2 and \fIconfig\fP(8) are required reading.
.PP
To ease the transition to new kernels,
the 4.3BSD and \*(4B
bootstrap routines pass the identity of the boot device
through to the kernel.
The kernel then uses that device as its root file system.
Thus, for example, if you boot from \fI/dev/\*(Dk1a\fP,
the kernel will use \*(Dk1a as its root file system.
If \fI/dev/\*(Dk1b\fP is configured as a swap partition, 
it will be used as the initial swap area,
otherwise the normal primary swap area (\fI/dev/\*(Dk0b\fP) will be used.
The \*(4B bootstrap is backward compatible with 4.2BSD and \*(Ps,
so you can replace your old bootstrap if you use it
to boot your first \*(4B kernel.
.PP
Once you have extracted the \*(4B system and booted from it,
you will have to build a kernel customized for your configuration.
If you have any local device drivers,
they will have to be incorporated into the new kernel.
See section 4.1.3 and ``Building 4.3BSD UNIX Systems with Config.''
.PP
If converting from 4.2BSD, \*(Ps, or the CCI 1.21 release, your old
file systems must be converted.
.if \n(Vx \{\
The standard disk partitions in \*(4B are the same as those
in 4.2BSD and \*(Ps,
except for those on the DEC UDA50; see section 4.3.2 for details.
.\}
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
.if \n(Th \{\
Merging your files from 4.2BSD into \*(4B
.\}
.if \n(Vx \{\
Merging your files from 4.2 or 4.3BSD into \*(4B
.\}
.PP
When your system is booting reliably and you have the \*(4B
root and /usr file systems fully installed you will be ready
to continue with the next step in the conversion process,
merging your old files into the new system.
.PP
If you saved the files on a \fItar\fP tape, extract them
into a scratch directory, say /usr/convert:
.DS
\fB#\fP \fImkdir /usr/convert\fP
\fB#\fP \fIcd /usr/convert\fP
\fB#\fP \fItar xp\fP
.DE
.PP
The data files marked in the previous table with a dagger (\(dg)
may be used without change from the previous system.
Those data files marked with a double dagger (\(dd) have syntax 
changes or substantial enhancements.
You should start with the \*(4B version and carefully
integrate any local changes into the new file.
Usually these local modifications can be incorporated
without conflict into the new file;
some exceptions are noted below.
The files marked with an asterisk (*) require
particular attention and are discussed below.
.PP
If you have any homegrown device drivers in /dev/MAKEDEV.local
that use major device numbers reserved by the system you
will have to modify the commands used to create the devices or alter
the system device configuration tables in /sys/\*(mC/conf.c.
Otherwise /dev/MAKEDEV.local can be used without change
from 4.2 or \*(Ps.
.PP
System security changes require adding several new ``well-known'' groups 
to /etc/group.
The groups that are needed by the system as distributed are:
.DS
.TS
l c.
name	number
_
wheel	0
daemon	1
kmem	2
sys	3
tty	4
operator	5
bin	10
.TE
.DE
Only users in the ``wheel'' group are permitted to \fIsu\fP to ``root''.
Most programs that manage directories in /usr/spool
now run set-group-id to ``daemon'' so that users cannot
directly access the files in the spool directories.
The special files that access kernel memory, /dev/kmem
and /dev/mem, are made readable only by group ``kmem''.
Standard system programs that require this access are
made set-group-id to that group.
The group ``sys'' is intended to control access to kernel sources,
and other sources belong to group ``bin.''
Rather than make user's terminals writable by all users,
they are now placed in group ``tty'' and made only group writable.
Programs that should legitimately have access to write on user's terminals
such as \fItalkd\fP and \fIwrite\fP now run set-group-id to ``tty''.
The ``operator'' group controls access to disks.
By default, disks are readable by group ``operator'',
so that programs such as \fIdf\fP can access the file system
information without being set-user-id to ``root''.
The
.IR shutdown (8)
program is executable only by group operator
and is setuid to root so that members of group operator may shut down
the system without root access.
.PP
Several new users have also been added to the group of ``well-known'' users 
in /etc/passwd.
The current list is:
.DS
.TS
l c.
name	number
_
root	0
daemon	1
operator	2
games	7
uucp	66
nobody	32767
.TE
.DE
The ``daemon'' user is used for daemon processes that
do not need root privileges.
The ``operator'' user-id is used as an account for dumpers
so that they can log in without having the root password.
By placing them in the ``operator'' group, 
they can get read access to the disks.
The ``uucp'' login has existed long before \*(4B,
and is noted here just to provide a common user-id.
The password entry ``nobody'' has been added to specify
the user with least privilege.  The ``games'' user is a pseudo-user
that controls access to game programs.
.PP
After installing your updated password file,
you must run \fImkpasswd\fP\|(8) to create the \fIndbm\fP
password database.
Note that \fImkpasswd\fP is run whenever \fIvipw\fP\|(8) is run.
.PP
The format of the cron table, /usr/lib/crontab, has been changed
to specify the user-id that should be used to run a process.
The userid ``nobody'' is frequently useful for non-privileged programs.
.PP
Some of the commands previously in /etc/rc.local have been 
moved to /etc/rc;
several new functions are now handled by /etc/rc, /etc/netstart
and /etc/rc.local.
You should look closely at the prototype version of these files
and read the manual pages for the commands contained in it
before trying to merge your local copy.
Note in particular that \fIifconfig\fP has had many changes,
and that host names are now fully specified as domain-style names
(e.g, monet.Berkeley.EDU) for the benefit of the name server.
.PP
The C library and system binaries on the distribution tape
are compiled with new versions of
\fIgethostbyname\fP and \fIgethostbyaddr\fP which use
the name server,
.IR named (8).
If you have only a small network and are not connected
to a large network, you can use the distributed library routines without
any problems; they use a linear scan of the host table \fI/etc/hosts\fP
if the name server is not running.
If you are on the DARPA Internet or have a large local network,
it is recommend that you set up
and use the name server.
For instructions on how to set up the necessary configuration files,
refer to ``Name Server Operations Guide for BIND''.
Several programs rely on the host name returned by \fIgethostname\fP
to determine the local domain name.
.PP
If you want to compile your system to use the
host table lookup routines instead of the name server, you will
need to modify /usr/src/lib/libc/Makefile according to the instructions there
and then recompile all of the system and local programs (see section 6.6).
Next, you must run \fImkhosts\fP\|(8) to create the \fIndbm\fP
host table database from \fI/etc/hosts\fP.
.PP
The format of /etc/ttys has changed, see \fIttys\fP\|(5)
for details.
It now includes the terminal type and security options that were previously
placed in /etc/ttytype and /etc/securettys.
.PP
There is a new version of \fIsyslog\fP that uses a more generalized
facility/priority scheme.
This has changed the format of the syslog.conf file.
See \fIsyslogd\fP\|(8) for details.
\fISyslog\fP now logs kernel errors, 
allowing events such
as soft disk errors, filesystem-full messages, and other such error messages
to be logged without slowing down the system
while the messages print on the console.
It is also used by many of the system daemons
to monitor system problems more closely, for example
network routing changes.
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
.PP
The spooling directories saved on tape may be restored in their
eventual resting places without too much concern.  Be sure to
use the `p' option to \fItar\fP so that files are recreated with the
same file modes:
.DS
\fB#\fP \fIcd /usr\fP
\fB#\fP \fItar xp msgs spool/mail spool/uucp spool/uucppublic spool/news\fP
.DE
.PP
The following two sections contain additional notes concerning
changes in \*(4B that affect the installation of local files;
be sure to read them as well.
.NH 2
Hints on converting from 4.2BSD to \*(4B
.PP
This section summarizes the most significant changes between
4.2BSD and 4.3BSD, particularly those that are likely to 
cause difficulty in doing the conversion.
It does not include changes in the network;
see chapter 5 for information on setting up the network.
.PP
The mailbox locking protocol has changed;
it now uses the advisory locking facility to avoid concurrent
update of users' mail boxes.
If you have your own mail interface, be sure to update its locking protocol.
.PP
The kernel's limit on the number of open files has been
increased from 20 to 64.  It is now possible to change this limit almost
arbitrarily (there used to be a hard limit of 30).  The standard I/O library
autoconfigures to the kernel limit.
Note that file (``_iob'') entries may be allocated
by \fImalloc\fP from \fIfopen\fP;
this allocation has been known to cause problems with programs
that use their own memory allocators.
This does not occur until after 20 files have been opened
by the standard I/O library.
.PP
\fISelect\fP can be used with more than 32 descriptors
by using arrays of \fBint\fPs for the bit fields rather than single \fBint\fPs.
Programs that used \fIgetdtablesize\fP as their first argument to \fIselect\fP
will no longer work correctly.
Usually the program can be modified to correctly specify the number
of bits in an \fBint\fP.
Alternatively the program can be modified to use an array of \fBint\fPs.
There are a set of macros available in \fI<sys/types.h>\fP to simplify this.
See
.IR select (2).
.PP
Old core files will not be intelligible by the current debuggers
because of numerous changes to the user structure
and because the kernel stack has been enlarged.
The \fIa.out\fP header that was in the user structure is no longer present.
Locally-written debuggers that try to check the magic number
will need modification.
.PP
\fIFind\fP now has a database of file names,
constructed once a week from \fIcron\fP.
To find a file by name only,
the command \fIfind name\fP will look in the database for
files that match the name.  This is much faster than
\fIfind / \-name name \-print\fP.
.PP
Files may not be deleted from directories having the ``sticky'' (ISVTX) bit
set in their modes
except by the owner of the file or of the directory, or by the superuser.
This is primarily to protect users' files in publicly-writable directories
such as \fI/tmp\fP and \fI/usr/tmp\fP.
All publicly-writable directories should have their ``sticky'' bits set
with ``chmod +t.''
.PP
The include file \fI<time.h>\fP has returned to \fI/usr/include\fP,
and again contains the definitions for the C library time routines of
\fIctime\fP\|(3).
.PP
The \fIcompact\fP and \fIuncompact\fP programs have been supplanted
by the faster \fIcompress\fP.
If your user population has \fIcompact\fPed files, you will want
to install \fIuncompact\fP from /usr/src/old/compact.
.PP
The configuration of the virtual memory limits has been simplified.
A MAXDSIZ option, specified in bytes in the machine configuration file,
may be used to raise the maximum process region size from
the default of 17Mb to 32Mb or 64Mb.
The initial per-process limit is still 6Mb,
but can be raised up to MAXDSIZ with the \fIcsh limit\fP command.
.PP
Some \*(4B binaries will not run with a 4.2BSD kernel because
they take advantage of new functionality in \*(4B.
One noticeable example of this problem is \fIcsh\fP.
.if \n(Th \{\
Also, most terminal \fIioctl\fP operations are incompatible
between \*(4B and the vendor-supplied versions of 4.2BSD.
.\}
.PP
If you want to use \fIps\fP after booting a new kernel,
and before going multiuser, you must initialize its name list
database by running \fIps \-U\fP.
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
See section 2.1.6 as well as
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
Look at the prototype files, and modify /etc/rc.local as necessary;
c.f. section 5.6.1.
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
Whatever else is left is likely to be site specific or require
careful scrutiny before placing in its eventual resting place.
Refer to the documentation and source code 
before arbitrarily overwriting a file.
