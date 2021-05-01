#!/bin/sh

#
# Copyright (c) 2009 Peter Holm <pho@FreeBSD.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

# Test the implementation of i386_get_ldt() and i386_set_ldt() for 32 bit
# processes on amd64 by running wine and mplayer with a 32 bit codec.

# This is not a test script, but more of a howto document.

[ `uname -p` != "amd64" ] && echo "Must run on amd64" && exit

exit 0

This are notes of how to perform the test.

First of all you will need a i386 jail on amd64. This could be build like
this:

cat > jailbuild.sh <<EOF
#!/bin/sh

jail=/var/tmp/jail2 # Change this

mount | grep $jail | grep -q devfs && umount $jail/dev

here=`pwd`
chflags -R 0 $jail
rm -rf $jail
mkdir -p $jail

cd /var/tmp/deviant2 # You will need to change this!
make -j4 TARGET=i386 TARGET_ARCH=i386 DESTDIR=$jail world
make     TARGET=i386 TARGET_ARCH=i386 DESTDIR=$jail distribution

mount -t devfs devfs $jail/dev

cp /etc/fstab /etc/hosts /etc/resolv.conf $jail/etc
cp /boot/kernel/kernel $jail/boot/kernel
EOF

Before changing to the jail you may need these files:
- Fetch the Firefox i386 installer. (http://www.mozilla.com)
- Fetch a clip for a win32 codec:
  ftp http://www.jhepple.com/support/SampleMovies/Real_Media.rm

and place these in for examle $jail/root

Chroot to /var/tmp/jail /bin/sh

1) Install wine. For example by "UNAME_m=i386 pkg_add -r wine"
3) Run wine on the Firefox installer.

The mplayer test:

It would seem that the default build of mplayer does not contain
the i386 codec, so you have to build mplayer your self with option
"Enable win32 codec set on the IA32 arch".
Remember to set the environment variable UNAME_m to "i386".

cat > mplayer.sh <<EOF
#!/bin/sh

export DISPLAY=<your display host goes here>:0
while true;do
	pos=100
	for i in `jot 5`; do
		mplayer -vc rv40win -geometry $pos:$pos /root/samples/Real_Media.rm < \
			/dev/null > /dev/null 2>&1 &
		pos=$((pos + 50))
	done
	for i in `jot 5`; do
		wait
	done
done
EOF
