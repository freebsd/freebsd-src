# hints/gnu.sh
# Last modified: Thu Dec 10 20:47:28 CET 1998
# Mark Kettenis <kettenis@phys.uva.nl>

# libnsl is unusable on the Hurd.
# XXX remove this once SUNRPC is implemented.
set `echo X "$libswanted "| sed -e 's/ nsl / /'`
shift
libswanted="$*"

case "$optimize" in
'') optimize='-O2' ;;
esac

# Flags needed to produce shared libraries.
lddlflags='-shared'

# Flags needed by programs that use dynamic linking.
ccdlflags='-Wl,-E'

# The following routines are only available as stubs in GNU libc.
# XXX remove this once metaconf detects the GNU libc stubs.
d_msgctl='undef'
d_msgget='undef'
d_msgrcv='undef'
d_msgsnd='undef'
d_semctl='undef'
d_semget='undef'
d_semop='undef'
d_shmat='undef'
d_shmctl='undef'
d_shmdt='undef'
d_shmget='undef'
