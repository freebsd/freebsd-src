.\" Copyright (c) 1990 The Regents of the University of California.
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
.\"	@(#)renohints.t	6.4 (Berkeley) 4/17/91
.\"
.\" format with "tbl renohints.t | troff -ms"
.\"
.ds lq ``
.ds rq ''
.ds Bs BSD
.ds 4B 4.3\*(Bs-Reno
.ds Ps 4.3\*(Bs-tahoe
.de SM		\" smaller
\s-1\\$1\s0\\$2
..
.de Pn		\" pathname
\f(CW\\$1\fP\\$2
..
.de Li		\" literal
\f(CW\\$1\fP\\$2
..
.de I		\" italicize first arg
\fI\\$1\fP\^\\$2
..
.de Xr		\" manual reference
\fI\\$1\fP\^\\$2
..
.de Fn		\" function
\fI\\$1\fP\^()\\$2
..
.ds Vx VAX
.bd S B 3
.TL
Hints on Upgrading a 4.3BSD System to \*(4B
.br
August 16, 1990
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
.ds Ux \s-1UNIX\s0
.PP
This set of notes is intended to highlight changes in \*(4B
that will affect installation on existing systems
and to point out areas of the documentation that should be examined
before and while installing this system.
It augments and partially updates
\fIInstalling and Operating \*(Ps \*(Ux* on the \*(Vx\fP\|\(dg
.FS
*\*(Ux is a registered trademark of AT&T in the USA and other countries.
.FE
.FS
\(dg\*(Vx is a trademark of Digital Equipment Corporation.
.FE
and/or
\fIInstalling and Operating \*(Ps \*(Ux on the Tahoe.\fP
Those documents are still largely correct in describing the initial bootstrap
using a \*(4B boot tape (section 2).
However, because of the rearrangement of the file systems
and other changes, the details of the upgrade procedure (section 3)
have changed substantially.
It is suggested that section 3 be used as a guide for which configuration
files to save, but that these hints and the on-line examples and documents
be used to determine the correct location and format of the configuration
files.
.PP
If you are running 4.3\*(Bs rather than \*(Ps,
the comments on upgrading 4.3\*(Bs in the accompanying
\fIInstalling and Operating \*(Ps \*(Ux ...\fP
document should be examined closely before reading the rest of this
document.
Most of the changes and procedures described there are still valid,
such as the installation of disk labels,
but they will not be described here. 
.PP
Not all of the documentation is provided in printed form, but all
of it is available in electronic form in the
.Pn /usr/src/share/doc
and
.Pn /usr/src/man
directories on the distribution tape.
.NH 1
Installation overview
.PP
If you are running 4.2\*(Bs, 4.3\*(Bs or \*(Ps, upgrading your system
involves replacing your kernel and system utilities.
In general, there are three possible ways to install a new \*(Bs distribution:
(1) boot directly from the distribution tape, use it to load new binaries
onto empty disks, and then merge or restore any existing configuration files
and filesystems;
(2) use an existing 4.2\*(Bs or later system to extract the root and
.Pn /usr
filesystems from the distribution tape,
boot from the new system, then merge or restore existing
configuration files and filesystems; or
(3) extract the sources from the distribution tape onto an existing system,
and to use that system to cross-compile and install \*(4B.
For this release, the second alternative is strongly advised if at all possible,
with the third alternative reserved as a last resort.
In general, older binaries will continue to run under \*(Bs,
but there are many exceptions that are on the critical path
for getting the system running.
Ideally, the new system binaries (root and
.Pn /usr
filesystems) should be installed on spare disk partitions,
then site-specific files should be merged into them.
Once the new system is up and fully merged, the previous root and
.Pn /usr
filesystems could be reused.
Other existing filesystems can be retained and used,
except that (as usual) the new \fIfsck\fP should be run
before they are mounted.
.PP
It is \fBSTRONGLY\fP advised that you make full dumps of each filesystem
before beginning, especially any that you intened to modify in place
during the merge.
It is also desirable to run file system checks
of all filesystems to be converted to \*(4B before shutting down.
This is an excellent time to review your disk configuration
for possible tuning of the layout.
Most systems will need to provide a new filesystem for system use
mounted on
.Pn /var
(see below).
However, the
.Pn /tmp
filesystem can be an MFS virtual-memory-resident filesystem,
potentially freeing an existing disk partition.
(Additional swap space may be desirable as a consequence.)
See
.Xr mfs (8).
.NH 1
Installation summary
.PP
The recommended installation procedure includes the following steps.
The order of these steps will probably vary according to local needs.
.IP \(bu
Extract root and
.Pn /usr
filesystems from the distribution tapes.
.IP \(bu
Extract kernel and/or user-level sources from the distribution tape
if space permits.
This can serve as the backup documentation as needed.
.IP \(bu
Configure and boot a kernel for the local system.
This can be delayed if the generic kernel from the distribution
supports sufficient hardware to proceed.
.IP \(bu
Build a skeletal
.Pn /var
filesystem (see
.Xr mtree (8).
.IP \(bu
Merge site-dependent configuration files from
.Pn /etc
and
.Pn /usr/lib
into the new
.Pn /etc
directory.
Note that many file formats and contents have changed; see section 3.2
of this document.
.IP \(bu
Copy or merge files from
.Pn /usr/adm ,
.Pn /usr/spool ,
.Pn /usr/preserve ,
.Pn /usr/lib ,
and other locations into
.Pn /var .
.IP \(bu
Merge local macros, dictionaries, etc. into
.Pn /usr/share .
.IP \(bu
Merge and update local software to reflect the system changes.
.IP \(bu
Take off the rest of the morning, you've earned it!
.NH 1
Summary of changes
.PP
The following sections summarize system changes that should be reviewed
before attempting to install the system.
.NH 2
Filesystem organization
.PP
The most immediately obvious change in \*(4B is the reorganization
of the system filesystems.
Users of certain recent vendor releases have seen this general organization,
although \*(4B takes the reorganization a bit further.
The directories most affected are
.Pn /etc ,
which now contains only system configuration files;
.Pn /var ,
a new filesystem containing per-system spool and log files; and
.Pn /usr/share,
which contains most of the text files shareable across architectures
such as documentation and macros.
System administration programs formerly in
.Pn /etc
are now found in
.Pn /sbin
and
.Pn /usr/sbin .
Various programs and data files formerly in
.Pn /usr/lib
are now found in
.Pn /usr/libexec
and
.Pn /usr/libdata ,
respectively.
Administrative files formerly in
.Pn /usr/adm
are in
.Pn /var/account
and, similarly, log files are now in
.Pn /var/log .
The directory
.Pn /usr/ucb
has been merged into
.Pn /usr/bin ,
and the sources for programs in
.Pn /usr/bin
are split into
.Pn /usr/src/usr.bin
and
.Pn /usr/src/pgrm .
Other source directories parallel the destination directories;
.Pn /usr/src/etc
has been greatly expanded, and
.Pn /usr/src/share
is new.
The source for the manual pages, in general, are with the source
code for the applications they document.
Manual pages not closely corresponding to an application program
are found in
.Pn /usr/src/man .
The manual page
.Xr hier (7)
has been updated and made more detailed;
it is included in the printed documentation.
You should review it to familiarize yourself with the new layout.
.NH 2
/etc
.PP
The
.Pn /etc
directory now contains nearly all of the host-specific configuration
files.
Note that some file formats have changed,
and those configuration files containing pathnames are nearly all affected
by the reorganization.
See the examples provided in
.Pn /etc
(installed from
.Pn /usr/src/etc )
as a guide.
The following table lists some of the local configuration files
whose locations and/or contents have changed.
.DS I .3i
.TS
l l l
lfC lfC l.
4.3BSD and Earlier	4.3BSD-Reno	Comments
_	_	_
/etc/fstab	/etc/fstab	new format; see below
/etc/inetd.conf	/etc/inetd.conf	pathnames of executables changed
/etc/printcap	/etc/printcap	pathnames changed
/etc/syslog.conf	/etc/syslog.conf	pathnames of log files changed
/etc/ttys	/etc/ttys	pathnames of executables changed
/etc/passwd	/etc/master.passwd	new format; see below
/usr/lib/sendmail.cf	/etc/sendmail.cf	changed pathnames
/usr/lib/aliases	/etc/aliases	may contain changed pathnames
/etc/*.pid	/var/run/*.pid	
	
.T&
l l l
lfC lfC l.
New in 4.3BSD-Tahoe	4.3BSD-Reno	Comments
_	_	_
/usr/games/dm.config	/etc/dm.conf	configuration for games (see \fIdm\fP\|(8))
/etc/zoneinfo/localtime	/etc/localtime	timezone configuration
/etc/zoneinfo	/usr/share/zoneinfo	timezone configuration
	
.T&
l l l
lfC lfC l.
	New in 4.3BSD-Reno	Comments
_	_	_
	/etc/man.conf	lists directories searched by \fIman\fP\|(1)
	/etc/kerberosIV	Kerberos directory; see below
.TE
.DE
.NH 2
/root
.PP
The home directory of the user ``root''
is now
.Pn /root
rather than
.Pn / .
The file
.Pn /.profile
is still used when the system comes up in single-user mode,
although
.Pn /.profile
is normally a link to
.Pn /root/.profile .
.NH 2
Block devices and the root filesystem
.PP
The buffer cache in the kernel is now organized as a file block cache
rather than a device block cache.
As a consequence, cached blocks from a file
and from the corresponding block device would no longer be kept consistent.
The block device thus has little remaining value.
Three changes have been made for these reasons:
(1) block devices may not be opened while they are mounted,
and may not be mounted while open, so that the two versions of cached
file blocks cannot be created,
(2) filesystem checks of the root now use the raw device
to access the root filesystem, and
(3) the root filesystem is initially mounted read-only
so that nothing can be written back to disk during or after modification
of the raw filesystem by
.I fsck .
The root filesystem may be made writable while in single-user mode
with the command
.Li "mount -u /" .
(The mount command has an option to update the flags on a mounted filesystem,
including the ability to upgrade a filesystem from read-only to read-write.)
.NH 2
Mtree
.PP
A new utility,
.Xr mtree (8),
is provided to build and check filesystem hierarchies
with the proper contents, owners and permissions.
Scripts are provided in
.Pn /etc/mtree
(and
.Pn /usr/src/etc/mtree )
for the root,
.Pn /usr
and
.Pn /var
filesystems.
Once a filesystem has been made for
.Pn /var ,
.I mtree
should be used to create a directory hierarchy there.
.NH 2
Shadow password files
.PP
The password file format and location have changed in order to protect
the encrypted passwords stored there.
The actual password file is now stored in
.Pn /etc/master.passwd .
The hashed dbm password files do not contain encrypted passwords,
but contain the file offset to the entry with the password in
.Pn /etc/master.passwd
(which is readable only by root).
Thus, the
.Fn getpwnam
and
.Fn getpwuid
functions will no longer return an encrypted password string to non-root
callers.
An old-style passwd file is created in
.Pn /etc/passwd
by the
.Xr vipw (8)
and
.Xr mkpasswd (8)
programs.
See also
.Xr passwd (5).
.NH 2
Kerberos
.PP
The Kerberos authentication server from MIT (version 4)
is included in this release.
See \fIkerberos\fP\|(1) for a general, if MIT-specific,
introduction.
If it is configured,
.Xr login (1),
.Xr passwd (1),
.Xr rlogin (1)
and
.Xr rsh (1)
will all begin to use it automatically.
The file
.Pn /etc/kerberosIV/README
describes the configuration.
Each system needs the file
.Pn /etc/kerberosIV/krb.conf
to set its realm and local servers,
and a private key stored in
.Pn /etc/kerberosIV/srvtab
(see
.Xr ext_srvtab (8)).
The Kerberos server should be set up on a single, physically secure,
server machine.
Users and hosts may be added to the server database manually with
.Xr kdb_edit (8),
or users on authorized hosts can add themselves and a Kerberos
password upon verification of their ``local'' (passwd-file) password
using the
.Xr register (1)
program.
.PP
Note that by default the password-changing program
.Xr passwd (1)
changes the Kerberos password, which must exist.
The
.Li \-l
option to
.Xr passwd (1)
changes the ``local'' password if one exists.
.PP
Note that Version 5 of Kerberos will be released soon,
and that Version 4 will probably be replaced at that time.
.NH 2
Make and Makefiles
.PP
This release uses a completely new version of the
.I make
program derived from the
.I pmake
program developed by the Sprite project at Berkeley.
It supports existing makefiles, although certain incorrect makefiles
may fail.
The makefiles for the \*(4B sources make extensive use of the new
facilities, especially conditionals and file inclusion, and are thus
completely incompatible with older versions of
.I make
(but nearly all of the makefiles are now trivial!).
The standard include files for
.I make
are in
.Pn /usr/share/mk .
There is a README file in
.Pn /usr/src/share/mk .
.PP
Another global change supported by the new
.I make
is designed to allow multiple architectures to share a copy of the sources.
If a subdirectory named
.Pn obj
is present in the current directory,
.I make
descends into that directory and creates all object and other files there.
We use this by building a directory hierarchy in
.Pn /usr/obj
that parallels
.Pn /usr/src .
We then create the
.Pn obj
subdirectories in
.Pn /usr/src
as symbolic links to the corresponding directories in
.Pn /usr/obj .
(Both of these steps are automated; the makefile in the
.Pn /usr/src
directory has an example of a target (``shadow'') which builds
the object file system, and ``make obj'' in a directory
including the standard \*(Bs rules in its Makefile makes the
.Pn obj
links in the current directory and recursively in the normal subdirectories.)
We have one
.Pn /usr/obj
hierarchy on the local system, and another on each
system that shares the source filesystem.
.NH 2
lex, yacc
.PP
New versions of
.Xr lex (1)
(``flex'') and 
.Xr yacc (1)
(``zoo'')
have replaced their AT&T-derived predecessors.
These should be installed early on if attempting to cross-compile \*(4B
on another system.
Note that the new
.Xr lex
program is not completely backward compatible with historic versions of
.Xr lex ,
although it is believed that all documented features are supported.
.NH 2
NFS
.PP
Network filesystem access is available in \*(4B.
An implementation of the Network File System (NFS) was contributed
by Rick Macklem of the University of Guelph.
Its use will be fairly familiar to users of other implementations of NFS.
See the manual pages
.Xr mount (8),
.Xr mountd (8),
.Xr fstab (5),
.Xr exports (5),
.Xr nfsd (8)
and
.Xr nfsiod (8).
The format of
.Pn /etc/fstab
has changed from previous \*(Bs releases
to a blank-separated format to allow colons in pathnames.
Tahoe users should note that older versions of the CCI console processor
PROMs are annoyed at this change; placing the line
.DS
.ft C
/dev/xfd0a:/: /	ufs	xx 1 1
.ft P
.DE
at the beginning of
.Pn /etc/fstab
(where \fIxfd\fP is replaced
by the appropriate console processor name for the boot disk)
may placate the CP.
Otherwise, automatic boots after power-up will fail with messages
about problems in
.Pn /etc/fstab ,
culminating in suppresion of ``auto-file mount.''
This is a problem only if automatic reboots fail.
.NH 2
Automounter
.PP
An implementation of an auto-mounter daemon,
.I amd,
was contributed by Jan-Simon Pendry of the
Imperial College of Science, Technology & Medicine.
See the source directory,
.Pn /usr/src/usr.sbin/amd
and its
.Pn doc
and
.Pn text
subdirectories for further information.
.NH 2
POSIX terminal interface
.PP
The \*(4B system uses the IEEE P1003.1 (POSIX.1) terminal interface
rather than the previous \*(Bs terminal interface.
The new interface has nearly all of the functionality of the old interface,
extending the POSIX interface as necessary.
Both the old
.I ioctl
calls and old options to
.Xr stty (1)
are emulated.
This emulation is expected to be removed in a future release, so
conversion to the new interface is encouraged.
.NH 2
POSIX job control
.PP
The POSIX.1 job control interface is implemented in \*(4B.
A new system call,
.Fn setsid ,
is used to create a job-control session consisting of a single process
group with one member, the caller, which becomes a session leader.
Only a session leader may acquire a controlling terminal.
This is done explicitly via a
.SM TIOCSCTTY
.Fn ioctl
call, not implicitly by an
.Fn open
call.
The call fails if the terminal is in use.
Programs that allocate controlling terminals (or pseudo-terminals)
require modification to work in this environment.
Versions of
.I xterm
are provided for releases X11R4 and X10R4 in
.Pn /usr/src/contrib/xterm .
New library routines are available for allocating and initializing
pseudo-terminals and other terminals as controlling terminal; see
.Pn /usr/src/lib/libutil/pty.c
and
.Pn /usr/src/lib/libutil/login_tty.c .
.PP
The POSIX job control model formalizes the previous conventions
used in setting up a process group.
Unfortunately, this requires that changes be made in a defined order
and with some synchronization that were not necessary in the past.
Older job control shells (csh, ksh) will generally not operate correctly
with the new system.
.NH 2
Other POSIX changes
.PP
Most of the other kernel interfaces have been changed to correspond
with the POSIX.1 interface, although that work is not quite complete.
See the relevant manual pages, perhaps in conjunction with the IEEE POSIX
standard.
.PP
Many of the utilities have been changed to work as described in draft 9
of the POSIX.2 Shell and Utilities document.
Additional changes are certain in this area.
.NH 2
ISO OSI networking
.PP
\*(4B provides some support for the ISO OSI protocols CLNP,
TP4, and ES-IS.
User level libraries and processes
implement the application protocols such as FTAM and X.500;
these are available in ISODE,
the ISO Development Environment by Marshall Rose,
which is available via anonymous FTP
(but is not included on the distribution tape).
.PP
Kernel support for the ISO OSI protocols is enabled with the ISO option
in the kernel configuration file.
The
.Xr iso (4)
manual page describes the protocols and addressing;
see also
.Xr clnp (4),
.Xr tp (4)
and
.Xr cltp (4).
The OSI equivalent to ARP is ESIS (End System to Intermediate System Routeing
Protocol); running this protocol is mandatory, however one can manually add
translations for machines that do not participate by use of the
.Xr route (8)
command.
Additional information is provided in the manual page describing
.Xr esis (4).
.NH 2
Revised route command
.PP
The command
.Xr route (8)
has a new syntax and a number of new capabilities:
it can install routes with a specified destination and mask,
and can change route characteristics such as hop count, packet size
and window size.
.NH 2
New sockaddr format
.PP
The format of the
.I sockaddr
structure (the structure used to describe a generic network address with an
address family and family-specific data)
has changed from previous releases,
as have the address family-specific versions of this structure.
The
.I sa_family
family field has been split into a length,
.IR sa_len ,
and a family,
.IR sa_family .
System calls that pass a
.I sockaddr
structure into the kernel (e.g.
.Fn sendto
and
.Fn connect )
have a separate parameter that specifies the 
.I sockaddr
length, and thus it is not necessary to fill in the
.I sa_len
field for those system calls.
System calls that pass a
.I sockaddr
structure back from the kernel (e.g. 
.Fn recvfrom
and
.Fn accept )
receive a completely filled-in
.I sockaddr
structure, thus the length field is valid.
Because this would not work for old binaries,
the new library uses a different system call number.
Thus, most networking programs compiled under \*(4B are incompatible
with older systems.
.PP
Although this change is mostly source and binary compatible
with old programs, there are three exceptions.
Programs with statically initialized
.I sockaddr
structures
(usually the Internet form, a
.I sockaddr_in )
are not compatible.
Generally, such programs should be changed to fill in the structure
at run time, as C allows no way to initialize a structure without
assuming the order and number of fields.
Also, programs with use structures to describe a network packet format
that contain embedded
.I sockaddr
structures also require modification; a definition of an
.I osockaddr
structure is provided for this purpose.
Finally, programs that use the
.SM SIOCGIFCONF
ioctl to get a complete list of interface addresses
need to check the
.I sa_len
field when iterating through the array of addresses returned,
as not all of the structures returned have the same length
(in fact, this is nearly guaranteed by the presence of link-layer
address structures).
.NH 2
/dev/fd
.PP
The directory
.Pn /dev/fd
contains special files
.Pn 0
through
.Pn 63
which, when opened, duplicate the corresponding file descriptor.
The names
.Pn /dev/stdin
.Pn /dev/stdout
.Pn /dev/stderr
refer to file descriptors 0, 1 and 2.
See
.Xr fd (4)
for more information.
.NH 2
Ktrace
.PP
A system-call tracing facility is provided in \*(4B
that records all of the system calls made by a process or group of processes
and their outcomes.
The facility is restricted to root until certain privilege tests can
be put into place.
See
.Xr ktrace (1)
and
.Xr kdump (1).
.KS
.NH 1
Example script for copying files to /var
.PP
The following commands provide a guide for copying spool and log files from
an existing system into a new
.Pn /var
filesystem.
At least the following directories should already exist on
.Pn /var :
.Pn output ,
.Pn log ,
.Pn backups
and
.Pn db .
.LP
.DS
.nf
.ft CW
SRC=/oldroot/usr

cd $SRC; tar cf - msgs preserve | (cd /var && tar xpf -)

# copy $SRC/spool to /var
cd $SRC/spool
tar cf - at mail rwho | (cd /var && tar xpf -)
tar cf - ftp mqueue news secretmail uucp uucppublic | \e
	(cd /var/spool && tar xpf -)

# everything else in spool is probably a printer area
mkdir .save
mv at ftp mail mqueue rwho secretmail uucp uucppublic .save
tar cf - * | (cd /var/spool/output && tar xpf -)
mv .save/* .
rmdir .save

cd /var/spool/mqueue
mv syslog.7 /var/log/maillog.7
mv syslog.6 /var/log/maillog.6
mv syslog.5 /var/log/maillog.5
mv syslog.4 /var/log/maillog.4
mv syslog.3 /var/log/maillog.3
mv syslog.2 /var/log/maillog.2
mv syslog.1 /var/log/maillog.1
mv syslog.0 /var/log/maillog.0
mv syslog /var/log/maillog

# move $SRC/adm to /var
cd $SRC/adm
tar cf - . | (cd /var/account && tar  xpf -)
cd /var/account
rm -f msgbuf
mv messages messages.[0-9] ../log
mv wtmp wtmp.[0-9] ../log
mv lastlog ../log
.DE
.KE
