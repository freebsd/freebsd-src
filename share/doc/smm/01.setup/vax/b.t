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
.\"	@(#)b.t	6.5 (Berkeley) 5/7/91
.\"
.de IR
\fI\\$1\fP\|\\$2
..
.ds LH "Installing/Operating \*(4B
.nr H1 6
.nr H2 0
.ds RH "Appendix B \- loading the tape monitor
.ds CF \*(DY
.bp
.LG
.B
.ce
APPENDIX B \- LOADING THE TAPE MONITOR
.sp 2
.R
.NL
.PP
This section describes how the bootstrap monitor located on
the first tape of the distribution tape set may be loaded.
This should not be necessary, but has been included as a fallback
measure if it is not possible to read the distributed
console medium.  \fBWARNING\fP:  the bootstraps supplied below
may not work, in certain instances on an 11/730 because they
use a buffered data path for transferring data from tape to
memory; consult our group if you are unable to load the 
monitor on an 11/730.  All of the addresses given below refer
to the first SBIA on the 8600.
.PP
To load the tape bootstrap monitor, first
mount the magnetic tape on drive 0 at load point, making
sure that the write ring is not inserted.
Temporarily
set the reboot switch on an 11/780 or 11/730 to off;
on an 8600 or 11/750 set the power-on action to halt.
(In normal operation an 11/785, 11/780, or 11/730
will have the reboot switch on,
and an 8600 or 11/750 will have the power-on action
set to boot/restart.)
.PP
If you have an 8600 or 11/780 give the commands:
.RT
.DS
\fB>>>\fP\fIHALT\fP
\fB>>>\fP\fIUNJAM\fP
.DE
Then, on any machine, give the init command:
.DS
\fB>>>\fP\fII\fP
.DE
and then
key in at location 200 and execute either the TS, HT, TM, or MT
bootstrap that follows, as appropriate.  \fBNOTE\fP: All of the addresses
given in this section refer to the first SBIA on the 8600.
Also, the VAX 8200 console does not accept
the ``D +'' command, so the second command becomes ``D 204 D05A0000'',
the third ``D 208 3BEF'', the fourth ``D 20C 800CA00'', the fifth
``D 210 32EFC1'', and so forth.  Alternatively, you could
try booting BOOT58 via ``B/R5:800'' with a diagnostic floppy.
.PP
The machine's printouts are shown in boldface,
explanatory comments are within ( ).
You can use `delete' to erase a character and `control U' to kill the
whole line.
.br
.ne 5
.sp
.ID
.nf
TS bootstrap

\fB>>>\|\fPD/P 200 3AEFD0
\fB>>>\|\fPD + D05A0000
\fB>>>\|\fPD + 3BEF
\fB>>>\|\fPD + 800CA00
\fB>>>\|\fPD + 32EFC1
\fB>>>\|\fPD + CA010000
\fB>>>\|\fPD + EFC10804
\fB>>>\|\fPD + 24
\fB>>>\|\fPD + 15508F
\fB>>>\|\fPD + ABB45B00
\fB>>>\|\fPD + 2AB9502
\fB>>>\|\fPD + 8FB0FB18
\fB>>>\|\fPD + 956B024C
\fB>>>\|\fPD + FB1802AB
\fB>>>\|\fPD + 25C8FB0
\fB>>>\|\fPD + 6B
        (The next two deposits set up the addresses of the UNIBUS)
        (adapter and its memory; the 20006000 here is the address of)
        (uba0 and the 2013E000 the address of the I/O page, umem0)
	(on an 8600 or 11/780)
\fB>>>\|\fPD + 20006000		(8600/780 uba0)
	(8600/780 uba1: 20008000, uba2 2000A000)
	(8200 uba at node 0: 20000000)
	(750 uba0: F30000, uba1: F32000; 730 uba: F26000)
\fB>>>\|\fPD + 2013E000		(8600/780 umem0)
	(8600/780 umem1: 2017E000, umem2: 201BE000)
	(8200 umem at node 0: 20400000)
	(750 umem0: FFE000, umem1: FBE000; 730 umem: FFE000)
\fB>>>\|\fPD + 80000000
\fB>>>\|\fPD + 254C004
\fB>>>\|\fPD + 80000
\fB>>>\|\fPD + 264
\fB>>>\|\fPD + E
\fB>>>\|\fPD + C001
\fB>>>\|\fPD + 2000000
\fB>>>\|\fPS 200

	N.B.: uba and umem addresses can be determined algorithmically
	on 8200 machines as follows:
		uba(\fInode\fP) = 20000000 + (2000 * \fInode\fP)
		umem(\fInode\fP) = 20400000 + (40000 * \fInode\fP)

HT bootstrap

\fB>>>\|\fPD/P 200 3EEFD0
\fB>>>\|\fPD + C55A0000
\fB>>>\|\fPD + 3BEF
\fB>>>\|\fPD + 808F00
\fB>>>\|\fPD + C15B0000
\fB>>>\|\fPD + C05B5A5B
\fB>>>\|\fPD + 4008F
\fB>>>\|\fPD + D05B00
\fB>>>\|\fPD + 9D004AA
\fB>>>\|\fPD + C08F326B
\fB>>>\|\fPD + D424AB14
\fB>>>\|\fPD + 8FD00CAA
\fB>>>\|\fPD + 80000000
\fB>>>\|\fPD + 320800CA
\fB>>>\|\fPD + AAFE008F
\fB>>>\|\fPD + 6B39D010
\fB>>>\|\fPD + 0
        (The next two deposits set up the addresses of the MASSBUS)
        (adapter and the drive number for the tape formatter)
        (the 20010000 here is the address of mba0 on an 8600 or)
	(11/780 and the 0 indicates the formatter is drive 0 on mba0)
\fB>>>\|\fPD + 20010000		(8600/780 mba0)
	(8600/780 mba1: 20012000; 750 mba0: F28000, mba1: F2A000)
\fB>>>\|\fPD + 0				(Formatter unit number in range 0-7)
\fB>>>\|\fPS 200
\fB>>>\|\fPS 200

TM bootstrap

\fB>>>\|\fPD/P 200 2AEFD0
\fB>>>\|\fPD + D0510000
\fB>>>\|\fPD + 2000008F
\fB>>>\|\fPD + 800C180
\fB>>>\|\fPD + 804C1D4
\fB>>>\|\fPD + 1AEFD0
\fB>>>\|\fPD + C8520000
\fB>>>\|\fPD + F5508F
\fB>>>\|\fPD + 8FAE5200
\fB>>>\|\fPD + 4A20200
\fB>>>\|\fPD + B006A2B4
\fB>>>\|\fPD + 2A203
        (The following two numbers are uba0 and umem0 on a 8600/780)
	(See TS above for values for other CPU's and UBA's)
\fB>>>\|\fPD + 20006000		(8600/780 uba0)
\fB>>>\|\fPD + 2013E000		(8600/780 umem0)
\fB>>>\|\fPS 200
\fB>>>\|\fPS 200
\fB>>>\|\fPS 200

MT bootstrap

\fB>>>\|\fPD/P 200 46EFD0
\fB>>>\|\fPD + C55A0000
\fB>>>\|\fPD + 43EF
\fB>>>\|\fPD + 808F00
\fB>>>\|\fPD + C15B0000
\fB>>>\|\fPD + C05B5A5B
\fB>>>\|\fPD + 4008F
\fB>>>\|\fPD + 19A5B00
\fB>>>\|\fPD + 49A04AA
\fB>>>\|\fPD + AAD408AB
\fB>>>\|\fPD + 8FD00C
\fB>>>\|\fPD + CA800000
\fB>>>\|\fPD + 8F320800
\fB>>>\|\fPD + 10AAFE00
\fB>>>\|\fPD + 2008F3C
\fB>>>\|\fPD + ABD014AB
\fB>>>\|\fPD + FE15044
\fB>>>\|\fPD + 399AF850
\fB>>>\|\fPD + 6B
        (The next two deposits set up the addresses of the MASSBUS)
        (adapter and the drive number for the tape formatter)
        (the 20012000 here is the address of mba1 on an 8600 or)
	(11/780 and the 0 indicates the formatter is drive 0 on mba1)
\fB>>>\|\fPD + 20012000
\fB>>>\|\fPD + 0
\fB>>>\|\fPS 200
\fB>>>\|\fPS 200
\fB>>>\|\fPS 200
\fB>>>\|\fPS 200
.fi
.sp
(no functioning toggle-in code exists for the UT device)
.DE
.PP
If the tape doesn't move the first time you start the bootstrap
program with ``S 200'' you probably have entered the program
incorrectly (but also check that the tape is online).
Start over and check your typing.
For the HT, MT, and TM bootstraps you will not be able to see the
tape motion as you advance through the first few blocks
as the tape motion is all within the vacuum columns.
.PP
.\"XXX this next step is unnecessary! the register are still correct,
.\" at least for the TS bootstrap XXX
Next, deposit in R10 the address of the tape MBA/UBA and in R11 the
address of the device registers or unit number from one of:
.DS
.TS
lw(1.5i) l.
\fB>>>\|\fPD/G A 20006000	(for tapes on 8600/780 uba0)
\fB>>>\|\fPD/G A 20008000	(for tapes on 8600/780 uba1)
\fB>>>\|\fPD/G A 20010000	(for tapes on 8600/780 mba0)
\fB>>>\|\fPD/G A 20012000	(for tapes on 8600/780 mba1)
\fB>>>\|\fPD/G A 20000000	(for tapes on 8200 uba at node 0)
\fB>>>\|\fPD/G A F30000	(for tapes on 750 uba0)
\fB>>>\|\fPD/G A F32000	(for tapes on 750 uba1)
\fB>>>\|\fPD/G A F28000	(for tapes on 750 mba0)
\fB>>>\|\fPD/G A F2A000	(for tapes on 750 mba1)
\fB>>>\|\fPD/G A F26000	(for tapes on 730 uba0)
.TE
.DE
and for register 11:
.DS
.TS
lw(1.5i) l.
\fB>>>\|\fPD/G B 0	(for TM03/TM78 formatters at mba? drive 0)
\fB>>>\|\fPD/G B 1	(for TM03/TM78 formatters at mba? drive 1)
\fB>>>\|\fPD/G B 2013F550	(for TM11/TS11/TU80 tapes on 8600/780 uba0)
\fB>>>\|\fPD/G A 20400000	(for TM11/TS11/TU80 on 8200 uba at node 0)
\fB>>>\|\fPD/G B FFF550	(for TM11/TS11/TU80 tapes on 750 or 730 uba0)
.TE
.DE
Then start the bootstrap program with
.DS
\fB>>>\|\fPS 0
.DE
.PP
The console should type
.DS
.I
\fB=\fP
.R
.DE
You are now talking to the tape bootstrap monitor.
At any point in the following procedure you can return
to this section, reload the tape bootstrap, and restart the
procedure.  The console monitor is identical to that
loaded from a TU58 console cassette, follow the instructions
in section 2 as they apply to this device.  The only
exception is that when using programs loaded from the
tape bootstrap monitor, programs will always return to
the monitor (the ``='' prompt).  This saves your having
to type in the above toggle-in code for each program to
be loaded.
