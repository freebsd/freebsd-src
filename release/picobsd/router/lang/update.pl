#!/bin/sh
# $Id: update.pl,v 1.1.1.1 1998/08/27 17:38:44 abial Exp $
echo -n "Uaktualniam katalog /etc na dyskietce...  "
mount /dev/fd0a /start_floppy
cd /etc
cp -Rp . /start_floppy/etc/
echo " Zrobione."
echo -n "Uaktualniam parametry jadra..."
kget /start_floppy/kernel.config
umount /dev/fd0a
echo " Zrobione."
