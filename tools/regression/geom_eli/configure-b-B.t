#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
mdconfig -a -t malloc -s `expr $sectors + 1` -u $no || exit 1

echo "1..17"

geli init -P -K /dev/null md${no}
if [ $? -eq 0 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi

geli dump md${no} | egrep 'flags: 0x0$' >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 2"
else
	echo "not ok 2"
fi

geli init -b -P -K /dev/null md${no}
if [ $? -eq 0 ]; then
	echo "ok 3"
else
	echo "not ok 3"
fi

geli dump md${no} | egrep 'flags: 0x2$' >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 4"
else
	echo "not ok 4"
fi

geli configure -B md${no}
if [ $? -eq 0 ]; then
	echo "ok 5"
else
	echo "not ok 5"
fi

geli dump md${no} | egrep 'flags: 0x0$' >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 6"
else
	echo "not ok 6"
fi

geli configure -b md${no}
if [ $? -eq 0 ]; then
	echo "ok 7"
else
	echo "not ok 7"
fi

geli dump md${no} | egrep 'flags: 0x2$' >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 8"
else
	echo "not ok 8"
fi

geli attach -p -k /dev/null md${no}
if [ $? -eq 0 ]; then
	echo "ok 9"
else
	echo "not ok 9"
fi

geli list md${no}.eli | egrep '^Flags: .*BOOT' >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 10"
else
	echo "not ok 10"
fi

geli configure -B md${no}
if [ $? -eq 0 ]; then
	echo "ok 11"
else
	echo "not ok 11"
fi

geli list md${no}.eli | egrep '^Flags: .*BOOT' >/dev/null
if [ $? -ne 0 ]; then
	echo "ok 12"
else
	echo "not ok 12"
fi

geli dump md${no} | egrep 'flags: 0x0$' >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 13"
else
	echo "not ok 13"
fi

geli configure -b md${no}
if [ $? -eq 0 ]; then
	echo "ok 14"
else
	echo "not ok 14"
fi

geli list md${no}.eli | egrep '^Flags: .*BOOT' >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 15"
else
	echo "not ok 15"
fi

geli dump md${no} | egrep 'flags: 0x2$' >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 16"
else
	echo "not ok 16"
fi

geli detach md${no}
if [ $? -eq 0 ]; then
	echo "ok 17"
else
	echo "not ok 17"
fi

mdconfig -d -u $no
