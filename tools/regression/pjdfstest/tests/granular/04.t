#!/bin/sh
# $FreeBSD$

desc="NFSv4 granular permissions checking - ACL_WRITE_OWNER"

dir=`dirname $0`
. ${dir}/../misc.sh

[ "${os}:${fs}" = "FreeBSD:ZFS" ] || quick_exit

echo "1..52"

n0=`namegen`
n1=`namegen`
n2=`namegen`

expect 0 mkdir ${n2} 0755
cdir=`pwd`
cd ${n2}

# ACL_WRITE_OWNER permits to set gid to our own only.
expect 0 create ${n0} 0644
expect 0,0 lstat ${n0} uid,gid
expect EPERM -u 65534 -g 65532,65531 chown ${n0} -1 65532
expect 0,0 lstat ${n0} uid,gid
expect 0 prependacl ${n0} user:65534:write_owner::allow
expect EPERM -u 65534 -g 65532,65531 chown ${n0} -1 65530
expect 0,0 lstat ${n0} uid,gid
expect 0 -u 65534 -g 65532,65531 chown ${n0} -1 65532
expect 0,65532 lstat ${n0} uid,gid
expect 0 unlink ${n0}

# ACL_WRITE_OWNER permits to set uid to our own only.
expect 0 create ${n0} 0644
expect 0,0 lstat ${n0} uid,gid
expect EPERM -u 65534 -g 65532,65531 chown ${n0} 65534 65531
expect 0,0 lstat ${n0} uid,gid
expect 0 prependacl ${n0} user:65534:write_owner::allow
expect EPERM -u 65534 -g 65532,65531 chown ${n0} 65530 65531
expect 0,0 lstat ${n0} uid,gid
expect 0 -u 65534 -g 65532,65531 chown ${n0} 65534 65531
expect 65534,65531 lstat ${n0} uid,gid
expect 0 unlink ${n0}

# When non-owner calls chown(2) successfully, set-uid and set-gid bits are
# removed, except when both uid and gid are equal to -1.
expect 0 create ${n0} 0644
expect 0 prependacl ${n0} user:65534:write_owner::allow
expect 0 chmod ${n0} 06555
expect 06555 lstat ${n0} mode
expect 0 -u 65534 -g 65533,65532 chown ${n0} 65534 65532
expect 0555,65534,65532 lstat ${n0} mode,uid,gid
expect 0 chmod ${n0} 06555
expect 06555 lstat ${n0} mode
expect 0 -u 65534 -g 65533,65532 chown ${n0} -1 65533
expect 0555,65534,65533 lstat ${n0} mode,uid,gid
expect 0 chmod ${n0} 06555
expect 06555 lstat ${n0} mode
expect 0 -u 65534 -g 65533,65532 chown ${n0} -1 -1
expect 06555,65534,65533 lstat ${n0} mode,uid,gid
expect 0 unlink ${n0}

expect 0 mkdir ${n0} 0755
expect 0 prependacl ${n0} user:65534:write_owner::allow
expect 0 chmod ${n0} 06555
expect 06555 lstat ${n0} mode
expect 0 -u 65534 -g 65533,65532 chown ${n0} 65534 65532
expect 0555,65534,65532 lstat ${n0} mode,uid,gid
expect 0 chmod ${n0} 06555
expect 06555 lstat ${n0} mode
expect 0 -u 65534 -g 65533,65532 chown ${n0} -1 65533
expect 0555,65534,65533 lstat ${n0} mode,uid,gid
expect 0 chmod ${n0} 06555
expect 06555 lstat ${n0} mode
expect 0 -u 65534 -g 65533,65532 chown ${n0} -1 -1
expect 06555,65534,65533 lstat ${n0} mode,uid,gid
expect 0 rmdir ${n0}

cd ${cdir}
expect 0 rmdir ${n2}
