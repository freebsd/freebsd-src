#!/bin/sh
#
# Copyright (c) 1994-2009 Poul-Henning Kamp.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

set -e

exec < /dev/null

if [ `uname -m` = "i386" ] ; then
	TARGET_PART=`df / | sed '
	1d
	s/[    ].*//
	s,/dev/,,
	s,s1a,s3a,
	s,s2a,s1a,
	s,s3a,s2a,
	'`

	# Where our build-bits are to be found
	FREEBSD_PART=`echo $TARGET_PART | sed 's/s[12]a/s3/'`
elif [ `uname -m` = "amd64" ] ; then
	TARGET_PART=`df / | sed '
	1d
	s/[    ].*//
	s,/dev/,,
	s,s1a,s3a,
	s,s2a,s1a,
	s,s3a,s2a,
	'`

	# Where our build-bits are to be found
	FREEBSD_PART=`echo $TARGET_PART | sed 's/s[12]a/s3/'`
else
	TARGET_PART=unknown
	FREEBSD_PART=unknown
fi

# Relative to /freebsd
PORTS_PATH=ports
SRC_PATH=src
# OBJ_PATH=obj

# Name of kernel
KERNCONF=GENERIC

# srcconf
#SRCCONF="SRCCONF=/usr/src/src.conf"

# -j arg to make(1)

ncpu=`sysctl -n kern.smp.cpus`
if [ $ncpu -gt 1 ] ; then
	JARG="-j $ncpu"
fi

# serial console ?
SERCONS=false

# Remotely mounted distfiles
# REMOTEDISTFILES=fs:/rdonly/distfiles

# Proxy
#FTP_PROXY=http://127.0.0.1:3128/
#HTTP_PROXY=http://127.0.0.1:3128/
#export FTP_PROXY HTTP_PROXY

PORTS_WE_WANT='
'

PORTS_OPTS="BATCH=YES MAKE_IDEA=YES A4=yes"

CONFIGFILES='
'

cleanup() (
)

before_ports() (
)

before_ports_chroot() (
)

final_root() (
)

final_chroot() (
)

#######################################################################
#######################################################################

usage () {
	(
        echo "Usage: $0 [-b/-k/-w] [-c config_file]"
        echo "  -b      suppress builds (both kernel and world)"
        echo "  -k      suppress buildkernel"
        echo "  -w      suppress buildworld"
        echo "  -p      used cached packages"
        echo "  -c      specify config file"
        ) 1>&2
        exit 2
}

#######################################################################
#######################################################################

if [ ! -f $0 ] ; then
	echo "Must be able to access self ($0)" 1>&2
	exit 1
fi

if grep -q 'Magic String: 0`0nQT40W%l,CX&' $0 ; then
	true
else
	echo "self ($0) does not contain magic string" 1>&2
	exit 1
fi

#######################################################################

set -e

log_it() (
	set +x
	a="$*"
	set `cat /tmp/_sb_log`
	TX=`date +%s`
	echo "$1 $TX" > /tmp/_sb_log
	DT=`expr $TX - $1 || true`
	DL=`expr $TX - $2 || true`
	echo -n "### `date +%H:%M:%S`"
	printf " ### %5d ### %5d ### %s\n" $DT $DL "$a"
)

#######################################################################


ports_recurse() (
	set +x
	for d
	do
		if [ ! -d $d ] ; then
			echo "Missing port $d" 1>&2
			exit 2
		fi
		if grep -q "^$d\$" /tmp/_.plist ; then
			true
		else
			(
			cd $d
			ports_recurse `make -V _DEPEND_DIRS ${PORTS_OPTS}`
			)
			echo $d >> /tmp/_.plist
		fi
	done
)

ports_build() (
	set +x

	true > /tmp/_.plist
	ports_recurse $PORTS_WE_WANT 

	# Now build & install them
	for p in `cat /tmp/_.plist`
	do
		t=`echo $p | sed 's,/usr/ports/,,'`
		pn=`cd $p && make package-name`
		if [ "x${PKG_DIR}" != "x" -a -f ${PKG_DIR}/$pn.tbz ] ; then
			if [ "x$use_pkg" = "x-p" ] ; then
				log_it "install $p from ${PKG_DIR}/$pn.tbz"
				pkg_add ${PKG_DIR}/$pn.tbz
			fi
		fi
		i=`pkg_info -qO $t`
		if [ -z "$i" ] ; then
			log_it "build $p"
			b=`echo $p | tr / _`
			(
				set -x
				cd /usr/ports
				cd $p
				set +e
				make clean ${PORTS_OPTS}
				if make install ${PORTS_OPTS} ; then
					if [ "x${PKG_DIR}" != "x" ] ; then
						make package ${PORTS_OPTS}
						mv *.tbz ${PKG_DIR}
					fi
				else
					log_it FAIL build $p
				fi
				make clean
			) > _.$b 2>&1 < /dev/null
			date
		fi
	done
)

