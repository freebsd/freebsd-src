#!/bin/sh

exitcode=0

# Only attempt OpenSSL-specific tests when compiled with the library.

if grep '^#define HAVE_LIBCRYPTO 1$' ../config.h >/dev/null
then
	passed=`cat .passed`
	failed=`cat .failed`
	if ./TESTonce esp1 02-sunrise-sunset-esp.pcap esp1.out '-E "0x12345678@192.1.2.45 3des-cbc-hmac96:0x4043434545464649494a4a4c4c4f4f515152525454575758"'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
	if ./TESTonce esp2 08-sunrise-sunset-esp2.pcap esp2.out '-E "0x12345678@192.1.2.45 3des-cbc-hmac96:0x43434545464649494a4a4c4c4f4f51515252545457575840,0xabcdabcd@192.0.1.1 3des-cbc-hmac96:0x434545464649494a4a4c4c4f4f5151525254545757584043"'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
	if ./TESTonce esp3 02-sunrise-sunset-esp.pcap esp1.out '-E "3des-cbc-hmac96:0x4043434545464649494a4a4c4c4f4f515152525454575758"'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
	# Reading the secret(s) from a file does not work with Capsicum.
	if grep '^#define HAVE_CAPSICUM 1$' ../config.h >/dev/null
	then
		FORMAT='    %-35s: TEST SKIPPED (compiled w/Capsicum)\n'
		printf "$FORMAT" esp4
		printf "$FORMAT" esp5
		printf "$FORMAT" espudp1
		printf "$FORMAT" ikev2pI2
		printf "$FORMAT" isakmp4
	else
		if ./TESTonce esp4 08-sunrise-sunset-esp2.pcap esp2.out '-E "file esp-secrets.txt"'
		then
			passed=`expr $passed + 1`
			echo $passed >.passed
		else
			failed=`expr $failed + 1`
			echo $failed >.failed
			exitcode=1
		fi
		if ./TESTonce esp5 08-sunrise-sunset-aes.pcap esp5.out '-E "file esp-secrets.txt"'
		then
			passed=`expr $passed + 1`
			echo $passed >.passed
		else
			failed=`expr $failed + 1`
			echo $failed >.failed
			exitcode=1
		fi
		if ./TESTonce espudp1 espudp1.pcap espudp1.out '-nnnn -E "file esp-secrets.txt"'
		then
			passed=`expr $passed + 1`
			echo $passed >.passed
		else
			failed=`expr $failed + 1`
			echo $failed >.failed
			exitcode=1
		fi
		if ./TESTonce ikev2pI2 ikev2pI2.pcap ikev2pI2.out '-E "file ikev2pI2-secrets.txt" -v -v -v -v'
		then
			passed=`expr $passed + 1`
			echo $passed >.passed
		else
			failed=`expr $failed + 1`
			echo $failed >.failed
			exitcode=1
		fi
		if ./TESTonce isakmp4 isakmp4500.pcap isakmp4.out '-E "file esp-secrets.txt"'
		then
			passed=`expr $passed + 1`
			echo $passed >.passed
		else
			failed=`expr $failed + 1`
			echo $failed >.failed
			exitcode=1
		fi
	fi
	if ./TESTonce bgp-as-path-oobr-ssl bgp-as-path-oobr.pcap bgp-as-path-oobr-ssl.out '-vvv -e'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
	if ./TESTonce bgp-aigp-oobr-ssl bgp-aigp-oobr.pcap bgp-aigp-oobr-ssl.out '-vvv -e'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
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
	if ./TESTonce bgp-as-path-oobr-nossl bgp-as-path-oobr.pcap bgp-as-path-oobr-nossl.out '-vvv -e'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
	if ./TESTonce bgp-aigp-oobr-nossl bgp-aigp-oobr.pcap bgp-aigp-oobr-nossl.out '-vvv -e'
	then
		passed=`expr $passed + 1`
		echo $passed >.passed
	else
		failed=`expr $failed + 1`
		echo $failed >.failed
		exitcode=1
	fi
fi

exit $exitcode
