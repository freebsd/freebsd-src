#!/bin/sh
#
# $FreeBSD$
#

uidrange="60000:100000"
gidrange="60000:100000"
uidinrange="nobody"
uidoutrange="daemon"
gidinrange="nobody" # We expect $uidinrange in this group
gidoutrange="daemon" # We expect $uidinrange in this group

#
# Setup
#

: ${TMPDIR=/tmp}
if [ $(id -u) -ne 0 ]; then
	echo "1..0 # SKIP test must be run as root"
	exit 0
fi
if ! playground=$(mktemp -d $TMPDIR/tmp.XXXXXXX); then
	echo "1..0 # SKIP failed to create temporary directory"
	exit 0
fi
trap "rmdir $playground" EXIT INT TERM
if ! mdmfs -s 25m md $playground; then
	echo "1..0 # SKIP failed to mount md device"
	exit 0
fi
chmod a+rwx $playground
md_device=$(mount -p | grep "$playground" | awk '{ gsub(/^\/dev\//, "", $1); print $1 }')
trap "umount -f $playground; mdconfig -d -u $md_device; rmdir $playground" EXIT INT TERM
if [ -z "$md_device" ]; then
	mount -p | grep $playground
	echo "1..0 # md device not properly attached to the system"
fi

ugidfw remove 1

file1=$playground/test-$uidinrange
file2=$playground/test-$uidoutrange
cat > $playground/test-script.sh <<'EOF'
#!/bin/sh
: > $1
EOF
if [ $? -ne 0 ]; then
	echo "1..0 # SKIP failed to create test script"
	exit 0
fi
echo "1..30"

command1="sh $playground/test-script.sh $file1"
command2="sh $playground/test-script.sh $file2"

echo "# $uidinrange file:"
su -m $uidinrange -c "if $command1; then echo ok; else echo not ok; fi"
chown "$uidinrange":"$gidinrange" $file1
chmod a+w $file1

echo "# $uidoutrange file:"
if $command2; then echo ok; else echo not ok; fi
chown "$uidoutrange":"$gidoutrange" $file2
chmod a+w $file2

#
# No rules
#
echo "# no rules $uidinrange:"
su -fm $uidinrange -c "if $command1; then echo ok; else echo not ok; fi"
echo "# no rules $uidoutrange:"
su -fm $uidoutrange -c "if $command1; then echo ok; else echo not ok; fi"

#
# Subject Match on uid
#
ugidfw set 1 subject uid $uidrange object mode rasx
echo "# subject uid in range:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"
echo "# subject uid out range:"
su -fm $uidoutrange -c "if $command1; then echo ok; else echo not ok; fi"

#
# Subject Match on gid
#
ugidfw set 1 subject gid $gidrange object mode rasx
echo "# subject gid in range:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"
echo "# subject gid out range:"
su -fm $uidoutrange -c "if $command1; then echo ok; else echo not ok; fi"

#
# Subject Match on jail
#
rm -f $playground/test-jail
echo "# subject matching jailid:"
jailid=`jail -i / localhost 127.0.0.1 /usr/sbin/daemon -f /bin/sh -c "(sleep 5; touch $playground/test-jail) &"`
ugidfw set 1 subject jailid $jailid object mode rasx
sleep 10
if [ -f $playground/test-jail ]; then echo not ok; else echo ok; fi

rm -f $playground/test-jail
echo "# subject nonmatching jailid:"
jailid=`jail -i / localhost 127.0.0.1 /usr/sbin/daemon -f /bin/sh -c "(sleep 5; touch $playground/test-jail) &"`
sleep 10
if [ -f $playground/test-jail ]; then echo ok; else echo not ok; fi

#
# Object uid
#
ugidfw set 1 subject object uid $uidrange mode rasx
echo "# object uid in range:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"
echo "# object uid out range:"
su -fm $uidinrange -c "if $command2; then echo ok; else echo not ok; fi"
ugidfw set 1 subject object uid $uidrange mode rasx
echo "# object uid in range (differennt subject):"
su -fm $uidoutrange -c "if $command1; then echo not ok; else echo ok; fi"
echo "# object uid out range (differennt subject):"
su -fm $uidoutrange -c "if $command2; then echo ok; else echo not ok; fi"

#
# Object gid
#
ugidfw set 1 subject object gid $uidrange mode rasx
echo "# object gid in range:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"
echo "# object gid out range:"
su -fm $uidinrange -c "if $command2; then echo ok; else echo not ok; fi"
echo "# object gid in range (different subject):"
su -fm $uidoutrange -c "if $command1; then echo not ok; else echo ok; fi"
echo "# object gid out range (different subject):"
su -fm $uidoutrange -c "if $command2; then echo ok; else echo not ok; fi"

#
# Object filesys
#
ugidfw set 1 subject uid $uidrange object filesys / mode rasx
echo "# object out of filesys:"
su -fm $uidinrange -c "if $command1; then echo ok; else echo not ok; fi"
ugidfw set 1 subject uid $uidrange object filesys $playground mode rasx
echo "# object in filesys:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"

#
# Object suid
#
ugidfw set 1 subject uid $uidrange object suid mode rasx
echo "# object notsuid:"
su -fm $uidinrange -c "if $command1; then echo ok; else echo not ok; fi"
chmod u+s $file1
echo "# object suid:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"
chmod u-s $file1

#
# Object sgid
#
ugidfw set 1 subject uid $uidrange object sgid mode rasx
echo "# object notsgid:"
su -fm $uidinrange -c "if $command1; then echo ok; else echo not ok; fi"
chmod g+s $file1
echo "# object sgid:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"
chmod g-s $file1

#
# Object uid matches subject
#
ugidfw set 1 subject uid $uidrange object uid_of_subject mode rasx
echo "# object uid notmatches subject:"
su -fm $uidinrange -c "if $command2; then echo ok; else echo not ok; fi"
echo "# object uid matches subject:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"

#
# Object gid matches subject
#
ugidfw set 1 subject uid $uidrange object gid_of_subject mode rasx
echo "# object gid notmatches subject:"
su -fm $uidinrange -c "if $command2; then echo ok; else echo not ok; fi"
echo "# object gid matches subject:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"

#
# Object type
#
ugidfw set 1 subject uid $uidrange object type dbclsp mode rasx
echo "# object not type:"
su -fm $uidinrange -c "if $command1; then echo ok; else echo not ok; fi"
ugidfw set 1 subject uid $uidrange object type r mode rasx
echo "# object type:"
su -fm $uidinrange -c "if $command1; then echo not ok; else echo ok; fi"

