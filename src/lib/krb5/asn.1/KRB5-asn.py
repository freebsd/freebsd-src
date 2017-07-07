-- lib/krb5/asn.1/KRB5-asn.py
--
-- Copyright 1989 by the Massachusetts Institute of Technology.
--
-- Export of this software from the United States of America may
--   require a specific license from the United States Government.
--   It is the responsibility of any person or organization contemplating
--   export to obtain such a license before exporting.
-- 
-- WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
-- distribute this software and its documentation for any purpose and
-- without fee is hereby granted, provided that the above copyright
-- notice appear in all copies and that both that copyright notice and
-- this permission notice appear in supporting documentation, and that
-- the name of M.I.T. not be used in advertising or publicity pertaining
-- to distribution of the software without specific, written prior
-- permission.  Furthermore if you modify this software you must label
-- your software as modified software and not distribute it in such a
-- fashion that it might be confused with the original M.I.T. software.
-- M.I.T. makes no representations about the suitability of
-- this software for any purpose.  It is provided "as is" without express
-- or implied warranty.
--
-- ASN.1 definitions for the kerberos network objects
--
-- Do not change the order of any structure containing some
-- element_KRB5_xx unless the corresponding translation code is also
-- changed.
--

KRB5 DEFINITIONS ::=
BEGIN

-- needed to do the Right Thing with pepsy; this isn't a valid ASN.1
-- token, however.

SECTIONS encode decode none

-- the order of stuff in this file matches the order in the draft RFC

Realm ::= GeneralString

HostAddress ::= SEQUENCE  {
	addr-type[0]			INTEGER,
	address[1]			OCTET STRING
}

HostAddresses ::=	SEQUENCE OF SEQUENCE {
	addr-type[0]	INTEGER,
	address[1]	OCTET STRING
}

AuthorizationData ::=	SEQUENCE OF SEQUENCE {
	ad-type[0]	INTEGER,
	ad-data[1]	OCTET STRING
}

KDCOptions ::= BIT STRING {
	reserved(0),
	forwardable(1),
	forwarded(2),
	proxiable(3),
	proxy(4),
	allow-postdate(5),
	postdated(6),
	unused7(7),
	renewable(8),
	unused9(9),
	renewable-ok(27),
	enc-tkt-in-skey(28),
	renew(30),
	validate(31)
}

LastReq ::=	SEQUENCE OF SEQUENCE {
	lr-type[0]	INTEGER,
	lr-value[1]	KerberosTime
}

KerberosTime ::=	GeneralizedTime -- Specifying UTC time zone (Z)

PrincipalName ::= SEQUENCE{
	name-type[0]	INTEGER,
	name-string[1]	SEQUENCE OF GeneralString
}

Ticket ::=	[APPLICATION 1] SEQUENCE {
	tkt-vno[0]	INTEGER,
	realm[1]	Realm,
	sname[2]	PrincipalName,
	enc-part[3]	EncryptedData	-- EncTicketPart
}

TransitedEncoding ::= SEQUENCE {
	tr-type[0]	INTEGER, -- Only supported value is 1 == DOMAIN-COMPRESS
	contents[1]	OCTET STRING
}

-- Encrypted part of ticket
EncTicketPart ::=	[APPLICATION 3] SEQUENCE {
	flags[0]	TicketFlags,
	key[1]		EncryptionKey,
	crealm[2]	Realm,
	cname[3]	PrincipalName,
	transited[4]	TransitedEncoding,
	authtime[5]	KerberosTime,
	starttime[6]	KerberosTime OPTIONAL,
	endtime[7]	KerberosTime,
	renew-till[8]	KerberosTime OPTIONAL,
	caddr[9]	HostAddresses OPTIONAL,
	authorization-data[10]	AuthorizationData OPTIONAL
}

-- Unencrypted authenticator
Authenticator ::=	[APPLICATION 2] SEQUENCE  {
	authenticator-vno[0]	INTEGER,
	crealm[1]		Realm,
	cname[2]		PrincipalName,
	cksum[3]		Checksum OPTIONAL,
	cusec[4]		INTEGER,
	ctime[5]		KerberosTime,
	subkey[6]		EncryptionKey OPTIONAL,
	seq-number[7]		INTEGER OPTIONAL,
	authorization-data[8]	AuthorizationData OPTIONAL
}

