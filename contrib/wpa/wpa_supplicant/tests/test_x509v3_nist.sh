#!/bin/sh

# X.509 Path Validation Test Suite, Version 1.07
# http://csrc.nist.gov/pki/testing/x509paths_old.html
# http://csrc.nist.gov/pki/testing/x509tests.tgz

if [ -z "$1" ]; then
    echo "usage: $0 <path to X509tests directory>"
    exit 1
fi

TESTS=$1

if [ ! -d $TESTS ]; then
    echo "Not a directory: $TESTS"
    exit 1
fi

X509TEST="./test_x509v3 -v"
TMPOUT=test_x509v3_nist.out

# TODO: add support for validating CRLs

END="End Certificate "
ROOT="Trust Anchor "
ICA="Intermediate Certificate "

SUCCESS=""
FAILURE=""

function run_test
{
    NUM=$1
    RES=$2
    shift 2
    $X509TEST "$@" > $TMPOUT.$NUM
    VALRES=$?
    OK=0
    if [ $RES -eq 0 ]; then
	# expecting success
	if [ $VALRES -eq 0 ]; then
	    OK=1
	else
	    echo "test$NUM failed - expected validation success"
	    OK=0
	fi
    else
	# expecting failure
	if [ $VALRES -eq 0 ]; then
	    echo "test$NUM failed - expected validation failure"
	    OK=0
	else
	    REASON=`grep "Certificate chain validation failed: " $TMPOUT.$NUM`
	    if [ $? -eq 0 ]; then
		REASONNUM=`echo "$REASON" | colrm 1 37`
		if [ $REASONNUM -eq $RES ]; then
		    OK=1
		else
		    echo "test$NUM failed - expected validation result $RES; result was $REASONNUM"
		    OK=0
		fi
	    else
		echo "test$NUM failed - expected validation failure; other type of error detected"
		OK=0
	    fi
	fi
    fi
    if [ $OK -eq 1 ]; then
	rm $TMPOUT.$NUM
	SUCCESS="$SUCCESS $NUM"
    else
	FAILURE="$FAILURE $NUM"
    fi
}

P=$TESTS/test

