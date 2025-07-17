.\" Copyright (c) 1983, 1993
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
.\" 3. Neither the name of the University nor the names of its contributors
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
.NH 1
Access control
.PP
The printer system maintains protected spooling areas so that
users cannot circumvent printer accounting or
remove files other than their own.
The strategy used to maintain protected
spooling areas is as follows:
.IP \(bu 3
The spooling area is writable only by a \fIdaemon\fP user
and \fIdaemon\fP group.
.IP \(bu 3
The \fIlpr\fP program runs set-user-id to \fIroot\fP and
set-group-id to group \fIdaemon\fP.  The \fIroot\fP access permits
reading any file required. Accessibility is verified
with an \fIaccess\fP\|(2) call.  The group ID
is used in setting up proper ownership of files
in the spooling area for \fIlprm\fP.
.IP \(bu 3
Control files in a spooling area are made with \fIdaemon\fP
ownership and group ownership \fIdaemon\fP.  Their mode is 0660.
This insures control files are not modified by a user
and that no user can remove files except through \fIlprm\fP.
.IP \(bu 3
The spooling programs,
\fIlpd\fP, \fIlpq\fP, and \fIlprm\fP run set-user-id to \fIroot\fP
and set-group-id to group \fIdaemon\fP to access spool files and printers.
.IP \(bu 3
The printer server, \fIlpd\fP,
uses the same verification procedures as \fIrshd\fP\|(8C)
in authenticating remote clients.  The host on which a client
resides must be present in the file /etc/hosts.equiv or /etc/hosts.lpd and
the request message must come from a reserved port number.
.PP
In practice, none of \fIlpd\fP, \fIlpq\fP, or
\fIlprm\fP would have to run as user \fIroot\fP if remote
spooling were not supported.  In previous incarnations of
the printer system \fIlpd\fP ran set-user-id to \fIdaemon\fP,
set-group-id to group \fIspooling\fP, and \fIlpq\fP and \fIlprm\fP ran
set-group-id to group \fIspooling\fP.
