#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
keyfile=`mktemp /tmp/$base.XXXXXX` || exit 1
backupfile=`mktemp /tmp/$base.XXXXXX` || exit 1

echo "1..13"

dd if=/dev/random of=${keyfile} bs=512 count=16 >/dev/null 2>&1

mdconfig -a -t malloc -s $sectors -u $no || exit 1

# -B none
rm -f /var/backups/md${no}.eli
geli init -B none -P -K $keyfile md${no} 2>/dev/null
if [ ! -f /var/backups/md${no}.eli ]; then
	echo "ok 1 - -B none"
else
	echo "not ok 1 - -B none"
fi

# no -B
rm -f /var/backups/md${no}.eli
geli init -P -K $keyfile md${no} >/dev/null 2>&1
if [ -f /var/backups/md${no}.eli ]; then
	echo "ok 2 - no -B"
else
	echo "not ok 2 - no -B"
fi
geli clear md${no}
geli attach -p -k $keyfile md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 3 - no -B"
else
	echo "not ok 3 - no -B"
fi
if [ ! -c /dev/md${no}.eli ]; then
	echo "ok 4 - no -B"
else
	echo "not ok 4 - no -B"
fi
geli restore /var/backups/md${no}.eli md${no}
if [ $? -eq 0 ]; then
	echo "ok 5 - no -B"
else
	echo "not ok 5 - no -B"
fi
geli attach -p -k $keyfile md${no} 2>/dev/null
if [ $? -eq 0 ]; then
	echo "ok 6 - no -B"
else
	echo "not ok 6 - no -B"
fi
if [ -c /dev/md${no}.eli ]; then
	echo "ok 7 - no -B"
else
	echo "not ok 7 - no -B"
fi
geli detach md${no}
rm -f /var/backups/md${no}.eli

# -B file
rm -f $backupfile
geli init -B $backupfile -P -K $keyfile md${no} >/dev/null 2>&1
if [ -f $backupfile ]; then
	echo "ok 8 - -B file"
else
	echo "not ok 8 - -B file"
fi
geli clear md${no}
geli attach -p -k $keyfile md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 9 - -B file"
else
	echo "not ok 9 - -B file"
fi
if [ ! -c /dev/md${no}.eli ]; then
	echo "ok 10 - -B file"
else
	echo "not ok 10 - -B file"
fi
geli restore $backupfile md${no}
if [ $? -eq 0 ]; then
	echo "ok 11 - -B file"
else
	echo "not ok 11 - -B file"
fi
geli attach -p -k $keyfile md${no} 2>/dev/null
if [ $? -eq 0 ]; then
	echo "ok 12 - -B file"
else
	echo "not ok 12 - -B file"
fi
if [ -c /dev/md${no}.eli ]; then
	echo "ok 13 - -B file"
else
	echo "not ok 13 - -B file"
fi
geli detach md${no}
rm -f $backupfile

mdconfig -d -u $no
rm -f $keyfile
