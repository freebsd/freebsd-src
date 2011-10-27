# $FreeBSD$

LIB=	asn1
LDFLAGS=	-Wl,--no-undefined
INCS=	asn1_err.h heim_asn1.h
LDADD=	-lcom_err -lroken
DPADD=	${LIBCOM_ERR} ${LIBROKEN}

SRCS=	asn1_err.c \
	asn1_err.h \
	der_copy.c \
	der_cmp.c \
	der_free.c \
	der_format.c \
	der_get.c \
	der_length.c \
	der_put.c \
	extra.c \
	timegm.c \
	${GEN:S/.x$/.c/}

CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken -I.

GEN_RFC2459 = \
	asn1_Version.x \
	asn1_id_pkcs_1.x \
	asn1_id_pkcs1_rsaEncryption.x \
	asn1_id_pkcs1_md2WithRSAEncryption.x \
	asn1_id_pkcs1_md5WithRSAEncryption.x \
	asn1_id_pkcs1_sha1WithRSAEncryption.x \
	asn1_id_pkcs1_sha256WithRSAEncryption.x \
	asn1_id_pkcs1_sha384WithRSAEncryption.x \
	asn1_id_pkcs1_sha512WithRSAEncryption.x \
	asn1_id_heim_rsa_pkcs1_x509.x \
	asn1_id_pkcs_2.x \
	asn1_id_pkcs2_md2.x \
	asn1_id_pkcs2_md4.x \
	asn1_id_pkcs2_md5.x \
	asn1_id_rsa_digestAlgorithm.x \
	asn1_id_rsa_digest_md2.x \
	asn1_id_rsa_digest_md4.x \
	asn1_id_rsa_digest_md5.x \
	asn1_id_pkcs_3.x \
	asn1_id_pkcs3_rc2_cbc.x \
	asn1_id_pkcs3_rc4.x \
	asn1_id_pkcs3_des_ede3_cbc.x \
	asn1_id_rsadsi_encalg.x \
	asn1_id_rsadsi_rc2_cbc.x \
	asn1_id_rsadsi_des_ede3_cbc.x \
	asn1_id_secsig_sha_1.x \
	asn1_id_nistAlgorithm.x \
	asn1_id_nist_aes_algs.x \
	asn1_id_aes_128_cbc.x \
	asn1_id_aes_192_cbc.x \
	asn1_id_aes_256_cbc.x \
	asn1_id_nist_sha_algs.x \
	asn1_id_sha256.x \
	asn1_id_sha224.x \
	asn1_id_sha384.x \
	asn1_id_sha512.x \
	asn1_id_dhpublicnumber.x \
	asn1_id_x9_57.x \
	asn1_id_dsa.x \
	asn1_id_dsa_with_sha1.x \
	asn1_id_x520_at.x \
	asn1_id_at_commonName.x \
	asn1_id_at_surname.x \
	asn1_id_at_serialNumber.x \
	asn1_id_at_countryName.x \
	asn1_id_at_localityName.x \
	asn1_id_at_streetAddress.x \
	asn1_id_at_stateOrProvinceName.x \
	asn1_id_at_organizationName.x \
	asn1_id_at_organizationalUnitName.x \
	asn1_id_at_name.x \
	asn1_id_at_givenName.x \
	asn1_id_at_initials.x \
	asn1_id_at_generationQualifier.x \
	asn1_id_at_pseudonym.x \
	asn1_id_Userid.x \
	asn1_id_domainComponent.x \
	asn1_id_x509_ce.x \
	asn1_id_uspkicommon_card_id.x \
	asn1_id_uspkicommon_piv_interim.x \
	asn1_id_netscape.x \
	asn1_id_netscape_cert_comment.x \
	asn1_id_ms_cert_enroll_domaincontroller.x \
	asn1_id_ms_client_authentication.x \
	asn1_AlgorithmIdentifier.x \
	asn1_AttributeType.x \
	asn1_AttributeValue.x \
	asn1_TeletexStringx.x \
	asn1_DirectoryString.x \
	asn1_Attribute.x \
	asn1_AttributeTypeAndValue.x \
	asn1_AuthorityInfoAccessSyntax.x \
	asn1_AccessDescription.x \
	asn1_RelativeDistinguishedName.x \
	asn1_RDNSequence.x \
	asn1_Name.x \
	asn1_CertificateSerialNumber.x \
	asn1_Time.x \
	asn1_Validity.x \
	asn1_UniqueIdentifier.x \
	asn1_SubjectPublicKeyInfo.x \
	asn1_Extension.x \
	asn1_Extensions.x \
	asn1_TBSCertificate.x \
	asn1_Certificate.x \
	asn1_Certificates.x \
	asn1_ValidationParms.x \
	asn1_DomainParameters.x \
	asn1_DHPublicKey.x \
	asn1_OtherName.x \
	asn1_GeneralName.x \
	asn1_GeneralNames.x \
	asn1_id_x509_ce_keyUsage.x \
	asn1_KeyUsage.x \
	asn1_id_x509_ce_authorityKeyIdentifier.x \
	asn1_KeyIdentifier.x \
	asn1_AuthorityKeyIdentifier.x \
	asn1_id_x509_ce_subjectKeyIdentifier.x \
	asn1_SubjectKeyIdentifier.x \
	asn1_id_x509_ce_basicConstraints.x \
	asn1_BasicConstraints.x \
	asn1_id_x509_ce_nameConstraints.x \
	asn1_BaseDistance.x \
	asn1_GeneralSubtree.x \
	asn1_GeneralSubtrees.x \
	asn1_NameConstraints.x \
	asn1_id_x509_ce_privateKeyUsagePeriod.x \
	asn1_id_x509_ce_certificatePolicies.x \
	asn1_id_x509_ce_policyMappings.x \
	asn1_id_x509_ce_subjectAltName.x \
	asn1_id_x509_ce_issuerAltName.x \
	asn1_id_x509_ce_subjectDirectoryAttributes.x \
	asn1_id_x509_ce_policyConstraints.x \
	asn1_id_x509_ce_extKeyUsage.x \
	asn1_ExtKeyUsage.x \
	asn1_id_x509_ce_cRLDistributionPoints.x \
	asn1_id_x509_ce_deltaCRLIndicator.x \
	asn1_id_x509_ce_issuingDistributionPoint.x \
	asn1_id_x509_ce_holdInstructionCode.x \
	asn1_id_x509_ce_invalidityDate.x \
	asn1_id_x509_ce_certificateIssuer.x \
	asn1_id_x509_ce_inhibitAnyPolicy.x \
	asn1_DistributionPointReasonFlags.x \
	asn1_DistributionPointName.x \
	asn1_DistributionPoint.x \
	asn1_CRLDistributionPoints.x \
	asn1_DSASigValue.x \
	asn1_DSAPublicKey.x \
	asn1_DSAParams.x \
	asn1_RSAPublicKey.x \
	asn1_RSAPrivateKey.x \
	asn1_DigestInfo.x \
	asn1_TBSCRLCertList.x \
	asn1_CRLCertificateList.x \
	asn1_id_x509_ce_cRLNumber.x \
	asn1_id_x509_ce_freshestCRL.x \
	asn1_id_x509_ce_cRLReason.x \
	asn1_CRLReason.x \
	asn1_PKIXXmppAddr.x \
	asn1_id_pkix.x \
	asn1_id_pkix_on.x \
	asn1_id_pkix_on_dnsSRV.x \
	asn1_id_pkix_on_xmppAddr.x \
	asn1_id_pkix_kp.x \
	asn1_id_pkix_kp_serverAuth.x \
	asn1_id_pkix_kp_clientAuth.x \
	asn1_id_pkix_kp_emailProtection.x \
	asn1_id_pkix_kp_timeStamping.x \
	asn1_id_pkix_kp_OCSPSigning.x \
	asn1_id_pkix_pe.x \
	asn1_id_pkix_pe_authorityInfoAccess.x \
	asn1_id_pkix_pe_proxyCertInfo.x \
	asn1_id_pkix_ppl.x \
	asn1_id_pkix_ppl_anyLanguage.x \
	asn1_id_pkix_ppl_inheritAll.x \
	asn1_id_pkix_ppl_independent.x \
	asn1_ProxyPolicy.x \
	asn1_ProxyCertInfo.x 

