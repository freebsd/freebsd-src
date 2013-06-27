#!/bin/sh -x

# to be run in kernel compile directory

dist="$(pwd)/dist"

rsync_bin="rsync_vps.$(uname -m)"

rm -r ${dist}
make install DESTDIR=${dist}
mkdir -p ${dist}/sbin
mkdir -p ${dist}/usr/sbin
mkdir -p ${dist}/usr/share/man
mkdir -p ${dist}/usr/share/man/man4
mkdir -p ${dist}/usr/share/man/man5
mkdir -p ${dist}/usr/share/man/man8
mkdir -p ${dist}/usr/share/man/man9
cd ../../../../usr.sbin/vpsctl && make clean && make && make install DESTDIR=${dist} && make clean && cd - || exit 1
cd ../../../../sbin/mount_vpsfs && make clean && make && make install DESTDIR=${dist} && make clean && cd - || exit 1
cp ../../../../tools/vps/rsync/${rsync_bin} ${dist}/usr/sbin/rsync_vps
cp ../../../../tools/vps/dist-README ${dist}/README

cd dist
tar czpvf ../dist.tgz *
cd ..
ls -lh dist.tgz
rm -r ${dist}

exit 0

# EOF
