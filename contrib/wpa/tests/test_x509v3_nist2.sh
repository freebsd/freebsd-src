#!/bin/bash

# Public Key Interoperability Test Suite (PKITS)
# http://csrc.nist.gov/pki/testing/x509paths.html
# http://csrc.nist.gov/groups/ST/crypto_apps_infra/documents/PKITS_data.zip

if [ -z "$1" ]; then
    echo "usage: $0 <path to root test directory>"
    exit 1
fi

TESTS=$1

if [ ! -d $TESTS ]; then
    echo "Not a directory: $TESTS"
    exit 1
fi

X509TEST="$PWD/test-x509v3 -v"
TMPOUT="$PWD/test_x509v3_nist2.out"

# TODO: add support for validating CRLs

SUCCESS=""
FAILURE=""

function run_test
{
    NUM=$1
    RES=$2
    shift 2
    $X509TEST "$@" TrustAnchorRootCertificate.crt > $TMPOUT.$NUM
    VALRES=$?
    OK=0
    if [ $RES -eq 0 ]; then
	# expecting success
	if [ $VALRES -eq 0 ]; then
	    OK=1
	else
	    echo "$NUM failed - expected validation success"
	    OK=0
	fi
    else
	# expecting failure
	if [ $VALRES -eq 0 ]; then
	    echo "$NUM failed - expected validation failure"
	    OK=0
	else
	    REASON=`grep "Certificate chain validation failed: " $TMPOUT.$NUM`
	    if [ $? -eq 0 ]; then
		REASONNUM=`echo "$REASON" | colrm 1 37`
		if [ $REASONNUM -eq $RES ]; then
		    OK=1
		else
		    echo "$NUM failed - expected validation result $RES; result was $REASONNUM"
		    OK=0
		fi
	    else
		if [ $RES -eq -1 ]; then
		    if grep -q "Failed to parse X.509 certificate" $TMPOUT.$NUM; then
			OK=1
		    else
			echo "$NUM failed - expected parsing failure; other type of error detected"
			OK=0
		    fi
		else
		    echo "$NUM failed - expected validation failure; other type of error detected"
		    OK=0
		fi
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

pushd $TESTS/certs

run_test 4.1.1 0 ValidCertificatePathTest1EE.crt GoodCACert.crt
run_test 4.1.2 1 InvalidCASignatureTest2EE.crt BadSignedCACert.crt
run_test 4.1.3 1 InvalidEESignatureTest3EE.crt GoodCACert.crt

run_test 4.2.1 4 InvalidCAnotBeforeDateTest1EE.crt BadnotBeforeDateCACert.crt
run_test 4.2.2 4 InvalidEEnotBeforeDateTest2EE.crt GoodCACert.crt
run_test 4.2.3 0 Validpre2000UTCnotBeforeDateTest3EE.crt GoodCACert.crt
run_test 4.2.4 0 ValidGeneralizedTimenotBeforeDateTest4EE.crt GoodCACert.crt
run_test 4.2.5 4 InvalidCAnotAfterDateTest5EE.crt BadnotAfterDateCACert.crt
run_test 4.2.6 4 InvalidEEnotAfterDateTest6EE.crt GoodCACert.crt
run_test 4.2.7 4 Invalidpre2000UTCEEnotAfterDateTest7EE.crt GoodCACert.crt
run_test 4.2.8 0 ValidGeneralizedTimenotAfterDateTest8EE.crt GoodCACert.crt

run_test 4.3.1 5 InvalidNameChainingTest1EE.crt GoodCACert.crt
run_test 4.3.2 5 InvalidNameChainingOrderTest2EE.crt NameOrderingCACert.crt
run_test 4.3.3 0 ValidNameChainingWhitespaceTest3EE.crt GoodCACert.crt
run_test 4.3.4 0 ValidNameChainingWhitespaceTest4EE.crt GoodCACert.crt
run_test 4.3.5 0 ValidNameChainingCapitalizationTest5EE.crt GoodCACert.crt
run_test 4.3.6 0 ValidNameUIDsTest6EE.crt UIDCACert.crt
run_test 4.3.7 0 ValidRFC3280MandatoryAttributeTypesTest7EE.crt RFC3280MandatoryAttributeTypesCACert.crt
run_test 4.3.8 0 ValidRFC3280OptionalAttributeTypesTest8EE.crt RFC3280OptionalAttributeTypesCACert.crt
run_test 4.3.9 0 ValidUTF8StringEncodedNamesTest9EE.crt UTF8StringEncodedNamesCACert.crt
run_test 4.3.10 0 ValidRolloverfromPrintableStringtoUTF8StringTest10EE.crt RolloverfromPrintableStringtoUTF8StringCACert.crt
run_test 4.3.11 0 ValidUTF8StringCaseInsensitiveMatchTest11EE.crt UTF8StringCaseInsensitiveMatchCACert.crt

run_test 4.4.1 1 InvalidMissingCRLTest1EE.crt NoCRLCACert.crt
# skip rest of 4.4.x tests since CRLs are not yet supported

run_test 4.5.1 0 ValidBasicSelfIssuedOldWithNewTest1EE.crt BasicSelfIssuedNewKeyOldWithNewCACert.crt BasicSelfIssuedNewKeyCACert.crt
run_test 4.5.2 3 InvalidBasicSelfIssuedOldWithNewTest2EE.crt BasicSelfIssuedNewKeyOldWithNewCACert.crt BasicSelfIssuedNewKeyCACert.crt
run_test 4.5.3 0 ValidBasicSelfIssuedNewWithOldTest3EE.crt BasicSelfIssuedOldKeyNewWithOldCACert.crt BasicSelfIssuedOldKeyCACert.crt
run_test 4.5.4 0 ValidBasicSelfIssuedNewWithOldTest4EE.crt BasicSelfIssuedOldKeyNewWithOldCACert.crt BasicSelfIssuedOldKeyCACert.crt
run_test 4.5.5 3 InvalidBasicSelfIssuedNewWithOldTest5EE.crt BasicSelfIssuedOldKeyNewWithOldCACert.crt BasicSelfIssuedOldKeyCACert.crt
run_test 4.5.6 0 ValidBasicSelfIssuedCRLSigningKeyTest6EE.crt BasicSelfIssuedCRLSigningKeyCRLCert.crt BasicSelfIssuedCRLSigningKeyCACert.crt
run_test 4.5.7 3 InvalidBasicSelfIssuedCRLSigningKeyTest7EE.crt BasicSelfIssuedCRLSigningKeyCRLCert.crt BasicSelfIssuedCRLSigningKeyCACert.crt
run_test 4.5.8 1 InvalidBasicSelfIssuedCRLSigningKeyTest8EE.crt BasicSelfIssuedCRLSigningKeyCRLCert.crt BasicSelfIssuedCRLSigningKeyCACert.crt

run_test 4.6.1 1 InvalidMissingbasicConstraintsTest1EE.crt MissingbasicConstraintsCACert.crt
run_test 4.6.2 1 InvalidcAFalseTest2EE.crt basicConstraintsCriticalcAFalseCACert.crt
run_test 4.6.3 1 InvalidcAFalseTest3EE.crt basicConstraintsNotCriticalcAFalseCACert.crt
run_test 4.6.4 0 ValidbasicConstraintsNotCriticalTest4EE.crt basicConstraintsNotCriticalCACert.crt
run_test 4.6.5 1 InvalidpathLenConstraintTest5EE.crt pathLenConstraint0subCACert.crt pathLenConstraint0CACert.crt
run_test 4.6.6 1 InvalidpathLenConstraintTest6EE.crt pathLenConstraint0subCACert.crt pathLenConstraint0CACert.crt
run_test 4.6.7 0 ValidpathLenConstraintTest7EE.crt pathLenConstraint0CACert.crt
run_test 4.6.8 0 ValidpathLenConstraintTest8EE.crt pathLenConstraint0CACert.crt
run_test 4.6.9 1 InvalidpathLenConstraintTest9EE.crt pathLenConstraint6subsubCA00Cert.crt pathLenConstraint6subCA0Cert.crt pathLenConstraint6CACert.crt
run_test 4.6.10 1 InvalidpathLenConstraintTest10EE.crt pathLenConstraint6subsubCA00Cert.crt pathLenConstraint6subCA0Cert.crt pathLenConstraint6CACert.crt
run_test 4.6.11 1 InvalidpathLenConstraintTest11EE.crt pathLenConstraint6subsubsubCA11XCert.crt pathLenConstraint6subsubCA11Cert.crt pathLenConstraint6subCA1Cert.crt pathLenConstraint6CACert.crt
run_test 4.6.12 1 InvalidpathLenConstraintTest12EE.crt pathLenConstraint6subsubsubCA11XCert.crt pathLenConstraint6subsubCA11Cert.crt pathLenConstraint6subCA1Cert.crt pathLenConstraint6CACert.crt
run_test 4.6.13 0 ValidpathLenConstraintTest13EE.crt pathLenConstraint6subsubsubCA41XCert.crt pathLenConstraint6subsubCA41Cert.crt pathLenConstraint6subCA4Cert.crt pathLenConstraint6CACert.crt
run_test 4.6.14 0 ValidpathLenConstraintTest14EE.crt pathLenConstraint6subsubsubCA41XCert.crt pathLenConstraint6subsubCA41Cert.crt pathLenConstraint6subCA4Cert.crt pathLenConstraint6CACert.crt
run_test 4.6.15 0 ValidSelfIssuedpathLenConstraintTest15EE.crt pathLenConstraint0SelfIssuedCACert.crt pathLenConstraint0CACert.crt
run_test 4.6.16 1 InvalidSelfIssuedpathLenConstraintTest16EE.crt pathLenConstraint0subCA2Cert.crt pathLenConstraint0SelfIssuedCACert.crt pathLenConstraint0CACert.crt
run_test 4.6.17 0 ValidSelfIssuedpathLenConstraintTest17EE.crt pathLenConstraint1SelfIssuedsubCACert.crt pathLenConstraint1subCACert.crt pathLenConstraint1SelfIssuedCACert.crt pathLenConstraint1CACert.crt

run_test 4.7.1 1 InvalidkeyUsageCriticalkeyCertSignFalseTest1EE.crt keyUsageCriticalkeyCertSignFalseCACert.crt
run_test 4.7.2 1 InvalidkeyUsageNotCriticalkeyCertSignFalseTest2EE.crt keyUsageNotCriticalkeyCertSignFalseCACert.crt
run_test 4.7.3 0 ValidkeyUsageNotCriticalTest3EE.crt keyUsageNotCriticalCACert.crt
run_test 4.7.4 1 InvalidkeyUsageCriticalcRLSignFalseTest4EE.crt keyUsageCriticalcRLSignFalseCACert.crt
run_test 4.7.5 1 InvalidkeyUsageNotCriticalcRLSignFalseTest5EE.crt keyUsageNotCriticalcRLSignFalseCACert.crt

run_test 4.8.1 0 ValidCertificatePathTest1EE.crt GoodCACert.crt
run_test 4.8.2 0 AllCertificatesNoPoliciesTest2EE.crt NoPoliciesCACert.crt
run_test 4.8.3 0 DifferentPoliciesTest3EE.crt PoliciesP2subCACert.crt GoodCACert.crt
run_test 4.8.4 0 DifferentPoliciesTest4EE.crt GoodsubCACert.crt GoodCACert.crt
run_test 4.8.5 0 DifferentPoliciesTest5EE.crt PoliciesP2subCA2Cert.crt GoodCACert.crt
run_test 4.8.6 0 OverlappingPoliciesTest6EE.crt PoliciesP1234subsubCAP123P12Cert.crt PoliciesP1234subCAP123Cert.crt PoliciesP1234CACert.crt
run_test 4.8.7 0 DifferentPoliciesTest7EE.crt PoliciesP123subsubCAP12P1Cert.crt PoliciesP123subCAP12Cert.crt PoliciesP123CACert.crt
run_test 4.8.8 0 DifferentPoliciesTest8EE.crt PoliciesP12subsubCAP1P2Cert.crt PoliciesP12subCAP1Cert.crt PoliciesP12CACert.crt
run_test 4.8.9 0 DifferentPoliciesTest9EE.crt PoliciesP123subsubsubCAP12P2P1Cert.crt PoliciesP123subsubCAP12P2Cert.crt PoliciesP123subCAP12Cert.crt PoliciesP123CACert.crt
run_test 4.8.10 0 AllCertificatesSamePoliciesTest10EE.crt PoliciesP12CACert.crt
run_test 4.8.11 0 AllCertificatesanyPolicyTest11EE.crt anyPolicyCACert.crt
run_test 4.8.12 0 DifferentPoliciesTest12EE.crt PoliciesP3CACert.crt
run_test 4.8.13 0 AllCertificatesSamePoliciesTest13EE.crt PoliciesP123CACert.crt
run_test 4.8.14 0 AnyPolicyTest14EE.crt anyPolicyCACert.crt
run_test 4.8.15 0 UserNoticeQualifierTest15EE.crt
run_test 4.8.16 0 UserNoticeQualifierTest16EE.crt GoodCACert.crt
run_test 4.8.17 0 UserNoticeQualifierTest17EE.crt GoodCACert.crt
run_test 4.8.18 0 UserNoticeQualifierTest18EE.crt PoliciesP12CACert.crt
run_test 4.8.19 0 UserNoticeQualifierTest19EE.crt TrustAnchorRootCertificate.crt
run_test 4.8.20 0 CPSPointerQualifierTest20EE.crt GoodCACert.crt

run_test 4.16.1 0 ValidUnknownNotCriticalCertificateExtensionTest1EE.crt
run_test 4.16.2 -1 InvalidUnknownCriticalCertificateExtensionTest2EE.crt

if false; then
# DSA tests
run_test 4.1.4 0 ValidDSASignaturesTest4EE.crt DSACACert.crt
fi

popd


echo "Successful tests:$SUCCESS"
echo "Failed tests:$FAILURE"
