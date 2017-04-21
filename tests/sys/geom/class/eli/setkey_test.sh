#!/bin/sh
# $FreeBSD$

. $(dirname $0)/conf.sh

base=`basename $0`
sectors=100
rnd=`mktemp $base.XXXXXX` || exit 1
keyfile1=`mktemp $base.XXXXXX` || exit 1
keyfile2=`mktemp $base.XXXXXX` || exit 1
keyfile3=`mktemp $base.XXXXXX` || exit 1
keyfile4=`mktemp $base.XXXXXX` || exit 1
keyfile5=`mktemp $base.XXXXXX` || exit 1
passphrase1=$(cat /dev/random | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1) || exit 1
passfile1=`mktemp $base.XXXXXX` || exit 1
passphrase2=$(cat /dev/random | tr -dc 'a-zA-Z0-9' | fold -w 32 | head -n 1) || exit 1
passfile2=`mktemp $base.XXXXXX` || exit 1
mdconfig -a -t malloc -s `expr $sectors + 1` -u $no || exit 1

echo "1..42"

echo $passphrase1 > $passfile1
echo $passphrase2 > $passfile2

dd if=/dev/random of=${rnd} bs=512 count=${sectors} >/dev/null 2>&1
hash1=`dd if=${rnd} bs=512 count=${sectors} 2>/dev/null | md5`
dd if=/dev/random of=${keyfile1} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile2} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile3} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile4} bs=512 count=16 >/dev/null 2>&1
dd if=/dev/random of=${keyfile5} bs=512 count=16 >/dev/null 2>&1

geli init -B none -P -K $keyfile1 md${no}
geli attach -p -k $keyfile1 md${no}

dd if=${rnd} of=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null
hash2=`dd if=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null | md5`

# Change current key (0) for attached provider.
geli setkey -P -K $keyfile2 md${no} >/dev/null
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
geli setkey -n 1 -P -K $keyfile3 md${no} >/dev/null 2>&1
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
geli setkey -p -k $keyfile3 -P -K $keyfile4 md${no} >/dev/null
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
geli setkey -n 0 -p -k $keyfile4 -P -K $keyfile5 md${no} >/dev/null
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

# Set passphrase on key 1
geli setkey -n 1 -J $passfile2 -K $keyfile2 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 12"
else
	echo "not ok 12"
fi
geli detach md${no}

# Set passphrase on key 0 detached provider
geli setkey -n 0 -p -k $keyfile5 -J $passfile1 -K $keyfile1 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 13"
else
	echo "not ok 13"
fi

# Attach with key 0
geli attach -j $passfile1 -k $keyfile1 md${no}
if [ $? -eq 0 ]; then
	echo "ok 14"
else
	echo "not ok 14"
fi
hash7=`dd if=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null | md5`
geli detach md${no}

# Init with key
geli init -B none -P -K $keyfile1 md${no}
if [ $? -eq 0 ]; then
	echo "ok 15"
else
	echo "not ok 15"
fi

# Attach with key 0
geli attach -p -k $keyfile1 md${no}
if [ $? -eq 0 ]; then
	echo "ok 16"
	dd if=${rnd} of=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null
else
	echo "not ok 16"
fi

iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} == "-1" ] || [ ${iters} == "4294967295" ]; then
	echo "ok 17"
else
	echo "not ok 17"
fi

# Set key 1
geli setkey -n 1 -P -K $keyfile2 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 18"
else
	echo "not ok 18"
fi

# Set passphrase on key 1
geli setkey -n 1 -i 10 -J $passfile2 -K $keyfile2 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 19"
else
	echo "not ok 19"
fi

# Set passphrase on key 1 with different iterations
geli setkey -n 1 -i 20 -J $passfile2 -K $keyfile2 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 20"
else
	echo "not ok 20"
fi
hash8=`dd if=/dev/md${no}.eli bs=512 count=${sectors} 2>/dev/null | md5`

# Set passphrase on key 0 with different iterations
geli setkey -n 0 -i 30 -J $passfile1 -K $keyfile1 md${no} 2>/dev/null
if [ $? -ne 0 ]; then
	echo "ok 21"
else
	echo "not ok 21"
fi
geli detach md${no}

geli init -B none -J $passfile1 -K $keyfile1 md${no}
geli attach -j $passfile1 -k $keyfile1 md${no}
# Test that iterations is set
iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} != "-1" ] && [ ${iters} != "4294967295" ]; then
	echo "ok 22"
else
	echo "not ok 22"
fi

# Set passphrase on key 0 with different iterations
geli setkey -n 0 -i 0 -J $passfile2 -K $keyfile2 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 23"
else
	echo "not ok 23"
fi

geli setkey -n 0 -P -K $keyfile1 md${no} >/dev/null
# Test that iterations is not set
iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} == "-1" ] || [ ${iters} == "4294967295" ]; then
	echo "ok 24"
