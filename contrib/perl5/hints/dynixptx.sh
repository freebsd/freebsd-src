# Sequent Dynix/Ptx v. 4 hints
# Created 1996/03/15 by Brad Howerter, bhower@wgc.woodward.com

# Modified 1998/11/10 by Martin J. Bligh, mbligh@sequent.com
# to incorporate work done by Kurtis D. Rader & myself.

# Use Configure -Dcc=gcc to use gcc.

# cc wants -G for dynamic loading
lddlflags='-G'

# Remove inet to avoid this error in Configure, which causes Configure
# to be unable to figure out return types:
# dynamic linker: ./ssize: can't find libinet.so,
# link with -lsocket instead of -linet

libswanted=`echo $libswanted | sed -e 's/ inet / /'`

# Configure defaults to usenm='y', which doesn't work very well
usenm='n'

# removed d_vfork='define'; we can't use it any more ...

case "$optimize" in
'') optimize='-Wc,-O3 -W0,-xstring' ;;
esac

# We override d_socket because it's very hard for Configure to get it right
# in Dynix/Ptx, for several reasons.
# (1) the socket interface is in libsocket.so -- this wouldn't be so hard
#     for Configure to fathom...but it gets more tangled.
# (2) if the system has been patched there can be libsocket.so.1.FOO.BAR,
#     the FOO.BAR being the old version of the system before the patching.
#     Configure picks up the old broken version.
# (3) libsocket.so points to either libsocket.so.1 (v4.2)
#     or libsocket.so.1.1 (v4.4)  The socket call in libsocket.so.1.1
#     (BSD socket library) is called bsd_socket(), and has a macro wrapper
#     to hide this.
# This information kindly provided by Martin J. Bligh of Sequent.
# As he puts it:
# "Sequent has unusual capabilities, taking it above and beyond
#  the complexity of any other vendor" :-)
#
# Jarkko Hietaniemi November 1998

case "$osvers" in
4.4*) # configure doesn't find sockets, as they're in libsocket, not libc
        d_socket='define'
        d_oldsock='undef'
        d_sockpair='define'
        ;;
4.2*) # on ptx/TCP 4.2, we can use BSD sockets, but they're not the default.
        cppflags="$cppflags -Wc,+bsd-socket"
        ccflags="$ccflags -Wc,+bsd-socket"
        ldflags="$ldflags -Wc,+bsd-socket"
        d_socket='define'
        d_oldsock='undef'
        d_sockpair='define'
    ;;
esac
