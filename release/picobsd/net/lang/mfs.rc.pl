#!/bin/sh
# $Id: mfs.rc.pl,v 1.3 1998/08/10 19:17:10 abial Exp $
# System startup script run by init on autoboot
# or after single-user.

stty status '^T'

trap : 2
trap : 3	# shouldn't be needed

HOME=/; export HOME
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin
export PATH

trap "echo 'Reboot zostal przerwany'; exit 1" 3

############################################
### Special setup for one floppy PICOBSD ###
############################################
echo ""
echo "-----------------------------------------------"
echo "  Prosze czekac. Trwa uruchamianie systemu..."
echo "-----------------------------------------------"
echo ""
echo "Wczytuje konfiguracje /etc z dyskietki..."
mount -o rdonly /dev/fd0a /start_floppy
cd /start_floppy/etc
cp -Rp . /etc/
cd /etc
pwd_mkdb -p ./master.passwd
umount /dev/fd0a
echo "Ok. (Jesli chcesz, mozesz juz wyjac dyskietke)"
echo ""
. rc
exit 0