GEN_CMS = \
	asn1_CMSAttributes.x \
	asn1_CMSCBCParameter.x \
	asn1_CMSEncryptedData.x \
	asn1_CMSIdentifier.x \
	asn1_CMSRC2CBCParameter.x \
	asn1_CMSVersion.x \
	asn1_CertificateList.x \
	asn1_CertificateRevocationLists.x \
	asn1_CertificateSet.x \
	asn1_ContentEncryptionAlgorithmIdentifier.x \
	asn1_ContentInfo.x \
	asn1_ContentType.x \
	asn1_DigestAlgorithmIdentifier.x \
	asn1_DigestAlgorithmIdentifiers.x \
	asn1_EncapsulatedContentInfo.x \
	asn1_EncryptedContent.x \
	asn1_EncryptedContentInfo.x \
	asn1_EncryptedKey.x \
	asn1_EnvelopedData.x \
	asn1_IssuerAndSerialNumber.x \
	asn1_KeyEncryptionAlgorithmIdentifier.x \
	asn1_KeyTransRecipientInfo.x \
	asn1_MessageDigest.x \
	asn1_OriginatorInfo.x \
	asn1_RecipientIdentifier.x \
	asn1_RecipientInfo.x \
	asn1_RecipientInfos.x \
	asn1_SignatureAlgorithmIdentifier.x \
	asn1_SignatureValue.x \
	asn1_SignedData.x \
	asn1_SignerIdentifier.x \
	asn1_SignerInfo.x \
	asn1_SignerInfos.x \
	asn1_id_pkcs7.x \
	asn1_id_pkcs7_data.x \
	asn1_id_pkcs7_digestedData.x \
	asn1_id_pkcs7_encryptedData.x \
	asn1_id_pkcs7_envelopedData.x \
	asn1_id_pkcs7_signedAndEnvelopedData.x \
	asn1_id_pkcs7_signedData.x \
	asn1_UnprotectedAttributes.x

