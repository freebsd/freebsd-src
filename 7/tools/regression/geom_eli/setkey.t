#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
rnd=`mktemp /tmp/$base.XXXXXX` || exit 1
keyfile1=`mktemp /tmp/$base.XXXXXX` || exit 1
keyfile2=`mktemp /tmp/$base.XXXXXX` || exit 1
keyfile3=`mktemp /tmp/$base.XXXXXX` || exit 1
keyfile4=`mktemp /tmp/$base.XXXXXX` || exit 1
keyfile5=`mktemp /tmp/$base.XXXXXX` || exit 1
mdconfig -a -t malloc -s `expr $sectors + 1` -u $no || exit 1

echo "1..16"

dd if=/dev/random of=${rnd} bs=512 count=${sectors} >/dev/null 2>&1
hash1=`dd if=${rnd} bs=512 count=${sectors} 2>/dev/null | md5`
dd if=/dev/random of=${keyfile1} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile2} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile3} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile4} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile5} bs=512 count=16 >/dev/null 2>&1

geli init -P -K $keyfile1 md${no}
geli attach -p -k $keyfile1 md${no}

dd if=${rnd} of=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null
rm -f $rnd
hash2=`dd if=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null | md5`

# Change current key (0) for attached provider.
geli setkey -P -K $keyfile2 md${no}
if [ $? -eq 0 ]; then
	echo "ok 1"
else
	echo "not ok 1"
fi
geli detach md${no}

# We cannot use keyfile1 anymore.
geli attach -p -k $keyfile1 md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 2"
else
	echo "not ok 2"
fi

# Attach with new key.
geli attach -p -k $keyfile2 md${no}
if [ $? -eq 0 ]; then
	echo "ok 3"
else
	echo "not ok 3"
fi
hash3=`dd if=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null | md5`

# Change key 1 for attached provider.
geli setkey -n 1 -P -K $keyfile3 md${no}
if [ $? -eq 0 ]; then
	echo "ok 4"
else
	echo "not ok 4"
fi
geli detach md${no}

# Attach with key 1.
geli attach -p -k $keyfile3 md${no}
if [ $? -eq 0 ]; then
	echo "ok 5"
else
	echo "not ok 5"
fi
hash4=`dd if=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null | md5`
geli detach md${no}

# Change current (1) key for detached provider.
geli setkey -p -k $keyfile3 -P -K $keyfile4 md${no}
if [ $? -eq 0 ]; then
	echo "ok 6"
else
	echo "not ok 6"
fi

# We cannot use keyfile3 anymore.
geli attach -p -k $keyfile3 md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 7"
else
	echo "not ok 7"
fi

# Attach with key 1.
geli attach -p -k $keyfile4 md${no}
if [ $? -eq 0 ]; then
	echo "ok 8"
else
	echo "not ok 8"
fi
hash5=`dd if=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null | md5`
geli detach md${no}

# Change key 0 for detached provider.
geli setkey -n 0 -p -k $keyfile4 -P -K $keyfile5 md${no}
if [ $? -eq 0 ]; then
	echo "ok 9"
else
	echo "not ok 9"
fi

# We cannot use keyfile2 anymore.
geli attach -p -k $keyfile2 md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 10"
else
	echo "not ok 10"
fi

# Attach with key 0.
geli attach -p -k $keyfile5 md${no}
if [ $? -eq 0 ]; then
	echo "ok 11"
else
	echo "not ok 11"
fi
hash6=`dd if=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null | md5`
geli detach md${no}

if [ ${hash1} = ${hash2} ]; then
	echo "ok 12"
else
	echo "not ok 12"
fi
if [ ${hash1} = ${hash3} ]; then
	echo "ok 13"
else
	echo "not ok 13"
fi
if [ ${hash1} = ${hash4} ]; then
	echo "ok 14"
else
	echo "not ok 14"
fi
if [ ${hash1} = ${hash5} ]; then
	echo "ok 15"
else
	echo "not ok 15"
fi
if [ ${hash1} = ${hash6} ]; then
	echo "ok 16"
else
	echo "not ok 16"
fi

mdconfig -d -u $no
rm -f $keyfile1 $keyfile2 $keyfile3 $keyfile4 $keyfile5
