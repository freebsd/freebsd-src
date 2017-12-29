#!/bin/sh
# $FreeBSD$

. $(dirname $0)/conf.sh

echo 1..27

BLK=512
BLKS_PER_MB=2048

md=$(attach_md -t malloc -s40m)
i=1

fsck_md()
{
	local is_clean

	out=$(fsck_ffs -Ffy ${md}a.eli)
	if [ $? -eq 0 -o $? -eq 7 ]; then
		echo "ok $i - fsck says ${md}a.eli is clean"
	else
		echo "not ok $i - fsck says ${md}a.eli is dirty"
	fi
	i=$((i + 1))
}

setsize() {
    partszMB=$1 unitszMB=$2

    {
	echo a: $(($partszMB * $BLKS_PER_MB)) 0 4.2BSD 1024 8192
	echo c: $(($unitszMB * $BLKS_PER_MB)) 0 unused 0 0
    } | bsdlabel -R $md /dev/stdin
}

# Initialise

setsize 10 40 || echo -n "not "
echo ok $i - "Sized ${md}a to 10m"
i=$((i + 1))

echo secret >tmp.key
geli init -Bnone -PKtmp.key ${md}a || echo -n "not "
echo ok $i - "Initialised geli on ${md}a"
i=$((i + 1))
geli attach -pk tmp.key ${md}a || echo -n "not "
echo ok $i - "Attached ${md}a as ${md}a.eli"
i=$((i + 1))

newfs -U ${md}a.eli >/dev/null || echo -n "not "
echo ok $i - "Initialised the filesystem on ${md}a.eli"
i=$((i + 1))
fsck_md

# Doing a backup, resize & restore must be forced (with -f) as geli
# verifies that the provider size in the metadata matches the consumer.

geli backup ${md}a tmp.meta || echo -n "not "
echo ok $i - "Backed up ${md}a metadata"
i=$((i + 1))

geli detach ${md}a.eli || echo -n "not "
echo ok $i - "Detached ${md}a.eli"
i=$((i + 1))

setsize 20 40 || echo -n "not "
echo ok $i - "Sized ${md}a to 20m"
i=$((i + 1))
geli attach -pktmp.key ${md}a && echo -n "not "
echo ok $i - "Attaching ${md}a fails after resizing the consumer"
i=$((i + 1))

geli restore tmp.meta ${md}a && echo -n "not "
echo ok $i - "Restoring metadata on ${md}a.eli fails without -f"
i=$((i + 1))
geli restore -f tmp.meta ${md}a || echo -n "not "
echo ok $i - "Restoring metadata on ${md}a.eli can be forced"
i=$((i + 1))

geli attach -pktmp.key ${md}a || echo -n "not "
echo ok $i - "Attaching ${md}a is now possible"
i=$((i + 1))

growfs -y ${md}a.eli >/dev/null || echo -n "not "
echo ok $i - "Extended the filesystem on ${md}a.eli"
i=$((i + 1))

fsck_md

# Now do the resize properly

geli detach ${md}a.eli || echo -n "not "
echo ok $i - "Detached ${md}a.eli"
i=$((i + 1))

setsize 30 40 || echo -n "not "
echo ok $i - "Sized ${md}a to 30m"
i=$((i + 1))

geli resize -s20m ${md}a || echo -n "not "
echo ok $i - "Resizing works ok"
i=$((i + 1))
geli resize -s20m ${md}a && echo -n "not "
echo ok $i - "Resizing doesn't work a 2nd time (no old metadata)"
i=$((i + 1))

geli attach -pktmp.key ${md}a || echo -n "not "
echo ok $i - "Attaching ${md}a works ok"
i=$((i + 1))

growfs -y ${md}a.eli >/dev/null || echo -n "not "
echo ok $i - "Extended the filesystem on ${md}a.eli"
i=$((i + 1))

fsck_md

geli detach ${md}a.eli
gpart destroy -F $md >/dev/null


# Verify that the man page example works, changing ada0 to $md,
# 1g to 20m, 2g to 30m and keyfile to tmp.key, and adding -B none
# to geli init.

gpart create -s GPT $md || echo -n "not "
echo ok $i - "Installed a GPT on ${md}"
i=$((i + 1))
gpart add -s 20m -t freebsd-ufs -i 1 $md || echo -n "not "
echo ok $i - "Added a 20m partition in slot 1"
i=$((i + 1))
geli init -B none -K tmp.key -P ${md}p1 || echo -n "not "
echo ok $i - "Initialised geli on ${md}p1"
i=$((i + 1))
gpart resize -s 30m -i 1 $md || echo -n "not "
echo ok $i - "Resized partition ${md}p1 to 30m"
i=$((i + 1))
geli resize -s 20m ${md}p1 || echo -n "not "
echo ok $i - "Resized geli on ${md}p1 to 30m"
i=$((i + 1))
geli attach -k tmp.key -p ${md}p1 || echo -n "not "
echo ok $i - "Attached ${md}p1.eli"
i=$((i + 1))

geli detach ${md}p1.eli

rm tmp.*
