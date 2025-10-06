#!/bin/sh

ver="$1"

echo
echo --------------------------------------
echo Installing PuTTY version ${ver}
echo --------------------------------------

cd /tmp

case "${ver}" in
snapshot)
	tarball=putty.tar.gz
	url=https://tartarus.org/~simon/putty-snapshots/${tarball}
	;;
*)
	tarball=putty-${ver}.tar.gz
	url=https://the.earth.li/~sgtatham/putty/${ver}/${tarball}
	;;
esac

if [ ! -f ${tarball} ]; then
	wget -q ${url}
fi

mkdir -p /tmp/puttybuild
cd /tmp/puttybuild

tar xfz /tmp/${tarball} && cd putty-*
if [ -f CMakeLists.txt ]; then
	cmake . && cmake --build . -j4 && sudo cmake --build . --target install
else
	./configure && make -j4 && sudo make install
fi
sudo rm -rf /tmp/puttybuild
/usr/local/bin/plink -V
