#!/bin/sh
# $FreeBSD: src/release/picobsd/dial/lang/update.pl,v 1.2.2.2 1999/08/29 15:52:40 peter Exp $
pwd=`pwd`
echo -n "Uaktualniam katalog /etc na dyskietce...  "
mount /dev/fd0a /start_floppy
if [ "X$?" != "X0" ]
then
	echo ""
	echo "B³±d podczas montowania read/write dyskietki!"
	echo "Sprawd¼, czy nie jest zabezpieczona przed zapisem..."
	exit 1
fi
cd /etc
cp -Rp . /start_floppy/etc/
echo " Zrobione."
echo -n "Uaktualniam parametry j±dra..."
kget /start_floppy/boot/kernel.conf
umount /dev/fd0a
cd /etc
cd ${pwd}
echo " Zrobione."