TicketFlags ::= BIT STRING {
	reserved(0),
	forwardable(1),
	forwarded(2),
	proxiable(3),
	proxy(4),
	may-postdate(5),
	postdated(6),
	invalid(7),
	renewable(8),
	initial(9)
}

AS-REQ ::= [APPLICATION 10] KDC-REQ
TGS-REQ ::= [APPLICATION 12] KDC-REQ

KDC-REQ ::= SEQUENCE {
	pvno[1]		INTEGER,
	msg-type[2]	INTEGER,
	padata[3]	SEQUENCE OF PA-DATA OPTIONAL,
	req-body[4]	KDC-REQ-BODY
}

PA-DATA ::= SEQUENCE {
	padata-type[1]	INTEGER,
	pa-data[2]	OCTET STRING -- might be encoded AP-REQ
}

KDC-REQ-BODY ::=	SEQUENCE {
	 kdc-options[0]	KDCOptions,
	 cname[1]	PrincipalName OPTIONAL, -- Used only in AS-REQ
	 realm[2]	Realm, -- Server's realm  Also client's in AS-REQ
	 sname[3]	PrincipalName OPTIONAL,
	 from[4]	KerberosTime OPTIONAL,
	 till[5]	KerberosTime,
	 rtime[6]	KerberosTime OPTIONAL,
	 nonce[7]	INTEGER,
	 etype[8]	SEQUENCE OF INTEGER, -- EncryptionType, 
			-- in preference order
	 addresses[9]	HostAddresses OPTIONAL,
	 enc-authorization-data[10]	EncryptedData OPTIONAL, 
			-- AuthorizationData
	 additional-tickets[11]	SEQUENCE OF Ticket OPTIONAL
}

AS-REP ::= [APPLICATION 11] KDC-REP
TGS-REP ::= [APPLICATION 13] KDC-REP
KDC-REP ::= SEQUENCE {
	pvno[0]				INTEGER,
	msg-type[1]			INTEGER,
	padata[2]			SEQUENCE OF PA-DATA OPTIONAL,
	crealm[3]			Realm,
	cname[4]			PrincipalName,
	ticket[5]			Ticket,		-- Ticket
	enc-part[6]			EncryptedData	-- EncKDCRepPart
}

EncASRepPart ::= [APPLICATION 25] EncKDCRepPart
EncTGSRepPart ::= [APPLICATION 26] EncKDCRepPart
EncKDCRepPart ::=  SEQUENCE {
	key[0]		EncryptionKey,
	last-req[1]	LastReq,
	nonce[2]	INTEGER,
	key-expiration[3]	KerberosTime OPTIONAL,
	flags[4]	TicketFlags,
	authtime[5]	KerberosTime,
	starttime[6]	KerberosTime OPTIONAL,
	endtime[7]	KerberosTime,
	renew-till[8]	KerberosTime OPTIONAL,
	srealm[9]	Realm,
	sname[10]	PrincipalName,
	caddr[11]	HostAddresses OPTIONAL
}

AP-REQ ::= [APPLICATION 14] SEQUENCE {
	pvno[0]				INTEGER,
	msg-type[1]			INTEGER,
	ap-options[2]			APOptions,
	ticket[3]			Ticket,
	authenticator[4]		EncryptedData	-- Authenticator
}

APOptions ::= BIT STRING {
	reserved(0),
	use-session-key(1),
	mutual-required(2)
}

AP-REP ::= [APPLICATION 15] SEQUENCE {
	pvno[0]				INTEGER,
	msg-type[1]			INTEGER,
	enc-part[2]			EncryptedData	-- EncAPRepPart
}

EncAPRepPart ::= [APPLICATION 27] SEQUENCE {
	ctime[0]			KerberosTime,
	cusec[1]			INTEGER,
	subkey[2]			EncryptionKey OPTIONAL,
	seq-number[3]			INTEGER OPTIONAL
}

KRB-SAFE ::= [APPLICATION 20] SEQUENCE {
	pvno[0]				INTEGER,
	msg-type[1]			INTEGER,
	safe-body[2]			KRB-SAFE-BODY,
	cksum[3]			Checksum			
}

