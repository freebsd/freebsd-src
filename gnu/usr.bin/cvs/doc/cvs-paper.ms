.\" soelim cvs.ms | pic | tbl | troff -ms
.\" @(#)cvs.ms 1.2 92/01/30
.\"
.\"	troff source to the cvs USENIX article, Winter 1990, Washington, D.C.
.\"	Copyright (c) 1989, Brian Berliner
.\"
.\"	This program is free software; you can redistribute it and/or modify
.\"	it under the terms of the GNU General Public License as published by
.\"	the Free Software Foundation; either version 1, or (at your option)
.\"	any later version.
.\"
.\"	This program is distributed in the hope that it will be useful,
.\"	but WITHOUT ANY WARRANTY; without even the implied warranty of
.\"	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
.\"	GNU General Public License for more details.
.\"
.\"	You should have received a copy of the GNU General Public License
.\"	along with this program; if not, write to the Free Software
.\"	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
.\"
.\"	The author can be reached at:  berliner@prisma.com
.\"
.de SP
.if n .sp
.if t .sp .5
..
.de hl
.br
.in +0.5i
\l'\\n(LLu-1i'
.in -0.5i
.sp
..
.OH ""
.nr PS 11
.nr PO 1.25i
.pl -0.2i
.TL
.ps 14
.ft B
.nf
CVS II:
Parallelizing Software Development
.fi
.ft
.ps
.AU
.ps 12
.ft I
Brian Berliner
.ft
.ps
.AI
.ps 12
.ft I
Prisma, Inc.
5465 Mark Dabling Blvd.
Colorado Springs, CO  80918
berliner@prisma.com
.ft
.ps
.AB
The program described in this paper fills a need in the UNIX
community for a freely available tool to manage software revision and
release control in a multi-developer, multi-directory, multi-group
environment.
This tool also addresses the increasing need for tracking third-party vendor
source distributions while trying to maintain local modifications to
earlier releases.
.AE
.NH
Background
.PP
In large software development projects, it is usually necessary for more
than one software developer to be modifying (usually different) modules of the
code at the same time.
Some of these code modifications are done in an
experimental sense, at least until the code functions correctly, and some
testing of the entire program is usually necessary.
Then, the modifications are returned to a master source repository
so that others in the project can
enjoy the new bug-fix or functionality.
In order to manage such a project, some sort of revision control system is
necessary.
.PP
Specifically, UNIX\**
.FS
UNIX is a registered trademark of AT&T.
.FE
kernel development is an excellent example of the
problems that an adequate revision control system must address.
The SunOS\**
.FS
SunOS is a trademark of Sun Microsystems, Inc.
.FE
kernel is composed of over a thousand files spread across a
hierarchy of dozens of directories.\**
.FS
Yes, the SunOS 4.0 kernel is composed of over a \fIthousand\fP files!
.FE
Pieces of the kernel must be edited
by many software developers within an organization.
While undesirable in
theory, it is not uncommon to have two or more people making
modifications to the same file within the kernel sources in
order to facilitate a desired change.
Existing revision control systems like
.SM
RCS
.LG
[Tichy] or
.SM
SCCS
.LG
[Bell] serialize file modifications by
allowing only one developer to have a writable copy of a particular file at
any one point in time.
That developer is said to
have \*Qlocked\*U the file for his exclusive use, and no other developer is
allowed to check out a writable copy of the file until the locking
developer has finished impeding others' productivity.
Development pressures of productivity and deadlines
often force organizations to require that multiple developers be able to
simultaneously edit
copies of the same revision controlled file.
.PP
The necessity for multiple developers to modify the same file concurrently
questions the value of serialization-based policies in traditional revision
control.
This paper discusses the approach that
Prisma took in adapting a standard revision control system,
.SM
RCS\c
.LG
, along with an existing public-domain collection of shell scripts that sits
atop
.SM
RCS
.LG
and provides the basic conflict-resolution algorithms.
The resulting
program, \fBcvs\fP, addresses not only the issue of conflict-resolution in
a multi-developer open-editing environment, but also the issues of
software release control and vendor source support and integration.
.NH
The CVS Program
.PP
\fBcvs\fP
(Concurrent Versions System)
is a front end to the
.SM
RCS
.LG
revision control system which extends
the notion of revision control from a collection of files in a single
directory to a hierarchical collection of directories each containing
revision controlled files.
Directories and files in the \fBcvs\fP system can be combined together in
many ways to form a software release.
\fBcvs\fP
provides the functions necessary to manage these software releases and to
control the concurrent editing of source files among multiple software
developers.
.PP
The six major features of \fBcvs\fP are listed below, and will be
described in more detail in the following sections:
.RS
.IP 1.
Concurrent access and conflict-resolution algorithms to guarantee that
source changes are not \*Qlost.\*U
.IP 2.
Support for tracking third-party vendor source distributions while
maintaining the local modifications made to those sources.
.IP 3.
A flexible module database that provides a symbolic mapping of names to
components of a larger software distribution.
This symbolic mapping provides for location independence within the software
release and, for example, allows one to check out a copy of the \*Qdiff\*U
program without ever knowing that the sources to \*Qdiff\*U actually reside
in the \*Qbin/diff\*U directory.
.IP 4.
Configurable logging support allows all \*Qcommitted\*U source file changes
to be logged using an arbitrary program to save the log messages in a file,
notesfile, or news database.
.IP 5.
A software release can be symbolically tagged and checked out at any time
based on that tag.
An exact copy of a previous software release can be checked out at
any time, \fIregardless\fP of whether files or directories have been
added/removed from the \*Qcurrent\*U software release.
As well,
a \*Qdate\*U can be used to check out the \fIexact\fP version of the software
release as of the specified date.
.IP 6.
A \*Qpatch\*U format file [Wall] can be produced between two software
releases, even if the releases span multiple directories.
.RE
.PP
The sources maintained by \fBcvs\fP are kept within a single directory
hierarchy known as the \*Qsource repository.\*U
This \*Qsource repository\*U holds the actual
.SM
RCS
.LG
\*Q,v\*U files directly, as well as a special per-repository directory
(\c
.SM
CVSROOT.adm\c
.LG
) which contains a small number of administrative files that describe the
repository and how it can be accessed.
See Figure 1 for a picture of the \fBcvs\fP tree.
.KF
.hl
.DS B
.PS
line from 4.112,9.200 to 5.550,8.887
line from 5.447,8.884 to 5.550,8.887 to 5.458,8.933
line from 4.112,9.200 to 4.550,8.950
line from 4.451,8.978 to 4.550,8.950 to 4.476,9.021
line from 4.112,9.200 to 3.737,8.887
line from 3.798,8.971 to 3.737,8.887 to 3.830,8.932
line from 3.612,8.762 to 4.737,8.137
line from 4.638,8.164 to 4.737,8.137 to 4.662,8.208
line from 3.612,8.762 to 3.737,8.137
line from 3.693,8.231 to 3.737,8.137 to 3.742,8.240
line from 3.612,8.762 to 2.612,8.200
line from 2.687,8.271 to 2.612,8.200 to 2.712,8.227
line from 2.362,9.262 to 2.737,8.950
line from 2.645,8.995 to 2.737,8.950 to 2.677,9.033
line from 2.362,9.262 to 1.925,8.950
line from 1.992,9.028 to 1.925,8.950 to 2.021,8.988
line from 3.362,9.762 to 4.050,9.387
line from 3.950,9.413 to 4.050,9.387 to 3.974,9.457
line from 3.362,9.762 to 2.487,9.387
line from 2.570,9.450 to 2.487,9.387 to 2.589,9.404
.ps 11
"newfs.c,v" at 4.487,8.043 ljust
.ps 11
"mkfs.c,v" at 3.487,8.043 ljust
.ps 11
"Makefile,v" at 2.237,8.043 ljust
.ps 11
"newfs" at 3.487,8.793 ljust
.ps 11
"halt.c,v" at 5.487,8.793 ljust
.ps 11
"Makefile,v" at 4.237,8.793 ljust
.ps 11
"modules,v" at 2.487,8.793 ljust
.ps 11
"loginfo,v" at 1.488,8.793 ljust
.ps 11
"etc" at 3.987,9.293 ljust
.ps 11
"CVSROOT.adm" at 1.988,9.293 ljust
.ps 11
"/src/master" at 2.987,9.793 ljust
.PE
.DE
.hl
.ce 100
.LG
\fBFigure 1.\fP
.SM
\fBcvs\fP Source Repository
.ce 0
.sp
.KE
.NH 2
Software Conflict Resolution\**
.FS
The basic conflict-resolution algorithms
used in the \fBcvs\fP program find their roots
in the original work done by Dick Grune at Vrije Universiteit in Amsterdam
and posted to \fBcomp.sources.unix\fP in the volume 6 release sometime in 1986.
This original version of \fBcvs\fP was a collection of shell scripts that
combined to form a front end to the
.SM
RCS
.LG
programs.
.FE
.PP
\fBcvs\fP allows several software developers to edit personal copies of a
revision controlled file concurrently.
The revision number of each checked out file is maintained independently
for each user, and \fBcvs\fP forces the checked out file to be current with
the \*Qhead\*U revision before it can be \*Qcommitted\*U as a permanent change.
A checked out file is brought up-to-date with the \*Qhead\*U revision using
the \*Qupdate\*U command of \fBcvs\fP.
This command compares the \*Qhead\*U revision number with that of the user's
file and performs an
.SM
RCS
.LG
merge operation if they are not the same.
The result of the merge is a file that contains the user's modifications
and those modifications that were \*Qcommitted\*U after the user
checked out his version of the file (as well as a backup copy of the
user's original file).
\fBcvs\fP points out any conflicts during the merge.
It is the user's responsibility to resolve these conflicts
and to \*Qcommit\*U his/her changes when ready.
.PP
Although the \fBcvs\fP conflict-resolution algorithm was defined in 1986,
it is remarkably similar to the \*QCopy-Modify-Merge\*U scenario included
with NSE\**
.FS
NSE is the Network Software Environment, a product of Sun Microsystems, Inc.
.FE
and described in [Honda] and [Courington].
The following explanation from [Honda] also applies to \fBcvs\fP:
.QP
Simply stated, a developer copies an object without locking it, modifies
the copy, and then merges the modified copy with the original.
This paradigm allows developers to work in isolation from one another since
changes are made to copies of objects.
Because locks are not used, development is not serialized and can proceed
in parallel.
Developers, however, must merge objects after the changes have been made.
In particular, a developer must resolve conflicts when the same object has
been modified by someone else.
.PP
In practice, Prisma has found that conflicts that occur when the same
object has been modified by someone else are quite rare.
When they do happen, the changes made by the other developer are usually
easily resolved.
This practical use has shown that the \*QCopy-Modify-Merge\*U paradigm is a
correct and useful one.
.NH 2
Tracking Third-Party Source Distributions
.PP
Currently, a large amount of software is based on source
distributions from a third-party distributor.
It is often the case that local modifications are to be made to this
distribution, \fIand\fP that the vendor's future releases should be
tracked.
Rolling your local modifications forward into the new vendor release is a
time-consuming task, but \fBcvs\fP can ease this burden somewhat.
The \fBcheckin\fP program of \fBcvs\fP initially sets up a source
repository by integrating the source modules directly from the vendor's
release, preserving the directory hierarchy of the vendor's distribution.
The branch support of
.SM
RCS
.LG
is used to build this vendor release as a branch of the main
.SM
RCS
.LG
trunk.
Figure 2 shows how the \*Qhead\*U tracks a sample vendor
branch when no local modifications have been made to the file.
.KF
.hl
.DS B
.PS
ellipse at 3.237,6.763 wid 1.000 ht 0.500
dashwid = 0.050i
line dashed from 3.237,7.513 to 3.737,7.513 to 3.737,9.762 to 4.237,9.762
line from 4.138,9.737 to 4.237,9.762 to 4.138,9.787
line dashed from 2.237,8.262 to 3.237,8.262 to 3.237,7.013
line from 3.212,7.112 to 3.237,7.013 to 3.262,7.112
line from 3.737,6.763 to 4.237,6.763
line from 4.138,6.737 to 4.237,6.763 to 4.138,6.788
line from 2.237,6.763 to 2.737,6.763
line from 2.637,6.737 to 2.737,6.763 to 2.637,6.788
line from 1.738,6.013 to 1.738,6.513
line from 1.762,6.413 to 1.738,6.513 to 1.713,6.413
line from 1.238,7.013 to 2.237,7.013 to 2.237,6.513 to 1.238,6.513 to 1.238,7.013
line from 4.237,9.012 to 5.237,9.012 to 5.237,8.512 to 4.237,8.512 to 4.237,9.012
line from 4.237,8.012 to 5.237,8.012 to 5.237,7.513 to 4.237,7.513 to 4.237,8.012
line from 4.237,7.013 to 5.237,7.013 to 5.237,6.513 to 4.237,6.513 to 4.237,7.013
line from 4.737,7.013 to 4.737,7.513
line from 4.763,7.413 to 4.737,7.513 to 4.712,7.413
line from 4.737,8.012 to 4.737,8.512
line from 4.763,8.412 to 4.737,8.512 to 4.712,8.412
line from 4.237,10.012 to 5.237,10.012 to 5.237,9.512 to 4.237,9.512 to 4.237,10.012
line from 4.737,9.012 to 4.737,9.512
line from 4.763,9.412 to 4.737,9.512 to 4.712,9.412
line from 5.987,5.013 to 5.987,6.013 to 0.988,6.013 to 0.988,5.013 to 5.987,5.013
.ps 11
"\"HEAD\"" at 1.550,8.231 ljust
.ps 11
"'SunOS'" at 2.987,6.293 ljust
.ps 11
"1.1.1" at 3.050,6.793 ljust
.ps 11
"1.1" at 1.613,6.793 ljust
.ps 11
"1.1.1.1" at 4.487,6.793 ljust
.ps 11
"1.1.1.2" at 4.487,7.793 ljust
.ps 11
"1.1.1.3" at 4.487,8.793 ljust
.ps 11
"1.1.1.4" at 4.487,9.793 ljust
.ps 11
"'SunOS_4_0'" at 5.487,6.793 ljust
.ps 11
"'SunOS_4_0_1'" at 5.487,7.793 ljust
.ps 11
"'YAPT_5_5C'" at 5.487,8.793 ljust
.ps 11
"'SunOS_4_0_3'" at 5.487,9.793 ljust
.ps 11
"rcsfile.c,v" at 2.987,5.543 ljust
.PE
.DE
.hl
.ce 100
.LG
\fBFigure 2.\fP
.SM
\fBcvs\fP Vendor Branch Example
.ce 0
.sp .3
.KE
Once this is done, developers can check out files and make local changes to
the vendor's source distribution.
These local changes form a new branch to the tree which is then used as the
source for future check outs.
Figure 3 shows how the \*Qhead\*U moves to the main
.SM
RCS
.LG
trunk when a local modification is made.
.KF
.hl
.DS B
.PS
ellipse at 3.237,6.763 wid 1.000 ht 0.500
dashwid = 0.050i
line dashed from 2.800,9.075 to 1.738,9.075 to 1.738,8.012
line from 1.713,8.112 to 1.738,8.012 to 1.762,8.112
line from 1.738,7.013 to 1.738,7.513
line from 1.762,7.413 to 1.738,7.513 to 1.713,7.413
line from 1.238,8.012 to 2.237,8.012 to 2.237,7.513 to 1.238,7.513 to 1.238,8.012
line from 3.737,6.763 to 4.237,6.763
line from 4.138,6.737 to 4.237,6.763 to 4.138,6.788
line from 2.237,6.763 to 2.737,6.763
line from 2.637,6.737 to 2.737,6.763 to 2.637,6.788
line from 1.738,6.013 to 1.738,6.513
line from 1.762,6.413 to 1.738,6.513 to 1.713,6.413
line from 1.238,7.013 to 2.237,7.013 to 2.237,6.513 to 1.238,6.513 to 1.238,7.013
line from 4.237,9.012 to 5.237,9.012 to 5.237,8.512 to 4.237,8.512 to 4.237,9.012
line from 4.237,8.012 to 5.237,8.012 to 5.237,7.513 to 4.237,7.513 to 4.237,8.012
line from 4.237,7.013 to 5.237,7.013 to 5.237,6.513 to 4.237,6.513 to 4.237,7.013
line from 4.737,7.013 to 4.737,7.513
line from 4.763,7.413 to 4.737,7.513 to 4.712,7.413
line from 4.737,8.012 to 4.737,8.512
line from 4.763,8.412 to 4.737,8.512 to 4.712,8.412
line from 4.237,10.012 to 5.237,10.012 to 5.237,9.512 to 4.237,9.512 to 4.237,10.012
line from 4.737,9.012 to 4.737,9.512
line from 4.763,9.412 to 4.737,9.512 to 4.712,9.412
line from 5.987,5.013 to 5.987,6.013 to 0.988,6.013 to 0.988,5.013 to 5.987,5.013
.ps 11
"1.2" at 1.613,7.793 ljust
.ps 11
"\"HEAD\"" at 2.862,9.043 ljust
.ps 11
"'SunOS'" at 2.987,6.293 ljust
.ps 11
"1.1.1" at 3.050,6.793 ljust
.ps 11
"1.1" at 1.613,6.793 ljust
.ps 11
"1.1.1.1" at 4.487,6.793 ljust
.ps 11
"1.1.1.2" at 4.487,7.793 ljust
.ps 11
"1.1.1.3" at 4.487,8.793 ljust
.ps 11
"1.1.1.4" at 4.487,9.793 ljust
.ps 11
"'SunOS_4_0'" at 5.487,6.793 ljust
.ps 11
"'SunOS_4_0_1'" at 5.487,7.793 ljust
.ps 11
"'YAPT_5_5C'" at 5.487,8.793 ljust
.ps 11
"'SunOS_4_0_3'" at 5.487,9.793 ljust
.ps 11
"rcsfile.c,v" at 2.987,5.543 ljust
.PE
.DE
.hl
.ce 100
.LG
\fBFigure 3.\fP
.SM
\fBcvs\fP Local Modification to Vendor Branch
.ce 0
.sp
.KE
.PP
When a new version of the vendor's source distribution arrives, the
\fBcheckin\fP program adds the new and changed vendor's files to the
already existing source repository.
For files that have not been changed locally, the new file from the
vendor becomes the current \*Qhead\*U revision.
For files that have been modified locally, \fBcheckin\fP warns that the
file must be merged with the new vendor release.
The \fBcvs\fP \*Qjoin\*U command is a useful tool that aids this process by
performing the necessary
.SM
RCS
.LG
merge, as is done above when performing an \*Qupdate.\*U
.PP
There is also limited support for \*Qdual\*U derivations for source files.
See Figure 4 for a sample dual-derived file.
.KF
.hl
.DS B
.PS
ellipse at 2.337,8.575 wid 0.700 ht 0.375
ellipse at 2.312,9.137 wid 0.700 ht 0.375
line from 1.225,9.012 to 1.225,9.363
line from 1.250,9.263 to 1.225,9.363 to 1.200,9.263
line from 0.875,9.725 to 1.600,9.725 to 1.600,9.363 to 0.875,9.363 to 0.875,9.725
line from 0.875,9.012 to 1.600,9.012 to 1.600,8.650 to 0.875,8.650 to 0.875,9.012
line from 4.050,10.200 to 4.775,10.200 to 4.775,9.850 to 4.050,9.850 to 4.050,10.200
line from 4.050,9.475 to 4.775,9.475 to 4.775,9.113 to 4.050,9.113 to 4.050,9.475
line from 4.050,8.762 to 4.775,8.762 to 4.775,8.400 to 4.050,8.400 to 4.050,8.762
line from 4.425,8.762 to 4.425,9.113
line from 4.450,9.013 to 4.425,9.113 to 4.400,9.013
line from 4.425,9.475 to 4.425,9.850
line from 4.450,9.750 to 4.425,9.850 to 4.400,9.750
line from 3.050,10.000 to 3.775,10.000 to 3.775,9.637 to 3.050,9.637 to 3.050,10.000
line from 3.050,9.312 to 3.775,9.312 to 3.775,8.950 to 3.050,8.950 to 3.050,9.312
line from 0.713,7.325 to 0.713,8.075 to 4.925,8.075 to 4.925,7.325 to 0.713,7.325
line from 1.238,8.075 to 1.238,8.637
line from 1.262,8.537 to 1.238,8.637 to 1.213,8.537
line from 1.613,8.825 to 1.975,8.575
line from 1.878,8.611 to 1.975,8.575 to 1.907,8.652
line from 2.675,8.575 to 4.050,8.575
line from 3.950,8.550 to 4.050,8.575 to 3.950,8.600
line from 2.675,9.137 to 3.050,9.137
line from 2.950,9.112 to 3.050,9.137 to 2.950,9.162
line from 3.425,9.325 to 3.425,9.637
line from 3.450,9.537 to 3.425,9.637 to 3.400,9.537
line from 1.613,8.825 to 1.925,9.137
line from 1.872,9.049 to 1.925,9.137 to 1.837,9.084
.ps 11
"'BSD'" at 2.138,9.481 ljust
.ps 11
"1.2" at 1.113,9.543 ljust
.ps 11
"1.1" at 1.125,8.831 ljust
.ps 11
"1.1.1.1" at 4.175,8.543 ljust
.ps 11
"1.1.1.2" at 4.175,9.281 ljust
.ps 11
"1.1.1.3" at 4.175,9.993 ljust
.ps 11
"1.1.2.2" at 3.175,9.793 ljust
.ps 11
"1.1.2.1" at 3.175,9.106 ljust
.ps 11
"rcsfile.c,v" at 2.425,7.706 ljust
.ps 11
"1.1.1" at 2.175,8.568 ljust
.ps 11
"'SunOS'" at 2.125,8.243 ljust
.ps 11
"1.1.2" at 2.163,9.131 ljust
.PE
.DE
.hl
.ce 100
.LG
\fBFigure 4.\fP
.SM
\fBcvs\fP Support For \*QDual\*U Derivations
.ce 0
.sp
.KE
This example tracks the SunOS distribution but includes major changes from
Berkeley.
These BSD files are saved directly in the
.SM
RCS
.LG
file off a new branch.
.NH 2
Location Independent Module Database
.PP
\fBcvs\fP contains support for a simple, yet powerful, \*Qmodule\*U database.
For reasons of efficiency, this database is stored in \fBndbm\fP\|(3) format.
The module database is used to apply names to collections of directories
and files as a matter of convenience for checking out pieces of a large
software distribution.
The database records the physical location of the sources as a form of
information hiding, allowing one to check out whole directory hierarchies
or individual files without regard for their actual location within the
global source distribution.
.PP
Consider the following small sample of a module database, which must be
tailored manually to each specific source repository environment:
.DS
\f(CW		#key      [-option argument] directory [files...]
		diff      bin/diff
		libc      lib/libc
		sys       -o sys/tools/make_links sys
		modules   -i mkmodules CVSROOT.adm modules
		kernel    -a sys lang/adb
		ps        bin Makefile ps.c\fP
.DE
.PP
The \*Qdiff\*U and \*Qlibc\*U modules refer to whole directory hierarchies that
are extracted on check out.
The \*Qsys\*U module extracts the \*Qsys\*U hierarchy, and runs the
\*Qmake_links\*U program at the end of the check out process (the \fI-o\fP
option specifies a program to run on check\fIo\fPut).
The \*Qmodules\*U module allows one to edit the module database file and
runs the \*Qmkmodules\*U program on check\fIi\fPn to regenerate the
\fBndbm\fP database that \fBcvs\fP uses.
The \*Qkernel\*U module is an alias (as the \fI-a\fP option specifies)
which causes the remaining arguments after the \fI-a\fP to be interpreted
exactly as if they had been specified on the command line.
This is useful for objects that require shared pieces of code from far away
places to be compiled (as is the case with the kernel debugger, \fBkadb\fP,
which shares code with the standard \fBadb\fP debugger).
The \*Qps\*U module shows that the source for \*Qps\*U lives in the \*Qbin\*U
directory, but only \fIMakefile\fP and \fIps.c\fP are required to build the
object.
.PP
The module database at Prisma is now populated for the entire UNIX
distribution and thereby allows us to issue the
following convenient commands to check out components of the UNIX
distribution without regard for their actual location within the master source
repository:
.DS
\f(CW		example% cvs checkout diff
		example% cvs checkout libc ps
		example% cd diff; make\fP
.DE
.PP
In building the module database file, it is quite possible to have name
conflicts within a global software distribution.
For example, SunOS provides two \fBcat\fP programs:
one for the standard environment, \fI/bin/cat\fP, and one for the System V
environment, \fI/usr/5bin/cat\fP.
We resolved this conflict by naming the standard \fBcat\fP module
\*Qcat\*U, and the System V \fBcat\fP module \*Q5cat\*U.
Similar name modifications must be applied to other conflicting names, as
might be found between a utility program and a library function, though
Prisma chose not to include individual library functions within the module
database at this time.
.NH 2
Configurable Logging Support
.PP
The \fBcvs\fP \*Qcommit\*U command is used to make a permanent change to the
master source repository (where the
.SM
RCS
.LG
\*Q,v\*U files live).
Whenever a \*Qcommit\*U is done, the log message for the change is carefully
logged by an arbitrary program (in a file, notesfile, news database, or
mail).
For example, a collection of these updates can be used to produce release
notices.
\fBcvs\fP can be configured to send log updates through one or more filter
programs, based on a regular expression match on the directory that is
being changed.
This allows multiple related or unrelated projects to exist within a single
\fBcvs\fP source repository tree, with each different project sending its
\*Qcommit\*U reports to a unique log device.
.PP
A sample logging configuration file might look as follows:
.DS
\f(CW	#regex      filter-program
	DEFAULT     /usr/local/bin/nfpipe -t %s utils.updates
	^diag       /usr/local/bin/nfpipe -t %s diag.updates
	^local      /usr/local/bin/nfpipe -t %s local.updates
	^perf       /usr/local/bin/nfpipe -t %s perf.updates
	^sys        /usr/local/bin/nfpipe -t %s kernel.updates\fP
.DE
.PP
This sample allows the diagnostics and performance groups to
share the same source repository with the kernel and utilities groups.
Changes that they make are sent directly to their own notesfile [Essick]
through the \*Qnfpipe\*U program.
A sufficiently simple title is substituted for the \*Q%s\*U argument before
the filter program is executed.
This logging configuration file is tailored manually to each specific
source repository environment.
.NH 2
Tagged Releases and Dates
.PP
Any release can be given a symbolic tag name that is stored directly in the
.SM
RCS
.LG
files.
This tag can be used at any time to get an exact copy of any previous
release.
With equal ease, one can also extract an exact copy of the source files as
of any arbitrary date in the past as well.
Thus, all that's required to tag the current kernel, and to tag the kernel
as of the Fourth of July is:
.DS
\f(CW	example% cvs tag TEST_KERNEL kernel
	example% cvs tag -D 'July 4' PATRIOTIC_KERNEL kernel\fP
.DE
The following command would retrieve an exact copy of the test kernel at
some later date:
.DS
\f(CW	example% cvs checkout -fp -rTEST_KERNEL kernel\fP
.DE
The \fI-f\fP option causes only files that match the specified tag to be
extracted, while the \fI-p\fP option automatically prunes empty directories.
Consequently, directories added to the kernel after the test kernel was
tagged are not included in the newly extracted copy of the test kernel.
.PP
The \fBcvs\fP date support has exactly the same interface as that provided
with
.SM
RCS\c
.LG
, however \fBcvs\fP must process the \*Q,v\*U files directly due to the
special handling required by the vendor branch support.
The standard
.SM
RCS
.LG
date handling only processes one branch (or the main trunk) when checking
out based on a date specification.
\fBcvs\fP must instead process the current \*Qhead\*U branch and, if a
match is not found, proceed to look for a match on the vendor branch.
This, combined with reasons of performance, is why \fBcvs\fP processes
revision (symbolic and numeric) and date specifications directly from the
\*Q,v\*U files.
.NH 2
Building \*Qpatch\*U Source Distributions
.PP
\fBcvs\fP can produce a \*Qpatch\*U format [Wall] output file which can be
used to bring a previously released software distribution current with the
newest release.
This patch file supports an entire directory hierarchy within a single
patch, as well as being able to add whole new files to the previous
release.
One can combine symbolic revisions and dates together to display changes in
a very generic way:
.DS
\f(CW	example% cvs patch -D 'December 1, 1988' \e
	                   -D 'January 1, 1989' sys\fP
.DE
This example displays the kernel changes made in the month of December,
1988.
To release a patch file, for example, to take the \fBcvs\fP distribution
from version 1.0 to version 1.4 might be done as follows:
.DS
\f(CW	example% cvs patch -rCVS_1_0 -rCVS_1_4 cvs\fP
.DE
.NH
CVS Experience
.NH 2
Statistics
.PP
A quick summary of the scale that \fBcvs\fP is addressing today
can be found in Table 1.
.KF
.TS
box center tab(:);
c s
c s
c | c
l | n .
\fB\s+2Revision Control Statistics at Prisma
as of 11/11/89\fP\s-2
_
How Many...:Total
=
Files:17243
Directories:1005
Lines of code:3927255
Removed files:131
Software developers:14
Software groups:6
Megabytes of source:128
.TE
.ce 100
.LG
\fBTable 1.\fP
.SM
\fBcvs\fP Statistics
.ce 0
.sp .3
.KE
Table 2 shows the history of files changed or added and the number
of source lines affected by the change at Prisma.
Only changes made to the kernel sources are included.
.KF
.TS
box center tab(:);
c s s s s
c s s s s
c || c | c || c | c
c || c | c || c | c
l || n | n || n | n.
\fB\s+2Prisma Kernel Source File Changes
By Month, 1988-1989\fP\s-2
_
Month:# Changed:# Lines:# Added:# Lines
\^:Files:Changed:Files:Added
=
Dec:87:3619:68:9266
Jan:39:4324:0:0
Feb:73:1578:5:3550
Mar:99:5301:18:11461
Apr:112:7333:11:5759
May:138:5371:17:13986
Jun:65:2261:27:12875
Jul:34:2000:1:58
Aug:65:6378:8:4724
Sep:266:23410:113:39965
Oct:22:621:1:155
Total:1000:62196:269:101799
.TE
.ce 100
.LG
\fBTable 2.\fP
.SM
\fBcvs\fP Usage History for the Kernel
.ce 0
.sp
.KE
The large number of source file changes made in September are the result of
merging the SunOS 4.0.3 sources into the kernel.
This merge process is described in section 3.3.
.NH 2
Performance
.PP
The performance of \fBcvs\fP is currently quite reasonable.
Little effort has been expended on tuning \fBcvs\fP, although performance
related decisions were made during the \fBcvs\fP design.
For example, \fBcvs\fP parses the
.SM
RCS
.LG
\*Q,v\*U files directly instead of running an
.SM
RCS
.LG
process.
This includes following branches as well as integrating with the vendor
source branches and the main trunk when checking out files based on a date.
.PP
Checking out the entire kernel source tree (1223 files/59 directories)
currently takes 16 wall clock minutes on a Sun-4/280.
However, bringing the tree up-to-date with the current kernel sources, once
it has been checked out, takes only 1.5 wall clock minutes.
Updating the \fIcomplete\fP 128 MByte source tree under \fBcvs\fP control
(17243 files/1005 directories) takes roughly 28 wall clock minutes and
utilizes one-third of the machine.
For now this is entirely acceptable; improvements on these numbers will
possibly be made in the future.
.NH 2
The SunOS 4.0.3 Merge
.PP
The true test of the \fBcvs\fP vendor branch support came with the arrival
of the SunOS 4.0.3 source upgrade tape.
As described above, the \fBcheckin\fP program was used to install the new
sources and the resulting output file listed the files that had been
locally modified, needing to be merged manually.
For the kernel, there were 94 files in conflict.
The \fBcvs\fP \*Qjoin\*U command was used on each of the 94 conflicting
files, and the remaining conflicts were resolved.
.PP
The \*Qjoin\*U command performs an \fBrcsmerge\fP operation.
This in turn uses \fI/usr/lib/diff3\fP to produce a three-way diff file.
As it happens, the \fBdiff3\fP program has a hard-coded limit of 200
source-file changes maximum.
This proved to be too small for a few of the kernel files that needed
merging by hand, due to the large number of local changes that Prisma had
made.
The \fBdiff3\fP problem was solved by increasing the hard-coded limit by an
order of magnitude.
.PP
The SunOS 4.0.3 kernel source upgrade distribution contained
346 files, 233 of which were modifications to previously released files,
and 113 of which were newly added files.
\fBcheckin\fP added the 113 new files to the source repository
without intervention.
Of the 233 modified files, 139 dropped in cleanly by \fBcheckin\fP, since
Prisma had not made any local changes to them, and 94 required manual
merging due to local modifications.
The 233 modified files consisted of 20,766 lines of differences.
It took one developer two days to manually merge the 94 files using the
\*Qjoin\*U command and resolving conflicts manually.
An additional day was required for kernel debugging.
The entire process of merging over 20,000 lines of differences was
completed in less than a week.
This one time-savings alone was justification enough for the \fBcvs\fP
development effort; we expect to gain even more when tracking future SunOS
releases.
.NH
Future Enhancements and Current Bugs
.PP
Since \fBcvs\fP was designed to be incomplete, for reasons of design
simplicity, there are naturally a good
number of enhancements that can be made to make it more useful.
As well, some nuisances exist in the current implementation.
.RS
.IP \(bu 3
\fBcvs\fP does not currently \*Qremember\*U who has a checked out a copy of a
module.
As a result, it is impossible to know who might be working on the same
module that you are.
A simple-minded database that is updated nightly would likely suffice.
.IP \(bu 3
Signal processing, keyboard interrupt handling in particular, is currently
somewhat weak.
This is due to the heavy use of the \fBsystem\fP\|(3) library
function to execute
.SM
RCS
.LG
programs like \fBco\fP and \fBci\fP.
It sometimes takes multiple interrupts to make \fBcvs\fP quit.
This can be fixed by using a home-grown \fBsystem\fP\|() replacement.
.IP \(bu 3
Security of the source repository is currently not dealt with directly.
The usual UNIX approach of user-group-other security permissions through
the file system is utilized, but nothing else.
\fBcvs\fP could likely be a set-group-id executable that checks a
protected database to verify user access permissions for particular objects
before allowing any operations to affect those objects.
.IP \(bu 3
With every checked-out directory, \fBcvs\fP maintains some administrative
files that record the current revision numbers of the checked-out files as
well as the location of the respective source repository.
\fBcvs\fP does not recover nicely at all if these administrative files are
removed.
.IP \(bu 3
The source code for \fBcvs\fP has been tested extensively on Sun-3 and
Sun-4 systems, all running SunOS 4.0 or later versions of the operating
system.
Since the code has not yet been compiled under other platforms, the overall
portability of the code is still questionable.
.IP \(bu 3
As witnessed in the previous section, the \fBcvs\fP method for tracking
third party vendor source distributions can work quite nicely.
However, if the vendor changes the directory structure or the file names
within the source distribution, \fBcvs\fP has no way of matching the old
release with the new one.
It is currently unclear as to how to solve this, though it is certain to
happen in practice.
.RE
.NH
Availability
.PP
The \fBcvs\fP program sources can be found in a recent posting to the
\fBcomp.sources.unix\fP newsgroup.
It is also currently available via anonymous ftp from \*Qprisma.com\*U.
Copying rights for \fBcvs\fP will be covered by the GNU General Public
License.
.NH
Summary
.PP
Prisma has used \fBcvs\fP since December, 1988.
It has evolved to meet our specific needs of revision and release control.
We will make our code freely available so that others can
benefit from our work, and can enhance \fBcvs\fP to meet broader needs yet.
.PP
Many of the other software release and revision control systems, like the
one described in [Glew], appear to use a collection of tools that are
geared toward specific environments \(em one set of tools for the kernel,
one set for \*Qgeneric\*U software, one set for utilities, and one set for
kernel and utilities.
Each of these tool sets apparently handle some specific aspect of the
problem uniquely.
\fBcvs\fP took a somewhat different approach.
File sharing through symbolic or hard links is not addressed; instead, the
disk space is simply burned since it is \*Qcheap.\*U
Support for producing objects for multiple architectures is not addressed;
instead, a parallel checked-out source tree must be used for each
architecture, again wasting disk space to simplify complexity and ease of
use \(em punting on this issue allowed \fIMakefile\fPs to remain
unchanged, unlike the approach taken in [Mahler], thereby maintaining closer
compatibility with the third-party vendor sources.
\fBcvs\fP is essentially a source-file server, making no assumptions or
special handling of the sources that it controls.
To \fBcvs\fP:
.QP
A source is a source, of course, of course, unless of course the source is
Mr. Ed.\**
.FS
\fBcvs\fP, of course, does not really discriminate against Mr. Ed.\**
.FE
.FS
Yet.
.FE
.LP
Sources are maintained, saved, and retrievable at any time based on
symbolic or numeric revision or date in the past.
It is entirely up to \fBcvs\fP wrapper programs to provide for release
environments and such.
.PP
The major advantage of \fBcvs\fP over the
many other similar systems that have already been designed is the
simplicity of \fBcvs\fP.
\fBcvs\fP contains only three programs that do all the work of release
and revision control, and two manually-maintained administrative
files for each source repository.
Of course, the deciding factor of any tool is whether people use it, and if
they even \fIlike\fP to use it.
At Prisma, \fBcvs\fP prevented members of the kernel
group from killing each other.
.NH
Acknowledgements
.PP
Many thanks to Dick Grune at Vrije Universiteit in Amsterdam for his work
on the original version of \fBcvs\fP and for making it available to the
world.
Thanks to Jeff Polk of Prisma for helping with the design of the module
database, vendor branch support, and for writing the \fBcheckin\fP shell
script.
Thanks also to the entire software group at Prisma for taking the
time to review the paper and correct my grammar.
.NH
References
.IP [Bell] 12
Bell Telephone Laboratories.
\*QSource Code Control System User's Guide.\*U
\fIUNIX System III Programmer's Manual\fP, October 1981.
.IP [Courington] 12
Courington, W.
\fIThe Network Software Environment\fP,
Sun Technical Report FE197-0, Sun Microsystems Inc, February 1989.
.IP [Essick] 12
Essick, Raymond B. and Robert Bruce Kolstad.
\fINotesfile Reference Manual\fP,
Department of Computer Science Technical Report #1081,
University of Illinois at Urbana-Champaign, Urbana, Illinois,
1982, p. 26.
.IP [Glew] 12
Glew, Andy.
\*QBoxes, Links, and Parallel Trees:
Elements of a Configuration Management System.\*U
\fIWorkshop Proceedings of the Software Management Conference\fP, USENIX,
New Orleans, April 1989.
.IP [Grune] 12
Grune, Dick.
Distributed the original shell script version of \fBcvs\fP in the
\fBcomp.sources.unix\fP volume 6 release in 1986.
.IP [Honda] 12
Honda, Masahiro and Terrence Miller.
\*QSoftware Management Using a CASE Environment.\*U
\fIWorkshop Proceedings of the Software Management Conference\fP, USENIX,
New Orleans, April 1989.
.IP [Mahler] 12
Mahler, Alex and Andreas Lampen.
\*QAn Integrated Toolset for Engineering Software Configurations.\*U
\fIProceedings of the ACM SIGSOFT/SIGPLAN Software Engineering Symposium on
Practical Software Development Environments\fP, ACM, Boston, November 1988.
Described is the \fBshape\fP toolkit posted to the
\fBcomp.sources.unix\fP newsgroup in the volume 19 release.
.IP [Tichy] 12
Tichy, Walter F.
\*QDesign, Implementation, and Evaluation of a Revision Control System.\*U
\fIProceedings of the 6th International Conference on Software
Engineering\fP, IEEE, Tokyo, September 1982.
.IP [Wall] 12
Wall, Larry.
The \fBpatch\fP program is an indispensable tool for applying a diff file
to an original.
Can be found on uunet.uu.net in ~ftp/pub/patch.tar.
