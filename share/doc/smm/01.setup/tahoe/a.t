.\" Copyright (c) 1988 The Regents of the University of California.
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
.\"	@(#)a.t	1.6 (Berkeley) 5/7/91
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
tape is in the same order, but on a single tape.
The first tape contains the following files on it:
.DS
.TS
l l l l.
Tape file	Block size	Records*	Contents
_
one	1024	600	file system containing \fIboot\fP, \fIvdformat\fP, and \fIcopy\fP
two	10240	205	``mini root'' file system
three	10240	578	\fIdump\fP\|(8) of distribution root file system
four	20480	1489	\fItar\fP\|(1) image of binaries and libraries in /usr
.TE
.DE
The second tape contains the following file:
.DS
.TS
l l l l.
Tape file	Block size	# Records	Contents
_
one	20480	1911	\fItar\fP\|(1) image of /usr/src
.\"three	580	\fItar\fP\|(1) image of /usr/lib/vfont
.TE
.DE
The third tape contains the following files:
.DS
.TS
l l l l.
Tape file	Block size	# Records	Contents
_
one	20480	415	\fItar\fP\|(1) image of /sys, including GENERIC system
two	20480	1577	\fItar\fP\|(1) image of user contributed software
.\"two	250	\fItar\fP\|(1) image of /usr/ingres
.TE
.DE
.KS
.PP
The distribution tape is made with the shell scripts located
in the directory /sys/tahoedist.  To build a distribution tape
one must first create a mini root file system with the \fIbuildmini\fP
shell script, and a boot file system with the \fIbuildboot\fP
shell script.
.de DS
.ne 2i
.nf
.in .5i
..
.de DE
.fi
.in
..
.DS
#!/bin/sh -
#
# Copyright (c) 1988 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of California, Berkeley.  The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)buildboot	1.4 (Berkeley) 7/29/88
#
boot=dk2b
boottype=xfd
bootmnt=/tmp/mini
DISTROOT=/nbsd

date
umount /dev/${boot}
newfs -s 1200 ${boot} ${boottype}
fsck /dev/r${boot}
mount /dev/${boot} ${bootmnt}
cp $DISTROOT/stand/copy ${bootmnt}
#cp $DISTROOT/stand/vdformat ${bootmnt}
cp $DISTROOT/boot ${bootmnt}
cp $DISTROOT/wcs ${bootmnt}
cp $DISTROOT/fppwcs ${bootmnt}
#cp $DISTROOT/poc ${bootmnt}
#cp $DISTROOT/poc1 ${bootmnt}
#cp $DISTROOT/poc2 ${bootmnt}
#cp $DISTROOT/fppoc ${bootmnt}
sync
umount /dev/${boot}
fsck /dev/${boot}
date
.DE
.KE
.DS
#!/bin/sh -
#
# Copyright (c) 1988 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of California, Berkeley.  The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)buildmini	1.5 (Berkeley) 7/28/88
#
dist=/sys/tahoedist
miniroot=dk4a
minimnt=/tmp/mini
#
date
umount /dev/${miniroot}
newfs -s 4096 ${miniroot}
fsck /dev/r${miniroot}
mount /dev/${miniroot} ${minimnt}
cd ${minimnt}; sh ${dist}/get
cd ${dist}; sync
umount /dev/${miniroot}
fsck /dev/${miniroot}
date
.DE
.PP
.DS
The \fIbuildmini\fP script uses the \fIget\fP script to build the file system.

#!/bin/sh -
#
# Copyright (c) 1988 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of California, Berkeley.  The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)get	1.5 (Berkeley) 7/7/88
#
# Shell script to build a mini-root file system in preparation for building
# a distribution tape.  The file system created here is image copied onto
# tape, then image copied onto disk as the "first" step in a cold boot of
# 4.3BSD systems.
#
DISTROOT=/nbsd
#
if [ `pwd` = '/' ]
then
	echo You just '(almost)' destroyed the root
	exit
fi

# copy in kernel
cp $DISTROOT/sys/GENERIC/vmunix .

# create necessary directories
DIRLIST="bin dev etc a tmp stand"
rm -rf $DIRLIST
mkdir $DIRLIST

# copy in files from /etc
ETCFILE="disklabel disktab fsck ifconfig init mknod mount newfs restore \e
	rrestore umount"
for i in $ETCFILE; do
	cp $DISTROOT/etc/$i etc/$i
done

# copy in files from /bin
BINFILE="[ awk cat cp dd echo ed expr ls make mkdir mt mv rcp rm sh stty \e
	sync"
for i in $BINFILE; do
	cp $DISTROOT/bin/$i bin/$i
done
ln bin/stty bin/STTY

# copy in files from /stand
STANDFILE="copy vdformat"
for i in $STANDFILE; do
	cp $DISTROOT/stand/$i stand/$i
done

# copy in files from /
#DOTFILE=".profile boot fppoc fppwcs poc poc1 poc2 wcs"
DOTFILE=".profile boot wcs"
for i in $DOTFILE; do
	cp $DISTROOT/$i $i
done

# initialize /dev
cp $DISTROOT/dev/MAKEDEV dev/MAKEDEV
chmod +x dev/MAKEDEV
cp /dev/null dev/MAKEDEV.local
(cd dev; ./MAKEDEV std dk0; ./MAKEDEV cy0; mv rmt12 cy0; rm *mt*)

# initialize /etc/passwd
cat >etc/passwd <<EOF
root::0:10::/:/bin/sh
EOF

# initialize /etc/group
cat >etc/group <<EOF
wheel:*:0:
staff:*:10:
EOF

# initialize /etc/fstab
cat >etc/fstab <<EOF
/dev/xfd0a:/a:xx:1:1
/dev/dk0a:/a:xx:1:1
EOF

