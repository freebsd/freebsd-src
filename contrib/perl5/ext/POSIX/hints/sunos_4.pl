# SunOS 4.1.3 has two extra fields in struct tm.  This works around
# the problem.  Other BSD platforms may have similar problems.
# This state of affairs also persists in glibc2, found
# on linux systems running libc6.
#  XXX A Configure test is needed.

# Although <unistd.h> is inappropriate in general for SunOS, we need it
# in POSIX.xs to get the correct prototype for ttyname().

$self->{CCFLAGS} = $Config{ccflags} . ' -DSTRUCT_TM_HASZONE -DI_UNISTD' ;
