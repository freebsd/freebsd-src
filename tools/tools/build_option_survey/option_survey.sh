#!/bin/sh
# This file is in the public domain
# $FreeBSD$

if [ "x$1" != "x" ] ; then
	OPLIST=$1
else
	OPLIST=no_list
fi

OPLIST=_.options

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
RDIR=${ODIR}/_.result
export ODIR MNT RDIR


# Clean and recrate the ODIR

if false ; then 
	if rm -rf ${ODIR} ; then
		true
	else
		chflags -R noschg ${ODIR}
		rm -rf ${ODIR}
	fi
	mkdir -p ${ODIR}

fi

# Build the reference world

if false ; then 
	echo '' > ${ODIR}/make.conf
	MAKEOBJDIRPREFIX=$ODIR/_.ref 
	export MAKEOBJDIRPREFIX
	bw
fi

# Parse option list into subdirectories with make.conf files.

if false ; then
	rm -rf ${RDIR}
	grep -v '^[ 	]*#' $OPLIST | while read o
	do
		echo "$o=/dev/YES" > ${ODIR}/_make.conf
		m=`md5 < ${ODIR}/_make.conf`
		mkdir -p ${RDIR}/$m
		mv ${ODIR}/_make.conf ${RDIR}/$m/make.conf
	done
fi

# Do the reference installworld 

if false ; then
	echo '' > ${ODIR}/make.conf
	MAKEOBJDIRPREFIX=$ODIR/_.ref 
	export MAKEOBJDIRPREFIX
	mkdir -p ${RDIR}/Ref
	iw
	cp ${ODIR}/_.df ${RDIR}/Ref
	cp ${ODIR}/_.mtree ${RDIR}/Ref
	cp ${ODIR}/_.du ${RDIR}/Ref
fi

# Run through each testtarget in turn

if true ; then
	for d in ${RDIR}/[0-9a-z]*
	do
		if [ ! -d $d ] ; then
			continue;
		fi
		echo '------------------------------------------------'
		cat $d/make.conf
		echo '------------------------------------------------'
		cp $d/make.conf ${ODIR}/make.conf

		if [ ! -f $d/iw/done ] ; then
			echo "# Trying IW"
			rm -rf $d/iw
			mkdir -p $d/iw
			MAKEOBJDIRPREFIX=$ODIR/_.ref 
			export MAKEOBJDIRPREFIX
			if iw ; then
				cp ${ODIR}/_.df $d/iw
				cp ${ODIR}/_.mtree $d/iw
				cp ${ODIR}/_.du $d/iw
			else
				cp ${ODIR}/_.iw $d/iw || true
				cp ${ODIR}/_.ik $d/iw || true
			fi
			touch $d/iw/done
		fi
		if [ ! -f $d/bw/done ] ; then
			echo "# Trying BW"
			MAKEOBJDIRPREFIX=$ODIR/_.tst 
			export MAKEOBJDIRPREFIX
			if bw ; then
				mkdir -p $d/w
				if iw ; then
					cp ${ODIR}/_.df $d/w
					cp ${ODIR}/_.mtree $d/w
					cp ${ODIR}/_.du $d/w
				else
					cp ${ODIR}/_.iw $d/w || true
					cp ${ODIR}/_.ik $d/w || true
				fi
				touch $d/w/done
				echo "# Trying W"
				mkdir -p $d/bw
				echo '' > ${ODIR}/make.conf
				if iw ; then
					cp ${ODIR}/_.df $d/bw
					cp ${ODIR}/_.mtree $d/bw
					cp ${ODIR}/_.du $d/bw
				else
					cp ${ODIR}/_.iw $d/bw || true
					cp ${ODIR}/_.ik $d/bw || true
				fi
				touch $d/bw/done
			else
				mkdir -p $d/bw
				cp ${ODIR}/_.bw $d/bw || true
				cp ${ODIR}/_.bk $d/bw || true
				touch $d/bw/done
			fi
		fi
	done
fi
