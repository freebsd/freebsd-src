#!/bin/sh

# Test file operations using long random file names containing UTF-16 surrogate pairs

MDUNIT=10
FS=/mnt
LOCALE=C.UTF-8
FILES=1000

export LANG=$LOCALE

randomfilename () {
	name=""
	count=$(jot -r 1 10 3)
	for r in $(jot -r $count 7 0); do
		r=$(( r + 0 ))
		case $(jot -r 1 3 1) in
		1)
			emoji="\0360\0237\0230\020$r"
			c=$(echo -e $emoji)
			;;
		*)
			food="\0360\0237\0245\022$r"
			c=$(echo -e $food)
			;;
		esac
		if [ $r -gt 0 ]; then
			for i in $(jot $r); do
				name="$name$i"
			done
		fi
		count=$(( count - 1 ))
		if [ "$count" -gt 0 ]; then
			name="$name$c"
		fi
	done
	echo "$name"
}

(
	set -e

	mdconfig -u $MDUNIT -t malloc -s 512m
	newfs_msdos -c 8 -F 32 /dev/md$MDUNIT > /dev/null 2>&1
	mkdir -p $FS
	mount_msdosfs -L $LOCALE /dev/md$MDUNIT $FS

	mkdir -p $FS/test
	cd $FS/test

	for i in $(jot $FILES); do
		testfiles="$testfiles
$(randomfilename)"
	done

	testfiles=$(echo "$testfiles" | grep "." | sort -R | uniq)

	for f in $testfiles; do
		echo "$f" > $f
	done
	for f in $(echo "$testfiles" | sort -R); do
		cp $f $f.tmp
	done
	for f in $(echo "$testfiles" | sort -R); do
		mv $f.tmp $f
	done
	for f in $(echo "$testfiles" | sort -R); do
		rm $f
	done
)

failed=$?

cd

[ "$failed" -ne 0 ] && ls $FS/test

umount /dev/md$MDUNIT

#[ "$failed" -ne 0 ] && hd /dev/md$MDUNIT > /tmp/msdos24.dump

fsck_msdosfs -y /dev/md$MDUNIT

mdconfig -d -u $MDUNIT 2>/dev/null

exit $failed
