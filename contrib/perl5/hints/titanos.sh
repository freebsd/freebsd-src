# Hints file (perl 4.019) for Kubota Pacific's Titan 3000 Series Machines.
# Created by: JT McDuffie (jt@kpc.com)  26 DEC 1991
# p5ed by: Jarkko Hietaniemi <jhi@iki.fi> Aug 27 1994
#  NOTE:   You should run Configure with tcsh (yes, tcsh).
# Comments by Andy Dougherty <doughera@lafcol.lafayette.edu> 28 Mar 1995
alignbytes="8"
byteorder="4321"
castflags='0'
gidtype='ushort'
groupstype='unsigned short'
intsize='4'
usenm='true'
nm_opt='-eh'
malloctype='void *'
models='none'
ccflags="$ccflags -I/usr/include/net -DDEBUGGING -DSTANDARD_C"
cppflags="$cppflags -I/usr/include/net -DDEBUGGING -DSTANDARD_C"
stdchar='unsigned char'
#
# Apparently there are some harmful libs in Configure's $libswanted.
# Perl5.000 had: libs='-lnsl -ldbm -lPW -lmalloc -lm'
# Unfortunately, this line prevents users from including things like
# -lgdbm and -ldb, which they may or may not have or want.
# We should probably fiddle with libswanted instead of libs.
# And even there, we should only bother to delete harmful libraries.
# However, I don't know what they are or why they should be deleted,
# so this will have to do for now.  --AD  28 Mar 1995
libswanted='sfio nsl dbm gdbm db PW malloc m'
#
# Extensions:  This system can not compile POSIX. We'll let Configure 
# figure out the others. 
useposix='n'
#
uidtype='ushort'
voidflags='7'
inclwanted='/usr/include /usr/include/net'
# Setting libpth shouldn't be needed any more.
# libpth='/usr/lib /usr/local/lib /lib'
pth='. /bin /usr/bin /usr/ucb /usr/local/bin /usr/X11/bin /usr/lbin /etc /usr/lib'