run_test 1 0 "${P}1/${END}CP.01.01.crt" "${P}1/${ROOT}CP.01.01.crt"
run_test 2 1 "${P}2/${END}CP.01.02.crt" "${P}2/${ICA}CP.01.02.crt" "${P}2/${ROOT}CP.01.01.crt"
run_test 3 1 "${P}3/${END}CP.01.03.crt" "${P}3/${ICA}CP.01.03.crt" "${P}3/${ROOT}CP.01.01.crt"
run_test 4 0 "${P}4/${END}CP.02.01.crt" "${P}4/${ICA}2 CP.02.01.crt" "${P}4/${ICA}1 CP.02.01.crt" "${P}4/${ROOT}CP.01.01.crt"
run_test 5 4 "${P}5/${END}CP.02.02.crt" "${P}5/${ICA}CP.02.02.crt" "${P}5/${ROOT}CP.01.01.crt"
run_test 6 4 "${P}6/${END}CP.02.03.crt" "${P}6/${ICA}CP.02.03.crt" "${P}6/${ROOT}CP.01.01.crt"
run_test 7 0 "${P}7/${END}CP.02.04.crt" "${P}7/${ICA}CP.02.04.crt" "${P}7/${ROOT}CP.01.01.crt"
run_test 8 4 "${P}8/${END}CP.02.05.crt" "${P}8/${ICA}CP.02.05.crt" "${P}8/${ROOT}CP.01.01.crt"
run_test 9 4 "${P}9/${END}CP.03.01.crt" "${P}9/${ICA}CP.03.01.crt" "${P}9/${ROOT}CP.01.01.crt"
run_test 10 4 "${P}10/${END}CP.03.02.crt" "${P}10/${ICA}CP.03.02.crt" "${P}10/${ROOT}CP.01.01.crt"
run_test 11 4 "${P}11/${END}CP.03.03.crt" "${P}11/${ICA}CP.03.03.crt" "${P}11/${ROOT}CP.01.01.crt"
run_test 12 0 "${P}12/${END}CP.03.04.crt" "${P}12/${ICA}CP.03.04.crt" "${P}12/${ROOT}CP.01.01.crt"
run_test 13 5 "${P}13/${END}CP.04.01.crt" "${P}13/${ICA}CP.04.01.crt" "${P}13/${ROOT}CP.01.01.crt"
run_test 14 5 "${P}14/${END}CP.04.02.crt" "${P}14/${ICA}CP.04.02.crt" "${P}14/${ROOT}CP.01.01.crt"
run_test 15 0 "${P}15/${END}CP.04.03.crt" "${P}15/${ICA}CP.04.03.crt" "${P}15/${ROOT}CP.01.01.crt"
run_test 16 0 "${P}16/${END}CP.04.04.crt" "${P}16/${ICA}CP.04.04.crt" "${P}16/${ROOT}CP.01.01.crt"
run_test 17 0 "${P}17/${END}CP.04.05.crt" "${P}17/${ICA}CP.04.05.crt" "${P}17/${ROOT}CP.01.01.crt"
run_test 18 0 "${P}18/${END}CP.04.06.crt" "${P}18/${ICA}CP.04.06.crt" "${P}18/${ROOT}CP.01.01.crt"
run_test 19 1 "${P}19/${END}CP.05.01.crt" "${P}19/${ICA}CP.05.01.crt" "${P}19/${ROOT}CP.01.01.crt"
run_test 20 3 "${P}20/${END}CP.06.01.crt" "${P}20/${ICA}CP.06.01.crt" "${P}20/${ROOT}CP.01.01.crt"
run_test 21 3 "${P}21/${END}CP.06.02.crt" "${P}21/${ICA}CP.06.02.crt" "${P}21/${ROOT}CP.01.01.crt"
run_test 22 1 "${P}22/${END}IC.01.01.crt" "${P}22/${ICA}IC.01.01.crt" "${P}22/${ROOT}CP.01.01.crt"
run_test 23 1 "${P}23/${END}IC.02.01.crt" "${P}23/${ICA}IC.02.01.crt" "${P}23/${ROOT}CP.01.01.crt"
run_test 24 0 "${P}24/${END}IC.02.02.crt" "${P}24/${ICA}IC.02.02.crt" "${P}24/${ROOT}CP.01.01.crt"
run_test 25 1 "${P}25/${END}IC.02.03.crt" "${P}25/${ICA}IC.02.03.crt" "${P}25/${ROOT}CP.01.01.crt"
run_test 26 0 "${P}26/${END}IC.02.04.crt" "${P}26/${ICA}IC.02.04.crt" "${P}26/${ROOT}CP.01.01.crt"
run_test 27 0 "${P}27/${END}IC.04.01.crt" "${P}27/${ICA}IC.04.01.crt" "${P}27/${ROOT}CP.01.01.crt"
run_test 28 1 "${P}28/${END}IC.05.01.crt" "${P}28/${ICA}IC.05.01.crt" "${P}28/${ROOT}CP.01.01.crt"
run_test 29 1 "${P}29/${END}IC.05.02.crt" "${P}29/${ICA}IC.05.02.crt" "${P}29/${ROOT}CP.01.01.crt"
run_test 30 0 "${P}30/${END}IC.05.03.crt" "${P}30/${ICA}IC.05.03.crt" "${P}30/${ROOT}CP.01.01.crt"
run_test 31 1 "${P}31/${END}IC.06.01.crt" "${P}31/${ICA}IC.06.01.crt" "${P}31/${ROOT}CP.01.01.crt"
run_test 32 1 "${P}32/${END}IC.06.02.crt" "${P}32/${ICA}IC.06.02.crt" "${P}32/${ROOT}CP.01.01.crt"
run_test 33 0 "${P}33/${END}IC.06.03.crt" "${P}33/${ICA}IC.06.03.crt" "${P}33/${ROOT}CP.01.01.crt"
run_test 34 0 "${P}34/${END}PP.01.01.crt" "${P}34/${ICA}PP.01.01.crt" "${P}34/${ROOT}CP.01.01.crt"
run_test 35 0 "${P}35/${END}PP.01.02.crt" "${P}35/${ICA}PP.01.02.crt" "${P}35/${ROOT}CP.01.01.crt"
run_test 36 0 "${P}36/${END}PP.01.03.crt" "${P}36/${ICA}2 PP.01.03.crt" "${P}36/${ICA}1 PP.01.03.crt" "${P}36/${ROOT}CP.01.01.crt"
run_test 37 0 "${P}37/${END}PP.01.04.crt" "${P}37/${ICA}2 PP.01.04.crt" "${P}37/${ICA}1 PP.01.04.crt" "${P}37/${ROOT}CP.01.01.crt"
run_test 38 0 "${P}38/${END}PP.01.05.crt" "${P}38/${ICA}2 PP.01.05.crt" "${P}38/${ICA}1 PP.01.05.crt" "${P}38/${ROOT}CP.01.01.crt"
run_test 39 0 "${P}39/${END}PP.01.06.crt" "${P}39/${ICA}3 PP.01.06.crt" "${P}39/${ICA}2 PP.01.06.crt" "${P}39/${ICA}1 PP.01.06.crt" "${P}39/${ROOT}CP.01.01.crt"
run_test 40 0 "${P}40/${END}PP.01.07.crt" "${P}40/${ICA}3 PP.01.07.crt" "${P}40/${ICA}2 PP.01.07.crt" "${P}40/${ICA}1 PP.01.07.crt" "${P}40/${ROOT}CP.01.01.crt"
run_test 41 0 "${P}41/${END}PP.01.08.crt" "${P}41/${ICA}3 PP.01.08.crt" "${P}41/${ICA}2 PP.01.08.crt" "${P}41/${ICA}1 PP.01.08.crt" "${P}41/${ROOT}CP.01.01.crt"
run_test 42 0 "${P}42/${END}PP.01.09.crt" "${P}42/${ICA}4 PP.01.09.crt" "${P}42/${ICA}3 PP.01.09.crt" "${P}42/${ICA}2 PP.01.09.crt" "${P}42/${ICA}1 PP.01.09.crt" "${P}42/${ROOT}CP.01.01.crt"
run_test 43 0 "${P}43/${END}PP.06.01.crt" "${P}43/${ICA}4 PP.06.01.crt" "${P}43/${ICA}3 PP.06.01.crt" "${P}43/${ICA}2 PP.06.01.crt" "${P}43/${ICA}1 PP.06.01.crt" "${P}43/${ROOT}CP.01.01.crt"
run_test 44 0 "${P}44/${END}PP.06.02.crt" "${P}44/${ICA}4 PP.06.02.crt" "${P}44/${ICA}3 PP.06.02.crt" "${P}44/${ICA}2 PP.06.02.crt" "${P}44/${ICA}1 PP.06.02.crt" "${P}44/${ROOT}CP.01.01.crt"
run_test 45 0 "${P}45/${END}PP.06.03.crt" "${P}45/${ICA}4 PP.06.03.crt" "${P}45/${ICA}3 PP.06.03.crt" "${P}45/${ICA}2 PP.06.03.crt" "${P}45/${ICA}1 PP.06.03.crt" "${P}45/${ROOT}CP.01.01.crt"
run_test 46 0 "${P}46/${END}PP.06.04.crt" "${P}46/${ICA}4 PP.06.04.crt" "${P}46/${ICA}3 PP.06.04.crt" "${P}46/${ICA}2 PP.06.04.crt" "${P}46/${ICA}1 PP.06.04.crt" "${P}46/${ROOT}CP.01.01.crt"
run_test 47 0 "${P}47/${END}PP.06.05.crt" "${P}47/${ICA}4 PP.06.05.crt" "${P}47/${ICA}3 PP.06.05.crt" "${P}47/${ICA}2 PP.06.05.crt" "${P}47/${ICA}1 PP.06.05.crt" "${P}47/${ROOT}CP.01.01.crt"
run_test 48 0 "${P}48/${END}PP.08.01.crt" "${P}48/${ICA}PP.08.01.crt" "${P}48/${ROOT}CP.01.01.crt"
run_test 49 0 "${P}49/${END}PP.08.02.crt" "${P}49/${ICA}PP.08.02.crt" "${P}49/${ROOT}CP.01.01.crt"
run_test 50 0 "${P}50/${END}PP.08.03.crt" "${P}50/${ICA}PP.08.03.crt" "${P}50/${ROOT}CP.01.01.crt"
run_test 51 0 "${P}51/${END}PP.08.04.crt" "${P}51/${ICA}PP.08.04.crt" "${P}51/${ROOT}CP.01.01.crt"
run_test 52 0 "${P}52/${END}PP.08.05.crt" "${P}52/${ICA}PP.08.05.crt" "${P}52/${ROOT}CP.01.01.crt"
run_test 53 0 "${P}53/${END}PP.08.06.crt" "${P}53/${ICA}PP.08.06.crt" "${P}53/${ROOT}CP.01.01.crt"
run_test 54 1 "${P}54/${END}PL.01.01.crt" "${P}54/${ICA}2 PL.01.01.crt" "${P}54/${ICA}1 PL.01.01.crt" "${P}54/${ROOT}CP.01.01.crt"
run_test 55 1 "${P}55/${END}PL.01.02.crt" "${P}55/${ICA}2 PL.01.02.crt" "${P}55/${ICA}1 PL.01.02.crt" "${P}55/${ROOT}CP.01.01.crt"
run_test 56 0 "${P}56/${END}PL.01.03.crt" "${P}56/${ICA}PL.01.03.crt" "${P}56/${ROOT}CP.01.01.crt"
run_test 57 0 "${P}57/${END}PL.01.04.crt" "${P}57/${ICA}PL.01.04.crt" "${P}57/${ROOT}CP.01.01.crt"
run_test 58 1 "${P}58/${END}PL.01.05.crt" "${P}58/${ICA}3 PL.01.05.crt" "${P}58/${ICA}2 PL.01.05.crt" "${P}58/${ICA}1 PL.01.05.crt" "${P}58/${ROOT}CP.01.01.crt"
run_test 59 1 "${P}59/${END}PL.01.06.crt" "${P}59/${ICA}3 PL.01.06.crt" "${P}59/${ICA}2 PL.01.06.crt" "${P}59/${ICA}1 PL.01.06.crt" "${P}59/${ROOT}CP.01.01.crt"
run_test 60 1 "${P}60/${END}PL.01.07.crt" "${P}60/${ICA}4 PL.01.07.crt" "${P}60/${ICA}3 PL.01.07.crt" "${P}60/${ICA}2 PL.01.07.crt" "${P}60/${ICA}1 PL.01.07.crt" "${P}60/${ROOT}CP.01.01.crt"
run_test 61 1 "${P}61/${END}PL.01.08.crt" "${P}61/${ICA}4 PL.01.08.crt" "${P}61/${ICA}3 PL.01.08.crt" "${P}61/${ICA}2 PL.01.08.crt" "${P}61/${ICA}1 PL.01.08.crt" "${P}61/${ROOT}CP.01.01.crt"
run_test 62 0 "${P}62/${END}PL.01.09.crt" "${P}62/${ICA}4 PL.01.09.crt" "${P}62/${ICA}3 PL.01.09.crt" "${P}62/${ICA}2 PL.01.09.crt" "${P}62/${ICA}1 PL.01.09.crt" "${P}62/${ROOT}CP.01.01.crt"
run_test 63 0 "${P}63/${END}PL.01.10.crt" "${P}63/${ICA}4 PL.01.10.crt" "${P}63/${ICA}3 PL.01.10.crt" "${P}63/${ICA}2 PL.01.10.crt" "${P}63/${ICA}1 PL.01.10.crt" "${P}63/${ROOT}CP.01.01.crt"


echo "Successful tests:$SUCCESS"
echo "Failed tests:$FAILURE"
