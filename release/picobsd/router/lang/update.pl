#!/bin/sh
# $Id: update.pl,v 1.2 1998/08/10 19:17:55 abial Exp $
echo -n "Uaktualniam katalog /etc na dyskietce...  "
mount /dev/fd0a /start_floppy
cd /etc
cp -Rp . /start_floppy/etc/
echo " Zrobione."
echo -n "Uaktualniam parametry jadra..."
kget -incore /start_floppy/kernel.config /stand/vanilla
umount /dev/fd0a
echo " Zrobione."
