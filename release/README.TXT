For a normal CDROM or network installation, all you need to copy onto an
actual floppy from this directory is the boot.flp image (for 1.44MB floppies).

NOTE: These images are NOT DOS files!  You cannot simply copy them to
a DOS floppy as regular files, you need to *image* copy them to the
floppy with fdimage.exe under DOS or `dd' under UNIX.

For example:

To create the boot floppy image from DOS, you'd do something like
this:

C> fdimage boot.flp a:

Assuming that you'd copied fdimage.exe and boot.flp into a directory
somewhere.  If you were doing this from the base of a CD distribution,
then the *exact* command would be:

E> tools\fdimage floppies\boot.flp a:


If you're creating the boot floppy from a UNIX machine, you may find
that:

        dd if=floppies/boot.flp of=/dev/rfd0

or

        dd if=floppies/boot.flp of=/dev/floppy

work well, depending on your hardware and operating system environment
(different versions of UNIX have totally different names for the
floppy drive - neat, huh? :-).