GEN_K5=	asn1_AD_AND_OR.x \
	asn1_AD_IF_RELEVANT.x \
	asn1_AD_KDCIssued.x \
	asn1_AD_MANDATORY_FOR_KDC.x \
	asn1_AD_LoginAlias.x \
	asn1_APOptions.x \
	asn1_AP_REP.x \
	asn1_AP_REQ.x \
	asn1_AS_REP.x \
	asn1_AS_REQ.x \
	asn1_AUTHDATA_TYPE.x \
	asn1_Authenticator.x \
	asn1_AuthorizationData.x \
	asn1_AuthorizationDataElement.x \
	asn1_CKSUMTYPE.x \
	asn1_ChangePasswdDataMS.x \
	asn1_Checksum.x \
	asn1_ENCTYPE.x \
	asn1_ETYPE_INFO.x \
	asn1_ETYPE_INFO2.x \
	asn1_ETYPE_INFO2_ENTRY.x \
	asn1_ETYPE_INFO_ENTRY.x \
	asn1_EncAPRepPart.x \
	asn1_EncASRepPart.x \
	asn1_EncKDCRepPart.x \
	asn1_EncKrbCredPart.x \
	asn1_EncKrbPrivPart.x \
	asn1_EncTGSRepPart.x \
	asn1_EncTicketPart.x \
	asn1_EncryptedData.x \
	asn1_EncryptionKey.x \
	asn1_EtypeList.x \
	asn1_HostAddress.x \
	asn1_HostAddresses.x \
	asn1_KDCOptions.x \
	asn1_KDC_REP.x \
	asn1_KDC_REQ.x \
	asn1_KDC_REQ_BODY.x \
	asn1_KRB_CRED.x \
	asn1_KRB_ERROR.x \
	asn1_KRB_PRIV.x \
	asn1_KRB_SAFE.x \
	asn1_KRB_SAFE_BODY.x \
	asn1_KerberosString.x \
	asn1_KerberosTime.x \
	asn1_KrbCredInfo.x \
	asn1_LR_TYPE.x \
	asn1_LastReq.x \
	asn1_MESSAGE_TYPE.x \
	asn1_METHOD_DATA.x \
	asn1_NAME_TYPE.x \
	asn1_PADATA_TYPE.x \
	asn1_PA_DATA.x \
	asn1_PA_ENC_SAM_RESPONSE_ENC.x \
	asn1_PA_ENC_TS_ENC.x \
	asn1_PA_PAC_REQUEST.x \
	asn1_PA_S4U2Self.x \
	asn1_PA_SAM_CHALLENGE_2.x \
	asn1_PA_SAM_CHALLENGE_2_BODY.x  \
	asn1_PA_SAM_REDIRECT.x \
	asn1_PA_SAM_RESPONSE_2.x \
	asn1_PA_SAM_TYPE.x \
	asn1_PA_ClientCanonicalized.x \
	asn1_PA_ClientCanonicalizedNames.x \
	asn1_PA_SvrReferralData.x \
	asn1_PROV_SRV_LOCATION.x \
	asn1_Principal.x \
	asn1_PrincipalName.x \
	asn1_Realm.x \
	asn1_SAMFlags.x \
	asn1_TGS_REP.x \
	asn1_TGS_REQ.x \
	asn1_TYPED_DATA.x \
	asn1_Ticket.x \
	asn1_TicketFlags.x \
	asn1_TransitedEncoding.x \
	asn1_TypedData.x \
	asn1_krb5int32.x \
	asn1_krb5uint32.x \
	asn1_KRB5SignedPathData.x \
	asn1_KRB5SignedPathPrincipals.x \
	asn1_KRB5SignedPath.x

