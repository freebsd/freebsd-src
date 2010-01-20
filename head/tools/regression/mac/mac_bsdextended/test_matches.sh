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

playground="/stuff/nobody/" # Must not be on root fs

#
# Setup
#
rm -f $playground/test*
ugidfw remove 1

file1=$playground/test-$uidinrange
file2=$playground/test-$uidoutrange
cat <<EOF> $playground/test-script.pl
if (open(F, ">" . shift)) { exit 0; } else { exit 1; }
EOF
command1="perl $playground/test-script.pl $file1"
command2="perl $playground/test-script.pl $file2"

echo -n "$uidinrange file: "
su -m $uidinrange -c "$command1 && echo good"
chown "$uidinrange":"$gidinrange" $file1
chmod a+w $file1

echo -n "$uidoutrange file: "
$command2 && echo good
chown "$uidoutrange":"$gidoutrange" $file2
chmod a+w $file2

#
# No rules
#
echo -n "no rules $uidinrange: "
su -fm $uidinrange -c "$command1 && echo good"
echo -n "no rules $uidoutrange: "
su -fm $uidoutrange -c "$command1 && echo good"

#
# Subject Match on uid
#
ugidfw set 1 subject uid $uidrange object mode rasx
echo -n "subject uid in range: "
su -fm $uidinrange -c "$command1 || echo good"
echo -n "subject uid out range: "
su -fm $uidoutrange -c "$command1 && echo good"

#
# Subject Match on gid
#
ugidfw set 1 subject gid $gidrange object mode rasx
echo -n "subject gid in range: "
su -fm $uidinrange -c "$command1 || echo good"
echo -n "subject gid out range: "
su -fm $uidoutrange -c "$command1 && echo good"

#
# Subject Match on jail
#
echo -n "subject matching jailid: "
rm -f $playground/test-jail
jailid=`jail -i / localhost 127.0.0.1 /usr/sbin/daemon -f /bin/sh -c "(sleep 3; touch $playground/test-jail) &"`
ugidfw set 1 subject jailid $jailid object mode rasx
sleep 6
if [ ! -f $playground/test-jail ] ; then echo good ; fi

echo -n "subject nonmatching jailid: "
rm -f $playground/test-jail
jailid=`jail -i / localhost 127.0.0.1 /usr/sbin/daemon -f /bin/sh -c "(sleep 3; touch $playground/test-jail) &"`
sleep 6
if [ -f $playground/test-jail ] ; then echo good ; fi

#
# Object uid
#
ugidfw set 1 subject object uid $uidrange mode rasx
echo -n "object uid in range: "
su -fm $uidinrange -c "$command1 || echo good"
echo -n "object uid out range: "
su -fm $uidinrange -c "$command2 && echo good"
ugidfw set 1 subject object uid $uidrange mode rasx
echo -n "object uid in range (differennt subject): "
su -fm $uidoutrange -c "$command1 || echo good"
echo -n "object uid out range (differennt subject): "
su -fm $uidoutrange -c "$command2 && echo good"

#
# Object gid
#
ugidfw set 1 subject object gid $uidrange mode rasx
echo -n "object gid in range: "
su -fm $uidinrange -c "$command1 || echo good"
echo -n "object gid out range: "
su -fm $uidinrange -c "$command2 && echo good"
echo -n "object gid in range (differennt subject): "
su -fm $uidoutrange -c "$command1 || echo good"
echo -n "object gid out range (differennt subject): "
su -fm $uidoutrange -c "$command2 && echo good"

#
# Object filesys
#
ugidfw set 1 subject uid $uidrange object filesys / mode rasx
echo -n "object out of filesys: "
su -fm $uidinrange -c "$command1 && echo good"
ugidfw set 1 subject uid $uidrange object filesys $playground mode rasx
echo -n "object in filesys: "
su -fm $uidinrange -c "$command1 || echo good"

#
# Object suid
#
ugidfw set 1 subject uid $uidrange object suid mode rasx
echo -n "object notsuid: "
su -fm $uidinrange -c "$command1 && echo good"
chmod u+s $file1
echo -n "object suid: "
su -fm $uidinrange -c "$command1 || echo good"
chmod u-s $file1

#
# Object sgid
#
ugidfw set 1 subject uid $uidrange object sgid mode rasx
echo -n "object notsgid: "
su -fm $uidinrange -c "$command1 && echo good"
chmod g+s $file1
echo -n "object sgid: "
su -fm $uidinrange -c "$command1 || echo good"
chmod g-s $file1

#
# Object uid matches subject
#
ugidfw set 1 subject uid $uidrange object uid_of_subject mode rasx
echo -n "object uid notmatches subject: "
su -fm $uidinrange -c "$command2 && echo good"
echo -n "object uid matches subject: "
su -fm $uidinrange -c "$command1 || echo good"

#
# Object gid matches subject
#
ugidfw set 1 subject uid $uidrange object gid_of_subject mode rasx
echo -n "object gid notmatches subject: "
su -fm $uidinrange -c "$command2 && echo good"
echo -n "object gid matches subject: "
su -fm $uidinrange -c "$command1 || echo good"

#
# Object type
#
ugidfw set 1 subject uid $uidrange object type dbclsp mode rasx
echo -n "object not type: "
su -fm $uidinrange -c "$command1 && echo good"
ugidfw set 1 subject uid $uidrange object type r mode rasx
echo -n "object type: "
su -fm $uidinrange -c "$command1 || echo good"

