.\" Copyright (c) 1986, 1993
.\"	The Regents of the University of California.  All rights reserved.
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
.\"	@(#)00.contents	8.1 (Berkeley) 7/5/93
.\" $FreeBSD: src/share/doc/smm/contents/contents.ms,v 1.1.10.1 2000/08/27 17:31:41 gshapiro Exp $
.\"
.OH '''SMM Contents'
.EH 'SMM Contents'''
.TL
UNIX System Manager's Manual (SMM)
.sp
\s-24.4 Berkeley Software Distribution\s+2
.sp
\fRJune, 1993\fR
.PP
This volume contains manual pages and supplementary documents useful to system
administrators.
The information in these documents applies to
the 4.4BSD system as distributed by U.C. Berkeley.
.SH
Reference Manual \- Section 8
.tl '''(8)'
.IP
Section 8 of the UNIX Programmer's Manual contains information related to
system operation, administration,  and maintenance.
.SH
System Installation and Administration
.IP
.tl 'Installing and Operating 4.4BSD''SMM:1'
.QP
The definitive reference document for those occasions when
you find you need to start over again.

.IP
.tl 'Building 4.4BSD Kernels with \fIConfig\fP''SMM:2'
.QP
In-depth discussions of the use and operation of the \fIconfig\fP
program, and how to build your very own Unix kernel.

.IP
.tl 'Fsck \- The UNIX File System Check Program''SMM:3'
.QP
A reference document for using the \fIfsck\fP program during
times of file system distress.

.IP
.tl 'Disc Quotas in a UNIX Environment''SMM:4'
.QP
A light introduction to the techniques
for limiting the use of disc resources.

.IP
.tl 'A Fast File System for UNIX''SMM:5'
.QP
A description of the 4.4BSD file system organization,
design and implementation.

.IP
.tl 'The 4.4BSD NFS Implementation''SMM:6'
.QP
An overview of the design, implementation, and use of NFS on 4.4BSD.

.IP
.tl 'Line Printer Spooler Manual''SMM:7'
.QP
This document describes the structure and installation procedure
for the line printer spooling system.  

.IP
.tl 'Sendmail Installation and Operation Guide''SMM:8'
.QP
The last word in installing and operating the \fIsendmail\fP program.

.ne 3
.IP
.tl 'Name Server Operations Guide for BIND''SMM:10'
.QP
Setting up and operating the name to Internet addressing software.
If you have a network this will be of interest.

.IP
.tl 'Timed Installation and Operation Guide''SMM:11'
.QP
Describes how to maintain time synchronization between machines
in a local network.

.IP
.tl 'The Berkeley UNIX Time Synchronization Protocol''SMM:12'
.QP
The protocols and algorithms used by timed,
the network time synchronization daemon.

.IP
.tl 'AMD \- The 4.4BSD Automounter''SMM:13'
.QP
Automatically mounting file systems on demand.

.IP
.tl 'Installation and Operation of UUCP''SMM:14'
.QP
Describes the implementation of uucp; for the installer and administrator.

.IP
.tl 'A Dial\-Up Network of UNIX Systems''SMM:15'
.QP
Describes UUCP, a program for communicating files between UNIX systems.

.IP
.tl 'On the Security of UNIX''SMM:16'
.QP
Hints on how to break UNIX, and how to avoid your system being broken.

.IP
.tl 'Password Security \- A Case History''SMM:17'
.QP
How the bad guys used to be able to break the password algorithm, and why
they cannot now (at least not so easily).

.IP
.tl 'Networking Implementation Notes, 4.4BSD Edition''SMM:18'
.QP
A concise description of the system interfaces used within the
networking subsystem.

.IP
.tl 'The PERL Programming Language''SMM:19'
.QP
The Practical Extraction and Report Language is ideal for
writing those pesky adminitration scripts.
