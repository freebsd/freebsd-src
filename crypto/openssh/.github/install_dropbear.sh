#!/bin/sh
#
# Install specified Dropbear version into /usr/local.
#

ver="$1"

echo
echo ---------------------------------------------
echo Installing dropbear version: ${ver}
echo ---------------------------------------------

set -e

cd /tmp

if [ ! -d dropbear-clean ]; then
	git clone https://github.com/mkj/dropbear.git dropbear-clean
	(cd dropbear-clean && git config --global advice.detachedHead false)
fi

rm -rf dropbear
cp -a dropbear-clean dropbear
cd dropbear
git checkout "$ver"
git status
echo "Building Dropbear version '$ver'"
(
	autoreconf 2>&1 &&
	./configure 2>&1 &&
	make clean 2>&1 &&
	make 2>&1 &&
	sudo make install 2>&1) >/tmp/db-build.log 2>&1 || cat /tmp/db-build.log

echo "Installed dropbear version: '$(/usr/local/bin/dbclient -V 2>&1)'"
