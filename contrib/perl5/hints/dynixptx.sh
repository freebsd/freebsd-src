# Sequent Dynix/Ptx v. 4 hints
# Created 1996/03/15 by Brad Howerter, bhower@wgc.woodward.com
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

# Reported by bruce@aps.org ("Bruce P. Schuck") as needed for
# DYNIX/ptx 4.0 V4.2.1 to get socket i/o to work
# Not defined by default in case they break other versions.
# These probably need to be worked into a piece of code that
# checks for the need for this setting.
# cppflags='-Wc,+abi-socket -I/usr/local/include'
# ccflags='-Wc,+abi-socket -I/usr/local/include'
