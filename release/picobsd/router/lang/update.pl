#!/bin/sh
# $Id: update.pl,v 1.2.2.1 1999/05/07 10:03:38 abial Exp $
echo -n "Uaktualniam katalog /etc na dyskietce...  "
mount /dev/fd0a /start_floppy
cd /etc
cp -Rp . /start_floppy/etc/
echo " Zrobione."
echo -n "Uaktualniam parametry jadra..."
kget /start_floppy/boot/kernel.conf
umount /dev/fd0a
echo " Zrobione."
