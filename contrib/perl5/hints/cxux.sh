#! /local/gnu/bin/bash
# Hints for the CX/UX 7.1 operating system running on Concurrent (formerly
# Harris) NightHawk machines.  written by Tom.Horsley@mail.ccur.com
#
# This config is setup for dynamic linking and the Concurrent C compiler.

# Check some things and print warnings if this isn't going to work...
#
case ${SDE_TARGET:-ELF} in
   [Cc][Oo][Ff][Ff]|[Oo][Cc][Ss]) echo ''
      echo ''								>&2
      echo WARNING: Do not build perl 5 with the SDE_TARGET set to	>&2
      echo generate coff object - perl 5 must be built in the ELF	>&2
      echo environment.							>&2
      echo ''								>&2
      echo '';;
   [Ee][Ll][Ff]) : ;;
   *) echo ''								>&2
      echo 'Unknown SDE_TARGET value: '$SDE_TARGET			>&2
      echo ''								>&2 ;;
esac

case `uname -r` in
   [789]*) : ;;
   *) echo ''
      echo ''								>&2
      echo WARNING: Perl 5 requires shared library support, it cannot	>&2
      echo be built on releases of CX/UX prior to 7.0 with this hints	>&2
      echo file. You\'ll have to do a separate port for the statically	>&2
      echo linked COFF environment.					>&2
      echo ''								>&2
      echo '';;
esac

# Internally at Concurrent, we use a source management tool which winds up
# giving us read-only copies of source trees that are mostly symbolic links.
# That upsets the perl build process when it tries to edit opcode.h and
# embed.h or touch perly.c or perly.h, so turn those files into "real" files
# when Configure runs. (If you already have "real" source files, this won't
# do anything).
#
if [ -x /usr/local/mkreal ]
then
   for i in '.' '..'
   do
      for j in embed.h opcode.h perly.h perly.c
      do
         if [ -h $i/$j ]
         then
            ( cd $i ; /usr/local/mkreal $j ; chmod 666 $j )
         fi
      done
   done
fi

# We DO NOT want -lmalloc
#
libswanted=`echo ' '$libswanted' ' | sed -e 's/ malloc / /'`

# Stick the low-level elf library path in first.
#
glibpth="/usr/sde/elf/usr/lib $glibpth"

# Need to use Concurrent cc for most of these options to be meaningful (if
# you want to get this to work with gcc, you're on your own :-). Passing
# -Bexport to the linker when linking perl is important because it leaves
# the interpreter internal symbols visible to the shared libs that will be
# loaded on demand (and will try to reference those symbols). The -u option
# to drag 'sigaction' into the perl main program is to make sure it gets
# defined for the posix shared library (for some reason sigaction is static,
# rather than being defined in libc.so.1). The 88110compat option makes sure
# the code will run on both 88100 and 88110 machines. The define is added to
# trigger a work around for a compiler bug which shows up in pp.c.
#
cc='/bin/cc -Xa -Qtarget=M88110compat -DCXUX_BROKEN_CONSTANT_CONVERT'
cccdlflags='-Zelf -Zpic'
ccdlflags='-Zelf -Zlink=dynamic -Wl,-Bexport -u sigaction'
lddlflags='-Zlink=so'

# Configure imagines that it sees a pw_quota field, but it is really in a
# different structure than the one it thinks it is looking at.
d_pwquota='undef'

# Configure sometimes finds what it believes to be ndbm header files on the
# system and imagines that we have the NDBM library, but we really don't.
# There is something there that once resembled ndbm, but it is purely
# for internal use in some tool and has been hacked beyond recognition
# (or even function :-)
#
i_ndbm='undef'

# Don't use the perl malloc
#
d_mymalloc='undef'
usemymalloc='n'

cat <<'EOM' >&4

WARNING: If you are using ksh to run the Configure script, you may find it
failing in mysterious ways (such as failing to find library routines which
are known to exist). Configure seems to push ksh beyond its limits
sometimes. Try using env to strip unnecessary things out of the environment
and run Configure with /sbin/sh. That sometimes seems to produce more
accurate results.

EOM
