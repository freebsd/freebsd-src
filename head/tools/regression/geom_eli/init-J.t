#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
keyfile0=`mktemp /tmp/$base.XXXXXX` || exit 1
keyfile1=`mktemp /tmp/$base.XXXXXX` || exit 1
passfile0=`mktemp /tmp/$base.XXXXXX` || exit 1
passfile1=`mktemp /tmp/$base.XXXXXX` || exit 1
mdconfig -a -t malloc -s `expr $sectors + 1` -u $no || exit 1

echo "1..150"

dd if=/dev/random of=${keyfile0} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile1} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random bs=512 count=16 2>/dev/null | sha1 > ${passfile0}
dd if=/dev/random bs=512 count=16 2>/dev/null | sha1 > ${passfile1}

i=1
for iter in -1 0 64; do
	geli init -i ${iter} -B none -J ${passfile0} -P md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli init -i ${iter} -B none -J ${passfile0} -P -K ${keyfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli init -i ${iter} -B none -J ${passfile0} -K ${keyfile0} md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile0} -p md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${passfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${keyfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${passfile0} -p md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${keyfile0} -k ${passfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${keyfile0} -k ${keyfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${passfile0} -k ${passfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${passfile0} -k ${keyfile0} md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	cat ${keyfile0} | geli attach -j ${passfile0} -k - md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	cat ${passfile0} | geli attach -j - -k ${keyfile0} md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))

	geli init -i ${iter} -B none -J ${passfile0} -J ${passfile1} -P md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli init -i ${iter} -B none -J ${passfile0} -J ${passfile1} -P -K ${keyfile0} -K ${keyfile1} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli init -i ${iter} -B none -J ${passfile0} -J ${passfile1} -K ${keyfile0} -K ${keyfile1} md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile0} -p md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile1} -p md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${passfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${passfile1} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile0} -k ${keyfile1} -p md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${passfile0} -j ${passfile1} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile0} -j ${passfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile0} -j ${passfile1} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile1} -j ${passfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile1} -j ${passfile1} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile0} -j ${passfile0} -j ${passfile1} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile1} -j ${passfile0} -j ${passfile1} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile0} -k ${keyfile1} -j ${passfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile0} -k ${keyfile1} -j ${passfile1} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile1} -k ${keyfile0} -j ${passfile0} -j ${passfile1} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile0} -k ${keyfile1} -j ${passfile1} -j ${passfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -k ${keyfile1} -k ${keyfile0} -j ${passfile1} -j ${passfile0} md${no} 2>/dev/null && echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli attach -j ${passfile0} -j ${passfile1} -k ${keyfile0} -k ${keyfile1} md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	cat ${passfile0} | geli attach -j - -j ${passfile1} -k ${keyfile0} -k ${keyfile1} md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	cat ${passfile1} | geli attach -j ${passfile0} -j - -k ${keyfile0} -k ${keyfile1} md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	cat ${keyfile0} | geli attach -j ${passfile0} -j ${passfile1} -k - -k ${keyfile1} md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	cat ${keyfile1} | geli attach -j ${passfile0} -j ${passfile1} -k ${keyfile0} -k - md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	cat ${keyfile0} ${keyfile1} | geli attach -j ${passfile0} -j ${passfile1} -k - md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	cat ${passfile0} ${passfile1} | awk '{printf "%s", $0}' | geli attach -j - -k ${keyfile0} -k ${keyfile1} md${no} 2>/dev/null || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
	geli detach md${no} || echo -n "not "
	echo "ok ${i}"; i=$((i+1))
done

mdconfig -d -u $no
rm -f ${keyfile0} ${keyfile1} ${passfile0} ${passfile1}
