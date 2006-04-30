#!/bin/sh
# $FreeBSD$

base=`basename $0`
no=45
sectors=100
keyfile1=`mktemp /tmp/$base.XXXXXX` || exit 1
keyfile2=`mktemp /tmp/$base.XXXXXX` || exit 1
keyfile3=`mktemp /tmp/$base.XXXXXX` || exit 1
keyfile4=`mktemp /tmp/$base.XXXXXX` || exit 1
mdconfig -a -t malloc -s `expr $sectors + 1` -u $no || exit 1

echo "1..14"

dd if=/dev/random of=${keyfile1} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile2} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile3} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile4} bs=512 count=16 >/dev/null 2>&1

geli init -P -K $keyfile1 md${no}
geli attach -p -k $keyfile1 md${no}
geli setkey -n 1 -P -K $keyfile2 md${no}

# Remove key 0 for attached provider.
geli delkey -n 0 md${no}
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

# Attach with key 1.
geli attach -p -k $keyfile2 md${no}
if [ $? -eq 0 ]; then
	echo "ok 3"
else
	echo "not ok 3"
fi

# We cannot remove last key without -f option (for attached provider).
geli delkey -n 1 md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 4"
else
	echo "not ok 4"
fi

# Remove last key for attached provider.
geli delkey -f -n 1 md${no}
if [ $? -eq 0 ]; then
	echo "ok 5"
else
	echo "not ok 5"
fi

# If there are no valid keys, but provider is attached, we can save situation.
geli setkey -n 0 -P -K $keyfile3 md${no}
if [ $? -eq 0 ]; then
	echo "ok 6"
else
	echo "not ok 6"
fi
geli detach md${no}

# We cannot use keyfile2 anymore.
geli attach -p -k $keyfile2 md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 7"
else
	echo "not ok 7"
fi

# Attach with key 0.
geli attach -p -k $keyfile3 md${no}
if [ $? -eq 0 ]; then
	echo "ok 8"
else
	echo "not ok 8"
fi

# Setup key 1.
geli setkey -n 1 -P -K $keyfile4 md${no}
if [ $? -eq 0 ]; then
	echo "ok 9"
else
	echo "not ok 9"
fi
geli detach md${no}

# Remove key 1 for detached provider.
geli delkey -n 1 md${no}
if [ $? -eq 0 ]; then
	echo "ok 10"
else
	echo "not ok 10"
fi

# We cannot use keyfile4 anymore.
geli attach -p -k $keyfile4 md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 11"
else
	echo "not ok 11"
fi

# We cannot remove last key without -f option (for detached provider).
geli delkey -n 0 md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 12"
else
	echo "not ok 12"
fi

# Remove last key for detached provider.
geli delkey -f -n 0 md${no}
if [ $? -eq 0 ]; then
	echo "ok 13"
else
	echo "not ok 13"
fi

# We cannot use keyfile3 anymore.
geli attach -p -k $keyfile3 md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 14"
else
	echo "not ok 14"
fi

mdconfig -d -u $no
rm -f $keyfile1 $keyfile2 $keyfile3 $keyfile4
