#!/bin/sh
# $FreeBSD$

TMP=/tmp/$$.
set -e
for TEST in "i386 512" "i386 4096" "alpha 512" "pc98 512" "pc98 4096"
do
	set $TEST
	ARCH=$1
	SEC=$2
	echo "ARCH $ARCH SEC $SEC"
	MD=`mdconfig -a -t malloc -s 2m -S $SEC`
	trap "exec 7</dev/null; rm -f ${TMP}* ; mdconfig -d -u ${MD}" EXIT INT TERM

	./bsdlabel -m ${ARCH} -r -w $MD auto

	dd if=/dev/$MD of=${TMP}i0 count=1 bs=8k > /dev/null 2>&1
	if [ "$ARCH" = "alpha" ] ; then
		dd if=${TMP}i0 of=${TMP}b0 iseek=1 count=15 > /dev/null 2>&1
	else
		cp ${TMP}i0 ${TMP}b0
	fi
	./bsdlabel -m ${ARCH} $MD > ${TMP}l0

	sed '
	/  c:/{
	p
	s/c:/a:/
	s/4096/1024/
	s/512/64/
	}
	' ${TMP}l0 > ${TMP}l1

	./bsdlabel -m ${ARCH} -R $MD ${TMP}l1
	if [ -c /dev/${MD}a ] ; then
		echo "PASS: Created a: partition" 1>&2
	else
		echo "FAIL: Did not create a: partition" 1>&2
		exit 2
	fi

	# Spoil and rediscover

	true > /dev/${MD}
	if [ -c /dev/${MD}a ] ; then
		echo "PASS: Recreated a: partition after spoilage" 1>&2
	else
		echo "FAIL: Did not recreate a: partition after spoilage" 1>&2
		exit 2
	fi

	dd if=/dev/$MD of=${TMP}i1 count=1 bs=8k > /dev/null 2>&1
	sed '
	/  c:/{
	p
	s/c:/a:/
	s/4096/2048/
	s/512/256/
	}
	' ${TMP}l0 > ${TMP}l2

	./bsdlabel -m ${ARCH} -R $MD ${TMP}l2
	dd if=/dev/$MD of=${TMP}i2 count=1 bs=8k > /dev/null 2>&1

	exec 7< /dev/${MD}a

	for t in a c
	do
		if dd if=${TMP}i2 of=/dev/${MD}$t bs=8k 2>/dev/null ; then
			echo "PASS: Could rewrite same label to ...$t while ...a open" 1>&2
		else
			echo "FAIL: Could not rewrite same label to ...$t while ...a open" 1>&2
			exit 2
		fi

		if dd if=${TMP}i1 of=/dev/${MD}$t bs=8k 2>/dev/null ; then
			echo "FAIL: Could label with smaller ...a to ...$t while ...a open" 1>&2
			exit 2
		else
			echo "PASS: Could not label with smaller ...a to ...$t while ...a open" 1>&2
		fi

		if dd if=${TMP}i0 of=/dev/${MD}$t 2>/dev/null ; then
			echo "FAIL: Could write label missing ...a to ...$t while ...a open" 1>&2
			exit 2
		else
			echo "PASS: Could not write label missing ...a to ...$t while ...a open" 1>&2
		fi
	done

	exec 7< /dev/null

	if dd if=${TMP}i0 of=/dev/${MD}c bs=8k 2>/dev/null ; then
		echo "PASS: Could write missing ...a label to ...c" 1>&2
	else
		echo "FAIL: Could not write missing ...a label to ...c" 1>&2
		exit 2
	fi

	if dd if=${TMP}i2 of=/dev/${MD}c bs=8k 2>/dev/null ; then
		echo "PASS: Could write large ...a label to ...c" 1>&2
	else
		echo "FAIL: Could not write large ...a label to ...c" 1>&2
		exit 2
	fi

	if dd if=${TMP}i1 of=/dev/${MD}c bs=8k 2>/dev/null ; then
		echo "PASS: Could write small ...a label to ...c" 1>&2
	else
		echo "FAIL: Could not write small ...a label to ...c" 1>&2
		exit 2
	fi

	if dd if=${TMP}i2 of=/dev/${MD}a bs=8k 2>/dev/null ; then
		echo "PASS: Could increase size of ...a by writing to ...a" 1>&2
	else
		echo "FAIL: Could not increase size of ...a by writing to ...a" 1>&2
		exit 2
	fi

	if dd if=${TMP}i1 of=/dev/${MD}a bs=8k 2>/dev/null ; then
		echo "FAIL: Could decrease size of ...a by writing to ...a" 1>&2
		exit 2
	else
		echo "PASS: Could not decrease size of ...a by writing to ...a" 1>&2
	fi

	if dd if=${TMP}i0 of=/dev/${MD}a bs=8k 2>/dev/null ; then
		echo "FAIL: Could delete ...a by writing to ...a" 1>&2
		exit 2
	else
		echo "PASS: Could not delete ...a by writing to ...a" 1>&2
	fi

	if ./bsdlabel -m ${ARCH} -B -b ${TMP}b0 ${MD} ; then
		if [ ! -c /dev/${MD}a ] ; then
			echo "FAILED: Writing bootcode killed ...a" 1>&2
			exit 2
		else
			echo "PASS: Could write bootcode while closed" 1>&2
		fi
	else
		echo "FAILED: Could not write bootcode while closed" 1>&2
		exit 2
	fi

	exec 7> /dev/${MD}c
	if ./bsdlabel -m ${ARCH} -B -b ${TMP}b0 ${MD} ; then
		if [ ! -c /dev/${MD}a ] ; then
			echo "FAILED: Writing bootcode killed ...a" 1>&2
			exit 2
		else
			echo "PASS: Could write bootcode while open" 1>&2
		fi
	else
		echo "FAILED: Could not write bootcode while open" 1>&2
		exit 2
	fi
	exec 7> /dev/null

	if dd if=${TMP}i0 of=/dev/${MD}c bs=8k 2>/dev/null ; then
		echo "PASS: Could delete ...a by writing to ...c" 1>&2
	else
		echo "FAIL: Could not delete ...a by writing to ...c" 1>&2
		exit 2
	fi

	# XXX: need to add a 'b' partition and check for overlaps.

	rm -f ${TMP}*
	mdconfig -d -u ${MD} 

done
trap "" EXIT INT TERM
exit 0
