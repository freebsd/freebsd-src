# $Id: mfs.rc.pl,v 1.2 1998/08/10 19:17:54 abial Exp $
# This file is interpreted by oinit(8)
#
ncons 2
motd /etc/motd
set HOME=/
set PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin

sysctl -w vm.defer_swapspace_pageouts=1 vm.disable_swapspace_pageouts=1

### Special setup for one floppy PICOBSD ###
echo
echo "----------------------------------------------"
echo "  Prosze czekac. Trwa uruchamianie systemu..."
echo "----------------------------------------------"
echo
echo "Wczytuje konfiguracje z /etc z dyskietki..."
mount -o rdonly /dev/fd0a /start_floppy
cd /start_floppy/etc
cp -Rp . /etc
cd /etc
umount /dev/fd0a
echo "Ok. (Jesli chcesz, mozesz juz wyjac dyskietke)"
echo
. rc