ports_prefetch() (
	(
	set +x
	true > /tmp/_.plist
	ports_recurse $PORTS_WE_WANT

	true > /mnt/_.prefetch
	# Now checksump/fetch them
	for p in `cat /tmp/_.plist`
	do
		echo "Prefetching $p" >> /mnt/_.prefetch
		b=`echo $p | tr / _`
		(
			cd $p
			if make checksum $PORTS_OPTS ; then
				true
			else
				make distclean
				make checksum $PORTS_OPTS || true
			fi
		) > /mnt/_.prefetch.$b 2>&1
	done
	) 
)

#######################################################################

do_world=true
do_kernel=true
use_pkg=""
c_arg=""

set +e
args=`getopt bc:hkpw $*`
if [ $? -ne 0 ] ; then
	usage
fi
set -e

set -- $args
for i
do
	case "$i"
	in
	-b)
		shift;
		do_world=false
		do_kernel=false
		;;
	-c)
		c_arg=$2
		if [ ! -f "$c_arg" ] ; then
			echo "Cannot read $c_arg" 1>&2
			usage
		fi
		. "$2"
		shift
		shift
		;;
	-h)
		usage
		;;
	-k)
		shift;
		do_kernel=false
		;;
	-p)
		shift;
		use_pkg="-p"
		;;
	-w)
		shift;
		do_world=false
		;;
	--)
		shift
		break;
		;;
	esac
done

#######################################################################

if [ "x$1" = "xchroot_script" ] ; then
	set +x
	set -e

	shift

	before_ports_chroot

	ports_build

	exit 0
fi

if [ "x$1" = "xfinal_chroot" ] ; then
	final_chroot
	exit 0
fi

