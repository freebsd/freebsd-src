#----------------------------------------------------------------
# QNX hints
#
# As of perl5.004_04, all tests pass under:
#  QNX 4.23A
#  Watcom 10.6 with Beta/970211.wcc.update.tar.F
#  socket3r.lib Nov21 1996.
#
# As with many unix ports, this one depends on a few "standard"
# unix utilities which are not necessarily standard for QNX.
#
# /bin/sh  This is used heavily by Configure and then by
#          perl itself. QNX's version is fine, but Configure
#          will choke on the 16-bit version, so if you are
#          running QNX 4.22, link /bin/sh to /bin32/ksh
# ar       This is the standard unix library builder.
#          We use wlib. With Watcom 10.6, when wlib is
#          linked as "ar", it behaves like ar and all is
#          fine. Under 9.5, a cover is required. One is
#          included in ../qnx
# nm       This is used (optionally) by configure to list
#          the contents of libraries. I will generate
#          a cover function on the fly in the UU directory.
# cpp      Configure and perl need a way to invoke a C
#          preprocessor. I have created a simple cover
#          for cc which does the right thing. Without this,
#          Configure will create it's own wrapper which works,
#          but it doesn't handle some of the command line arguments
#          that perl will throw at it.
# make     You really need GNU make to compile this. GNU make
#          ships by default with QNX 4.23, but you can get it
#          from quics for earlier versions.
#----------------------------------------------------------------
# Outstanding Issues:
#   lib/posix.t test fails on test 17 because acos(1) != 0.
#      Resolved in 970211 Beta
#   lib/io_udp.t test hangs because of a bug in getsockname().
#      Fixed in latest BETA socket3r.lib
#   There is currently no support for dynamically linked
#      libraries.
#----------------------------------------------------------------
# These hints were submitted by:
#   Norton T. Allen
#   Harvard University Atmospheric Research Project
#   allen@huarp.harvard.edu
#
# If you have suggestions or changes, please let me know.
#----------------------------------------------------------------

echo ""
echo "Some tests may fail. Please read the hints/qnx.sh file."
echo ""

#----------------------------------------------------------------
# At present, all QNX systems are equivalent architectures,
# so it is reasonable to call archname=x86-qnx rather than
# making an unnecessary distinction between AT-qnx and PCI-qnx,
# for example.
#----------------------------------------------------------------
archname='x86-qnx'

#----------------------------------------------------------------
# QNX doesn't come with a csh and the ports of tcsh I've used
# don't work reliably:
#----------------------------------------------------------------
csh=''
d_csh='undef'
full_csh=''

#----------------------------------------------------------------
# setuid scripts are secure under QNX.
#  (Basically, the same race conditions apply, but assuming
#  the scripts are located in a secure directory, the methods
#  for exploiting the race condition are defeated because
#  the loader expands the script name fully before executing
#  the interpreter.)
#----------------------------------------------------------------
d_suidsafe='define'

#----------------------------------------------------------------
# difftime is implemented as a preprocessor macro, so it doesn't show
# up in the libraries:
#----------------------------------------------------------------
d_difftime='define'

#----------------------------------------------------------------
# strtod is in the math library, but we can't tell Configure
# about the math library or it will confuse the linker
#----------------------------------------------------------------
d_strtod='define'

lib_ext='3r.lib'
libc='/usr/lib/clib3r.lib'

#----------------------------------------------------------------
# ccflags:
# I like to turn the warnings up high, but a few common
# constructs make a lot of noise, so I turn those warnings off.
# A few still remain...
#
# unix.h is required as a general rule for unixy applications.
#----------------------------------------------------------------
ccflags='-mf -w4 -Wc,-wcd=202 -Wc,-wcd=203 -Wc,-wcd=302 -Wc,-fi=unix.h'

#----------------------------------------------------------------
# ldflags:
# If you want debugging information, you must specify -g on the
# link as well as the compile. If optimize != -g, you should
# remove this.
#----------------------------------------------------------------
ldflags="-g -N1M"

so='none'
selecttype='fd_set *'

#----------------------------------------------------------------
# Add -lunix to list of libs. This is needed mainly so the nm
# search will find funcs in the unix lib. Including unix.h should
# automatically include the library without -l.
#----------------------------------------------------------------
libswanted="$libswanted unix"

if [ -z "`which ar 2>/dev/null`" ]; then
  cat <<-'EOF' >&4
	I don't see an 'ar', so I'm guessing you are running
	Watcom 9.5 or earlier. You may want to install the ar
	cover found in the qnx subdirectory of this distribution.
	It might reasonably be placed in /usr/local/bin.

	EOF
fi
#----------------------------------------------------------------
# Here is a nm script which fixes up wlib's output to look
# something like nm's, at least enough so that Configure can
# use it.
#----------------------------------------------------------------
if [ -z "`which nm 2>/dev/null`" ]; then
  cat <<-EOF
	Creating a quick-and-dirty nm cover for	Configure to use:

	EOF
  cat >./UU/nm <<-'EOF'
	#! /bin/sh
	#__USAGE
	#%C	<lib> [<lib> ...]
	#	Designed to mimic Unix's nm utility to list
	#	defined symbols in a library
	unset WLIB
	for i in $*; do wlib $i; done |
	  awk '
	    /^  / {
	      for (i = 1; i <= NF; i++) {
	        sub("_$", "", $i)
	        print "000000  T " $i
	      }
	    }'
	EOF
  chmod +x ./UU/nm
fi

cppstdin=`which cpp 2>/dev/null`
if [ -n "$cppstdin" ]; then
  cat <<-EOF >&4
	I found a cpp at $cppstdin and will assume it is a good
	thing to use. If this proves to be false, there is a
	thin cover for cpp in the qnx subdirectory of this
	distribution which you could move into your path.
	EOF
  cpprun="$cppstdin"
else
  cat <<-EOF >&4
	
	There is a cpp cover in the qnx subdirectory of this
	distribution which works a little better than the
	Configure default. You may wish to copy it to
	/usr/local/bin or some other suitable location.
	EOF
fi	
