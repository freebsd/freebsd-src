#!/bin/sh
# $FreeBSD$
pwd=`pwd`
echo -n "Uaktualniam katalog /etc na dyskietce...  "
mount /dev/fd0a /start_floppy
if [ "X$?" != "X0" ]
then
	echo ""
	echo "Blad podczas montowania read/write dyskietki!"
	echo "Sprawdz, czy nie jest zabezpieczona przed zapisem..."
	exit 1
fi
cd /etc
rm *.db
rm passwd
cp -Rp . /start_floppy/etc/
pwd_mkdb master.passwd
echo " Zrobione."
echo -n "Uaktualniam parametry jadra..."
kget /start_floppy/boot/kernel.conf
umount /dev/fd0a
cd ${pwd}
echo " Zrobione."
