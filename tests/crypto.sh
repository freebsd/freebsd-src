#!/bin/sh

srcdir=${1-..}
: echo crypto.sh using ${srcdir} from `pwd`

SRCDIR=$srcdir
export SRCDIR

testdir=${srcdir}/tests

exitcode=0
passedfile=tests/.passed
failedfile=tests/.failed
passed=`cat ${passedfile}`
failed=`cat ${failedfile}`

# Only attempt OpenSSL-specific tests when compiled with the library.

if grep '^#define HAVE_LIBCRYPTO 1$' config.h >/dev/null
then
	if ${testdir}/TESTonce esp1 ${testdir}/02-sunrise-sunset-esp.pcap ${testdir}/esp1.out '-E "0x12345678@192.1.2.45 3des-cbc-hmac96:0x4043434545464649494a4a4c4c4f4f515152525454575758"'
	then
		passed=`expr $passed + 1`
		echo $passed >${passedfile}
	else
		failed=`expr $failed + 1`
		echo $failed >${failedfile}
		exitcode=1
	fi
	if ${testdir}//TESTonce esp2 ${testdir}/08-sunrise-sunset-esp2.pcap ${testdir}/esp2.out '-E "0x12345678@192.1.2.45 3des-cbc-hmac96:0x43434545464649494a4a4c4c4f4f51515252545457575840,0xabcdabcd@192.0.1.1 3des-cbc-hmac96:0x434545464649494a4a4c4c4f4f5151525254545757584043"'
	then
		passed=`expr $passed + 1`
		echo $passed >${passedfile}
	else
		failed=`expr $failed + 1`
		echo $failed >${failedfile}
		exitcode=1
	fi
	if ${testdir}/TESTonce esp3 ${testdir}/02-sunrise-sunset-esp.pcap ${testdir}/esp1.out '-E "3des-cbc-hmac96:0x4043434545464649494a4a4c4c4f4f515152525454575758"'
	then
		passed=`expr $passed + 1`
		echo $passed >${passedfile}
	else
		failed=`expr $failed + 1`
		echo $failed >${failedfile}
		exitcode=1
	fi
	# Reading the secret(s) from a file does not work with Capsicum.
	if grep '^#define HAVE_CAPSICUM 1$' config.h >/dev/null
	then
		FORMAT='    %-35s: TEST SKIPPED (compiled w/Capsicum)\n'
		printf "$FORMAT" esp4
		printf "$FORMAT" esp5
		printf "$FORMAT" espudp1
		printf "$FORMAT" ikev2pI2
		printf "$FORMAT" isakmp4
	else
		if ${testdir}/TESTonce esp4 ${testdir}/08-sunrise-sunset-esp2.pcap ${testdir}/esp4.out '-E "file '${testdir}'/esp-secrets.txt"'
		then
			passed=`expr $passed + 1`
			echo $passed >${passedfile}
		else
			failed=`expr $failed + 1`
			echo $failed >${failedfile}
			exitcode=1
		fi
		if ${testdir}/TESTonce esp5 ${testdir}/08-sunrise-sunset-aes.pcap ${testdir}/esp5.out '-E "file '${testdir}'/esp-secrets.txt"'
		then
			passed=`expr $passed + 1`
			echo $passed >${passedfile}
		else
			failed=`expr $failed + 1`
			echo $failed >${failedfile}
			exitcode=1
		fi
		if ${testdir}/TESTonce espudp1 ${testdir}/espudp1.pcap ${testdir}/espudp1.out '-nnnn -E "file '${testdir}'/esp-secrets.txt"'
		then
			passed=`expr $passed + 1`
			echo $passed >${passedfile}
		else
			failed=`expr $failed + 1`
			echo $failed >${failedfile}
			exitcode=1
		fi
		if ${testdir}/TESTonce ikev2pI2 ${testdir}/ikev2pI2.pcap ${testdir}/ikev2pI2.out '-E "file '${testdir}'/ikev2pI2-secrets.txt" -v -v -v -v'
		then
			passed=`expr $passed + 1`
			echo $passed >${passedfile}
		else
			failed=`expr $failed + 1`
			echo $failed >${failedfile}
			exitcode=1
		fi
		if ${testdir}/TESTonce isakmp4 ${testdir}/isakmp4500.pcap ${testdir}/isakmp4.out '-E "file '${testdir}'/esp-secrets.txt"'
		then
			passed=`expr $passed + 1`
			echo $passed >${passedfile}
		else
			failed=`expr $failed + 1`
			echo $failed >${failedfile}
			exitcode=1
		fi
	fi
	if ${testdir}/TESTonce bgp-as-path-oobr-ssl ${testdir}/bgp-as-path-oobr.pcap ${testdir}/bgp-as-path-oobr-ssl.out '-vvv -e'
	then
		passed=`expr $passed + 1`
		echo $passed >${passedfile}
	else
		failed=`expr $failed + 1`
		echo $failed >${failedfile}
		exitcode=1
	fi
	if ${testdir}/TESTonce bgp-aigp-oobr-ssl ${testdir}/bgp-aigp-oobr.pcap ${testdir}/bgp-aigp-oobr-ssl.out '-vvv -e'
	then
		passed=`expr $passed + 1`
		echo $passed >${passedfile}
	else
		failed=`expr $failed + 1`
		echo $failed >${failedfile}
		exitcode=1
	fi
	FORMAT='    %-35s: TEST SKIPPED (compiled w/OpenSSL)\n'
	printf "$FORMAT" bgp-as-path-oobr-nossl
	printf "$FORMAT" bgp-aigp-oobr-nossl
else
	FORMAT='    %-35s: TEST SKIPPED (compiled w/o OpenSSL)\n'
	printf "$FORMAT" esp1
	printf "$FORMAT" esp2
	printf "$FORMAT" esp3
	printf "$FORMAT" esp4
	printf "$FORMAT" esp5
	printf "$FORMAT" espudp1
	printf "$FORMAT" ikev2pI2
	printf "$FORMAT" isakmp4
	printf "$FORMAT" bgp-as-path-oobr-ssl
	printf "$FORMAT" bgp-aigp-oobr-ssl
	if ${testdir}/TESTonce bgp-as-path-oobr-nossl ${testdir}/bgp-as-path-oobr.pcap ${testdir}/bgp-as-path-oobr-nossl.out '-vvv -e'
	then
		passed=`expr $passed + 1`
		echo $passed >${passedfile}
	else
		failed=`expr $failed + 1`
		echo $failed >${failedfile}
		exitcode=1
	fi
	if ${testdir}/TESTonce bgp-aigp-oobr-nossl ${testdir}/bgp-aigp-oobr.pcap ${testdir}/bgp-aigp-oobr-nossl.out '-vvv -e'
	then
		passed=`expr $passed + 1`
		echo $passed >${passedfile}
	else
		failed=`expr $failed + 1`
		echo $failed >${failedfile}
		exitcode=1
	fi
fi

exit $exitcode
