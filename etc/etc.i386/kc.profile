# $Header
#
# rc for kernel distribution floppy

PATH=/bin:/sbin
export PATH

reboot_it() {
	echo    ""
	echo    "halting the machine..."
	
	halt
	sync
	sync
	echo "Halt failed!  Try power-cycling the machine..."
	exit 1
}

bail_out() {
	echo    ""
	echo	"Time to reboot the machine!"
	echo	"Once the machine has halted (it'll tell you when),"
	echo	"remove the floppy from the disk drive and press"
	echo    "any key to reboot."
	reboot_it
}

echo    Enter '"copy"' at the prompt to copy the kernel on this
echo    floppy to your hard disk.  enter anything else to reboot,
echo	but wait for the machine to restart to remove the floppy.
echo    ""
echo -n "kc> "

read todo

if [ X"$todo" = Xcopy ]; then
	echo    ""
	echo    "What disk partition should the kernel be installed on?"
	echo    "(e.g., "wd0a", "sd0a", etc.)"
	echo    ""
	echo -n "copy kernel to> "
	while :; do
		read diskpart junk
		[ -c /dev/r$diskpart ] && break
		echo "${diskpart}: invalid partition"
		echo
		echo -n "copy kernel to> "
	done
	echo    ""
	echo    "Checking the filesystem on $diskpart..."
	fsck -y /dev/r$diskpart
	if [ $? -ne 0 ]; then
		echo ""
		echo "fsck failed...  Sorry, can't copy kernel..."
		bail_out
	fi
	echo    ""
	echo    "mounting $diskpart on /mnt..."
	mount /dev/$diskpart /mnt
	if [ $? -ne 0 ]; then
		echo ""
		echo "mount failed...  Sorry, can't copy kernel..."
		bail_out
	fi
	echo    ""
	echo    "Please wait.  Copying kernel..."
	cp /386bsd /mnt/386bsd
	if [ $? -ne 0 ]; then
		echo ""
		echo "Copy failed...  (?!?!?!)"
		bail_out
	fi
	echo    ""
	echo    "unmounting $diskpart..."
	umount /mnt > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo ""
		echo "unmount failed...  Shouldn't be a problem..."
	fi
	bail_out
fi

reboot_it
