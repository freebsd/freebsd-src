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
.\"	@(#)a.t	6.5 (Berkeley) 5/7/91
.\"
.de IR
\fI\\$1\fP\|\\$2
..
.ds LH "Installing/Operating \*(4B
.\".nr H1 6
.\".nr H2 0
.ds RH "Appendix A \- bootstrap details
.ds CF \*(DY
.bp
.LG
.B
.ce
APPENDIX A \- BOOTSTRAP DETAILS
.sp 2
.R
.NL
.PP
This appendix contains pertinent files and numbers regarding the
bootstrapping procedure for \*(4B.  You should never have to
look at this appendix.  However, if there are problems in installing
the distribution on your machine, the material contained here may
prove useful.
.SH
Contents of the distribution tape(s)
.PP
The distribution normally consists of three 1600bpi 2400' magnetic
tapes or one 6250bpi 2400' magnetic tape.
The layout of the 1600bpi tapes is listed below.  The 6250bpi
tape is in the same order, but is only on one tape.
The first tape contains the following files on it.  All
tape files are blocked in 10 kilobytes records, except for the
first file on the first tape that has 512 byte records.
.DS L
.TS
l l l.
Tape file	Records*	Contents
_
one	210	8 bootstrap monitor programs and a
		\fItp\fP\|(1) file containing \fIboot\fP, \fIformat\fP, and \fIcopy\fP
two	205	``mini root'' file system
three	430	\fIdump\fP\|(8) of distribution root file system
four	3000	\fItar\fP\|(1) image of binaries and libraries in /usr
.TE
.FS
* The number of records in each tape file are approximate
and do not correspond to the actual tape.
.FE
.DE
The second tape contains the following files:
.DS L
.TS
l l l.
Tape file	# Records	Contents
_
one	720	\fItar\fP\|(1) image of /sys, including GENERIC system
two	2500	\fItar\fP\|(1) image of /usr/src
three	580	\fItar\fP\|(1) image of /usr/lib/vfont
.TE
.DE
The third tape contains the following files:
.DS L
.TS
l l l.
Tape file	# Records	Contents
_
one	3660	\fItar\fP\|(1) image of user contributed software
two	250	\fItar\fP\|(1) image of /usr/ingres
.TE
.DE
.PP
The distribution tape is made with the shell scripts located
in the directory /sys/dist.  To build a distribution tape
one must first create a mini root file system with the \fIbuildmini\fP
shell script.
.DS
#!/bin/sh
#	@(#)buildmini	4.7 (Berkeley) 6/23/85
#
miniroot=hp0d
minitype=rm80
#
date
umount /dev/${miniroot}
newfs -s 4096 ${miniroot} ${minitype}
fsck /dev/r${miniroot}
mount /dev/${miniroot} /mnt
cd /mnt; sh /sys/dist/get
cd /sys/dist; sync
umount /dev/${miniroot}
fsck /dev/${miniroot}
date
.DE
The \fIbuildmini\fP
script uses the \fIget\fP script to build the
file system.
.ID
#!/bin/sh
#
#	@(#)get	4.23 (Berkeley) 4/9/86
#
# Shell script to build a mini-root file system
# in preparation for building a distribution tape.
# The file system created here is image copied onto
# tape, then image copied onto disk as the "first"
# step in a cold boot of 4.2 systems.
#
DISTROOT=/nbsd
#
if [ `pwd` = '/' ]
then
	echo You just '(almost)' destroyed the root
	exit
fi
cp $DISTROOT/sys/GENERIC/vmunix .
rm -rf bin; mkdir bin
rm -rf etc; mkdir etc
rm -rf a; mkdir a
rm -rf tmp; mkdir tmp
rm -rf usr; mkdir usr usr/mdec
rm -rf sys; mkdir sys sys/floppy sys/cassette sys/consolerl
cp $DISTROOT/etc/disktab etc
cp $DISTROOT/etc/newfs etc; strip etc/newfs
cp $DISTROOT/etc/mkfs etc; strip etc/mkfs
cp $DISTROOT/etc/restore etc; strip etc/restore
cp $DISTROOT/etc/init etc; strip etc/init
cp $DISTROOT/etc/mount etc; strip etc/mount
cp $DISTROOT/etc/mknod etc; strip etc/mknod
cp $DISTROOT/etc/fsck etc; strip etc/fsck
cp $DISTROOT/etc/umount etc; strip etc/umount
cp $DISTROOT/etc/arff etc; strip etc/arff
cp $DISTROOT/etc/flcopy etc; strip etc/flcopy
cp $DISTROOT/bin/mt bin; strip bin/mt
cp $DISTROOT/bin/ls bin; strip bin/ls
cp $DISTROOT/bin/sh bin; strip bin/sh
cp $DISTROOT/bin/mv bin; strip bin/mv
cp $DISTROOT/bin/sync bin; strip bin/sync
cp $DISTROOT/bin/cat bin; strip bin/cat
cp $DISTROOT/bin/mkdir bin; strip bin/mkdir
cp $DISTROOT/bin/stty bin; strip bin/stty; ln bin/stty bin/STTY
cp $DISTROOT/bin/echo bin; strip bin/echo
cp $DISTROOT/bin/rm bin; strip bin/rm
cp $DISTROOT/bin/cp bin; strip bin/cp
cp $DISTROOT/bin/expr bin; strip bin/expr
cp $DISTROOT/bin/[ bin; strip bin/[
cp $DISTROOT/bin/awk bin; strip bin/awk
cp $DISTROOT/bin/make bin; strip bin/make
cp $DISTROOT/usr/mdec/* usr/mdec
cp $DISTROOT/sys/floppy/[Ma-z0-9]* sys/floppy
cp $DISTROOT/sys/consolerl/[Ma-z0-9]* sys/consolerl
cp -r $DISTROOT/sys/cassette/[Ma-z0-9]* sys/cassette
cp $DISTROOT/sys/stand/boot boot
cp $DISTROOT/sys/stand/pcs750.bin pcs750.bin
cp $DISTROOT/.profile .profile
cat >etc/passwd <<EOF
root::0:10::/:/bin/sh
EOF
cat >etc/group <<EOF
wheel:*:0:
staff:*:10:
EOF
cat >etc/fstab <<EOF
/dev/hp0a:/a:xx:1:1
/dev/up0a:/a:xx:1:1
/dev/hk0a:/a:xx:1:1
/dev/ra0a:/a:xx:1:1
/dev/rb0a:/a:xx:1:1
EOF
cat >xtr <<'EOF'
: ${disk?'Usage: disk=xx0 type=tt tape=yy xtr'}
: ${type?'Usage: disk=xx0 type=tt tape=yy xtr'}
: ${tape?'Usage: disk=xx0 type=tt tape=yy xtr'}
echo 'Build root file system'
newfs ${disk}a ${type}
sync
echo 'Check the file system'
fsck /dev/r${disk}a
mount /dev/${disk}a /a
cd /a
echo 'Rewind tape'
mt -f /dev/${tape}0 rew
echo 'Restore the dump image of the root'
restore rsf 3 /dev/${tape}0
cd /
sync
umount /dev/${disk}a
sync
fsck /dev/r${disk}a
echo 'Root filesystem extracted'
echo
echo 'If this is an 8650 or 8600, update the console rl02'
echo 'If this is a 780 or 785, update the floppy'
echo 'If this is a 730, update the cassette'
EOF
chmod +x xtr
rm -rf dev; mkdir dev
cp $DISTROOT/sys/dist/MAKEDEV dev
chmod +x dev/MAKEDEV
cp /dev/null dev/MAKEDEV.local
cd dev
\&./MAKEDEV std hp0 hk0 up0 ra0 rb0
\&./MAKEDEV ts0; mv rmt12 ts0; rm *mt*;
\&./MAKEDEV tm0; mv rmt12 tm0; rm *mt*;
\&./MAKEDEV ht0; mv rmt12 ht0; rm *mt*;
\&./MAKEDEV ut0; mv rmt12 ut0; rm *mt*;
\&./MAKEDEV mt0; mv rmt4 xt0; rm *mt*; mv xt0 mt0
cd ..
sync
.DE
The mini root file system must have enough space to hold the
files found on a floppy or cassette.
.PP
Once a mini root file system is constructed, the \fImaketape\fP
script makes a distribution tape.  
.ID
#!/bin/sh
#
#	@(#)maketape	4.27 (Berkeley) 10/17/85
#
#	maketape [ 6250 | 1600 [ tapename [ remotetapemachine ] ] ]
miniroot=hp0d
tape=/dev/rmt12
type=6250
if [ $# -gt 0 ]; then type=$1; fi
if [ $# -gt 1 ]; then tape=$2; fi
tartape=$tape
if [ $# -gt 2 ]; then remote=$3; tartape='-'; fi
#
trap "rm -f /tmp/tape.$$; exit" 0 1 2 3 13 15
$remote mt -t ${tape} rew
date
umount /dev/hp2g
umount /dev/hp2a
mount -r /dev/hp2a /c/nbsd
mount -r /dev/hp2g /c/nbsd/usr
cd tp
tp cmf /tmp/tape.$$ boot copy format
cd /nbsd/sys/mdec
echo "Build 1st level boot block file"
cat tsboot htboot tmboot mtboot utboot noboot noboot /tmp/tape.$$ | \e
	$remote dd of=${tape} obs=512 conv=sync
cd /nbsd
sync
echo "Add dump of mini-root file system"
eval dd if=/dev/r${miniroot} count=205 bs=20b conv=sync ${remote+'|'} \e
	${remote-"of=$tape"} ${remote+'/usr/local/20b ">" $tape'}
echo "Add full dump of real file system"
/etc/${remote+r}dump 0uf $remote${remote+:}${tape} /c/nbsd
echo "Add tar image of /usr"
cd /nbsd/usr; eval tar cf ${tartape} adm bin dict doc games \e
	guest hosts include lib local man mdec msgs new \e
	preserve pub spool tmp ucb \e
		${remote+'| $remote /usr/local/20b ">" $tape'}
if [ ${type} != '6250' ]
then
	echo "Done, rewinding first tape"
	$remote mt -t ${tape} rew &
	echo "Mount second tape and hit return when ready"
	echo "(or type name of next tape drive)"
	read x
	if [ "$x" != "" ]
	then	tape=$x
	fi
fi
echo "Add tar image of system sources"
cd /nbsd/sys; eval tar cf ${tartape} . \e
	${remote+'| $remote /usr/local/20b ">" $tape'}
echo "Add user source code"
cd /nbsd/usr/src; eval tar cf ${tartape} Makefile bin etc games \e
	include lib local old ucb undoc usr.bin usr.lib \e
	${remote+'| $remote /usr/local/20b ">" $tape'}
echo "Add varian fonts"
cd /usr/lib/vfont; eval tar cf ${tartape} . \e
	${remote+'| $remote /usr/local/20b ">" $tape'}
if [ ${type} != '6250' ]
then
	echo "Done, rewinding second tape"
	$remote mt -t ${tape} rew &
	echo "Mount third tape and hit return when ready"
	echo "(or type name of next tape drive)"
	read x
	if [ "$x" != "" ]
	then	tape=$x
	fi
fi
echo "Add user contributed software"
cd /nbsd/usr/src/new; eval tar cf ${tartape} * \e
	${remote+'| $remote /usr/local/20b ">" $tape'}
echo "Add ingres source"
cd /nbsd/usr/ingres; eval tar cf ${tartape} . \e
	${remote+'| $remote /usr/local/20b ">" $tape'}
echo "Done, rewinding tape"
$remote mt -t ${tape} rew &
.DE
.PP
Summarizing then, to create a distribution tape you can
use the above scripts and the following commands.
.DS
\fB#\fP \fIbuildmini\fP
\fB#\fP \fImaketape\fP
\&...
(For 1600bpi tapes, the following will appear twice asking you to mount
 fresh tapes)
\fBDone, rewinding first tape\fP
\fBMount second tape and hit return when ready\fP
(remove the first tape and place a fresh one on the drive)
\&...
\fBDone, rewinding second tape\fP
.DE
.SH
Control status register addresses
.PP
The distribution uses many standalone device drivers
that presume the location of a UNIBUS device's control status
register (CSR).
The following table summarizes these values.
.DS
.TS
l l l.
Device name	Controller	CSR address (octal)
_
ra	DEC UDA50	0172150
rb	DEC 730 IDC	0175606
rk	DEC RK11	0177440
rl	DEC RL11	0174400
tm	EMULEX TC-11	0172520
ts	DEC TS11	0172520
up	EMULEX SC-21V	0176700
ut	SI 9700	0172440
.TE
.DE
All MASSBUS controllers are located at standard offsets
from the base address of the MASSBUS adapter register bank.
BI bus controllers are located automatically.
.SH
Generic system control status register addresses
.PP
The 
.I generic
version of the operating system supplied with the distribution
contains the UNIBUS devices listed below. 
These devices will be recognized
if the appropriate control status registers respond at any of the
listed UNIBUS addresses.
.DS
.TS
l l l.
Device name	Controller	CSR addresses (octal)
_
hk	DEC RK11	0177440
tm	EMULEX TC-11	0172520
tmscp	DEC TU81, TMSCP	0174500
ts	DEC TS11	0172520
ut	SI 9700	0172440
up	EMULEX SC-21V	0176700, 0174400, 0176300
ra	DEC UDA-50	0172150, 0172550, 0177550
rb	DEC 730 IDC	0175606
rl	DEC RL11	0174400
dm	DM11 equivalent	0170500
dh	DH11 equivalent	0160020, 0160040
dhu	DEC DHU11	0160440, 0160500
dz	DEC DZ11	0160100, 0160110, ... 0160170
dmf	DEC DMF32	0160340
dmz	DEC DMZ32	0160540
lp	DEC LP11	0177514
en	Xerox 3MB ethernet	0161000
ec	3Com ethernet	0164330
ex	Excelan ethernet	0164344
il	Interlan ethernet	0164000
de	DEC DEUNA	0174510
.TE
.DE
If devices other than the above are located at any 
of the addresses listed, the system may not bootstrap
properly.
