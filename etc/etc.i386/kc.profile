# $Header
#
# rc for kernel distribution floppy

PATH=/bin:/sbin
export PATH

#test=echo

reboot_it() {
	echo    ""
	echo    "halting the machine..."
	
	${test} halt
}

bail_out() {
	echo    ""
	echo	"Time to reboot the machine!"
	echo	"Once the machine has halted (it'll tell you when),"
	echo	"remove the floppy from the disk drive and press"
	echo    "any key to reboot."
	reboot_it
}

echo    enter '"copy"' at the prompt to copy the kernel on this
echo    floppy to your hard disk.  enter anything else to reboot,
echo	but wait for the machine to restart to remove the floppy.
echo    ""
echo -n "> "

read todo

if [ "$todo"X = copyX ]; then
	echo    ""
	echo    "what disk partition should the kernel be installed on?"
	echo    "(e.g. "wd0a", "sd0a", etc.)"
	echo    ""
	echo -n "> "
	
	read diskpart

	echo    ""
	echo    "checking the filesystem on $diskpart..."

	${test} fsck -y /dev/r$diskpart
	if [ $? -ne 0 ]; then
		echo ""
		echo "fsck failed...  sorry, can't copy kernel..."
		bail_out
	fi

	echo    ""
	echo    "mounting $diskpart on /mnt..."

	${test} mount /dev/$diskpart /mnt
	if [ $? -ne 0 ]; then
		echo ""
		echo "mount failed...  sorry, can't copy kernel..."
		bail_out
	fi

	echo    ""
	echo    "copying kernel..."

	${test} cp /netbsd /mnt/netbsd
	if [ $? -ne 0 ]; then
		echo ""
		echo "copy failed...  (?!?!?!)"
		bail_out
	fi

	echo    ""
	echo    "unmounting $diskpart..."

	${test} umount /mnt > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo ""
		echo "unmount failed...  shouldn't be a problem..."
	fi

	bail_out
fi

reboot_it
