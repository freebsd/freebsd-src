#
# hints file for NEWS-OS 4.x
#

echo
echo 'Compiling Tips:'
echo 'When you have found that ld complains "multiple defined" error'
echo 'on linking /lib/libdbm.a, do following instructions.'
echo '    cd /tmp                                (working on /tmp)'
echo '    cp /lib/libdbm.a dbm.o                 (copy current libdbm.a)'
echo '    ar cr libdbm.a dbm.o                   (make archive)'
echo '    mv /lib/libdbm.a /lib/libdbm.a.backup  (backup original library)'
echo '    cp /tmp/libdbm.a /lib                  (copy newer one)'
echo '    ranlib /lib/libdbm.a                   (ranlib for later use)'
echo

# No shared library.
so='none'
# Umm.. I like gcc.
cc='gcc'
# Configure does not find out where is libm.
plibpth='/usr/lib/cmplrs/cc'
# times() returns 'struct tms'
clocktype='struct tms'
# getgroups(2) returns integer (not gid_t)
groupstype='int'
# time(3) returns long (not time_t)
timetype='long'
# filemode type is int (not mode_t)
modetype='int'
# using sprintf(3) instead of gcvt(3)
d_Gconvert='sprintf((b),"%.*g",(n),(x))'
# No POSIX.
useposix='false'
