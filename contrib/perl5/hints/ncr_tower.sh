#  For SysV release 2, there are no directory functions defined.  To
#  prevent compile errors, acquire the functions written by Doug Gwynn.
#  They are contained in dirent.tar.gz and can be accessed from gnu 
#  repositories, as well as other places.
#
#	The following hints have been verified to work with PERL5 (001m) on
#  SysVr2 with the following caveat(s):
#	1. Maximum User program space (MAXSPACE) must be at least 2MB.
#	2. The directory functions mentioned above have been installed.
#
optimize='-O0'
ccflags="$ccflags -W2,-Sl,1500 -W0,-Sp,350,-Ss,2500 -Wp,-Sd,30"
d_mkdir=$undef
usemymalloc='y'
useposix='false'
so='none'
