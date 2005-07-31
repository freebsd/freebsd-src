#!/bin/sh
# This file is in the public domain
# $FreeBSD$

set -e

bw ( ) (
	cd ../../.. 
	make -j 4 buildworld \
		__MAKE_CONF=${ODIR}/make.conf \
		> ${ODIR}/_.bw 2>&1
	make -j 4 buildkernel \
		KERNCONF=GENERIC \
		__MAKE_CONF=${ODIR}/make.conf \
		> ${ODIR}/_.bk 2>&1
)

iw ( ) (
	dd if=/dev/zero of=${ODIR}/_.i bs=1m count=200
	mkdir -p ${MNT}
	MD=`mdconfig -a -t vnode -f ${ODIR}/_.i`
	trap "umount ${MNT} || true ; mdconfig -d -u $MD" 1 2 15 EXIT
	newfs -O1 -U -b 4096 -f 512 /dev/$MD
	mount /dev/${MD} ${MNT}

	cd ../../..
	make installworld \
		__MAKE_CONF=${ODIR}/make.conf \
		DESTDIR=${MNT} \
		> ${ODIR}/_.iw 2>&1
	cd etc
	make distribution \
		__MAKE_CONF=${ODIR}/make.conf \
		DESTDIR=${MNT} \
		> ${ODIR}/_.etc 2>&1
	cd ..
	make installkernel \
		KERNCONF=GENERIC \
		DESTDIR=${MNT} \
		__MAKE_CONF=${ODIR}/make.conf \
		> ${ODIR}/_.ik 2>&1

	sync ${MNT}
	( cd ${MNT} && mtree -c ) > ${ODIR}/_.mtree
	( cd ${MNT} && du ) > ${ODIR}/_.du
	( df -i ${MNT} ) > ${ODIR}/_.df
)

ODIR=/usr/obj/`pwd`
MNT=${ODIR}/_.mnt
MAKEOBJDIRPREFIX=$ODIR
export MAKEOBJDIRPREFIX ODIR MNT

if false ; then 
	if rm -rf ${ODIR} ; then
		true
	else
		chflags -R noschg ${ODIR}
		rm -rf ${ODIR}
	fi
	mkdir -p ${ODIR}

	echo '' > ${ODIR}/make.conf

	bw
fi

if false ; then
	rm -rf Tmp

	echo '' > ${ODIR}/make.conf

	if iw ; then
		m=Tmp/Ref
		mkdir -p $m
		cp ${ODIR}/_.df $m
		cp ${ODIR}/_.mtree $m
		cp ${ODIR}/_.du $m
	fi

	cat no_list | while read o
	do
		echo "IW $o"
		echo "$o=YES" > ${ODIR}/make.conf
		m=Tmp/`md5 < ${ODIR}/make.conf`/iw
		mkdir -p $m
		echo $m
		cp ${ODIR}/make.conf $m
		if iw ; then
			cp ${ODIR}/_.df $m
			cp ${ODIR}/_.mtree $m
			cp ${ODIR}/_.du $m
		else
			cp ${ODIR}/_.iw $m
			cp ${ODIR}/_.ik $m
		fi
	done
fi

if true ; then
	cat no_list | while read o
	do
		# First build+installworld
		echo "W $o"
		echo "$o=YES" > ${ODIR}/make.conf

		m=Tmp/`md5 < ${ODIR}/make.conf`/w
		mkdir -p $m
		echo $m
		cp ${ODIR}/make.conf $m

		if bw ; then
			true
		else
			cp ${ODIR}/_.bw $m || true
			cp ${ODIR}/_.bk $m || true
		fi
		if iw ; then
			cp ${ODIR}/_.df $m
			cp ${ODIR}/_.mtree $m
			cp ${ODIR}/_.du $m
		else
			cp ${ODIR}/_.iw $m
			cp ${ODIR}/_.ik $m
		fi

		# Then only buildworld
		echo "BW $o"
		m=Tmp/`md5 < ${ODIR}/make.conf`/bw
		mkdir -p $m
		echo $m
		cp ${ODIR}/make.conf $m
		echo '' > ${ODIR}/make.conf
		if iw ; then
			cp ${ODIR}/_.df $m
			cp ${ODIR}/_.mtree $m
			cp ${ODIR}/_.du $m
		else
			cp ${ODIR}/_.iw $m
			cp ${ODIR}/_.ik $m
		fi

		
	done
fi