# create xtr script
cat >xtr <<'EOF'
#!/bin/sh -e
: ${disk?'Usage: disk=xx0 tape=yy xtr'}
: ${tape?'Usage: disk=xx0 tape=yy xtr'}
echo 'Build root file system'
newfs ${disk}a
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
EOF

# make xtr script executable
chmod +x xtr

sync
.DE
.DS
Once a mini root file system is constructed, the \fImaketape\fP script makes a distribution tape.

#!/bin/sh -
#
# Copyright (c) 1988 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that the above copyright notice and this paragraph are
# duplicated in all such forms and that any documentation,
# advertising materials, and other materials related to such
# distribution and use acknowledge that the software was developed
# by the University of California, Berkeley.  The name of the
# University may not be used to endorse or promote products derived
# from this software without specific prior written permission.
# THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
# WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
#	@(#)maketape	5.5 (Berkeley) 7/29/88
#

# maketape [ 6250 | 1600 [ tapename [ remotetapemachine ] ] ]
miniroot=dk4a
bootroot=dk2b
nbsd=dk1a
nbsdusr=dk2c
tape=/dev/rmt20
type=6250
block=40
tflag=cbf
bprog="/usr/local/20b 20480"

if [ $# -gt 0 ]; then type=$1; fi
if [ $# -gt 1 ]; then tape=$2; fi
tartape=$tape
if [ $# -gt 2 ]; then remote=$3; tartape='-'; fi

$remote mt -t ${tape} rew
date
umount /dev/$nbsdusr
umount /dev/$nbsd
mount -r /dev/$nbsd /nbsd
mount -r /dev/$nbsdusr /nbsd/usr
cd /nbsd
sync

if [ $type = '1600a' ]
then
	type=1600
fi

echo "Add image of bootstrap file system"
eval dd if=/dev/r${bootroot} count=600 bs=1024 conv=sync ${remote+"| rsh"} \e
	${remote-"of=$tape"} ${remote+'/usr/local/20b 1024 ">" $tape'}

echo "Add image of mini-root file system"
eval dd if=/dev/r${miniroot} count=205 bs=10240 conv=sync ${remote+"| rsh"} \e
	${remote-"of=$tape"} ${remote+'/usr/local/20b ">" $tape'}

echo "Add full dump of real file system"
/etc/${remote+r}dump 0uf $remote${remote+:}${tape} /nbsd
echo "Add tar image of /usr"
cd /nbsd/usr; eval tar ${tflag} ${block} ${tartape} adm bin dict doc games \e
	guest hosts include lib local man msgs new \e
	preserve pub spool tmp ucb \e
		${remote+'| $remote ${bprog} ">" $tape'}
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

: tape2:
echo "Add user source code"
cd /nbsd/usr/src; eval tar ${tflag} ${block} ${tartape} Makefile bin cci \e
	etc games include lib local man old ucb undoc usr.bin usr.lib \e
	${remote+'| $remote ${bprog} ">" $tape'}

#echo "Add varian fonts"
#cd /usr/lib/vfont; eval tar ${tflag} ${block} ${tartape} . \e
#	${remote+'| $remote ${bprog} ">" $tape'}
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

: tape3:
echo "Add tar image of system sources"
cd /nbsd/sys; eval tar ${tflag} ${block} ${tartape} . \e
	${remote+'| $remote ${bprog} ">" $tape'}

echo "Add user contributed software"
# standard (always uncompressed) directories:
new="README Makefile B X ansi apl bib courier cpm dipress dsh \e
	enet help hyper jove kermit mkmf news notes nntp np100 \e
	patch pathalias rcs rn spms sunrpc tac tools umodem xns"
uncompress="emacs icon mh mmdf sumacc pup"
compress="emacs.tar.Z icon.tar.Z mh.tar.Z mmdf.tar.Z sumacc.tar.Z pup.tar.Z"
cd /nbsd/usr/src/new; eval tar ${tflag} ${block} ${tartape} ${new} \e
	${compress} ${remote+'| $remote ${bprog} ">" $tape'}

#echo "Add ingres source"
#cd /nbsd/usr/ingres; eval tar ${tflag} ${block} ${tartape} . \e
#	${remote+'| $remote ${bprog} ">" $tape'}

echo "Done, rewinding tape"
$remote mt -t ${tape} rew &
.DE
.SH
Control status register addresses
.PP
The distribution uses standalone device drivers
that presume the location of a VERSAbus device's control status
register (CSR).
The following table summarizes these values.
.DS
.TS
l l l.
Device name	Controller	CSR address (hex)
_
dk	CCI VDDC or SMDE	ffff2000, ffff2100, ffff2200, ..., ffff2700
cy	Cipher	ffff4000
.TE
.DE
.SH
Generic system control status register addresses
.PP
The 
.I generic
version of the operating system supplied with the distribution
contains the VERSAbus devices listed below. 
These devices will be recognized
if the appropriate control status registers respond at any of the
listed VERSAbus addresses.
.DS
.TS
l l l.
Device name	Controller	CSR addresses (hex)
_
dk	CCI VDDC or SMDE	ffff2000, ffff2100, ..., ffff2700
cy	Cipher	ffff4000, ffff6000
vx	CCI VIOC	fffe0000, fffe4000, ..., fffbc000
ace	ACC Ethernet	ffff0000, ffff0100
enp	CMC Ethernet	fff41000, fff61000
dr	IKON DR11W	ffff7000
ik	IKON DR11W w/ E&S PS300	ffff8000
.TE
.DE
If devices other than the above are located at any 
of the addresses listed, the system may not bootstrap
properly.