else
	echo "not ok 24"
fi

geli setkey -n 0 -J $passfile1 -K $keyfile1 md${no} >/dev/null
geli detach md${no}

# Set passphrase on key 0 with different iterations
geli setkey -n 0 -i 0 -j $passfile1 -k $keyfile1 -J $passfile2 -K $keyfile2 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 25"
else
	echo "not ok 25"
fi

geli setkey -n 0 -j $passfile2 -k $keyfile2 -P -K $keyfile1 md${no} >/dev/null
# Test that iterations is not set
iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} == "-1" ] || [ ${iters} == "4294967295" ]; then
	echo "ok 26"
else
	echo "not ok 26"
fi

geli attach -p -k $keyfile1 md${no}
geli setkey -n 0 -J $passfile1 -K $keyfile1 md${no} >/dev/null
geli setkey -n 1 -J $passfile2 -K $keyfile2 md${no} >/dev/null
geli delkey -a md${no}
# Test that iterations is not set
iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} == "-1" ] || [ ${iters} == "4294967295" ]; then
	echo "ok 27"
else
	echo "not ok 27"
fi

geli setkey -n 0 -J $passfile1 -K $keyfile1 md${no} >/dev/null
geli setkey -n 1 -J $passfile2 -K $keyfile2 md${no} >/dev/null
geli delkey -n 0 md${no}
# Test that iterations is set
iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} != "-1" ] && [ ${iters} != "4294967295" ]; then
	echo "ok 28"
else
	echo "not ok 28"
fi

geli delkey -n 1 -f md${no}
# Test that iterations is not set
iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} == "-1" ] || [ ${iters} == "4294967295" ]; then
	echo "ok 29"
else
	echo "not ok 29"
fi

geli setkey -n 0 -J $passfile1 -K $keyfile1 md${no} >/dev/null
geli setkey -n 1 -J $passfile2 -K $keyfile2 md${no} >/dev/null
geli detach md${no}

geli delkey -a md${no}
# Test that iterations is not set
iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} == "-1" ] || [ ${iters} == "4294967295" ]; then
	echo "ok 30"
else
	echo "not ok 30"
fi

geli init -B none -J $passfile1 -K $keyfile1 md${no}
geli setkey -n 1 -j $passfile1 -k $keyfile1 -J $passfile2 -K $keyfile2 md${no} >/dev/null
geli delkey -n 0 md${no}
# Test that iterations is set
iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} != "-1" ] && [ ${iters} != "4294967295" ]; then
	echo "ok 31"
else
	echo "not ok 31"
fi

geli delkey -n 1 -f md${no}
# Test that iterations is not set
iters=$(geli dump md${no} | awk -F": " '($1 == " iterations" || $1 == "iterations") { print $2 }')
if [ ${iters} == "-1" ] || [ ${iters} == "4294967295" ]; then
	echo "ok 32"
else
	echo "not ok 32"
fi

geli init -B none -P -K $keyfile1 md${no}
geli attach -p -k $keyfile1 md${no}

# Set passphrase on key 1 provider
geli setkey -n 1 -i 10 -J $passfile2 -K $keyfile2 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 33"
else
	echo "not ok 33"
fi

geli detach md${no}

geli init -B none -P -K $keyfile1 md${no}

# Set passphrase on key 1 detached provider
geli setkey -n 1 -i 10 -p -k $keyfile1 -J $passfile2 -K $keyfile2 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 34"
else
	echo "not ok 34"
fi

# Set passphrase on key 1 with different iterations
geli setkey -n 1 -i 20 -p -k $keyfile1 -J $passfile1 -K $keyfile1 md${no} >/dev/null
if [ $? -eq 0 ]; then
	echo "ok 35"
else
	echo "not ok 35"
fi

if [ ${hash1} = ${hash2} ]; then
	echo "ok 36"
else
	echo "not ok 36"
fi
if [ ${hash1} = ${hash3} ]; then
	echo "ok 37"
else
	echo "not ok 37"
fi
if [ ${hash1} = ${hash4} ]; then
	echo "ok 38"
else
	echo "not ok 38"
fi
if [ ${hash1} = ${hash5} ]; then
	echo "ok 39"
else
	echo "not ok 39"
fi
if [ ${hash1} = ${hash6} ]; then
	echo "ok 40"
else
	echo "not ok 40"
fi
if [ ${hash1} = ${hash7} ]; then
	echo "ok 41"
else
	echo "not ok 41"
fi
if [ ${hash1} = ${hash8} ]; then
	echo "ok 42"
else
	echo "not ok 42"
fi

rm -f $rnd $keyfile1 $keyfile2 $keyfile3 $keyfile4 $keyfile5 $passfile1 $passfile2