KRB-SAFE-BODY ::= SEQUENCE {
	user-data[0]			OCTET STRING,
	timestamp[1]			KerberosTime OPTIONAL,
	usec[2]				INTEGER OPTIONAL,
	seq-number[3]			INTEGER OPTIONAL,
	s-address[4]			HostAddress,	-- sender's addr
	r-address[5]			HostAddress OPTIONAL -- recip's addr 
}

KRB-PRIV ::=	[APPLICATION 21] SEQUENCE {
	pvno[0]		INTEGER,
	msg-type[1]	INTEGER,
	enc-part[3]	EncryptedData	-- EncKrbPrivPart 
}

EncKrbPrivPart ::=	[APPLICATION 28] SEQUENCE {
	user-data[0]	OCTET STRING,
	timestamp[1]	KerberosTime OPTIONAL,
	usec[2]		INTEGER OPTIONAL,
	seq-number[3]	INTEGER OPTIONAL,
	s-address[4]	HostAddress,	-- sender's addr
	r-address[5]	HostAddress OPTIONAL	-- recip's addr 
}

-- The KRB-CRED message allows easy forwarding of credentials.

KRB-CRED ::= [APPLICATION 22] SEQUENCE {
	pvno[0]		INTEGER,
	msg-type[1]	INTEGER, -- KRB_CRED
	tickets[2]	SEQUENCE OF Ticket,
	enc-part[3]	EncryptedData -- EncKrbCredPart 
}

EncKrbCredPart ::= [APPLICATION 29] SEQUENCE {
	ticket-info[0] 	SEQUENCE OF KRB-CRED-INFO,	
	nonce[1]	INTEGER OPTIONAL,
	timestamp[2]	KerberosTime OPTIONAL,
	usec[3]		INTEGER OPTIONAL,
	s-address[4]	HostAddress OPTIONAL,
	r-address[5]	HostAddress OPTIONAL
}

KRB-CRED-INFO	::=	SEQUENCE {
	key[0]		EncryptionKey,
        prealm[1] 	Realm OPTIONAL,
        pname[2] 	PrincipalName OPTIONAL,
        flags[3] 	TicketFlags OPTIONAL,
        authtime[4] 	KerberosTime OPTIONAL,
        starttime[5] 	KerberosTime OPTIONAL,
        endtime[6] 	KerberosTime OPTIONAL,
        renew-till[7] 	KerberosTime OPTIONAL,
        srealm[8] 	Realm OPTIONAL,
        sname[9] 	PrincipalName OPTIONAL,
        caddr[10] 	HostAddresses OPTIONAL 
}

KRB-ERROR ::=	[APPLICATION 30] SEQUENCE {
	pvno[0]		INTEGER,
	msg-type[1]	INTEGER,
	ctime[2]	KerberosTime OPTIONAL,
	cusec[3]	INTEGER OPTIONAL,
	stime[4]	KerberosTime,
	susec[5]	INTEGER,
	error-code[6]	INTEGER,
	crealm[7]	Realm OPTIONAL,
	cname[8]	PrincipalName OPTIONAL,
	realm[9]	Realm, -- Correct realm
	sname[10]	PrincipalName, -- Correct name
	e-text[11]	GeneralString OPTIONAL,
	e-data[12]	OCTET STRING OPTIONAL
}

EncryptedData ::=	SEQUENCE {
	etype[0]	INTEGER, -- EncryptionType
	kvno[1]		INTEGER OPTIONAL,
	cipher[2]	OCTET STRING -- CipherText
}

EncryptionKey ::= SEQUENCE {
	keytype[0]			INTEGER,
	keyvalue[1]			OCTET STRING
}

Checksum ::= SEQUENCE {
	cksumtype[0]			INTEGER,
	checksum[1]			OCTET STRING
}

METHOD-DATA ::= SEQUENCE {
	method-type[0]	INTEGER,
	method-data[1]	OCTET STRING OPTIONAL
}

ETYPE-INFO-ENTRY ::= SEQUENCE {
	etype[0]	INTEGER,
	salt[1]		OCTET STRING OPTIONAL
}

ETYPE-INFO ::= SEQUENCE OF ETYPE-INFO-ENTRY

PA-ENC-TS-ENC   ::= SEQUENCE {
       patimestamp[0]               KerberosTime, -- client's time
       pausec[1]                    INTEGER OPTIONAL
}

