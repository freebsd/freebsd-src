# hints/solaris_2.sh
# Last modified:  Wed May 27 13:04:45 EDT 1998
# Andy Dougherty  <doughera@lafcol.lafayette.edu>
# Based on input from lots of folks, especially
# Dean Roehrich <roehrich@ironwood-fddi.cray.com>

# If perl fails tests that involve dynamic loading of extensions, and
# you are using gcc, be sure that you are NOT using GNU as and ld.  One
# way to do that is to invoke Configure with
# 
#     sh Configure -Dcc='gcc -B/usr/ccs/bin/'
# 
 
# See man vfork.
usevfork=false

d_suidsafe=define

# Avoid all libraries in /usr/ucblib.
set `echo $glibpth | sed -e 's@/usr/ucblib@@'`
glibpth="$*"

# Remove bad libraries.  -lucb contains incompatible routines.
# -lld doesn't do anything useful.
# -lmalloc can cause a problem with GNU CC & Solaris.  Specifically,
# libmalloc.a may allocate memory that is only 4 byte aligned, but
# GNU CC on the Sparc assumes that doubles are 8 byte aligned.
# Thanks to  Hallvard B. Furuseth <h.b.furuseth@usit.uio.no>
set `echo " $libswanted " | sed -e 's@ ld @ @' -e 's@ malloc @ @' -e 's@ ucb @ @'`
libswanted="$*"

# Look for architecture name.  We want to suggest a useful default.
case "$archname" in
'')
    if test -f /usr/bin/arch; then
        archname=`/usr/bin/arch`
    	archname="${archname}-${osname}"
    elif test -f /usr/ucb/arch; then
        archname=`/usr/ucb/arch`
    	archname="${archname}-${osname}"
    fi
    ;;
esac

######################################################
# General sanity testing.  See below for excerpts from the Solaris FAQ.

# From roehrich@ironwood-fddi.cray.com Wed Sep 27 12:51:46 1995
# Date: Thu, 7 Sep 1995 16:31:40 -0500
# From: Dean Roehrich <roehrich@ironwood-fddi.cray.com>
# To: perl5-porters@africa.nicoh.com
# Subject: Re: On perl5/solaris/gcc

# Here's another draft of the perl5/solaris/gcc sanity-checker. 

case `type ${cc:-cc}` in
*/usr/ucb/cc*) cat <<END >&4

NOTE:  Some people have reported problems with /usr/ucb/cc.  
If you have difficulties, please make sure the directory
containing your C compiler is before /usr/ucb in your PATH.

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

# Use shell built-in 'type' command instead of /usr/bin/which to
# avoid possible csh start-up problems and also to use the same shell
# we'll be using to Configure and make perl.
# The path name is the last field in the output, but the type command
# has an annoying array of possible outputs, e.g.:
#	make is hashed (/opt/gnu/bin/make)
# 	cc is /usr/ucb/cc
#	foo not found
# use a command like type make | awk '{print $NF}' | sed 's/[()]//g'

# See if make(1) is GNU make(1).
# If it is, make sure the setgid bit is not set.
make -v > make.vers 2>&1
if grep GNU make.vers > /dev/null 2>&1; then
    tmp=`type make | awk '{print $NF}' | sed 's/[()]//g'`
    case "`/usr/bin/ls -lL $tmp`" in
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

# XXX EXPERIMENTAL  A.D.  2/27/1998
# XXX This script UU/cc.cbu will get 'called-back' by Configure after it
# XXX has prompted the user for the C compiler to use.
cat > UU/cc.cbu <<'EOSH'
# If the C compiler is gcc:
#   - check the fixed-includes
#   - check as(1) and ld(1), they should not be GNU
#	(GNU as and ld 2.8.1 and later are reportedly ok, however.)
# If the C compiler is not gcc:
#   - check as(1) and ld(1), they should not be GNU
#	(GNU as and ld 2.8.1 and later are reportedly ok, however.)
#
# Watch out in case they have not set $cc.

# Get gcc to share its secrets.
echo 'main() { return 0; }' > try.c
	# Indent to avoid propagation to config.sh
	verbose=`${cc:-cc} -v -o try try.c 2>&1`

if echo "$verbose" | grep '^Reading specs from' >/dev/null 2>&1; then
	#
	# Using gcc.
	#
	#echo Using gcc

	tmp=`echo "$verbose" | grep '^Reading' |
		awk '{print $NF}'  | sed 's/specs$/include/'`

	# Determine if the fixed-includes look like they'll work.
	# Doesn't work anymore for gcc-2.7.2.

	# See if as(1) is GNU as(1).  GNU as(1) won't work for this job.
	if echo "$verbose" | grep ' /usr/ccs/bin/as ' >/dev/null 2>&1; then
	    :
	else
	    cat <<END >&2

NOTE: You are using GNU as(1).  GNU as(1) will not build Perl.
I'm arranging to use /usr/ccs/bin/as by including -B/usr/ccs/bin/
in your ${cc:-cc} command.  (Note that the trailing "/" is required.)

