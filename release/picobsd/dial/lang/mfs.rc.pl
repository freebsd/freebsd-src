#!/bin/sh
# $Id: mfs.rc.pl,v 1.3 1998/08/07 19:29:57 abial Exp $

### WARNING !!!! ###
# We remove this file during execution (see EOF)
# Awful things happen if its size is > 1024B
trap : 2
trap : 3	# shouldn't be needed
HOME=/; export HOME
PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin
export PATH
trap "echo 'Reboot zostal przerwany'; exit 1" 3
sysctl -w vm.defer_swapspace_pageouts=1 vm.disable_swapspace_pageouts=1 2>&1 >/dev/null
echo ""
echo "-----------------------------------------------"
echo "  Prosze czekac. Trwa uruchamianie systemu..."
echo "-----------------------------------------------"
echo ""
echo "Wczytuje konfiguracje z /etc z dyskietki... "
mount -o rdonly /dev/fd0a /start_floppy
cd /start_floppy/etc
cp -Rp . /etc/
cd /etc
umount /start_floppy
echo "Ok. (Jesli chcesz, mozesz juz wyjac dyskietke)"
echo ""
. rc
exit 0