-- These ASN.1 definitions are NOT part of the official Kerberos protocol... 

-- New ASN.1 definitions for the kadmin protocol.
-- Originally contributed from the Sandia modifications

PasswdSequence ::= SEQUENCE {
	passwd[0]			OCTET STRING,
	phrase[1]			OCTET STRING
}

PasswdData ::= SEQUENCE {
	passwd-sequence-count[0]	INTEGER,
	passwd-sequence[1]		SEQUENCE OF PasswdSequence
}

-- encodings from 
-- Integrating Single-use Authentication Mechanisms with Kerberos

PA-SAM-CHALLENGE ::= SEQUENCE {
    sam-type[0]                 INTEGER,
    sam-flags[1]                SAMFlags,
    sam-type-name[2]            GeneralString OPTIONAL,
    sam-track-id[3]             GeneralString OPTIONAL,
    sam-challenge-label[4]      GeneralString OPTIONAL,
    sam-challenge[5]            GeneralString OPTIONAL,
    sam-response-prompt[6]      GeneralString OPTIONAL,
    sam-pk-for-sad[7]           OCTET STRING OPTIONAL,
    sam-nonce[8]                INTEGER OPTIONAL,
    sam-cksum[9]                Checksum OPTIONAL
}

PA-SAM-CHALLENGE-2 ::= SEQUENCE {
    sam-body[0]                 PA-SAM-CHALLENGE-2-BODY,
    sam-cksum[1]                SEQUENCE (1..MAX) OF Checksum,
    ...
}

PA-SAM-CHALLENGE-2-BODY ::= SEQUENCE {
    sam-type[0]                 INTEGER,
    sam-flags[1]                SAMFlags,
    sam-type-name[2]            GeneralString OPTIONAL,
    sam-track-id[3]             GeneralString OPTIONAL,
    sam-challenge-label[4]      GeneralString OPTIONAL,
    sam-challenge[5]            GeneralString OPTIONAL,
    sam-response-prompt[6]      GeneralString OPTIONAL,
    sam-pk-for-sad[7]           EncryptionKey OPTIONAL,
    sam-nonce[8]                INTEGER,
    sam-etype[9]		INTEGER,
    ...
}

-- these are [0].. [2] in the draft
SAMFlags ::= BIT STRING (SIZE (32..MAX))
    -- use-sad-as-key(0)
    -- send-encrypted-sad(1)
    -- must-pk-encrypt-sad(2)

PA-SAM-RESPONSE ::= SEQUENCE {
    sam-type[0]                 INTEGER,
    sam-flags[1]                SAMFlags,
    sam-track-id[2]             GeneralString OPTIONAL,
    -- sam-enc-key is reserved for future use, so I'm making it OPTIONAL - mwe
    sam-enc-key[3]              EncryptedData,
                                   -- PA-ENC-SAM-KEY
    sam-enc-nonce-or-ts[4]      EncryptedData,
                                   -- PA-ENC-SAM-RESPONSE-ENC
    sam-nonce[5]                INTEGER OPTIONAL,
    sam-patimestamp[6]          KerberosTime OPTIONAL
}

PA-SAM-RESPONSE-2 ::= SEQUENCE {
    sam-type[0]                 INTEGER,
    sam-flags[1]                SAMFlags,
    sam-track-id[2]             GeneralString OPTIONAL,
    sam-enc-nonce-or-sad[3]     EncryptedData,
                                   -- PA-ENC-SAM-RESPONSE-ENC
    sam-nonce[4]                INTEGER,
    ...
}

PA-ENC-SAM-KEY ::= SEQUENCE {
             sam-key[0]                 EncryptionKey
}

PA-ENC-SAM-RESPONSE-ENC ::= SEQUENCE {
     sam-nonce[0]               INTEGER OPTIONAL,
     sam-timestamp[1]           KerberosTime OPTIONAL,
     sam-usec[2]                INTEGER OPTIONAL,
     sam-passcode[3]            GeneralString OPTIONAL
}

PA-ENC-SAM-RESPONSE-ENC-2 ::= SEQUENCE {
     sam-nonce[0]               INTEGER,
     sam-sad[1]                 GeneralString OPTIONAL,
     ...
}
END
