For a normal CDROM or network installation, all you need to copy onto an
actual floppy from this directory is the boot.flp image (for 1.44MB floppies).

If you're on the ALPHA then the boot.flp image is probably larger
than any kind of floppy you have available and you will need to
either netboot it, load it from some other type of media (such
as a jaz drive) or use the kern.flp image described below.

This release still uses only one installation floppy, the boot.flp
image. For convenience (and for the DEC ALPHA architecture, on which
binaries are quite a bit larger), however, we also provide the
functionality of boot.flp now "decoupled" into a kern.flp image,
which contains just the boot kernel, and mfsroot.gz, which contains
the compressed MFS root image that is normally stored as part of
the kernel itself on the boot.flp image.  This allows you to boot
from kern.flp, which will fit on a 1.44MB floppy even on the alpha,
and then load mfsroot.gz from a 2nd floppy.  This also allows you
to easily make your own boot or MFS floppies should you need to customize
some aspect of the installation process.  As long as the kernel is compiled
with ``options MFS'' and ``options MFS_ROOT'', it will properly
boot an mfsroot.gz image when run.  The mfsroot.gz image is simply
a gzip'd filesystem image, something which can be made rather
easily using vnconfig(8).  If none of this makes any sense to you,
don't worry about it - just use the boot.flp image as always; nothing
has changed there.


NOTE: The *.flp images are NOT DOS files!  You cannot simply copy them
to a DOS or UFS floppy as regular files, you need to *image* copy them
to the floppy with fdimage.exe under DOS or `dd' under UNIX.

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

The only image which is copied onto a floppy as an ordinary file is
mfsroot.gz, should you actually be using that image for something.
