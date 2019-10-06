#!/bin/sh

exitcode=0
srcdir=${1-..}
: echo $0 using ${srcdir}

testdir=${srcdir}/tests
passedfile=tests/.passed
failedfile=tests/.failed
passed=`cat ${passedfile}`
failed=`cat ${failedfile}`

# NFLOG support depends on both DLT_NFLOG and working <pcap/nflog.h>

if grep '^#define HAVE_PCAP_NFLOG_H 1$' config.h >/dev/null
then
	if ${testdir}/TESTonce nflog-e ${testdir}/nflog.pcap ${testdir}/nflog-e.out '-e'
	then
		passed=`expr $passed + 1`
		echo $passed >${passedfile}
	else
		failed=`expr $failed + 1`
		echo $failed >${failedfile}
		exitcode=1
	fi
else
	printf '    %-35s: TEST SKIPPED (compiled w/o NFLOG)\n' 'nflog-e'
fi

exit $exitcode