END
	    cc="${cc:-cc} -B/usr/ccs/bin/"
	fi

	# See if ld(1) is GNU ld(1).  GNU ld(1) won't work for this job.
	# Recompute $verbose since we may have just changed $cc.
	verbose=`${cc:-cc} -v -o try try.c 2>&1 | grep ld 2>&1`
	if echo "$verbose" | grep ' /usr/ccs/bin/ld ' >/dev/null 2>&1; then
	    :
	else
        # It's not /usr/ccs/bin/ld - but it might be egcs's ld wrapper,
        # which calls /usr/ccs/bin/ld in turn. Passing -V to it will
        # make it show its true colors.

	    myld=`echo $verbose| grep ld | awk '/\/ld/ {print $1}'`
            # This assumes that gcc's output will not change, and that
            # /full/path/to/ld will be the first word of the output.

            # all Solaris versions of ld I've seen contain the magic
            # string used in the grep below.
            if $myld -V 2>&1 | grep "ld: Software Generation Utilities" >/dev/null 2>&1; then
                cat <<END >&2

Aha. You're using egcs and /usr/ccs/bin/ld.

END

            else
	    cat <<END >&2

NOTE: You are using GNU ld(1).  GNU ld(1) will not build Perl.
I'm arranging to use /usr/ccs/bin/ld by including -B/usr/ccs/bin/
in your ${cc:-cc} command.  (Note that the trailing "/" is required.)

END
	    cc="${cc:-cc} -B/usr/ccs/bin/"
            fi
	fi

else
	#
	# Not using gcc.
	#
	#echo Not using gcc

	# See if as(1) is GNU as(1).  GNU as(1) won't work for this job.
	case `as --version < /dev/null 2>&1` in
	*GNU*)
		cat <<END >&2

NOTE: You are using GNU as(1).  GNU as(1) will not build Perl.
You must arrange to use /usr/ccs/bin/as, perhaps by adding /usr/ccs/bin
to the beginning of your PATH.

END
		;;
	esac

	# See if ld(1) is GNU ld(1).  GNU ld(1) won't work for this job.
	# ld --version doesn't properly report itself as a GNU tool,
	# as of ld version 2.6, so we need to be more strict. TWP 9/5/96
	gnu_ld=false
	case `ld --version < /dev/null 2>&1` in
	*GNU*|ld\ version\ 2*)
		gnu_ld=true ;;
	*) ;;
	esac
	if $gnu_ld ; then :
	else
		# Try to guess from path
		case `type ld | awk '{print $NF}'` in
		*gnu*|*GNU*|*FSF*)
			gnu_ld=true ;;
		esac
	fi
	if $gnu_ld ; then
		cat <<END >&2

NOTE: You are apparently using GNU ld(1).  GNU ld(1) will not build Perl.
You must arrange to use /usr/ccs/bin/ld, perhaps by adding /usr/ccs/bin
to the beginning of your PATH.

END
	fi

fi

# as --version or ld --version might dump core.
rm -f try try.c
rm -f core

# XXX
EOSH

# This script UU/usethreads.cbu will get 'called-back' by Configure 
# after it has prompted the user for whether to use threads.
cat > UU/usethreads.cbu <<'EOCBU'
case "$usethreads" in
$define|true|[yY]*)
        ccflags="-D_REENTRANT $ccflags"

        # sched_yield is in -lposix4
        set `echo X "$libswanted "| sed -e 's/ c / posix4 pthread c /'`
        shift
        libswanted="$*"

        # On Solaris 2.6 x86 there is a bug with sigsetjmp() and siglongjmp()
        # when linked with the threads library, such that whatever positive
        # value you pass to siglongjmp(), sigsetjmp() returns 1.
        # Thanks to Simon Parsons <S.Parsons@ftel.co.uk> for this report.
        # Sun BugID is 4117946, "sigsetjmp always returns 1 when called by
        # siglongjmp in a MT program". As of 19980622, there is no patch
        # available.
        cat >try.c <<'EOM'
	/* Test for sig(set|long)jmp bug. */
	#include <setjmp.h>
	 
	main()
	{
	    sigjmp_buf env;
	    int ret;
	
	    ret = sigsetjmp(env, 1);
	    if (ret) { return ret == 2; }
	    siglongjmp(env, 2);
	}
EOM
        if test "`arch`" = i86pc -a "$osvers" = 2.6 && \
           ${cc:-cc} try.c -lpthread >/dev/null 2>&1 && ./a.out; then
 	    d_sigsetjmp=$undef
	    cat << 'EOM' >&2

You will see a *** WHOA THERE!!! ***  message from Configure for
d_sigsetjmp.  Keep the recommended value.  See hints/solaris_2.sh
for more information.

EOM
        fi
	;;
esac
EOCBU

# This is just a trick to include some useful notes.
cat > /dev/null <<'End_of_Solaris_Notes'

Here are some notes kindly contributed by Dean Roehrich.

