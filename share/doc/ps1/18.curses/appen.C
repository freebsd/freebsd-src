.\" Copyright (c) 1980 The Regents of the University of California.
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
.\"	from: @(#)appen.C	6.3 (Berkeley) 4/17/91
.\"	appen.C,v 1.2 1993/08/01 18:23:16 mycroft Exp
.\"
.ie t .oh '\*(Ln Appendix C''PS1:18-%'
.eh 'PS1:18-%''\*(Ln Appendix C'
.el .he ''\fIAppendix C\fR''
.bp
.(x
.ti 0
.b "Appendix C"
.)x
.sh 1 "Examples" 1
.pp
Here we present a few examples
of how to use the package.
They attempt to be representative,
though not comprehensive.
.sh 1 "Screen Updating"
.pp
The following examples are intended to demonstrate
the basic structure of a program
using the screen updating sections of the package.
Several of the programs require calculational sections
which are irrelevant of to the example,
and are therefore usually not included.
It is hoped that the data structure definitions
give enough of an idea to allow understanding
of what the relevant portions do.
The rest is left as an exercise to the reader,
and will not be on the final.
.sh 2 "Twinkle"
.pp
This is a moderately simple program which prints
pretty patterns on the screen
that might even hold your interest for 30 seconds or more.
It switches between patterns of asterisks,
putting them on one by one in random order,
and then taking them off in the same fashion.
It is more efficient to write this
using only the motion optimization,
as is demonstrated below.
.(l I
.so twinkle1.gr
.)l
.sh 2 "Life"
.pp
This program fragment models the famous computer pattern game of life
(Scientific American, May, 1974).
The calculational routines create a linked list of structures
defining where each piece is.
Nothing here claims to be optimal,
merely demonstrative.
This code, however,
is a very good place to use the screen updating routines,
as it allows them to worry about what the last position looked like,
so you don't have to.
It also demonstrates some of the input routines.
.(l I
.so life.gr
.)l
.sh 1 "Motion optimization"
.pp
The following example shows how motion optimization
is written on its own.
Programs which flit from one place to another without
regard for what is already there
usually do not need the overhead of both space and time
associated with screen updating.
They should instead use motion optimization.
.sh 2 "Twinkle"
.pp
The
.b twinkle
program
is a good candidate for simple motion optimization.
Here is how it could be written
(only the routines that have been changed are shown):
.(l
.so twinkle2.gr
.)l
