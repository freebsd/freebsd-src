/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef __KRBASN1_H__
#define __KRBASN1_H__

#include "k5-int.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>             /* For INT_MAX */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/*
 * If KRB5_MSGTYPE_STRICT is defined, then be strict about checking
 * the msgtype fields.  Unfortunately, there old versions of Kerberos
 * don't set these fields correctly, so we have to make allowances for
 * them.
 */
/* #define KRB5_MSGTYPE_STRICT */

/*
 * If KRB5_GENEROUS_LR_TYPE is defined, then we are generous about
 * accepting a one byte negative lr_type - which is not sign
 * extended. Prior to July 2000, we were sending a negative lr_type as
 * a positve single byte value - instead of a signed integer. This
 * allows us to receive the old value and deal
 */
#define KRB5_GENEROUS_LR_TYPE

typedef krb5_octet asn1_octet;
typedef krb5_error_code asn1_error_code;

typedef enum { PRIMITIVE = 0x00, CONSTRUCTED = 0x20 } asn1_construction;

typedef enum { UNIVERSAL = 0x00, APPLICATION = 0x40,
               CONTEXT_SPECIFIC = 0x80, PRIVATE = 0xC0 } asn1_class;

typedef int asn1_tagnum;
#define ASN1_TAGNUM_CEILING INT_MAX
#define ASN1_TAGNUM_MAX (ASN1_TAGNUM_CEILING-1)

/* This is Kerberos Version 5 */
#define KVNO 5

/* Universal Tag Numbers */
#define ASN1_BOOLEAN            1
#define ASN1_INTEGER            2
#define ASN1_BITSTRING          3
#define ASN1_OCTETSTRING        4
#define ASN1_NULL               5
#define ASN1_OBJECTIDENTIFIER   6
#define ASN1_ENUMERATED         10
#define ASN1_UTF8STRING         12
#define ASN1_SEQUENCE           16
#define ASN1_SET                17
#define ASN1_PRINTABLESTRING    19
#define ASN1_IA5STRING          22
#define ASN1_UTCTIME            23
#define ASN1_GENERALTIME        24
#define ASN1_GENERALSTRING      27

/* Kerberos Message Types */
#define ASN1_KRB_AS_REQ         10
#define ASN1_KRB_AS_REP         11
#define ASN1_KRB_TGS_REQ        12
#define ASN1_KRB_TGS_REP        13
#define ASN1_KRB_AP_REQ         14
#define ASN1_KRB_AP_REP         15
#define ASN1_KRB_SAFE           20
#define ASN1_KRB_PRIV           21
#define ASN1_KRB_CRED           22
#define ASN1_KRB_ERROR          30

#endif
