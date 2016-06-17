Changes by Matija Nalis (mnalis@jagor.srce.hr) on umsdos dentry fixing
(started by Peter T. Waltenberg <peterw@karaka.chch.cri.nz>)
(Final conversion to dentries Bill Hawes <whawes@star.net>)

There is no warning any more.
Both read-only and read-write stuff is fixed, both in
msdos-compatibile mode, and in umsdos EMD mode, and it seems stable.

Userland NOTE: new umsdos_progs (umssync, umssetup, udosctl & friends) that
will compile and work on 2.2.x+ kernels and glibc based systems, as well as
kernel patches and other umsdos related information may be found at
http://linux.voyager.hr/umsdos/

Information below is getting outdated slowly -- I'll fix it one day when I
get enough time - there are more important things to fix right now.

Legend: those lines marked with '+' on the beggining of line indicates it
passed all of my tests, and performed perfect in all of them.

Current status (010125) - UMSDOS 0.86j:

(1) pure MSDOS (no --linux-.--- EMD file):

READ:
+ readdir			- works
+ lookup			- works
+ read file			- works

WRITE:
+ creat file			- works
+ unlink file			- works
+ write file			- works
+ rename file (same dir)	- works
+ rename file (dif. dir)	- works
+ rename dir (same dir)		- works
+ rename dir (dif. dir)		- works
+ mkdir				- works
+ rmdir 			- works


(2) umsdos (with --linux-.--- EMD file):

READ:
+ readdir			- works
+ lookup 			- works
+ permissions/owners stuff	- works
+ long file names		- works
+ read file			- works
+ switching MSDOS/UMSDOS	- works
+ switching UMSDOS/MSDOS	- works
- pseudo root things		- works mostly. See notes below.
+ resolve symlink		- works
+ dereference symlink		- works
+ dangling symlink		- works
+ hard links			- works
+ special files (block/char devices, FIFOs, sockets...)	- works
+ various umsdos ioctls		- works


WRITE:
+ create symlink		- works
+ create hardlink		- works
+ create file			- works
+ create special file		- works
+ write to file			- works
+ rename file (same dir)	- works
+ rename file (dif. dir)	- works
+ rename hardlink (same dir)	- works
- rename hardlink (dif. dir)	- works, but see notes below.
+ rename symlink (same dir)	- works
+ rename symlink (dif. dir)	- works
+ rename dir (same dir)		- works
+ rename dir (dif. dir)		- works
+ unlink file			- works
+ notify_change (chown,perms)	- works
+ notify_change for hardlinks	- works
+ unlink hardlink		- works
+ mkdir				- works
+ rmdir 			- works
+ umssyncing (many ioctls)	- works


- CVF-FAT stuff (compressed DOS filesystem) - there is some support from Frank
  Gockel <gockel@sent13.uni-duisburg.de> to use it even under umsdosfs, but I
  have no way of testing it -- please let me know if there are problems specific
  to umsdos (for instance, it works under msdosfs, but not under umsdosfs).


Some current notes:

Note: creating and using pseudo-hardlinks is always non-perfect, especially
in filesystems that might be externally modified like umsdos. There is
example is specs file about it. Specifically, moving directory which
contains hardlinks will break them.

Note: (about creating hardlinks in pseudoroot mode) - hardlinks created in
pseudoroot mode are now again compatibile with 'normal' hardlinks, and vice
versa. Thanks to Sorin Iordachescu <sorin@rodae.ro> for providing fix.
See http://linux.voyager.hr/umsdos/hlbug.html for more info and upgrade
procedure if you used broken versions...

------------------------------------------------------------------------------

Some general notes:

Good idea when running development kernels is to have SysRq support compiled
in kernel, and use Sync/Emergency-remount-RO if you bump into problems (like
not being able to umount(2) umsdosfs, and because of it root partition also,
or panics which force you to reboot etc.)

I'm unfortunately somewhat out of time to read linux-kernel@vger, but I do
check for messages having "UMSDOS" in the subject, and read them.  I might
miss some in all that volume, though.  I should reply to any direct e-mail
in few days.  If I don't, probably I never got your message.
