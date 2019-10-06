#!/bin/sh

srcdir=${1-..}
: echo smb.sh using ${srcdir} from `pwd`

testdir=${srcdir}/tests

exitcode=0
passedfile=tests/.passed
failedfile=tests/.failed
passed=`cat ${passedfile}`
failed=`cat ${failedfile}`

# Only attempt OpenSSL-specific tests when compiled with the library.

if grep '^#define ENABLE_SMB 1$' config.h >/dev/null
then
    cat ${srcdir}/tests/SMBLIST | while read name input output options
    do
        case $name in
            \#*) continue;;
            '') continue;;
        esac
        rm -f core
        [ "$only" != "" -a "$name" != "$only" ] && continue
        SRCDIR=${srcdir}
        export SRCDIR
        # I hate shells with their stupid, useless subshells.
        passed=`cat ${passedfile}`
        failed=`cat ${failedfile}`
        (cd tests  # run TESTonce in tests directory
         if ${srcdir}/tests/TESTonce $name ${srcdir}/tests/$input ${srcdir}/tests/$output "$options"
         then
             passed=`expr $passed + 1`
             echo $passed >${passedfile}
         else
             failed=`expr $failed + 1`
             echo $failed >${failedfile}
         fi
         if [ -d COREFILES ]; then
             if [ -f core ]; then mv core COREFILES/$name.core; fi
         fi)
    done
    # I hate shells with their stupid, useless subshells.
    passed=`cat ${passedfile}`
    failed=`cat ${failedfile}`
fi

exit $exitcode
