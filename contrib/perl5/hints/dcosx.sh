# hints/dcosx.sh
# Last modified:  Thu Jan 16 11:38:12 EST 1996
# Stephen Zander  <stephen.zander@interlock.mckesson.com>
# hints for DC/OSx (Pyramid) & SINIX (Seimens: dc/osx rebadged)
# Based on the hints/solaris_2.sh file

# See man vfork.
usevfork=false

d_suidsafe=define

# Avoid all libraries in /usr/ucblib.
set `echo $glibpth | sed -e 's@/usr/ucblib@@'`
glibpth="$*"

# Remove bad libraries.
# -lucb contains incompatible routines.
set `echo " $libswanted " | sed -e 's@ ucb @ @'`
libswanted="$*"

# Here's another draft of the perl5/solaris/gcc sanity-checker. 

case $PATH in
*/usr/ucb*:/usr/bin:*|*/usr/ucb*:/usr/bin) cat <<END >&2

NOTE:  /usr/ucb/cc does not function properly.
Remove /usr/ucb from your PATH.

END
;;
esac


# Check that /dev/fd is mounted.  If it is not mounted, let the
# user know that suid scripts may not work.
/usr/bin/df /dev/fd 2>&1 > /dev/null
case $? in
0) ;;
*)
      cat <<END >&4

NOTE: Your system does not have /dev/fd mounted.  If you want to
be able to use set-uid scripts you must ask your system administrator
to mount /dev/fd.

END
      ;;
esac


# See if libucb can be found in /usr/lib.  If it is, warn the user
# that this may cause problems while building Perl extensions.
/usr/bin/ls /usr/lib/libucb* >/dev/null 2>&1
case $? in
0)
      cat <<END >&4

NOTE: libucb has been found in /usr/lib.  libucb should reside in
/usr/ucblib.  You may have trouble while building Perl extensions.

END
;;
esac


# See if make(1) is GNU make(1).
# If it is, make sure the setgid bit is not set.
make -v > make.vers 2>&1
if grep GNU make.vers > /dev/null 2>&1; then
    tmp=`/usr/bin/ksh -c "whence make"`
    case "`/usr/bin/ls -l $tmp`" in
    ??????s*)
          cat <<END >&2
      
NOTE: Your PATH points to GNU make, and your GNU make has the set-group-id
bit set.  You must either rearrange your PATH to put /usr/ccs/bin before the
GNU utilities or you must ask your system administrator to disable the
set-group-id bit on GNU make.

END
          ;;
    esac
fi
rm -f make.vers

# If the C compiler is gcc:
#   - check the fixed-includes
#   - check as(1) and ld(1), they should not be GNU
# If the C compiler is not gcc:
#   - check as(1) and ld(1), they should not be GNU
#   - increase the optimizing level to prevent object size warnings
#
# Watch out in case they have not set $cc.
case "`${cc:-cc} -v 2>&1`" in
*gcc*)
      #
      # Using gcc.
      #
      #echo Using gcc

      # Get gcc to share its secrets.
      echo 'main() { return 0; }' > try.c
      verbose=`${cc:-cc} -v -o try try.c 2>&1`
      rm -f try try.c
      tmp=`echo "$verbose" | grep '^Reading' |
              awk '{print $NF}'  | sed 's/specs$/include/'`

      # Determine if the fixed-includes look like they'll work.
      # Doesn't work anymore for gcc-2.7.2.

      # See if as(1) is GNU as(1).  GNU as(1) won't work for this job.
      case $verbose in
      */usr/ccs/bin/as*) ;;
      *)
          cat <<END >&2

NOTE: You are using GNU as(1).  GNU as(1) will not build Perl.
You must arrange to use /usr/ccs/bin/as, perhaps by setting
GCC_EXEC_PREFIX or by including -B/usr/ccs/bin in your cc command.

END
      ;;
      esac

      # See if ld(1) is GNU ld(1).  GNU ld(1) won't work for this job.
      case $verbose in
      */usr/ccs/bin/ld*) ;;
      *)
          cat <<END >&2

NOTE: You are using GNU ld(1).  GNU ld(1) will not build Perl.
You must arrange to use /usr/ccs/bin/ld, perhaps by setting
GCC_EXEC_PREFIX or by including -B/usr/ccs/bin in your cc command.

END
      ;;
      esac

      ;; #using gcc
*)
      optimize='-O -K Olimit:3064'
      #
      # Not using gcc.
      #
      #echo Not using gcc

      # See if as(1) is GNU as(1).  GNU as(1) won't work for this job.
      case `as --version < /dev/null 2>&1` in
      *GNU*)
              cat <<END >&2

NOTE: You are using GNU as(1).  GNU as(1) will not build Perl.
You must arrange to use /usr/ccs/bin, perhaps by adding it to the
beginning of your PATH.

END
              ;;
      esac

      # See if ld(1) is GNU ld(1).  GNU ld(1) won't work for this job.
      case `ld --version < /dev/null 2>&1` in
      *GNU*)
              cat <<END >&2

NOTE: You are using GNU ld(1).  GNU ld(1) will not build Perl.
You must arrange to use /usr/ccs/bin, perhaps by adding it to the
beginning of your PATH

END
              ;;
      esac

      ;; #not using gcc
esac

# as --version or ld --version might dump core.
rm -f core

# DC/OSx hides certain functions in a libc that looks dynamic but isn't
# because of this we reinclude -lc when building dynamic extenstions
libc='/usr/ccs/lib/libc.so'
lddlflags='-G -lc'

# DC/OSx gets overenthusiastic with symbol removal when building dynamically
ccdlflags='-Blargedynsym'

# System malloc is safer when using third part libs
usemymalloc='n'