if [ $# -gt 0 ] ; then
        echo "$0: Extraneous arguments supplied"
        usage
fi

#######################################################################

T0=`date +%s`
echo $T0 $T0 > /tmp/_sb_log

log_it Unmount everything
(
	( cleanup )
	umount /freebsd/distfiles || true
	umount /mnt/freebsd/distfiles || true
	umount /dev/${FREEBSD_PART} || true
	umount /mnt/freebsd || true
	umount /mnt/dev || true
	umount /mnt || true
	umount /dev/${TARGET_PART} || true
) # > /dev/null 2>&1

log_it Prepare running image
mkdir -p /freebsd
mount /dev/${FREEBSD_PART} /freebsd

#######################################################################

if [ ! -d /freebsd/${PORTS_PATH} ] ;  then
	echo PORTS_PATH does not exist 1>&2
	exit 1
fi

if [ ! -d /freebsd/${SRC_PATH} ] ;  then
	echo SRC_PATH does not exist 1>&2
	exit 1
fi

log_it TARGET_PART $TARGET_PART
sleep 5

rm -rf /usr/ports
ln -s /freebsd/${PORTS_PATH} /usr/ports

rm -rf /usr/src
ln -s /freebsd/${SRC_PATH} /usr/src

if $do_world ; then
	if [ "x${OBJ_PATH}" != "x" ] ; then
		rm -rf /usr/obj
		mkdir -p /freebsd/${OBJ_PATH}
		ln -s /freebsd/${OBJ_PATH} /usr/obj
	else
		rm -rf /usr/obj
		mkdir -p /usr/obj
	fi
fi

#######################################################################

for i in ${PORTS_WE_WANT}
do
	if [ ! -d $i ]  ; then
		echo "Port $i not found" 1>&2
		exit 2
	fi
done

export PORTS_WE_WANT
export PORTS_OPTS

#######################################################################

log_it Prepare destination partition
newfs -O2 -U /dev/${TARGET_PART} > /dev/null
mount /dev/${TARGET_PART} /mnt
mkdir -p /mnt/dev
mount -t devfs devfs /mnt/dev

if [ "x${REMOTEDISTFILES}" != "x" ] ; then
	rm -rf /freebsd/${PORTS_PATH}/distfiles
	ln -s /freebsd/distfiles /freebsd/${PORTS_PATH}/distfiles
	mkdir -p /freebsd/distfiles
	mount  ${REMOTEDISTFILES} /freebsd/distfiles
fi

log_it "Start prefetch of ports distfiles"
ports_prefetch &

if $do_world ; then
	(
	cd /usr/src
	log_it "Buildworld"
	make ${JARG} -s buildworld ${SRCCONF} > /mnt/_.bw 2>&1
	)
fi

if $do_kernel ; then
	(
	cd /usr/src
	log_it "Buildkernel"
	make ${JARG} -s buildkernel KERNCONF=$KERNCONF > /mnt/_.bk 2>&1
	)
fi


log_it Installworld
(cd /usr/src && make ${JARG} installworld DESTDIR=/mnt ${SRCCONF} ) \
	> /mnt/_.iw 2>&1

log_it distribution
(cd /usr/src/etc && make -m /usr/src/share/mk distribution DESTDIR=/mnt ${SRCCONF} ) \
	> /mnt/_.dist 2>&1

log_it Installkernel
(cd /usr/src && make ${JARG} installkernel DESTDIR=/mnt KERNCONF=$KERNCONF ) \
	> /mnt/_.ik 2>&1

if [ "x${OBJ_PATH}" != "x" ] ; then
	rmdir /mnt/usr/obj
	ln -s /freebsd/${OBJ_PATH} /mnt/usr/obj
fi

log_it Wait for ports prefetch
log_it "(Tail /mnt/_.prefetch for progress)"
wait

log_it Move filesystems

if [ "x${REMOTEDISTFILES}" != "x" ] ; then
	umount /freebsd/distfiles
fi
umount /dev/${FREEBSD_PART} || true
mkdir -p /mnt/freebsd
mount /dev/${FREEBSD_PART} /mnt/freebsd
if [ "x${REMOTEDISTFILES}" != "x" ] ; then
	mount  ${REMOTEDISTFILES} /mnt/freebsd/distfiles
fi

rm -rf /mnt/usr/ports || true
ln -s /freebsd/${PORTS_PATH} /mnt/usr/ports

rm -rf /mnt/usr/src || true
ln -s /freebsd/${SRC_PATH} /mnt/usr/src

log_it Build and install ports

# Make sure fetching will work in the chroot
if [ -f /etc/resolv.conf ] ; then
	log_it copy resolv.conf
	cp /etc/resolv.conf /mnt/etc
	chflags schg /mnt/etc/resolv.conf
fi

if [ -f /etc/localtime ] ; then
	log_it copy localtime
	cp /etc/localtime /mnt/etc
fi

log_it copy ports config files
(cd / ; find var/db/ports -print | cpio -dumpv /mnt )

log_it ldconfig in chroot
chroot /mnt sh /etc/rc.d/ldconfig start

log_it before_ports
( 
	before_ports 
)

log_it build ports
pwd
cp $0 /mnt/root
cp /tmp/_sb_log /mnt/tmp
b=`basename $0`
if [ "x$c_arg" != "x" ] ; then
	cp $c_arg /mnt/root
	chroot /mnt sh /root/$0 -c /root/`basename $c_arg` $use_pkg chroot_script 
else
	chroot /mnt sh /root/$0 $use_pkg chroot_script
fi
cp /mnt/tmp/_sb_log /tmp

log_it fixing fstab
sed "/[ 	]\/[ 	]/s;^[^ 	]*[ 	];/dev/${TARGET_PART}	;" \
	/etc/fstab > /mnt/etc/fstab

log_it create all mountpoints
grep -v '^[ 	]*#' /mnt/etc/fstab | 
while read a b c
do
	mkdir -p /mnt/$b
done

if [ "x$SERCONS" != "xfalse" ] ; then
	log_it serial console
	echo " -h" > /mnt/boot.config
	sed -i "" -e /ttyd0/s/off/on/ /mnt/etc/ttys
	sed -i "" -e /ttyu0/s/off/on/ /mnt/etc/ttys
	sed -i "" -e '/^ttyv[0-8]/s/	on/	off/' /mnt/etc/ttys
fi

log_it move config files
(
	cd /mnt
	mkdir root/configfiles_dist
	find ${CONFIGFILES} -print | cpio -dumpv root/configfiles_dist
)

(cd / && find ${CONFIGFILES} -print | cpio -dumpv /mnt)

log_it final_root
( final_root )
log_it final_chroot
cp /tmp/_sb_log /mnt/tmp
if [ "x$c_arg" != "x" ] ; then
	chroot /mnt sh /root/$0 -c /root/`basename $c_arg` final_chroot
else
	chroot /mnt sh /root/$0 final_chroot
fi
cp /mnt/tmp/_sb_log /tmp
log_it "Check these messages (if any):"
grep '^Stop' /mnt/_* || true
log_it DONE