GEN_PKINIT = \
	asn1_id_pkinit.x \
	asn1_id_pkauthdata.x \
	asn1_id_pkdhkeydata.x \
	asn1_id_pkrkeydata.x \
	asn1_id_pkekuoid.x \
	asn1_id_pkkdcekuoid.x \
	asn1_id_pkinit_san.x \
	asn1_id_pkinit_ms_eku.x \
	asn1_id_pkinit_ms_san.x \
	asn1_MS_UPN_SAN.x \
	asn1_DHNonce.x \
	asn1_KDFAlgorithmId.x \
	asn1_TrustedCA.x \
	asn1_ExternalPrincipalIdentifier.x \
	asn1_ExternalPrincipalIdentifiers.x \
	asn1_PA_PK_AS_REQ.x \
	asn1_PKAuthenticator.x \
	asn1_AuthPack.x \
	asn1_TD_TRUSTED_CERTIFIERS.x \
	asn1_TD_INVALID_CERTIFICATES.x \
	asn1_KRB5PrincipalName.x \
	asn1_AD_INITIAL_VERIFIED_CAS.x \
	asn1_DHRepInfo.x \
	asn1_PA_PK_AS_REP.x \
	asn1_KDCDHKeyInfo.x \
	asn1_ReplyKeyPack.x \
	asn1_TD_DH_PARAMETERS.x \
	asn1_PKAuthenticator_Win2k.x \
	asn1_AuthPack_Win2k.x \
	asn1_TrustedCA_Win2k.x \
	asn1_PA_PK_AS_REQ_Win2k.x \
	asn1_PA_PK_AS_REP_Win2k.x \
	asn1_KDCDHKeyInfo_Win2k.x \
	asn1_ReplyKeyPack_Win2k.x \
	asn1_PkinitSuppPubInfo.x 

GEN_PKCS8 = \
	asn1_PKCS8PrivateKeyAlgorithmIdentifier.x \
	asn1_PKCS8PrivateKey.x \
	asn1_PKCS8PrivateKeyInfo.x \
	asn1_PKCS8Attributes.x \
	asn1_PKCS8EncryptedPrivateKeyInfo.x \
	asn1_PKCS8EncryptedData.x

GEN_PKCS9 = \
	asn1_id_pkcs_9.x \
	asn1_id_pkcs9_contentType.x \
	asn1_id_pkcs9_emailAddress.x \
	asn1_id_pkcs9_messageDigest.x \
	asn1_id_pkcs9_signingTime.x \
	asn1_id_pkcs9_countersignature.x \
	asn1_id_pkcs_9_at_friendlyName.x \
	asn1_id_pkcs_9_at_localKeyId.x \
	asn1_id_pkcs_9_at_certTypes.x \
	asn1_id_pkcs_9_at_certTypes_x509.x \
	asn1_PKCS9_BMPString.x \
	asn1_PKCS9_friendlyName.x