-----
Generic notes about building Perl5 on Solaris:
- Use /usr/ccs/bin/make.
- If you use GNU make, remove its setgid bit.
- Remove all instances of *ucb* from your path.
- Make sure libucb is not in /usr/lib (it should be in /usr/ucblib).
- Do not use GNU as or GNU ld, or any of GNU binutils or GNU libc.
- Do not use /usr/ucb/cc.
- Do not change Configure's default answers, except for the path names.
- Do not use -lmalloc.
- Do not build on SunOS 4 and expect it to work properly on SunOS 5.
- /dev/fd must be mounted if you want set-uid scripts to work.


Here are the gcc-related questions and answers from the Solaris 2 FAQ.  Note
the themes:
	- run fixincludes
	- run fixincludes correctly
	- don't use GNU as or GNU ld

Question 5.7 covers the __builtin_va_alist problem people are always seeing.
Question 6.1.3 covers the GNU as and GNU ld issues which are always biting
people.
Question 6.9 is for those who are still trying to compile Perl4.

The latest Solaris 2 FAQ can be found in the following locations:
	rtfm.mit.edu:/pub/usenet-by-group/comp.sys.sun.admin
	ftp.fwi.uva.nl:/pub/solaris

Perl5 comes with a script in the top-level directory called "myconfig" which
will print a summary of the configuration in your config.sh.  My summary for
Solaris 2.4 and gcc 2.6.3 follows.  I have also built with gcc 2.7.0 and the
results are identical.  This configuration was generated with Configure's -d
option (take all defaults, don't bother prompting me).  All tests pass for
Perl5.001, patch.1m.

Summary of my perl5 (patchlevel 1) configuration:
  Platform:
    osname=solaris, osver=2.4, archname=sun4-solaris
    uname='sunos poplar 5.4 generic_101945-27 sun4d sparc '
    hint=recommended
  Compiler:
    cc='gcc', optimize='-O', ld='gcc'
    cppflags=''
    ccflags =''
    ldflags =''
    stdchar='unsigned char', d_stdstdio=define, usevfork=false
    voidflags=15, castflags=0, d_casti32=define, d_castneg=define
    intsize=4, alignbytes=8, usemymalloc=y, randbits=15
  Libraries:
    so=so
    libpth=/lib /usr/lib /usr/ccs/lib /usr/local/lib
    libs=-lsocket -lnsl -ldl -lm -lc -lcrypt
    libc=/usr/lib/libc.so
  Dynamic Linking:
    dlsrc=dl_dlopen.xs, dlext=so, d_dlsymun=undef
    cccdlflags='-fpic', ccdlflags=' ', lddlflags='-G'


Dean
roehrich@cray.com
9/7/95

-----------

From: Casper.Dik@Holland.Sun.COM (Casper H.S. Dik - Network Security Engineer)
Subject: Solaris 2 Frequently Asked Questions (FAQ) 1.48
Date: 25 Jul 1995 12:20:18 GMT

5.7) Why do I get __builtin_va_alist or __builtin_va_arg_incr undefined?

    You're using gcc without properly installing the gcc fixed
    include files.  Or you ran fixincludes after installing gcc
    w/o moving the gcc supplied varargs.h and stdarg.h files
    out of the way and moving them back again later.  This often
    happens when people install gcc from a binary distribution.
    If there's a tmp directory in gcc's include directory, fixincludes
    didn't complete.  You should have run "just-fixinc" instead.

    Another possible cause is using ``gcc -I/usr/include.''

6.1) Where is the C compiler or where can I get one?

    [...]

    3) Gcc.

    Gcc is available from the GNU archives in source and binary
    form.  Look in a directory called sparc-sun-solaris2 for
    binaries.  You need gcc 2.3.3 or later.  You should not use
    GNU as or GNU ld.  Make sure you run just-fixinc if you use
    a binary distribution.  Better is to get a binary version and
    use that to bootstrap gcc from source.

    [...]

    When you install gcc, don't make the mistake of installing
    GNU binutils or GNU libc, they are not as capable as their
    counterparts you get with Solaris 2.x.

6.9) I can't get perl 4.036 to compile or run.

    Run Configure, and use the solaris_2_0 hints, *don't* use
    the solaris_2_1 hints and don't use the config.sh you may
    already have.  First you must make sure Configure and make
    don't find /usr/ucb/cc.  (It must use gcc or the native C
    compiler: /opt/SUNWspro/bin/cc)

    Some questions need a special answer.

    Are your system (especially dbm) libraries compiled with gcc? [y] y

    yes: gcc 2.3.3 or later uses the standard calling
    conventions, same as Sun's C.

    Any additional cc flags? [ -traditional -Dvolatile=__volatile__
    -I/usr/ucbinclude] -traditional -Dvolatile=__volatile__
    Remove /usr/ucbinclude.

    Any additional libraries? [-lsocket -lnsl -ldbm -lmalloc -lm
    -lucb] -lsocket -lnsl  -lm

    Don't include -ldbm, -lmalloc and -lucb.

    Perl 5 compiled out of the box.

End_of_Solaris_Notes

