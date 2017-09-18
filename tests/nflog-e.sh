#!/bin/sh

exitcode=0

# NFLOG support depends on both DLT_NFLOG and working <pcap/nflog.h>

if grep '^#define HAVE_PCAP_NFLOG_H 1$' ../config.h >/dev/null
then
	passed=`cat .passed`
	failed=`cat .failed`
	if ./TESTonce nflog-e nflog.pcap nflog-e.out '-e'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
else
	printf '    %-35s: TEST SKIPPED (compiled w/o NFLOG)\n' 'nflog-e'
fi

exit $exitcode