GEN_PKCS12 = \
	asn1_id_pkcs_12.x \
	asn1_id_pkcs_12PbeIds.x \
	asn1_id_pbeWithSHAAnd128BitRC4.x \
	asn1_id_pbeWithSHAAnd40BitRC4.x \
	asn1_id_pbeWithSHAAnd3_KeyTripleDES_CBC.x \
	asn1_id_pbeWithSHAAnd2_KeyTripleDES_CBC.x \
	asn1_id_pbeWithSHAAnd128BitRC2_CBC.x \
	asn1_id_pbewithSHAAnd40BitRC2_CBC.x \
	asn1_id_pkcs12_bagtypes.x \
	asn1_id_pkcs12_keyBag.x \
	asn1_id_pkcs12_pkcs8ShroudedKeyBag.x \
	asn1_id_pkcs12_certBag.x \
	asn1_id_pkcs12_crlBag.x \
	asn1_id_pkcs12_secretBag.x \
	asn1_id_pkcs12_safeContentsBag.x \
	asn1_PKCS12_MacData.x \
	asn1_PKCS12_PFX.x \
	asn1_PKCS12_AuthenticatedSafe.x \
	asn1_PKCS12_CertBag.x \
	asn1_PKCS12_Attribute.x \
	asn1_PKCS12_Attributes.x \
	asn1_PKCS12_SafeBag.x \
	asn1_PKCS12_SafeContents.x \
	asn1_PKCS12_OctetString.x \
	asn1_PKCS12_PBEParams.x

GEN_DIGEST= asn1_DigestError.x \
	asn1_DigestInit.x \
	asn1_DigestInitReply.x \
	asn1_DigestREP.x \
	asn1_DigestREQ.x \
	asn1_DigestRepInner.x \
	asn1_DigestReqInner.x \
	asn1_DigestRequest.x \
	asn1_DigestResponse.x \
	asn1_DigestTypes.x \
	asn1_NTLMInit.x \
	asn1_NTLMInitReply.x \
	asn1_NTLMRequest.x \
	asn1_NTLMResponse.x

GEN_KX509 = \
	asn1_Kx509Response.x \
	asn1_Kx509Request.x

GEN+=	${GEN_RFC2459}
GEN+=	${GEN_CMS}
GEN+=	${GEN_K5}
GEN+=	${GEN_PKINIT}
GEN+=	${GEN_PKCS8}
GEN+=	${GEN_PKCS9}
GEN+=	${GEN_PKCS12}
GEN+=	${GEN_DIGEST}
GEN+=	${GEN_KX509}

CLEANFILES= ${GEN} ${GEN:S/.x$/.c/} *_asn1_files

GEN_ASN1=cms_asn1.h rfc2459_asn1.h krb5_asn1.h pkinit_asn1.h
GEN_ASN1+=pkcs8_asn1.h pkcs9_asn1.h pkcs12_asn1.h digest_asn1.h kx509_asn1.h 
SRCS+= ${GEN_ASN1}
INCS+= ${GEN_ASN1}
CLEANFILES+=${GEN_ASN1}

.ORDER: ${GEN} ${GEN_ASN1}

${GEN_CMS} cms_asn1.h: CMS.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} cms_asn1

${GEN_RFC2459} rfc2459_asn1.h: rfc2459.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile \
		--preserve-binary=TBSCertificate \
		--preserve-binary=TBSCRLCertList \
		--preserve-binary=Name \
		--sequence=GeneralNames \
		--sequence=Extensions \
		--sequence=CRLDistributionPoints ${.ALLSRC:M*.asn1} rfc2459_asn1

${GEN_K5} krb5_asn1.h: k5.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile \
		--encode-rfc1510-bit-string \
		--sequence=KRB5SignedPathPrincipals \
		--sequence=AuthorizationData \
		--sequence=METHOD-DATA \
		--sequence=ETYPE-INFO \
		--sequence=ETYPE-INFO2 ${.ALLSRC:M*.asn1} krb5_asn1

${GEN_PKINIT} pkinit_asn1.h: pkinit.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} pkinit_asn1

${GEN_PKCS8} pkcs8_asn1.h: pkcs8.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} pkcs8_asn1

${GEN_PKCS9} pkcs9_asn1.h: pkcs9.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} pkcs9_asn1

${GEN_PKCS12} pkcs12_asn1.h: pkcs12.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} pkcs12_asn1

${GEN_DIGEST} digest_asn1.h: digest.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} digest_asn1

${GEN_KX509} kx509_asn1.h: kx509.asn1 ../../tools/asn1_compile/asn1_compile
	../../tools/asn1_compile/asn1_compile ${.ALLSRC:M*.asn1} kx509_asn1

../../tools/asn1_compile/asn1_compile:
	cd ${.CURDIR}/../../tools/asn1_compile && ${MAKE}

.for I in ${GEN}
${I:R}.c: ${I}
	cat ${.ALLSRC} > ${.TARGET}
.endfor

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/asn1
