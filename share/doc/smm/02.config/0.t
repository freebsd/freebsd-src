.\" Copyright (c) 1983 The Regents of the University of California.
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
.\"	@(#)0.t	6.4 (Berkeley) 4/17/91
.\"
.nr GA 0
.bd S B 3
.de UX
.ie \\n(GA>0 \\$2UNIX\\$1
.el \{\
.if n \\$2UNIX\\$1*
.if t \\$2UNIX\\$1\\f1\(dg\\fP
.FS
.if n *UNIX is a Trademark of Bell Laboratories.
.if t \(dgUNIX is a Trademark of Bell Laboratories.
.FE
.nr GA 1\}
..
.TL
Building Berkeley 
.UX
Kernels with Config
.AU
Samuel J. Leffler and Michael J. Karels
.AI
Computer Systems Research Group
Department of Electrical Engineering and Computer Science
University of California, Berkeley
Berkeley, California  94720
.de IR
\fI\\$1\fP\\$2
..
.de DT
.TA 8 16 24 32 40 48 56 64 72 80
..
.AB
.PP
This document describes the use of
\fIconfig\fP\|(8) to configure and create bootable
4.3BSD system images.
It discusses the structure of system
configuration files and how to configure
systems with non-standard hardware configurations.
Sections describing the preferred way to
add new code to the system and how the system's autoconfiguration
process operates are included.  An appendix
contains a summary of the rules used by the system
in calculating the size of system data structures,
and also indicates some of the standard system size
limitations (and how to change them).
Other configuration options are also listed.
.sp
.LP
Revised April 17, 1991
.AE
.LP
.OH 'Building Kernels with Config''SMM:2-%'
.EH 'SMM:2-%''Building Kernels with Config'
