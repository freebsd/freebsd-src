For a normal CDROM or network installation, all you need to copy onto an
actual floppy from this directory is the boot.flp image (for 1.44MB floppies).

NOTE:  These images are NOT DOS files!  You cannot simply copy them to a DOS
floppy as regular files, you need to *image* copy them to the floppy with
rawrite.exe or `dd' under UNIX (again, please see the installation docs
[in INSTALL.TXT] for details).  Also note that the rawrite.exe program
ONLY WORKS UNDER DOS.  If you're running Win95, boot into DOS mode before
running it.  If you're running NT, use a different machine to make your
boot floppy.  Both Win95 and WinNT interfere with rawrite's abiltiy to
talk directly to the floppy drive.
