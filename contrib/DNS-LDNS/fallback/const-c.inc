#define PERL_constant_NOTFOUND	1
#define PERL_constant_NOTDEF	2
#define PERL_constant_ISIV	3
#define PERL_constant_ISNO	4
#define PERL_constant_ISNV	5
#define PERL_constant_ISPV	6
#define PERL_constant_ISPVN	7
#define PERL_constant_ISSV	8
#define PERL_constant_ISUNDEF	9
#define PERL_constant_ISUV	10
#define PERL_constant_ISYES	11

#ifndef NVTYPE
typedef double NV; /* 5.6 and later define NVTYPE, and typedef NV to it.  */
#endif
#ifndef aTHX_
#define aTHX_ /* 5.6 or later define this for threading support.  */
#endif
#ifndef pTHX_
#define pTHX_ /* 5.6 or later define this for threading support.  */
#endif

static int
constant_7 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_AA LDNS_AD LDNS_CD LDNS_DH LDNS_QR LDNS_RA LDNS_RD LDNS_TC */
  /* Offset 5 gives the best switch position.  */
  switch (name[5]) {
  case 'A':
    if (memEQ(name, "LDNS_AA", 7)) {
    /*                    ^       */
#ifdef LDNS_AA
      *iv_return = LDNS_AA;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_AD", 7)) {
    /*                    ^       */
#ifdef LDNS_AD
      *iv_return = LDNS_AD;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'C':
    if (memEQ(name, "LDNS_CD", 7)) {
    /*                    ^       */
#ifdef LDNS_CD
      *iv_return = LDNS_CD;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'D':
    if (memEQ(name, "LDNS_DH", 7)) {
    /*                    ^       */
      *iv_return = LDNS_DH;
      return PERL_constant_ISIV;
    }
    break;
  case 'Q':
    if (memEQ(name, "LDNS_QR", 7)) {
    /*                    ^       */
#ifdef LDNS_QR
      *iv_return = LDNS_QR;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_RA", 7)) {
    /*                    ^       */
#ifdef LDNS_RA
      *iv_return = LDNS_RA;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RD", 7)) {
    /*                    ^       */
#ifdef LDNS_RD
      *iv_return = LDNS_RD;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_TC", 7)) {
    /*                    ^       */
#ifdef LDNS_TC
      *iv_return = LDNS_TC;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_11 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_RSAMD5 LDNS_SHA256 LDNS_SHA384 */
  /* Offset 10 gives the best switch position.  */
  switch (name[10]) {
  case '4':
    if (memEQ(name, "LDNS_SHA38", 10)) {
    /*                         4      */
      *iv_return = LDNS_SHA384;
      return PERL_constant_ISIV;
    }
    break;
  case '5':
    if (memEQ(name, "LDNS_RSAMD", 10)) {
    /*                         5      */
      *iv_return = LDNS_RSAMD5;
      return PERL_constant_ISIV;
    }
    break;
  case '6':
    if (memEQ(name, "LDNS_SHA25", 10)) {
    /*                         6      */
      *iv_return = LDNS_SHA256;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_13 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_CERT_OID LDNS_CERT_PGP LDNS_CERT_URI LDNS_ECC_GOST LDNS_SIGN_DSA */
  /* Offset 12 gives the best switch position.  */
  switch (name[12]) {
  case 'A':
    if (memEQ(name, "LDNS_SIGN_DS", 12)) {
    /*                           A      */
      *iv_return = LDNS_SIGN_DSA;
      return PERL_constant_ISIV;
    }
    break;
  case 'D':
    if (memEQ(name, "LDNS_CERT_OI", 12)) {
    /*                           D      */
      *iv_return = LDNS_CERT_OID;
      return PERL_constant_ISIV;
    }
    break;
  case 'I':
    if (memEQ(name, "LDNS_CERT_UR", 12)) {
    /*                           I      */
      *iv_return = LDNS_CERT_URI;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_CERT_PG", 12)) {
    /*                           P      */
      *iv_return = LDNS_CERT_PGP;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_ECC_GOS", 12)) {
    /*                           T      */
      *iv_return = LDNS_ECC_GOST;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_14 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_CERT_IPGP LDNS_CERT_PKIX LDNS_CERT_SPKI LDNS_DSA_NSEC3 LDNS_HASH_GOST
     LDNS_RR_TYPE_A LDNS_RSASHA256 LDNS_RSASHA512 LDNS_STATUS_OK */
  /* Offset 13 gives the best switch position.  */
  switch (name[13]) {
  case '2':
    if (memEQ(name, "LDNS_RSASHA51", 13)) {
    /*                            2      */
      *iv_return = LDNS_RSASHA512;
      return PERL_constant_ISIV;
    }
    break;
  case '3':
    if (memEQ(name, "LDNS_DSA_NSEC", 13)) {
    /*                            3      */
      *iv_return = LDNS_DSA_NSEC3;
      return PERL_constant_ISIV;
    }
    break;
  case '6':
    if (memEQ(name, "LDNS_RSASHA25", 13)) {
    /*                            6      */
      *iv_return = LDNS_RSASHA256;
      return PERL_constant_ISIV;
    }
    break;
  case 'A':
    if (memEQ(name, "LDNS_RR_TYPE_", 13)) {
    /*                            A      */
      *iv_return = LDNS_RR_TYPE_A;
      return PERL_constant_ISIV;
    }
    break;
  case 'I':
    if (memEQ(name, "LDNS_CERT_SPK", 13)) {
    /*                            I      */
      *iv_return = LDNS_CERT_SPKI;
      return PERL_constant_ISIV;
    }
    break;
  case 'K':
    if (memEQ(name, "LDNS_STATUS_O", 13)) {
    /*                            K      */
      *iv_return = LDNS_STATUS_OK;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_CERT_IPG", 13)) {
    /*                            P      */
      *iv_return = LDNS_CERT_IPGP;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_HASH_GOS", 13)) {
    /*                            T      */
      *iv_return = LDNS_HASH_GOST;
      return PERL_constant_ISIV;
    }
    break;
  case 'X':
    if (memEQ(name, "LDNS_CERT_PKI", 13)) {
    /*                            X      */
      *iv_return = LDNS_CERT_PKIX;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_15 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_CERT_IPKIX LDNS_CERT_ISPKI LDNS_IP4ADDRLEN LDNS_IP6ADDRLEN
     LDNS_MAX_RDFLEN LDNS_PRIVATEDNS LDNS_PRIVATEOID LDNS_RDF_TYPE_A
     LDNS_RR_TYPE_A6 LDNS_RR_TYPE_DS LDNS_RR_TYPE_KX LDNS_RR_TYPE_MB
     LDNS_RR_TYPE_MD LDNS_RR_TYPE_MF LDNS_RR_TYPE_MG LDNS_RR_TYPE_MR
     LDNS_RR_TYPE_MX LDNS_RR_TYPE_NS LDNS_RR_TYPE_PX LDNS_RR_TYPE_RP
     LDNS_RR_TYPE_RT LDNS_STATUS_ERR */
  /* Offset 14 gives the best switch position.  */
  switch (name[14]) {
  case '6':
    if (memEQ(name, "LDNS_RR_TYPE_A", 14)) {
    /*                             6      */
      *iv_return = LDNS_RR_TYPE_A6;
      return PERL_constant_ISIV;
    }
    break;
  case 'A':
    if (memEQ(name, "LDNS_RDF_TYPE_", 14)) {
    /*                             A      */
      *iv_return = LDNS_RDF_TYPE_A;
      return PERL_constant_ISIV;
    }
    break;
  case 'B':
    if (memEQ(name, "LDNS_RR_TYPE_M", 14)) {
    /*                             B      */
      *iv_return = LDNS_RR_TYPE_MB;
      return PERL_constant_ISIV;
    }
    break;
  case 'D':
    if (memEQ(name, "LDNS_PRIVATEOI", 14)) {
    /*                             D      */
      *iv_return = LDNS_PRIVATEOID;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_M", 14)) {
    /*                             D      */
      *iv_return = LDNS_RR_TYPE_MD;
      return PERL_constant_ISIV;
    }
    break;
  case 'F':
    if (memEQ(name, "LDNS_RR_TYPE_M", 14)) {
    /*                             F      */
      *iv_return = LDNS_RR_TYPE_MF;
      return PERL_constant_ISIV;
    }
    break;
  case 'G':
    if (memEQ(name, "LDNS_RR_TYPE_M", 14)) {
    /*                             G      */
      *iv_return = LDNS_RR_TYPE_MG;
      return PERL_constant_ISIV;
    }
    break;
  case 'I':
    if (memEQ(name, "LDNS_CERT_ISPK", 14)) {
    /*                             I      */
      *iv_return = LDNS_CERT_ISPKI;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_IP4ADDRLE", 14)) {
    /*                             N      */
#ifdef LDNS_IP4ADDRLEN
      *iv_return = LDNS_IP4ADDRLEN;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_IP6ADDRLE", 14)) {
    /*                             N      */
#ifdef LDNS_IP6ADDRLEN
      *iv_return = LDNS_IP6ADDRLEN;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_MAX_RDFLE", 14)) {
    /*                             N      */
#ifdef LDNS_MAX_RDFLEN
      *iv_return = LDNS_MAX_RDFLEN;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_RR_TYPE_R", 14)) {
    /*                             P      */
      *iv_return = LDNS_RR_TYPE_RP;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_RR_TYPE_M", 14)) {
    /*                             R      */
      *iv_return = LDNS_RR_TYPE_MR;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_STATUS_ER", 14)) {
    /*                             R      */
      *iv_return = LDNS_STATUS_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'S':
    if (memEQ(name, "LDNS_PRIVATEDN", 14)) {
    /*                             S      */
      *iv_return = LDNS_PRIVATEDNS;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_D", 14)) {
    /*                             S      */
      *iv_return = LDNS_RR_TYPE_DS;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_N", 14)) {
    /*                             S      */
      *iv_return = LDNS_RR_TYPE_NS;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_RR_TYPE_R", 14)) {
    /*                             T      */
      *iv_return = LDNS_RR_TYPE_RT;
      return PERL_constant_ISIV;
    }
    break;
  case 'X':
    if (memEQ(name, "LDNS_CERT_IPKI", 14)) {
    /*                             X      */
      *iv_return = LDNS_CERT_IPKIX;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_K", 14)) {
    /*                             X      */
      *iv_return = LDNS_RR_TYPE_KX;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_M", 14)) {
    /*                             X      */
      *iv_return = LDNS_RR_TYPE_MX;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_P", 14)) {
    /*                             X      */
      *iv_return = LDNS_RR_TYPE_PX;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_16 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_CERT_ACPKIX LDNS_DEFAULT_TTL LDNS_KEY_SEP_KEY LDNS_RESOLV_INET
     LDNS_RR_CLASS_CH LDNS_RR_CLASS_HS LDNS_RR_CLASS_IN LDNS_RR_COMPRESS
     LDNS_RR_OVERHEAD LDNS_RR_TYPE_ANY LDNS_RR_TYPE_APL LDNS_RR_TYPE_DLV
     LDNS_RR_TYPE_EID LDNS_RR_TYPE_GID LDNS_RR_TYPE_KEY LDNS_RR_TYPE_LOC
     LDNS_RR_TYPE_NXT LDNS_RR_TYPE_OPT LDNS_RR_TYPE_PTR LDNS_RR_TYPE_SIG
     LDNS_RR_TYPE_SOA LDNS_RR_TYPE_SPF LDNS_RR_TYPE_SRV LDNS_RR_TYPE_TXT
     LDNS_RR_TYPE_UID LDNS_RR_TYPE_WKS LDNS_RR_TYPE_X25 LDNS_SECTION_ANY
     LDNS_SIGN_RSAMD5 LDNS_STATUS_NULL */
  /* Offset 13 gives the best switch position.  */
  switch (name[13]) {
  case 'A':
    if (memEQ(name, "LDNS_RR_TYPE_ANY", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_ANY;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_APL", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_APL;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_SECTION_ANY", 16)) {
    /*                            ^         */
      *iv_return = LDNS_SECTION_ANY;
      return PERL_constant_ISIV;
    }
    break;
  case 'D':
    if (memEQ(name, "LDNS_RR_TYPE_DLV", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_DLV;
      return PERL_constant_ISIV;
    }
    break;
  case 'E':
    if (memEQ(name, "LDNS_RR_COMPRESS", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_COMPRESS;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_OVERHEAD", 16)) {
    /*                            ^         */
#ifdef LDNS_RR_OVERHEAD
      *iv_return = LDNS_RR_OVERHEAD;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RR_TYPE_EID", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_EID;
      return PERL_constant_ISIV;
    }
    break;
  case 'G':
    if (memEQ(name, "LDNS_RR_TYPE_GID", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_GID;
      return PERL_constant_ISIV;
    }
    break;
  case 'K':
    if (memEQ(name, "LDNS_CERT_ACPKIX", 16)) {
    /*                            ^         */
      *iv_return = LDNS_CERT_ACPKIX;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_KEY_SEP_KEY", 16)) {
    /*                            ^         */
#ifdef LDNS_KEY_SEP_KEY
      *iv_return = LDNS_KEY_SEP_KEY;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RR_TYPE_KEY", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_KEY;
      return PERL_constant_ISIV;
    }
    break;
  case 'L':
    if (memEQ(name, "LDNS_RR_TYPE_LOC", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_LOC;
      return PERL_constant_ISIV;
    }
    break;
  case 'M':
    if (memEQ(name, "LDNS_SIGN_RSAMD5", 16)) {
    /*                            ^         */
      *iv_return = LDNS_SIGN_RSAMD5;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_RESOLV_INET", 16)) {
    /*                            ^         */
#ifdef LDNS_RESOLV_INET
      *iv_return = LDNS_RESOLV_INET;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RR_TYPE_NXT", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_NXT;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_RR_TYPE_OPT", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_OPT;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_RR_TYPE_PTR", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_PTR;
      return PERL_constant_ISIV;
    }
    break;
  case 'S':
    if (memEQ(name, "LDNS_RR_TYPE_SIG", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_SIG;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_SOA", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_SOA;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_SPF", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_SPF;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_SRV", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_SRV;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_DEFAULT_TTL", 16)) {
    /*                            ^         */
#ifdef LDNS_DEFAULT_TTL
      *iv_return = LDNS_DEFAULT_TTL;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RR_TYPE_TXT", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_TXT;
      return PERL_constant_ISIV;
    }
    break;
  case 'U':
    if (memEQ(name, "LDNS_RR_TYPE_UID", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_UID;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_STATUS_NULL", 16)) {
    /*                            ^         */
      *iv_return = LDNS_STATUS_NULL;
      return PERL_constant_ISIV;
    }
    break;
  case 'W':
    if (memEQ(name, "LDNS_RR_TYPE_WKS", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_WKS;
      return PERL_constant_ISIV;
    }
    break;
  case 'X':
    if (memEQ(name, "LDNS_RR_TYPE_X25", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_TYPE_X25;
      return PERL_constant_ISIV;
    }
    break;
  case '_':
    if (memEQ(name, "LDNS_RR_CLASS_CH", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_CLASS_CH;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_CLASS_HS", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_CLASS_HS;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_CLASS_IN", 16)) {
    /*                            ^         */
      *iv_return = LDNS_RR_CLASS_IN;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_17 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_CERT_IACPKIX LDNS_KEY_ZONE_KEY LDNS_MAX_LABELLEN LDNS_MAX_POINTERS
     LDNS_PACKET_QUERY LDNS_RDF_TYPE_ALG LDNS_RDF_TYPE_APL LDNS_RDF_TYPE_B64
     LDNS_RDF_TYPE_HEX LDNS_RDF_TYPE_LOC LDNS_RDF_TYPE_STR LDNS_RDF_TYPE_WKS
     LDNS_RESOLV_INET6 LDNS_RR_CLASS_ANY LDNS_RR_TYPE_AAAA LDNS_RR_TYPE_ATMA
     LDNS_RR_TYPE_AXFR LDNS_RR_TYPE_CERT LDNS_RR_TYPE_GPOS LDNS_RR_TYPE_ISDN
     LDNS_RR_TYPE_IXFR LDNS_RR_TYPE_LAST LDNS_RR_TYPE_NSAP LDNS_RR_TYPE_NSEC
     LDNS_RR_TYPE_NULL LDNS_RR_TYPE_SINK LDNS_RR_TYPE_TSIG LDNS_SIGN_RSASHA1 */
  /* Offset 16 gives the best switch position.  */
  switch (name[16]) {
  case '1':
    if (memEQ(name, "LDNS_SIGN_RSASHA", 16)) {
    /*                               1      */
      *iv_return = LDNS_SIGN_RSASHA1;
      return PERL_constant_ISIV;
    }
    break;
  case '4':
    if (memEQ(name, "LDNS_RDF_TYPE_B6", 16)) {
    /*                               4      */
      *iv_return = LDNS_RDF_TYPE_B64;
      return PERL_constant_ISIV;
    }
    break;
  case '6':
    if (memEQ(name, "LDNS_RESOLV_INET", 16)) {
    /*                               6      */
#ifdef LDNS_RESOLV_INET6
      *iv_return = LDNS_RESOLV_INET6;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'A':
    if (memEQ(name, "LDNS_RR_TYPE_AAA", 16)) {
    /*                               A      */
      *iv_return = LDNS_RR_TYPE_AAAA;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_ATM", 16)) {
    /*                               A      */
      *iv_return = LDNS_RR_TYPE_ATMA;
      return PERL_constant_ISIV;
    }
    break;
  case 'C':
    if (memEQ(name, "LDNS_RDF_TYPE_LO", 16)) {
    /*                               C      */
      *iv_return = LDNS_RDF_TYPE_LOC;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_NSE", 16)) {
    /*                               C      */
      *iv_return = LDNS_RR_TYPE_NSEC;
      return PERL_constant_ISIV;
    }
    break;
  case 'G':
    if (memEQ(name, "LDNS_RDF_TYPE_AL", 16)) {
    /*                               G      */
      *iv_return = LDNS_RDF_TYPE_ALG;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_TSI", 16)) {
    /*                               G      */
      *iv_return = LDNS_RR_TYPE_TSIG;
      return PERL_constant_ISIV;
    }
    break;
  case 'K':
    if (memEQ(name, "LDNS_RR_TYPE_SIN", 16)) {
    /*                               K      */
      *iv_return = LDNS_RR_TYPE_SINK;
      return PERL_constant_ISIV;
    }
    break;
  case 'L':
    if (memEQ(name, "LDNS_RDF_TYPE_AP", 16)) {
    /*                               L      */
      *iv_return = LDNS_RDF_TYPE_APL;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_NUL", 16)) {
    /*                               L      */
      *iv_return = LDNS_RR_TYPE_NULL;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_MAX_LABELLE", 16)) {
    /*                               N      */
#ifdef LDNS_MAX_LABELLEN
      *iv_return = LDNS_MAX_LABELLEN;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RR_TYPE_ISD", 16)) {
    /*                               N      */
      *iv_return = LDNS_RR_TYPE_ISDN;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_RR_TYPE_NSA", 16)) {
    /*                               P      */
      *iv_return = LDNS_RR_TYPE_NSAP;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_RDF_TYPE_ST", 16)) {
    /*                               R      */
      *iv_return = LDNS_RDF_TYPE_STR;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_AXF", 16)) {
    /*                               R      */
      *iv_return = LDNS_RR_TYPE_AXFR;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_IXF", 16)) {
    /*                               R      */
      *iv_return = LDNS_RR_TYPE_IXFR;
      return PERL_constant_ISIV;
    }
    break;
  case 'S':
    if (memEQ(name, "LDNS_MAX_POINTER", 16)) {
    /*                               S      */
#ifdef LDNS_MAX_POINTERS
      *iv_return = LDNS_MAX_POINTERS;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RDF_TYPE_WK", 16)) {
    /*                               S      */
      *iv_return = LDNS_RDF_TYPE_WKS;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_GPO", 16)) {
    /*                               S      */
      *iv_return = LDNS_RR_TYPE_GPOS;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_RR_TYPE_CER", 16)) {
    /*                               T      */
      *iv_return = LDNS_RR_TYPE_CERT;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_LAS", 16)) {
    /*                               T      */
      *iv_return = LDNS_RR_TYPE_LAST;
      return PERL_constant_ISIV;
    }
    break;
  case 'X':
    if (memEQ(name, "LDNS_CERT_IACPKI", 16)) {
    /*                               X      */
      *iv_return = LDNS_CERT_IACPKIX;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RDF_TYPE_HE", 16)) {
    /*                               X      */
      *iv_return = LDNS_RDF_TYPE_HEX;
      return PERL_constant_ISIV;
    }
    break;
  case 'Y':
    if (memEQ(name, "LDNS_KEY_ZONE_KE", 16)) {
    /*                               Y      */
#ifdef LDNS_KEY_ZONE_KEY
      *iv_return = LDNS_KEY_ZONE_KEY;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_PACKET_QUER", 16)) {
    /*                               Y      */
      *iv_return = LDNS_PACKET_QUERY;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_CLASS_AN", 16)) {
    /*                               Y      */
      *iv_return = LDNS_RR_CLASS_ANY;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_18 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_MAX_DOMAINLEN LDNS_MAX_PACKETLEN LDNS_PACKET_ANSWER
     LDNS_PACKET_IQUERY LDNS_PACKET_NODATA LDNS_PACKET_NOTIFY
     LDNS_PACKET_STATUS LDNS_PACKET_UPDATE LDNS_RCODE_FORMERR
     LDNS_RCODE_NOERROR LDNS_RCODE_NOTAUTH LDNS_RCODE_NOTIMPL
     LDNS_RCODE_NOTZONE LDNS_RCODE_NXRRSET LDNS_RCODE_REFUSED
     LDNS_RCODE_YXRRSET LDNS_RDF_SIZE_BYTE LDNS_RDF_SIZE_WORD
     LDNS_RDF_TYPE_AAAA LDNS_RDF_TYPE_ATMA LDNS_RDF_TYPE_INT8
     LDNS_RDF_TYPE_NONE LDNS_RDF_TYPE_NSAP LDNS_RDF_TYPE_NSEC
     LDNS_RDF_TYPE_TIME LDNS_RDF_TYPE_HIP  LDNS_RDF_TYPE_TYPE
     LDNS_RESOLV_ANCHOR LDNS_RESOLV_SEARCH LDNS_RR_CLASS_LAST
     LDNS_RR_CLASS_NONE LDNS_RR_TYPE_AFSDB LDNS_RR_TYPE_CNAME
     LDNS_RR_TYPE_COUNT LDNS_RR_TYPE_DHCID LDNS_RR_TYPE_DNAME
     LDNS_RR_TYPE_FIRST LDNS_RR_TYPE_HINFO LDNS_RR_TYPE_MAILA
     LDNS_RR_TYPE_MAILB LDNS_RR_TYPE_MINFO LDNS_RR_TYPE_NAPTR
     LDNS_RR_TYPE_NSEC3 LDNS_RR_TYPE_RRSIG LDNS_RR_TYPE_SSHFP
     LDNS_RR_TYPE_UINFO LDNS_RSASHA1_NSEC3 LDNS_SIGN_ECC_GOST
     LDNS_SIGN_HMACSHA1 */
  /* Offset 17 gives the best switch position.  */
  switch (name[17]) {
  case '1':
    if (memEQ(name, "LDNS_SIGN_HMACSHA", 17)) {
    /*                                1      */
      *iv_return = LDNS_SIGN_HMACSHA1;
      return PERL_constant_ISIV;
    }
    break;
  case '3':
    if (memEQ(name, "LDNS_RR_TYPE_NSEC", 17)) {
    /*                                3      */
      *iv_return = LDNS_RR_TYPE_NSEC3;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RSASHA1_NSEC", 17)) {
    /*                                3      */
      *iv_return = LDNS_RSASHA1_NSEC3;
      return PERL_constant_ISIV;
    }
    break;
  case '8':
    if (memEQ(name, "LDNS_RDF_TYPE_INT", 17)) {
    /*                                8      */
      *iv_return = LDNS_RDF_TYPE_INT8;
      return PERL_constant_ISIV;
    }
    break;
  case 'A':
    if (memEQ(name, "LDNS_PACKET_NODAT", 17)) {
    /*                                A      */
      *iv_return = LDNS_PACKET_NODATA;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RDF_TYPE_AAA", 17)) {
    /*                                A      */
      *iv_return = LDNS_RDF_TYPE_AAAA;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RDF_TYPE_ATM", 17)) {
    /*                                A      */
      *iv_return = LDNS_RDF_TYPE_ATMA;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_MAIL", 17)) {
    /*                                A      */
      *iv_return = LDNS_RR_TYPE_MAILA;
      return PERL_constant_ISIV;
    }
    break;
  case 'B':
    if (memEQ(name, "LDNS_RR_TYPE_AFSD", 17)) {
    /*                                B      */
      *iv_return = LDNS_RR_TYPE_AFSDB;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_MAIL", 17)) {
    /*                                B      */
      *iv_return = LDNS_RR_TYPE_MAILB;
      return PERL_constant_ISIV;
    }
    break;
  case 'C':
    if (memEQ(name, "LDNS_RDF_TYPE_NSE", 17)) {
    /*                                C      */
      *iv_return = LDNS_RDF_TYPE_NSEC;
      return PERL_constant_ISIV;
    }
    break;
  case 'D':
    if (memEQ(name, "LDNS_RCODE_REFUSE", 17)) {
    /*                                D      */
      *iv_return = LDNS_RCODE_REFUSED;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RDF_SIZE_WOR", 17)) {
    /*                                D      */
#ifdef LDNS_RDF_SIZE_WORD
      *iv_return = LDNS_RDF_SIZE_WORD;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RR_TYPE_DHCI", 17)) {
    /*                                D      */
      *iv_return = LDNS_RR_TYPE_DHCID;
      return PERL_constant_ISIV;
    }
    break;
  case 'E':
    if (memEQ(name, "LDNS_PACKET_UPDAT", 17)) {
    /*                                E      */
      *iv_return = LDNS_PACKET_UPDATE;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RCODE_NOTZON", 17)) {
    /*                                E      */
      *iv_return = LDNS_RCODE_NOTZONE;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RDF_SIZE_BYT", 17)) {
    /*                                E      */
#ifdef LDNS_RDF_SIZE_BYTE
      *iv_return = LDNS_RDF_SIZE_BYTE;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RDF_TYPE_NON", 17)) {
    /*                                E      */
      *iv_return = LDNS_RDF_TYPE_NONE;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RDF_TYPE_TIM", 17)) {
    /*                                E      */
      *iv_return = LDNS_RDF_TYPE_TIME;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RDF_TYPE_TYP", 17)) {
    /*                                E      */
      *iv_return = LDNS_RDF_TYPE_TYPE;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_CLASS_NON", 17)) {
    /*                                E      */
      *iv_return = LDNS_RR_CLASS_NONE;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_CNAM", 17)) {
    /*                                E      */
      *iv_return = LDNS_RR_TYPE_CNAME;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_DNAM", 17)) {
    /*                                E      */
      *iv_return = LDNS_RR_TYPE_DNAME;
      return PERL_constant_ISIV;
    }
    break;
  case 'G':
    if (memEQ(name, "LDNS_RDF_TYPE_HIP", 17)) {
    /*                                G      */
      *iv_return = LDNS_RDF_TYPE_HIP;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_RRSI", 17)) {
    /*                                G      */
      *iv_return = LDNS_RR_TYPE_RRSIG;
      return PERL_constant_ISIV;
    }
    break;
  case 'H':
    if (memEQ(name, "LDNS_RCODE_NOTAUT", 17)) {
    /*                                H      */
      *iv_return = LDNS_RCODE_NOTAUTH;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RESOLV_SEARC", 17)) {
    /*                                H      */
#ifdef LDNS_RESOLV_SEARCH
      *iv_return = LDNS_RESOLV_SEARCH;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'L':
    if (memEQ(name, "LDNS_RCODE_NOTIMP", 17)) {
    /*                                L      */
      *iv_return = LDNS_RCODE_NOTIMPL;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_MAX_DOMAINLE", 17)) {
    /*                                N      */
#ifdef LDNS_MAX_DOMAINLEN
      *iv_return = LDNS_MAX_DOMAINLEN;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_MAX_PACKETLE", 17)) {
    /*                                N      */
#ifdef LDNS_MAX_PACKETLEN
      *iv_return = LDNS_MAX_PACKETLEN;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_RR_TYPE_HINF", 17)) {
    /*                                O      */
      *iv_return = LDNS_RR_TYPE_HINFO;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_MINF", 17)) {
    /*                                O      */
      *iv_return = LDNS_RR_TYPE_MINFO;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_UINF", 17)) {
    /*                                O      */
      *iv_return = LDNS_RR_TYPE_UINFO;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_RDF_TYPE_NSA", 17)) {
    /*                                P      */
      *iv_return = LDNS_RDF_TYPE_NSAP;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_SSHF", 17)) {
    /*                                P      */
      *iv_return = LDNS_RR_TYPE_SSHFP;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_PACKET_ANSWE", 17)) {
    /*                                R      */
      *iv_return = LDNS_PACKET_ANSWER;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RCODE_FORMER", 17)) {
    /*                                R      */
      *iv_return = LDNS_RCODE_FORMERR;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RCODE_NOERRO", 17)) {
    /*                                R      */
      *iv_return = LDNS_RCODE_NOERROR;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RESOLV_ANCHO", 17)) {
    /*                                R      */
#ifdef LDNS_RESOLV_ANCHOR
      *iv_return = LDNS_RESOLV_ANCHOR;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RR_TYPE_NAPT", 17)) {
    /*                                R      */
      *iv_return = LDNS_RR_TYPE_NAPTR;
      return PERL_constant_ISIV;
    }
    break;
  case 'S':
    if (memEQ(name, "LDNS_PACKET_STATU", 17)) {
    /*                                S      */
      *iv_return = LDNS_PACKET_STATUS;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_RCODE_NXRRSE", 17)) {
    /*                                T      */
      *iv_return = LDNS_RCODE_NXRRSET;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RCODE_YXRRSE", 17)) {
    /*                                T      */
      *iv_return = LDNS_RCODE_YXRRSET;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_CLASS_LAS", 17)) {
    /*                                T      */
      *iv_return = LDNS_RR_CLASS_LAST;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_COUN", 17)) {
    /*                                T      */
      *iv_return = LDNS_RR_TYPE_COUNT;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_FIRS", 17)) {
    /*                                T      */
      *iv_return = LDNS_RR_TYPE_FIRST;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_SIGN_ECC_GOS", 17)) {
    /*                                T      */
      *iv_return = LDNS_SIGN_ECC_GOST;
      return PERL_constant_ISIV;
    }
    break;
  case 'Y':
    if (memEQ(name, "LDNS_PACKET_IQUER", 17)) {
    /*                                Y      */
      *iv_return = LDNS_PACKET_IQUERY;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_PACKET_NOTIF", 17)) {
    /*                                Y      */
      *iv_return = LDNS_PACKET_NOTIFY;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_19 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_KEY_REVOKE_KEY LDNS_PACKET_UNKNOWN LDNS_RCODE_NXDOMAIN
     LDNS_RCODE_SERVFAIL LDNS_RCODE_YXDOMAIN LDNS_RDF_TYPE_CLASS
     LDNS_RDF_TYPE_DNAME LDNS_RDF_TYPE_INT16 LDNS_RDF_TYPE_INT32
     LDNS_RESOLV_INETANY LDNS_RESOLV_KEYWORD LDNS_RESOLV_OPTIONS
     LDNS_RESOLV_RTT_INF LDNS_RESOLV_RTT_MIN LDNS_RR_CLASS_COUNT
     LDNS_RR_CLASS_FIRST LDNS_RR_NO_COMPRESS LDNS_RR_TYPE_DNSKEY
     LDNS_RR_TYPE_NIMLOC LDNS_RR_TYPE_TALINK LDNS_RR_TYPE_UNSPEC
     LDNS_SECTION_ANSWER LDNS_SIGN_DSA_NSEC3 LDNS_SIGN_RSASHA256
     LDNS_SIGN_RSASHA512 LDNS_STATUS_MEM_ERR LDNS_STATUS_NO_DATA
     LDNS_STATUS_SSL_ERR */
  /* Offset 14 gives the best switch position.  */
  switch (name[14]) {
  case 'A':
    if (memEQ(name, "LDNS_RR_TYPE_TALINK", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RR_TYPE_TALINK;
      return PERL_constant_ISIV;
    }
    break;
  case 'C':
    if (memEQ(name, "LDNS_RDF_TYPE_CLASS", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RDF_TYPE_CLASS;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_CLASS_COUNT", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RR_CLASS_COUNT;
      return PERL_constant_ISIV;
    }
    break;
  case 'D':
    if (memEQ(name, "LDNS_RDF_TYPE_DNAME", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RDF_TYPE_DNAME;
      return PERL_constant_ISIV;
    }
    break;
  case 'E':
    if (memEQ(name, "LDNS_KEY_REVOKE_KEY", 19)) {
    /*                             ^           */
#ifdef LDNS_KEY_REVOKE_KEY
      *iv_return = LDNS_KEY_REVOKE_KEY;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RESOLV_INETANY", 19)) {
    /*                             ^           */
#ifdef LDNS_RESOLV_INETANY
      *iv_return = LDNS_RESOLV_INETANY;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'F':
    if (memEQ(name, "LDNS_RR_CLASS_FIRST", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RR_CLASS_FIRST;
      return PERL_constant_ISIV;
    }
    break;
  case 'H':
    if (memEQ(name, "LDNS_SIGN_RSASHA256", 19)) {
    /*                             ^           */
      *iv_return = LDNS_SIGN_RSASHA256;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_SIGN_RSASHA512", 19)) {
    /*                             ^           */
      *iv_return = LDNS_SIGN_RSASHA512;
      return PERL_constant_ISIV;
    }
    break;
  case 'I':
    if (memEQ(name, "LDNS_RDF_TYPE_INT16", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RDF_TYPE_INT16;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RDF_TYPE_INT32", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RDF_TYPE_INT32;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_NIMLOC", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RR_TYPE_NIMLOC;
      return PERL_constant_ISIV;
    }
    break;
  case 'K':
    if (memEQ(name, "LDNS_PACKET_UNKNOWN", 19)) {
    /*                             ^           */
      *iv_return = LDNS_PACKET_UNKNOWN;
      return PERL_constant_ISIV;
    }
    break;
  case 'L':
    if (memEQ(name, "LDNS_STATUS_SSL_ERR", 19)) {
    /*                             ^           */
      *iv_return = LDNS_STATUS_SSL_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'M':
    if (memEQ(name, "LDNS_STATUS_MEM_ERR", 19)) {
    /*                             ^           */
      *iv_return = LDNS_STATUS_MEM_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_RR_TYPE_DNSKEY", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RR_TYPE_DNSKEY;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RR_TYPE_UNSPEC", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RR_TYPE_UNSPEC;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_SECTION_ANSWER", 19)) {
    /*                             ^           */
      *iv_return = LDNS_SECTION_ANSWER;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_SIGN_DSA_NSEC3", 19)) {
    /*                             ^           */
      *iv_return = LDNS_SIGN_DSA_NSEC3;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_RCODE_NXDOMAIN", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RCODE_NXDOMAIN;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RCODE_YXDOMAIN", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RCODE_YXDOMAIN;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_RR_NO_COMPRESS", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RR_NO_COMPRESS;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_RESOLV_OPTIONS", 19)) {
    /*                             ^           */
#ifdef LDNS_RESOLV_OPTIONS
      *iv_return = LDNS_RESOLV_OPTIONS;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RESOLV_RTT_INF", 19)) {
    /*                             ^           */
#ifdef LDNS_RESOLV_RTT_INF
      *iv_return = LDNS_RESOLV_RTT_INF;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_RESOLV_RTT_MIN", 19)) {
    /*                             ^           */
#ifdef LDNS_RESOLV_RTT_MIN
      *iv_return = LDNS_RESOLV_RTT_MIN;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'V':
    if (memEQ(name, "LDNS_RCODE_SERVFAIL", 19)) {
    /*                             ^           */
      *iv_return = LDNS_RCODE_SERVFAIL;
      return PERL_constant_ISIV;
    }
    break;
  case 'Y':
    if (memEQ(name, "LDNS_RESOLV_KEYWORD", 19)) {
    /*                             ^           */
#ifdef LDNS_RESOLV_KEYWORD
      *iv_return = LDNS_RESOLV_KEYWORD;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case '_':
    if (memEQ(name, "LDNS_STATUS_NO_DATA", 19)) {
    /*                             ^           */
      *iv_return = LDNS_STATUS_NO_DATA;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_20 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_ECDSAP384SHA384 LDNS_PACKET_NXDOMAIN LDNS_PACKET_QUESTION
     LDNS_PACKET_REFERRAL LDNS_RDF_SIZE_6BYTES LDNS_RDF_TYPE_PERIOD
     LDNS_RESOLV_KEYWORDS LDNS_RESOLV_SORTLIST LDNS_SIGN_HMACSHA256
     LDNS_STATUS_FILE_ERR LDNS_STATUS_NOT_IMPL */
  /* Offset 18 gives the best switch position.  */
  switch (name[18]) {
  case '5':
    if (memEQ(name, "LDNS_SIGN_HMACSHA256", 20)) {
    /*                                 ^        */
      *iv_return = LDNS_SIGN_HMACSHA256;
      return PERL_constant_ISIV;
    }
    break;
  case '8':
    if (memEQ(name, "LDNS_ECDSAP384SHA384", 20)) {
    /*                                 ^        */
      *iv_return = LDNS_ECDSAP384SHA384;
      return PERL_constant_ISIV;
    }
    break;
  case 'A':
    if (memEQ(name, "LDNS_PACKET_REFERRAL", 20)) {
    /*                                 ^        */
      *iv_return = LDNS_PACKET_REFERRAL;
      return PERL_constant_ISIV;
    }
    break;
  case 'D':
    if (memEQ(name, "LDNS_RESOLV_KEYWORDS", 20)) {
    /*                                 ^        */
#ifdef LDNS_RESOLV_KEYWORDS
      *iv_return = LDNS_RESOLV_KEYWORDS;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'E':
    if (memEQ(name, "LDNS_RDF_SIZE_6BYTES", 20)) {
    /*                                 ^        */
#ifdef LDNS_RDF_SIZE_6BYTES
      *iv_return = LDNS_RDF_SIZE_6BYTES;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'I':
    if (memEQ(name, "LDNS_PACKET_NXDOMAIN", 20)) {
    /*                                 ^        */
      *iv_return = LDNS_PACKET_NXDOMAIN;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_PACKET_QUESTION", 20)) {
    /*                                 ^        */
      *iv_return = LDNS_PACKET_QUESTION;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_RDF_TYPE_PERIOD", 20)) {
    /*                                 ^        */
      *iv_return = LDNS_RDF_TYPE_PERIOD;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_STATUS_NOT_IMPL", 20)) {
    /*                                 ^        */
      *iv_return = LDNS_STATUS_NOT_IMPL;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_STATUS_FILE_ERR", 20)) {
    /*                                 ^        */
      *iv_return = LDNS_STATUS_FILE_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'S':
    if (memEQ(name, "LDNS_RESOLV_SORTLIST", 20)) {
    /*                                 ^        */
#ifdef LDNS_RESOLV_SORTLIST
      *iv_return = LDNS_RESOLV_SORTLIST;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_21 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_RDF_SIZE_16BYTES LDNS_RDF_TYPE_B32_EXT LDNS_RDF_TYPE_SERVICE
     LDNS_RDF_TYPE_UNKNOWN LDNS_RESOLV_DEFDOMAIN LDNS_RR_TYPE_IPSECKEY
     LDNS_RR_TYPE_NSAP_PTR LDNS_SECTION_QUESTION LDNS_STATUS_NSEC3_ERR
     LDNS_STATUS_RES_NO_NS LDNS_STATUS_RES_QUERY */
  /* Offset 16 gives the best switch position.  */
  switch (name[16]) {
  case '2':
    if (memEQ(name, "LDNS_RDF_TYPE_B32_EXT", 21)) {
    /*                               ^           */
      *iv_return = LDNS_RDF_TYPE_B32_EXT;
      return PERL_constant_ISIV;
    }
    break;
  case '3':
    if (memEQ(name, "LDNS_STATUS_NSEC3_ERR", 21)) {
    /*                               ^           */
      *iv_return = LDNS_STATUS_NSEC3_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'B':
    if (memEQ(name, "LDNS_RDF_SIZE_16BYTES", 21)) {
    /*                               ^           */
#ifdef LDNS_RDF_SIZE_16BYTES
      *iv_return = LDNS_RDF_SIZE_16BYTES;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'E':
    if (memEQ(name, "LDNS_RR_TYPE_IPSECKEY", 21)) {
    /*                               ^           */
      *iv_return = LDNS_RR_TYPE_IPSECKEY;
      return PERL_constant_ISIV;
    }
    break;
  case 'K':
    if (memEQ(name, "LDNS_RDF_TYPE_UNKNOWN", 21)) {
    /*                               ^           */
      *iv_return = LDNS_RDF_TYPE_UNKNOWN;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_STATUS_RES_NO_NS", 21)) {
    /*                               ^           */
      *iv_return = LDNS_STATUS_RES_NO_NS;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_RESOLV_DEFDOMAIN", 21)) {
    /*                               ^           */
#ifdef LDNS_RESOLV_DEFDOMAIN
      *iv_return = LDNS_RESOLV_DEFDOMAIN;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_RR_TYPE_NSAP_PTR", 21)) {
    /*                               ^           */
      *iv_return = LDNS_RR_TYPE_NSAP_PTR;
      return PERL_constant_ISIV;
    }
    break;
  case 'Q':
    if (memEQ(name, "LDNS_STATUS_RES_QUERY", 21)) {
    /*                               ^           */
      *iv_return = LDNS_STATUS_RES_QUERY;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_RDF_TYPE_SERVICE", 21)) {
    /*                               ^           */
      *iv_return = LDNS_RDF_TYPE_SERVICE;
      return PERL_constant_ISIV;
    }
    break;
  case 'S':
    if (memEQ(name, "LDNS_SECTION_QUESTION", 21)) {
    /*                               ^           */
      *iv_return = LDNS_SECTION_QUESTION;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_22 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_RDF_TYPE_CERT_ALG LDNS_RDF_TYPE_IPSECKEY LDNS_RDF_TYPE_TSIGTIME
     LDNS_RESOLV_NAMESERVER LDNS_SECTION_AUTHORITY LDNS_STATUS_SYNTAX_ERR
     LDNS_STATUS_SYNTAX_TTL */
  /* Offset 14 gives the best switch position.  */
  switch (name[14]) {
  case 'C':
    if (memEQ(name, "LDNS_RDF_TYPE_CERT_ALG", 22)) {
    /*                             ^              */
      *iv_return = LDNS_RDF_TYPE_CERT_ALG;
      return PERL_constant_ISIV;
    }
    break;
  case 'I':
    if (memEQ(name, "LDNS_RDF_TYPE_IPSECKEY", 22)) {
    /*                             ^              */
      *iv_return = LDNS_RDF_TYPE_IPSECKEY;
      return PERL_constant_ISIV;
    }
    break;
  case 'M':
    if (memEQ(name, "LDNS_RESOLV_NAMESERVER", 22)) {
    /*                             ^              */
#ifdef LDNS_RESOLV_NAMESERVER
      *iv_return = LDNS_RESOLV_NAMESERVER;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_ERR", 22)) {
    /*                             ^              */
      *iv_return = LDNS_STATUS_SYNTAX_ERR;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_STATUS_SYNTAX_TTL", 22)) {
    /*                             ^              */
      *iv_return = LDNS_STATUS_SYNTAX_TTL;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_RDF_TYPE_TSIGTIME", 22)) {
    /*                             ^              */
      *iv_return = LDNS_RDF_TYPE_TSIGTIME;
      return PERL_constant_ISIV;
    }
    break;
  case 'U':
    if (memEQ(name, "LDNS_SECTION_AUTHORITY", 22)) {
    /*                             ^              */
      *iv_return = LDNS_SECTION_AUTHORITY;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_23 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_RR_TYPE_NSEC3PARAM LDNS_SECTION_ADDITIONAL LDNS_SIGN_RSASHA1_NSEC3
     LDNS_STATUS_ADDRESS_ERR LDNS_STATUS_EMPTY_LABEL LDNS_STATUS_INVALID_B64
     LDNS_STATUS_INVALID_HEX LDNS_STATUS_INVALID_INT LDNS_STATUS_INVALID_IP4
     LDNS_STATUS_INVALID_IP6 LDNS_STATUS_INVALID_STR LDNS_STATUS_NETWORK_ERR */
  /* Offset 21 gives the best switch position.  */
  switch (name[21]) {
  case '6':
    if (memEQ(name, "LDNS_STATUS_INVALID_B64", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_STATUS_INVALID_B64;
      return PERL_constant_ISIV;
    }
    break;
  case 'A':
    if (memEQ(name, "LDNS_RR_TYPE_NSEC3PARAM", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_RR_TYPE_NSEC3PARAM;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_SECTION_ADDITIONAL", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_SECTION_ADDITIONAL;
      return PERL_constant_ISIV;
    }
    break;
  case 'C':
    if (memEQ(name, "LDNS_SIGN_RSASHA1_NSEC3", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_SIGN_RSASHA1_NSEC3;
      return PERL_constant_ISIV;
    }
    break;
  case 'E':
    if (memEQ(name, "LDNS_STATUS_EMPTY_LABEL", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_STATUS_EMPTY_LABEL;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_STATUS_INVALID_HEX", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_STATUS_INVALID_HEX;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_STATUS_INVALID_INT", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_STATUS_INVALID_INT;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_STATUS_INVALID_IP4", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_STATUS_INVALID_IP4;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_STATUS_INVALID_IP6", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_STATUS_INVALID_IP6;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_STATUS_ADDRESS_ERR", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_STATUS_ADDRESS_ERR;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_STATUS_NETWORK_ERR", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_STATUS_NETWORK_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_STATUS_INVALID_STR", 23)) {
    /*                                    ^        */
      *iv_return = LDNS_STATUS_INVALID_STR;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_24 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_RDF_SIZE_DOUBLEWORD LDNS_RDF_TYPE_INT16_DATA LDNS_RDF_TYPE_NSEC3_SALT
     LDNS_RR_TYPE_NSEC3PARAMS LDNS_STATUS_CRYPTO_BOGUS LDNS_STATUS_CRYPTO_NO_DS
     LDNS_STATUS_DDD_OVERFLOW LDNS_STATUS_INTERNAL_ERR LDNS_STATUS_INVALID_TIME
     LDNS_STATUS_SOCKET_ERROR LDNS_STATUS_SYNTAX_EMPTY LDNS_STATUS_UNKNOWN_INET
     */
  /* Offset 17 gives the best switch position.  */
  switch (name[17]) {
  case '1':
    if (memEQ(name, "LDNS_RDF_TYPE_INT16_DATA", 24)) {
    /*                                ^             */
      *iv_return = LDNS_RDF_TYPE_INT16_DATA;
      return PERL_constant_ISIV;
    }
    break;
  case '3':
    if (memEQ(name, "LDNS_RR_TYPE_NSEC3PARAMS", 24)) {
    /*                                ^             */
      *iv_return = LDNS_RR_TYPE_NSEC3PARAMS;
      return PERL_constant_ISIV;
    }
    break;
  case 'B':
    if (memEQ(name, "LDNS_RDF_SIZE_DOUBLEWORD", 24)) {
    /*                                ^             */
#ifdef LDNS_RDF_SIZE_DOUBLEWORD
      *iv_return = LDNS_RDF_SIZE_DOUBLEWORD;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'C':
    if (memEQ(name, "LDNS_RDF_TYPE_NSEC3_SALT", 24)) {
    /*                                ^             */
      *iv_return = LDNS_RDF_TYPE_NSEC3_SALT;
      return PERL_constant_ISIV;
    }
    break;
  case 'I':
    if (memEQ(name, "LDNS_STATUS_INVALID_TIME", 24)) {
    /*                                ^             */
      *iv_return = LDNS_STATUS_INVALID_TIME;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_STATUS_INTERNAL_ERR", 24)) {
    /*                                ^             */
      *iv_return = LDNS_STATUS_INTERNAL_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_BOGUS", 24)) {
    /*                                ^             */
      *iv_return = LDNS_STATUS_CRYPTO_BOGUS;
      return PERL_constant_ISIV;
    }
    if (memEQ(name, "LDNS_STATUS_CRYPTO_NO_DS", 24)) {
    /*                                ^             */
      *iv_return = LDNS_STATUS_CRYPTO_NO_DS;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_STATUS_SOCKET_ERROR", 24)) {
    /*                                ^             */
      *iv_return = LDNS_STATUS_SOCKET_ERROR;
      return PERL_constant_ISIV;
    }
    break;
  case 'V':
    if (memEQ(name, "LDNS_STATUS_DDD_OVERFLOW", 24)) {
    /*                                ^             */
      *iv_return = LDNS_STATUS_DDD_OVERFLOW;
      return PERL_constant_ISIV;
    }
    break;
  case 'W':
    if (memEQ(name, "LDNS_STATUS_UNKNOWN_INET", 24)) {
    /*                                ^             */
      *iv_return = LDNS_STATUS_UNKNOWN_INET;
      return PERL_constant_ISIV;
    }
    break;
  case 'X':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_EMPTY", 24)) {
    /*                                ^             */
      *iv_return = LDNS_STATUS_SYNTAX_EMPTY;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_26 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_STATUS_LABEL_OVERFLOW LDNS_STATUS_SYNTAX_ALG_ERR
     LDNS_STATUS_SYNTAX_INCLUDE LDNS_STATUS_SYNTAX_TTL_ERR */
  /* Offset 20 gives the best switch position.  */
  switch (name[20]) {
  case 'E':
    if (memEQ(name, "LDNS_STATUS_LABEL_OVERFLOW", 26)) {
    /*                                   ^            */
      *iv_return = LDNS_STATUS_LABEL_OVERFLOW;
      return PERL_constant_ISIV;
    }
    break;
  case 'L':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_ALG_ERR", 26)) {
    /*                                   ^            */
      *iv_return = LDNS_STATUS_SYNTAX_ALG_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_INCLUDE", 26)) {
    /*                                   ^            */
      *iv_return = LDNS_STATUS_SYNTAX_INCLUDE;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_TTL_ERR", 26)) {
    /*                                   ^            */
      *iv_return = LDNS_STATUS_SYNTAX_TTL_ERR;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_27 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_NSEC3_VARS_OPTOUT_MASK LDNS_SECTION_ANY_NOQUESTION
     LDNS_STATUS_CRYPTO_NO_RRSIG LDNS_STATUS_CRYPTO_TSIG_ERR
     LDNS_STATUS_INVALID_B32_EXT LDNS_STATUS_INVALID_POINTER
     LDNS_STATUS_PACKET_OVERFLOW LDNS_STATUS_SYNTAX_TYPE_ERR */
  /* Offset 20 gives the best switch position.  */
  switch (name[20]) {
  case 'B':
    if (memEQ(name, "LDNS_STATUS_INVALID_B32_EXT", 27)) {
    /*                                   ^             */
      *iv_return = LDNS_STATUS_INVALID_B32_EXT;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_NO_RRSIG", 27)) {
    /*                                   ^             */
      *iv_return = LDNS_STATUS_CRYPTO_NO_RRSIG;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_STATUS_INVALID_POINTER", 27)) {
    /*                                   ^             */
      *iv_return = LDNS_STATUS_INVALID_POINTER;
      return PERL_constant_ISIV;
    }
    break;
  case 'S':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_TSIG_ERR", 27)) {
    /*                                   ^             */
      *iv_return = LDNS_STATUS_CRYPTO_TSIG_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'U':
    if (memEQ(name, "LDNS_NSEC3_VARS_OPTOUT_MASK", 27)) {
    /*                                   ^             */
#ifdef LDNS_NSEC3_VARS_OPTOUT_MASK
      *iv_return = LDNS_NSEC3_VARS_OPTOUT_MASK;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    if (memEQ(name, "LDNS_SECTION_ANY_NOQUESTION", 27)) {
    /*                                   ^             */
      *iv_return = LDNS_SECTION_ANY_NOQUESTION;
      return PERL_constant_ISIV;
    }
    break;
  case 'V':
    if (memEQ(name, "LDNS_STATUS_PACKET_OVERFLOW", 27)) {
    /*                                   ^             */
      *iv_return = LDNS_STATUS_PACKET_OVERFLOW;
      return PERL_constant_ISIV;
    }
    break;
  case 'Y':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_TYPE_ERR", 27)) {
    /*                                   ^             */
      *iv_return = LDNS_STATUS_SYNTAX_TYPE_ERR;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_28 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_STATUS_CRYPTO_NO_DNSKEY LDNS_STATUS_CRYPTO_VALIDATED
     LDNS_STATUS_SYNTAX_CLASS_ERR LDNS_STATUS_SYNTAX_DNAME_ERR
     LDNS_STATUS_SYNTAX_RDATA_ERR */
  /* Offset 20 gives the best switch position.  */
  switch (name[20]) {
  case 'A':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_VALIDATED", 28)) {
    /*                                   ^              */
      *iv_return = LDNS_STATUS_CRYPTO_VALIDATED;
      return PERL_constant_ISIV;
    }
    break;
  case 'D':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_RDATA_ERR", 28)) {
    /*                                   ^              */
      *iv_return = LDNS_STATUS_SYNTAX_RDATA_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'L':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_CLASS_ERR", 28)) {
    /*                                   ^              */
      *iv_return = LDNS_STATUS_SYNTAX_CLASS_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_DNAME_ERR", 28)) {
    /*                                   ^              */
      *iv_return = LDNS_STATUS_SYNTAX_DNAME_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_NO_DNSKEY", 28)) {
    /*                                   ^              */
      *iv_return = LDNS_STATUS_CRYPTO_NO_DNSKEY;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_30 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_RDF_TYPE_NSEC3_NEXT_OWNER LDNS_STATUS_CERT_BAD_ALGORITHM
     LDNS_STATUS_CRYPTO_SIG_EXPIRED LDNS_STATUS_SYNTAX_KEYWORD_ERR
     LDNS_STATUS_SYNTAX_VERSION_ERR */
  /* Offset 25 gives the best switch position.  */
  switch (name[25]) {
  case 'D':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_KEYWORD_ERR", 30)) {
    /*                                        ^           */
      *iv_return = LDNS_STATUS_SYNTAX_KEYWORD_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'N':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_VERSION_ERR", 30)) {
    /*                                        ^           */
      *iv_return = LDNS_STATUS_SYNTAX_VERSION_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_RDF_TYPE_NSEC3_NEXT_OWNER", 30)) {
    /*                                        ^           */
      *iv_return = LDNS_RDF_TYPE_NSEC3_NEXT_OWNER;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_SIG_EXPIRED", 30)) {
    /*                                        ^           */
      *iv_return = LDNS_STATUS_CRYPTO_SIG_EXPIRED;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_STATUS_CERT_BAD_ALGORITHM", 30)) {
    /*                                        ^           */
      *iv_return = LDNS_STATUS_CERT_BAD_ALGORITHM;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_32 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_STATUS_CRYPTO_ALGO_NOT_IMPL LDNS_STATUS_CRYPTO_NO_TRUSTED_DS
     LDNS_STATUS_DOMAINNAME_UNDERFLOW */
  /* Offset 20 gives the best switch position.  */
  switch (name[20]) {
  case 'L':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_ALGO_NOT_IMPL", 32)) {
    /*                                   ^                  */
      *iv_return = LDNS_STATUS_CRYPTO_ALGO_NOT_IMPL;
      return PERL_constant_ISIV;
    }
    break;
  case 'M':
    if (memEQ(name, "LDNS_STATUS_DOMAINNAME_UNDERFLOW", 32)) {
    /*                                   ^                  */
      *iv_return = LDNS_STATUS_DOMAINNAME_UNDERFLOW;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_NO_TRUSTED_DS", 32)) {
    /*                                   ^                  */
      *iv_return = LDNS_STATUS_CRYPTO_NO_TRUSTED_DS;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_35 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_RDATA_FIELD_DESCRIPTORS_COMMON LDNS_STATUS_CRYPTO_SIG_NOT_INCEPTED
     LDNS_STATUS_CRYPTO_TYPE_COVERED_ERR LDNS_STATUS_DNSSEC_EXISTENCE_DENIED
     LDNS_STATUS_SYNTAX_INTEGER_OVERFLOW */
  /* Offset 21 gives the best switch position.  */
  switch (name[21]) {
  case 'G':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_SIG_NOT_INCEPTED", 35)) {
    /*                                    ^                    */
      *iv_return = LDNS_STATUS_CRYPTO_SIG_NOT_INCEPTED;
      return PERL_constant_ISIV;
    }
    break;
  case 'I':
    if (memEQ(name, "LDNS_STATUS_DNSSEC_EXISTENCE_DENIED", 35)) {
    /*                                    ^                    */
      *iv_return = LDNS_STATUS_DNSSEC_EXISTENCE_DENIED;
      return PERL_constant_ISIV;
    }
    break;
  case 'P':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_TYPE_COVERED_ERR", 35)) {
    /*                                    ^                    */
      *iv_return = LDNS_STATUS_CRYPTO_TYPE_COVERED_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_RDATA_FIELD_DESCRIPTORS_COMMON", 35)) {
    /*                                    ^                    */
#ifdef LDNS_RDATA_FIELD_DESCRIPTORS_COMMON
      *iv_return = LDNS_RDATA_FIELD_DESCRIPTORS_COMMON;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_INTEGER_OVERFLOW", 35)) {
    /*                                    ^                    */
      *iv_return = LDNS_STATUS_SYNTAX_INTEGER_OVERFLOW;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_36 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_STATUS_CRYPTO_NO_TRUSTED_DNSKEY LDNS_STATUS_MISSING_RDATA_FIELDS_KEY
     LDNS_STATUS_SYNTAX_MISSING_VALUE_ERR LDNS_STATUS_WIRE_INCOMPLETE_QUESTION
     */
  /* Offset 14 gives the best switch position.  */
  switch (name[14]) {
  case 'N':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_MISSING_VALUE_ERR", 36)) {
    /*                             ^                            */
      *iv_return = LDNS_STATUS_SYNTAX_MISSING_VALUE_ERR;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_STATUS_WIRE_INCOMPLETE_QUESTION", 36)) {
    /*                             ^                            */
      *iv_return = LDNS_STATUS_WIRE_INCOMPLETE_QUESTION;
      return PERL_constant_ISIV;
    }
    break;
  case 'S':
    if (memEQ(name, "LDNS_STATUS_MISSING_RDATA_FIELDS_KEY", 36)) {
    /*                             ^                            */
      *iv_return = LDNS_STATUS_MISSING_RDATA_FIELDS_KEY;
      return PERL_constant_ISIV;
    }
    break;
  case 'Y':
    if (memEQ(name, "LDNS_STATUS_CRYPTO_NO_TRUSTED_DNSKEY", 36)) {
    /*                             ^                            */
      *iv_return = LDNS_STATUS_CRYPTO_NO_TRUSTED_DNSKEY;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_38 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     LDNS_STATUS_DNSSEC_NSEC_RR_NOT_COVERED
     LDNS_STATUS_MISSING_RDATA_FIELDS_RRSIG
     LDNS_STATUS_SYNTAX_INCLUDE_ERR_NOTIMPL
     LDNS_STATUS_SYNTAX_ITERATIONS_OVERFLOW
     LDNS_STATUS_WIRE_INCOMPLETE_ADDITIONAL */
  /* Offset 20 gives the best switch position.  */
  switch (name[20]) {
  case 'N':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_INCLUDE_ERR_NOTIMPL", 38)) {
    /*                                   ^                        */
      *iv_return = LDNS_STATUS_SYNTAX_INCLUDE_ERR_NOTIMPL;
      return PERL_constant_ISIV;
    }
    break;
  case 'O':
    if (memEQ(name, "LDNS_STATUS_WIRE_INCOMPLETE_ADDITIONAL", 38)) {
    /*                                   ^                        */
      *iv_return = LDNS_STATUS_WIRE_INCOMPLETE_ADDITIONAL;
      return PERL_constant_ISIV;
    }
    break;
  case 'R':
    if (memEQ(name, "LDNS_STATUS_MISSING_RDATA_FIELDS_RRSIG", 38)) {
    /*                                   ^                        */
      *iv_return = LDNS_STATUS_MISSING_RDATA_FIELDS_RRSIG;
      return PERL_constant_ISIV;
    }
    break;
  case 'S':
    if (memEQ(name, "LDNS_STATUS_DNSSEC_NSEC_RR_NOT_COVERED", 38)) {
    /*                                   ^                        */
      *iv_return = LDNS_STATUS_DNSSEC_NSEC_RR_NOT_COVERED;
      return PERL_constant_ISIV;
    }
    break;
  case 'T':
    if (memEQ(name, "LDNS_STATUS_SYNTAX_ITERATIONS_OVERFLOW", 38)) {
    /*                                   ^                        */
      *iv_return = LDNS_STATUS_SYNTAX_ITERATIONS_OVERFLOW;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant (pTHX_ const char *name, STRLEN len, IV *iv_return) {
  /* Initially switch on the length of the name.  */
  /* When generated this function returned values for the list of names given
     in this section of perl code.  Rather than manually editing these functions
     to add or remove constants, which would result in this comment and section
     of code becoming inaccurate, we recommend that you edit this section of
     code, and use it to regenerate a new set of constant functions which you
     then use to replace the originals.

     Regenerate these constant functions by feeding this entire source file to
     perl -x

#!/usr/bin/perl -w
use ExtUtils::Constant qw (constant_types C_constant XS_constant);

my $types = {map {($_, 1)} qw(IV)};
my @names = (qw(LDNS_AA LDNS_AD LDNS_CD LDNS_DEFAULT_TTL LDNS_IP4ADDRLEN
	       LDNS_IP6ADDRLEN LDNS_KEY_REVOKE_KEY LDNS_KEY_SEP_KEY
	       LDNS_KEY_ZONE_KEY LDNS_MAX_DOMAINLEN LDNS_MAX_LABELLEN
	       LDNS_MAX_PACKETLEN LDNS_MAX_POINTERS LDNS_MAX_RDFLEN
	       LDNS_NSEC3_VARS_OPTOUT_MASK LDNS_PORT LDNS_QR LDNS_RA LDNS_RD
	       LDNS_RDATA_FIELD_DESCRIPTORS_COMMON LDNS_RDF_SIZE_16BYTES
	       LDNS_RDF_SIZE_6BYTES LDNS_RDF_SIZE_BYTE LDNS_RDF_SIZE_DOUBLEWORD
	       LDNS_RDF_SIZE_WORD LDNS_RESOLV_ANCHOR LDNS_RESOLV_DEFDOMAIN
	       LDNS_RESOLV_INET LDNS_RESOLV_INET6 LDNS_RESOLV_INETANY
	       LDNS_RESOLV_KEYWORD LDNS_RESOLV_KEYWORDS LDNS_RESOLV_NAMESERVER
	       LDNS_RESOLV_OPTIONS LDNS_RESOLV_RTT_INF LDNS_RESOLV_RTT_MIN
	       LDNS_RESOLV_SEARCH LDNS_RESOLV_SORTLIST LDNS_RR_OVERHEAD
	       LDNS_TC),
            {name=>"LDNS_CERT_ACPKIX", type=>"IV", macro=>"1"},
            {name=>"LDNS_CERT_IACPKIX", type=>"IV", macro=>"1"},
            {name=>"LDNS_CERT_IPGP", type=>"IV", macro=>"1"},
            {name=>"LDNS_CERT_IPKIX", type=>"IV", macro=>"1"},
            {name=>"LDNS_CERT_ISPKI", type=>"IV", macro=>"1"},
            {name=>"LDNS_CERT_OID", type=>"IV", macro=>"1"},
            {name=>"LDNS_CERT_PGP", type=>"IV", macro=>"1"},
            {name=>"LDNS_CERT_PKIX", type=>"IV", macro=>"1"},
            {name=>"LDNS_CERT_SPKI", type=>"IV", macro=>"1"},
            {name=>"LDNS_CERT_URI", type=>"IV", macro=>"1"},
            {name=>"LDNS_DH", type=>"IV", macro=>"1"},
            {name=>"LDNS_DSA", type=>"IV", macro=>"1"},
            {name=>"LDNS_DSA_NSEC3", type=>"IV", macro=>"1"},
            {name=>"LDNS_ECC", type=>"IV", macro=>"1"},
            {name=>"LDNS_ECC_GOST", type=>"IV", macro=>"1"},
            {name=>"LDNS_ECDSAP384SHA384", type=>"IV", macro=>"1"},
            {name=>"LDNS_HASH_GOST", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_ANSWER", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_IQUERY", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_NODATA", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_NOTIFY", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_NXDOMAIN", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_QUERY", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_QUESTION", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_REFERRAL", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_STATUS", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_UNKNOWN", type=>"IV", macro=>"1"},
            {name=>"LDNS_PACKET_UPDATE", type=>"IV", macro=>"1"},
            {name=>"LDNS_PRIVATEDNS", type=>"IV", macro=>"1"},
            {name=>"LDNS_PRIVATEOID", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_FORMERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_NOERROR", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_NOTAUTH", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_NOTIMPL", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_NOTZONE", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_NXDOMAIN", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_NXRRSET", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_REFUSED", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_SERVFAIL", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_YXDOMAIN", type=>"IV", macro=>"1"},
            {name=>"LDNS_RCODE_YXRRSET", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_A", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_AAAA", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_ALG", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_APL", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_ATMA", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_B32_EXT", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_B64", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_CERT_ALG", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_CLASS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_DNAME", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_HEX", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_INT16", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_INT16_DATA", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_INT32", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_INT8", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_IPSECKEY", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_LOC", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_NONE", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_NSAP", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_NSEC", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_NSEC3_NEXT_OWNER", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_NSEC3_SALT", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_PERIOD", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_SERVICE", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_STR", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_TIME", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_HIP", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_TSIGTIME", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_TYPE", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_UNKNOWN", type=>"IV", macro=>"1"},
            {name=>"LDNS_RDF_TYPE_WKS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_CLASS_ANY", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_CLASS_CH", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_CLASS_COUNT", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_CLASS_FIRST", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_CLASS_HS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_CLASS_IN", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_CLASS_LAST", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_CLASS_NONE", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_COMPRESS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_NO_COMPRESS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_A", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_A6", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_AAAA", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_AFSDB", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_ANY", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_APL", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_ATMA", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_AXFR", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_CERT", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_CNAME", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_COUNT", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_DHCID", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_DLV", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_DNAME", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_DNSKEY", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_DS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_EID", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_FIRST", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_GID", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_GPOS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_HINFO", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_IPSECKEY", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_ISDN", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_IXFR", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_KEY", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_KX", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_LAST", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_LOC", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_MAILA", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_MAILB", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_MB", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_MD", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_MF", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_MG", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_MINFO", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_MR", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_MX", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NAPTR", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NIMLOC", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NSAP", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NSAP_PTR", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NSEC", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NSEC3", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NSEC3PARAM", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NSEC3PARAMS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NULL", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_NXT", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_OPT", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_PTR", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_PX", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_RP", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_RRSIG", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_RT", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_SIG", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_SINK", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_SOA", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_SPF", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_SRV", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_SSHFP", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_TALINK", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_TSIG", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_TXT", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_UID", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_UINFO", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_UNSPEC", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_WKS", type=>"IV", macro=>"1"},
            {name=>"LDNS_RR_TYPE_X25", type=>"IV", macro=>"1"},
            {name=>"LDNS_RSAMD5", type=>"IV", macro=>"1"},
            {name=>"LDNS_RSASHA1", type=>"IV", macro=>"1"},
            {name=>"LDNS_RSASHA1_NSEC3", type=>"IV", macro=>"1"},
            {name=>"LDNS_RSASHA256", type=>"IV", macro=>"1"},
            {name=>"LDNS_RSASHA512", type=>"IV", macro=>"1"},
            {name=>"LDNS_SECTION_ADDITIONAL", type=>"IV", macro=>"1"},
            {name=>"LDNS_SECTION_ANSWER", type=>"IV", macro=>"1"},
            {name=>"LDNS_SECTION_ANY", type=>"IV", macro=>"1"},
            {name=>"LDNS_SECTION_ANY_NOQUESTION", type=>"IV", macro=>"1"},
            {name=>"LDNS_SECTION_AUTHORITY", type=>"IV", macro=>"1"},
            {name=>"LDNS_SECTION_QUESTION", type=>"IV", macro=>"1"},
            {name=>"LDNS_SHA1", type=>"IV", macro=>"1"},
            {name=>"LDNS_SHA256", type=>"IV", macro=>"1"},
            {name=>"LDNS_SHA384", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_DSA", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_DSA_NSEC3", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_ECC_GOST", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_ECDSAP384SHA384", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_HMACSHA1", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_HMACSHA256", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_RSAMD5", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_RSASHA1", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_RSASHA1_NSEC3", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_RSASHA256", type=>"IV", macro=>"1"},
            {name=>"LDNS_SIGN_RSASHA512", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_ADDRESS_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CERT_BAD_ALGORITHM", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_ALGO_NOT_IMPL", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_BOGUS", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_EXPIRATION_BEFORE_INCEPTION", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_NO_DNSKEY", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_NO_DS", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_NO_RRSIG", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_NO_TRUSTED_DNSKEY", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_NO_TRUSTED_DS", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_SIG_EXPIRED", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_SIG_NOT_INCEPTED", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_TSIG_BOGUS", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_TSIG_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_TYPE_COVERED_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_UNKNOWN_ALGO", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_CRYPTO_VALIDATED", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_DDD_OVERFLOW", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_DNSSEC_EXISTENCE_DENIED", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_DNSSEC_NSEC3_ORIGINAL_NOT_FOUND", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_DNSSEC_NSEC_RR_NOT_COVERED", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_DNSSEC_NSEC_WILDCARD_NOT_COVERED", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_DOMAINNAME_OVERFLOW", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_DOMAINNAME_UNDERFLOW", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_EMPTY_LABEL", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_ENGINE_KEY_NOT_LOADED", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_FILE_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INTERNAL_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INVALID_B32_EXT", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INVALID_B64", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INVALID_HEX", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INVALID_INT", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INVALID_IP4", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INVALID_IP6", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INVALID_POINTER", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INVALID_STR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_INVALID_TIME", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_LABEL_OVERFLOW", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_MEM_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_MISSING_RDATA_FIELDS_KEY", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_MISSING_RDATA_FIELDS_RRSIG", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_NETWORK_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_NOT_IMPL", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_NO_DATA", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_NSEC3_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_NULL", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_OK", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_PACKET_OVERFLOW", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_RES_NO_NS", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_RES_QUERY", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SOCKET_ERROR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SSL_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_ALG_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_BAD_ESCAPE", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_CLASS_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_DNAME_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_EMPTY", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_INCLUDE", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_INCLUDE_ERR_NOTIMPL", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_INTEGER_OVERFLOW", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_ITERATIONS_OVERFLOW", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_KEYWORD_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_MISSING_VALUE_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_ORIGIN", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_RDATA_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_TTL", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_TTL_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_TYPE_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_SYNTAX_VERSION_ERR", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_UNKNOWN_INET", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_WIRE_INCOMPLETE_ADDITIONAL", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_WIRE_INCOMPLETE_ANSWER", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_WIRE_INCOMPLETE_AUTHORITY", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_WIRE_INCOMPLETE_HEADER", type=>"IV", macro=>"1"},
            {name=>"LDNS_STATUS_WIRE_INCOMPLETE_QUESTION", type=>"IV", macro=>"1"},
            {name=>"endif", type=>"IV", macro=>"1"},
            {name=>"if", type=>"IV", macro=>"1"});

print constant_types(), "\n"; # macro defs
foreach (C_constant ("LDNS", 'constant', 'IV', $types, undef, 3, @names) ) {
    print $_, "\n"; # C constant subs
}
print "\n#### XS Section:\n";
print XS_constant ("LDNS", $types);
__END__
   */

  switch (len) {
  case 2:
    if (name[0] == 'i' && name[1] == 'f') {
      *iv_return = if;
      return PERL_constant_ISIV;
    }
    break;
  case 5:
    if (memEQ(name, "endif", 5)) {
      *iv_return = endif;
      return PERL_constant_ISIV;
    }
    break;
  case 7:
    return constant_7 (aTHX_ name, iv_return);
    break;
  case 8:
    /* Names all of length 8.  */
    /* LDNS_DSA LDNS_ECC */
    /* Offset 5 gives the best switch position.  */
    switch (name[5]) {
    case 'D':
      if (memEQ(name, "LDNS_DSA", 8)) {
      /*                    ^        */
        *iv_return = LDNS_DSA;
        return PERL_constant_ISIV;
      }
      break;
    case 'E':
      if (memEQ(name, "LDNS_ECC", 8)) {
      /*                    ^        */
        *iv_return = LDNS_ECC;
        return PERL_constant_ISIV;
      }
      break;
    }
    break;
  case 9:
    /* Names all of length 9.  */
    /* LDNS_PORT LDNS_SHA1 */
    /* Offset 5 gives the best switch position.  */
    switch (name[5]) {
    case 'P':
      if (memEQ(name, "LDNS_PORT", 9)) {
      /*                    ^         */
#ifdef LDNS_PORT
        *iv_return = LDNS_PORT;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    case 'S':
      if (memEQ(name, "LDNS_SHA1", 9)) {
      /*                    ^         */
        *iv_return = LDNS_SHA1;
        return PERL_constant_ISIV;
      }
      break;
    }
    break;
  case 11:
    return constant_11 (aTHX_ name, iv_return);
    break;
  case 12:
    if (memEQ(name, "LDNS_RSASHA1", 12)) {
      *iv_return = LDNS_RSASHA1;
      return PERL_constant_ISIV;
    }
    break;
  case 13:
    return constant_13 (aTHX_ name, iv_return);
    break;
  case 14:
    return constant_14 (aTHX_ name, iv_return);
    break;
  case 15:
    return constant_15 (aTHX_ name, iv_return);
    break;
  case 16:
    return constant_16 (aTHX_ name, iv_return);
    break;
  case 17:
    return constant_17 (aTHX_ name, iv_return);
    break;
  case 18:
    return constant_18 (aTHX_ name, iv_return);
    break;
  case 19:
    return constant_19 (aTHX_ name, iv_return);
    break;
  case 20:
    return constant_20 (aTHX_ name, iv_return);
    break;
  case 21:
    return constant_21 (aTHX_ name, iv_return);
    break;
  case 22:
    return constant_22 (aTHX_ name, iv_return);
    break;
  case 23:
    return constant_23 (aTHX_ name, iv_return);
    break;
  case 24:
    return constant_24 (aTHX_ name, iv_return);
    break;
  case 25:
    /* Names all of length 25.  */
    /* LDNS_SIGN_ECDSAP384SHA384 LDNS_STATUS_SYNTAX_ORIGIN */
    /* Offset 15 gives the best switch position.  */
    switch (name[15]) {
    case 'P':
      if (memEQ(name, "LDNS_SIGN_ECDSAP384SHA384", 25)) {
      /*                              ^                */
        *iv_return = LDNS_SIGN_ECDSAP384SHA384;
        return PERL_constant_ISIV;
      }
      break;
    case 'T':
      if (memEQ(name, "LDNS_STATUS_SYNTAX_ORIGIN", 25)) {
      /*                              ^                */
        *iv_return = LDNS_STATUS_SYNTAX_ORIGIN;
        return PERL_constant_ISIV;
      }
      break;
    }
    break;
  case 26:
    return constant_26 (aTHX_ name, iv_return);
    break;
  case 27:
    return constant_27 (aTHX_ name, iv_return);
    break;
  case 28:
    return constant_28 (aTHX_ name, iv_return);
    break;
  case 29:
    /* Names all of length 29.  */
    /* LDNS_STATUS_CRYPTO_TSIG_BOGUS LDNS_STATUS_SYNTAX_BAD_ESCAPE */
    /* Offset 15 gives the best switch position.  */
    switch (name[15]) {
    case 'P':
      if (memEQ(name, "LDNS_STATUS_CRYPTO_TSIG_BOGUS", 29)) {
      /*                              ^                    */
        *iv_return = LDNS_STATUS_CRYPTO_TSIG_BOGUS;
        return PERL_constant_ISIV;
      }
      break;
    case 'T':
      if (memEQ(name, "LDNS_STATUS_SYNTAX_BAD_ESCAPE", 29)) {
      /*                              ^                    */
        *iv_return = LDNS_STATUS_SYNTAX_BAD_ESCAPE;
        return PERL_constant_ISIV;
      }
      break;
    }
    break;
  case 30:
    return constant_30 (aTHX_ name, iv_return);
    break;
  case 31:
    /* Names all of length 31.  */
    /* LDNS_STATUS_CRYPTO_UNKNOWN_ALGO LDNS_STATUS_DOMAINNAME_OVERFLOW */
    /* Offset 12 gives the best switch position.  */
    switch (name[12]) {
    case 'C':
      if (memEQ(name, "LDNS_STATUS_CRYPTO_UNKNOWN_ALGO", 31)) {
      /*                           ^                         */
        *iv_return = LDNS_STATUS_CRYPTO_UNKNOWN_ALGO;
        return PERL_constant_ISIV;
      }
      break;
    case 'D':
      if (memEQ(name, "LDNS_STATUS_DOMAINNAME_OVERFLOW", 31)) {
      /*                           ^                         */
        *iv_return = LDNS_STATUS_DOMAINNAME_OVERFLOW;
        return PERL_constant_ISIV;
      }
      break;
    }
    break;
  case 32:
    return constant_32 (aTHX_ name, iv_return);
    break;
  case 33:
    if (memEQ(name, "LDNS_STATUS_ENGINE_KEY_NOT_LOADED", 33)) {
      *iv_return = LDNS_STATUS_ENGINE_KEY_NOT_LOADED;
      return PERL_constant_ISIV;
    }
    break;
  case 34:
    /* Names all of length 34.  */
    /* LDNS_STATUS_WIRE_INCOMPLETE_ANSWER LDNS_STATUS_WIRE_INCOMPLETE_HEADER */
    /* Offset 28 gives the best switch position.  */
    switch (name[28]) {
    case 'A':
      if (memEQ(name, "LDNS_STATUS_WIRE_INCOMPLETE_ANSWER", 34)) {
      /*                                           ^            */
        *iv_return = LDNS_STATUS_WIRE_INCOMPLETE_ANSWER;
        return PERL_constant_ISIV;
      }
      break;
    case 'H':
      if (memEQ(name, "LDNS_STATUS_WIRE_INCOMPLETE_HEADER", 34)) {
      /*                                           ^            */
        *iv_return = LDNS_STATUS_WIRE_INCOMPLETE_HEADER;
        return PERL_constant_ISIV;
      }
      break;
    }
    break;
  case 35:
    return constant_35 (aTHX_ name, iv_return);
    break;
  case 36:
    return constant_36 (aTHX_ name, iv_return);
    break;
  case 37:
    if (memEQ(name, "LDNS_STATUS_WIRE_INCOMPLETE_AUTHORITY", 37)) {
      *iv_return = LDNS_STATUS_WIRE_INCOMPLETE_AUTHORITY;
      return PERL_constant_ISIV;
    }
    break;
  case 38:
    return constant_38 (aTHX_ name, iv_return);
    break;
  case 43:
    if (memEQ(name, "LDNS_STATUS_DNSSEC_NSEC3_ORIGINAL_NOT_FOUND", 43)) {
      *iv_return = LDNS_STATUS_DNSSEC_NSEC3_ORIGINAL_NOT_FOUND;
      return PERL_constant_ISIV;
    }
    break;
  case 44:
    /* Names all of length 44.  */
    /* LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY
       LDNS_STATUS_DNSSEC_NSEC_WILDCARD_NOT_COVERED */
    /* Offset 12 gives the best switch position.  */
    switch (name[12]) {
    case 'C':
      if (memEQ(name, "LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY", 44)) {
      /*                           ^                                      */
        *iv_return = LDNS_STATUS_CRYPTO_NO_MATCHING_KEYTAG_DNSKEY;
        return PERL_constant_ISIV;
      }
      break;
    case 'D':
      if (memEQ(name, "LDNS_STATUS_DNSSEC_NSEC_WILDCARD_NOT_COVERED", 44)) {
      /*                           ^                                      */
        *iv_return = LDNS_STATUS_DNSSEC_NSEC_WILDCARD_NOT_COVERED;
        return PERL_constant_ISIV;
      }
      break;
    }
    break;
  case 46:
    if (memEQ(name, "LDNS_STATUS_CRYPTO_EXPIRATION_BEFORE_INCEPTION", 46)) {
      *iv_return = LDNS_STATUS_CRYPTO_EXPIRATION_BEFORE_INCEPTION;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

