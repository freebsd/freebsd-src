/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 1992,1993 Trusted Information Systems, Inc.
 *
 * Permission to include this software in the Kerberos V5 distribution
 * was graciously provided by Trusted Information Systems.
 *
 * Trusted Information Systems makes no representation about the
 * suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 */
/*
 * Copyright (C) 1994 Massachusetts Institute of Technology
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*****************************************************************************
 * trval.c.c
 *****************************************************************************/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define OK 0
#define NOTOK (-1)

/* IDENTIFIER OCTET = TAG CLASS | FORM OF ENCODING | TAG NUMBER */

/* TAG CLASSES */
#define ID_CLASS   0xc0         /* bits 8 and 7 */
#define CLASS_UNIV 0x00         /* 0 = universal */
#define CLASS_APPL 0x40         /* 1 = application */
#define CLASS_CONT 0x80         /* 2 = context-specific */
#define CLASS_PRIV 0xc0         /* 3 = private */

/* FORM OF ENCODING */
#define ID_FORM   0x20          /* bit 6 */
#define FORM_PRIM 0x00          /* 0 = primitive */
#define FORM_CONS 0x20          /* 1 = constructed */

/* TAG NUMBERS */
#define ID_TAG    0x1f          /* bits 5-1 */
#define PRIM_BOOL 0x01          /* Boolean */
#define PRIM_INT  0x02          /* Integer */
#define PRIM_BITS 0x03          /* Bit String */
#define PRIM_OCTS 0x04          /* Octet String */
#define PRIM_NULL 0x05          /* Null */
#define PRIM_OID  0x06          /* Object Identifier */
#define PRIM_ODE  0x07          /* Object Descriptor */
#define CONS_EXTN 0x08          /* External */
#define PRIM_REAL 0x09          /* Real */
#define PRIM_ENUM 0x0a          /* Enumerated type */
#define PRIM_ENCR 0x0b          /* Encrypted */
#define CONS_SEQ  0x10          /* SEQUENCE/SEQUENCE OF */
#define CONS_SET  0x11          /* SET/SET OF */
#define DEFN_NUMS 0x12          /* Numeric String */
#define DEFN_PRTS 0x13          /* Printable String */
#define DEFN_T61S 0x14          /* T.61 String */
#define DEFN_VTXS 0x15          /* Videotex String */
#define DEFN_IA5S 0x16          /* IA5 String */
#define DEFN_UTCT 0x17          /* UTCTime */
#define DEFN_GENT 0x18          /* Generalized Time */
#define DEFN_GFXS 0x19          /* Graphics string (ISO2375) */
#define DEFN_VISS 0x1a          /* Visible string */
#define DEFN_GENS 0x1b          /* General string */
#define DEFN_CHRS 0x1c          /* Character string */

#define LEN_XTND        0x80    /* long or indefinite form */
#define LEN_SMAX        127     /* largest short form */
#define LEN_MASK        0x7f    /* mask to get number of bytes in length */
#define LEN_INDF        (-1)    /* indefinite length */

#define KRB5    /* Do krb5 application types */

int print_types = 0;
int print_id_and_len = 1;
int print_constructed_length = 1;
int print_primitive_length = 1;
int print_skip_context = 0;
int print_skip_tagnum = 1;
int print_context_shortcut = 0;
int do_hex = 0;
#ifdef KRB5
int print_krb5_types = 0;
#endif

int current_appl_type = -1;

int decode_len (FILE *, unsigned char *, int);
int do_prim (FILE *, int, unsigned char *, int, int);
int do_cons (FILE *, unsigned char *, int, int, int *);
int do_prim_bitstring (FILE *, int, unsigned char *, int, int);
int do_prim_int (FILE *, int, unsigned char *, int, int);
int do_prim_string (FILE *, int, unsigned char *, int, int);
void print_tag_type (FILE *, int, int);
int trval (FILE *, FILE *);
int trval2 (FILE *, unsigned char *, int, int, int *);


/****************************************************************************/

static int convert_nibble(int ch)
{
    if (isdigit(ch))
        return (ch - '0');
    if (ch >= 'a' && ch <= 'f')
        return (ch - 'a' + 10);
    if (ch >= 'A' && ch <= 'F')
        return (ch - 'A' + 10);
    return -1;
}

int trval(fin, fout)
    FILE        *fin;
    FILE        *fout;
{
    unsigned char *p;
    unsigned int maxlen;
    int len;
    int cc, cc2, n1, n2;
    int r;
    int rlen;

    maxlen = BUFSIZ;
    p = (unsigned char *)malloc(maxlen);
    len = 0;
    while ((cc = fgetc(fin)) != EOF) {
        if ((unsigned int) len == maxlen) {
            maxlen += BUFSIZ;
            p = (unsigned char *)realloc(p, maxlen);
        }
        if (do_hex) {
            if (cc == ' ' || cc == '\n' || cc == '\t')
                continue;
            cc2 = fgetc(fin);
            if (cc2 == EOF)
                break;
            n1 = convert_nibble(cc);
            n2 = convert_nibble(cc2);
            cc = (n1 << 4) + n2;
        }
        p[len++] = cc;
    }
    fprintf(fout, "<%d>", len);
    r = trval2(fout, p, len, 0, &rlen);
    fprintf(fout, "\n");
    (void) free(p);
    return(r);
}

int trval2(fp, enc, len, lev, rlen)
    FILE *fp;
    unsigned char *enc;
    int len;
    int lev;
    int *rlen;
{
    int l, eid, elen, xlen, r, rlen2 = 0;
    int rlen_ext = 0;

    r = OK;
    *rlen = -1;

    if (len < 2) {
        fprintf(fp, "missing id and length octets (%d)\n", len);
        return(NOTOK);
    }

    fprintf(fp, "\n");
    for (l=0; l<lev; l++) fprintf(fp, ".  ");

context_restart:
    eid = enc[0];
    elen = enc[1];

    if (print_id_and_len) {
        fprintf(fp, "%02x ", eid);
        fprintf(fp, "%02x ", elen);
    }

    if (elen == LEN_XTND) {
        fprintf(fp,
                "indefinite length encoding not implemented (0x%02x)\n", elen);
        return(NOTOK);
    }

    xlen = 0;
    if (elen & LEN_XTND) {
        xlen = elen & LEN_MASK;
        if (xlen > len - 2) {
            fprintf(fp, "extended length too long (%d > %d - 2)\n", xlen, len);
            return(NOTOK);
        }
        elen = decode_len(fp, enc+2, xlen);
    }

    if (elen > len - 2 - xlen) {
        fprintf(fp, "length too long (%d > %d - 2 - %d)\n", elen, len, xlen);
        return(NOTOK);
    }

    print_tag_type(fp, eid, lev);

    if (print_context_shortcut && (eid & ID_CLASS) == CLASS_CONT &&
        (eid & ID_FORM) == FORM_CONS && lev > 0) {
        rlen_ext += 2 + xlen;
        enc += 2 + xlen;
        fprintf(fp, " ");
        goto context_restart;
    }

    switch(eid & ID_FORM) {
    case FORM_PRIM:
        r = do_prim(fp, eid & ID_TAG, enc+2+xlen, elen, lev+1);
        *rlen = 2 + xlen + elen + rlen_ext;
        break;
    case FORM_CONS:
        if (print_constructed_length) {
            fprintf(fp, " constr");
            fprintf(fp, " <%d>", elen);
        }
        r = do_cons(fp, enc+2+xlen, elen, lev+1, &rlen2);
        *rlen = 2 + xlen + rlen2 + rlen_ext;
        break;
    }

    return(r);
}

int decode_len(fp, enc, len)
    FILE *fp;
    unsigned char *enc;
    int len;
{
    int rlen;
    int i;

    if (print_id_and_len)
        fprintf(fp, "%02x ", enc[0]);
    rlen = enc[0];
    for (i=1; i<len; i++) {
        if (print_id_and_len)
            fprintf(fp, "%02x ", enc[i]);
        rlen = (rlen * 0x100) + enc[i];
    }
    return(rlen);
}

/*
 * This is the printing function for bit strings
 */
int do_prim_bitstring(fp, tag, enc, len, lev)
    FILE *fp;
    int tag;
    unsigned char *enc;
    int len;
    int lev;
{
    int i;
    long        num = 0;

    if (tag != PRIM_BITS || len > 5)
        return 0;

    for (i=1; i < len; i++) {
        num = num << 8;
        num += enc[i];
    }

    fprintf(fp, " 0x%lx", num);
    if (enc[0])
        fprintf(fp, " (%d unused bits)", enc[0]);
    return 1;
}

/*
 * This is the printing function for integers
 */
int do_prim_int(fp, tag, enc, len, lev)
    FILE *fp;
    int tag;
    unsigned char *enc;
    int len;
    int lev;
{
    int i;
    long        num = 0;

    if (tag != PRIM_INT || len > 4)
        return 0;

    if (enc[0] & 0x80)
        num = -1;

    for (i=0; i < len; i++) {
        num = num << 8;
        num += enc[i];
    }

    fprintf(fp, " %ld", num);
    return 1;
}


/*
 * This is the printing function which we use if it's a string or
 * other other type which is best printed as a string
 */
int do_prim_string(fp, tag, enc, len, lev)
    FILE *fp;
    int tag;
    unsigned char *enc;
    int len;
    int lev;
{
    int i;

    /*
     * Only try this printing function with "reasonable" types
     */
    if ((tag < DEFN_NUMS) && (tag != PRIM_OCTS))
        return 0;

    for (i=0; i < len; i++)
        if (!isprint(enc[i]))
            return 0;
    fprintf(fp, " \"%.*s\"", len, enc);
    return 1;
}

int do_prim(fp, tag, enc, len, lev)
    FILE *fp;
    int tag;
    unsigned char *enc;
    int len;
    int lev;
{
    int n;
    int i;
    int j;
    int width;

    if (do_prim_string(fp, tag, enc, len, lev))
        return OK;
    if (do_prim_int(fp, tag, enc, len, lev))
        return OK;
    if (do_prim_bitstring(fp, tag, enc, len, lev))
        return OK;

    if (print_primitive_length)
        fprintf(fp, " <%d>", len);

    width = (80 - (lev * 3) - 8) / 4;

    for (n = 0; n < len; n++) {
        if ((n % width) == 0) {
            fprintf(fp, "\n");
            for (i=0; i<lev; i++) fprintf(fp, "   ");
        }
        fprintf(fp, "%02x ", enc[n]);
        if ((n % width) == (width-1)) {
            fprintf(fp, "    ");
            for (i=n-(width-1); i<=n; i++)
                if (isprint(enc[i])) fprintf(fp, "%c", enc[i]);
                else fprintf(fp, ".");
        }
    }
    if ((j = (n % width)) != 0) {
        fprintf(fp, "    ");
        for (i=0; i<width-j; i++) fprintf(fp, "   ");
        for (i=n-j; i<n; i++)
            if (isprint(enc[i])) fprintf(fp, "%c", enc[i]);
            else fprintf(fp, ".");
    }
    return(OK);
}

int do_cons(fp, enc, len, lev, rlen)
    FILE *fp;
    unsigned char *enc;
    int len;
    int lev;
    int *rlen;
{
    int n;
    int r = 0;
    int rlen2;
    int rlent;
    int save_appl;

    save_appl = current_appl_type;
    for (n = 0, rlent = 0; n < len; n+=rlen2, rlent+=rlen2) {
        r = trval2(fp, enc+n, len-n, lev, &rlen2);
        current_appl_type = save_appl;
        if (r != OK) return(r);
    }
    if (rlent != len) {
        fprintf(fp, "inconsistent constructed lengths (%d != %d)\n",
                rlent, len);
        return(NOTOK);
    }
    *rlen = rlent;
    return(r);
}

struct typestring_table {
    int k1, k2;
    char        *str;
    int new_appl;
};

static char *lookup_typestring(table, key1, key2)
    struct typestring_table *table;
    int key1, key2;
{
    struct typestring_table *ent;

    for (ent = table; ent->k1 > 0; ent++) {
        if ((ent->k1 == key1) &&
            (ent->k2 == key2)) {
            if (ent->new_appl)
                current_appl_type = ent->new_appl;
            return ent->str;
        }
    }
    return 0;
}


struct typestring_table univ_types[] = {
    { PRIM_BOOL, -1, "Boolean"},
    { PRIM_INT, -1, "Integer"},
    { PRIM_BITS, -1, "Bit String"},
    { PRIM_OCTS, -1, "Octet String"},
    { PRIM_NULL, -1, "Null"},
    { PRIM_OID, -1, "Object Identifier"},
    { PRIM_ODE, -1, "Object Descriptor"},
    { CONS_EXTN, -1, "External"},
    { PRIM_REAL, -1, "Real"},
    { PRIM_ENUM, -1, "Enumerated type"},
    { PRIM_ENCR, -1, "Encrypted"},
    { CONS_SEQ, -1, "Sequence/Sequence Of"},
    { CONS_SET, -1, "Set/Set Of"},
    { DEFN_NUMS, -1, "Numeric String"},
    { DEFN_PRTS, -1, "Printable String"},
    { DEFN_T61S, -1, "T.61 String"},
    { DEFN_VTXS, -1, "Videotex String"},
    { DEFN_IA5S, -1, "IA5 String"},
    { DEFN_UTCT, -1, "UTCTime"},
    { DEFN_GENT, -1, "Generalized Time"},
    { DEFN_GFXS, -1, "Graphics string (ISO2375)"},
    { DEFN_VISS, -1, "Visible string"},
    { DEFN_GENS, -1, "General string"},
    { DEFN_CHRS, -1, "Character string"},
    { -1, -1, 0}
};

#ifdef KRB5
struct typestring_table krb5_types[] = {
    { 1, -1, "Krb5 Ticket"},
    { 2, -1, "Krb5 Authenticator"},
    { 3, -1, "Krb5 Encrypted ticket part"},
    { 10, -1, "Krb5 AS-REQ packet"},
    { 11, -1, "Krb5 AS-REP packet"},
    { 12, -1, "Krb5 TGS-REQ packet"},
    { 13, -1, "Krb5 TGS-REP packet"},
    { 14, -1, "Krb5 AP-REQ packet"},
    { 15, -1, "Krb5 AP-REP packet"},
    { 20, -1, "Krb5 SAFE packet"},
    { 21, -1, "Krb5 PRIV packet"},
    { 22, -1, "Krb5 CRED packet"},
    { 30, -1, "Krb5 ERROR packet"},
    { 25, -1, "Krb5 Encrypted AS-REP part"},
    { 26, -1, "Krb5 Encrypted TGS-REP part"},
    { 27, -1, "Krb5 Encrypted AP-REP part"},
    { 28, -1, "Krb5 Encrypted PRIV part"},
    { 29, -1, "Krb5 Encrypted CRED part"},
    { -1, -1, 0}
};

struct typestring_table krb5_fields[] = {
    { 1000, 0, "name-type"}, /* PrincipalName */
    { 1000, 1, "name-string"},

    { 1001, 0, "etype"},        /* Encrypted data */
    { 1001, 1, "kvno"},
    { 1001, 2, "cipher"},

    { 1002, 0, "addr-type"},    /* HostAddress */
    { 1002, 1, "address"},

    { 1003, 0, "addr-type"},    /* HostAddresses */
    { 1003, 1, "address"},

    { 1004, 0, "ad-type"},      /* AuthorizationData */
    { 1004, 1, "ad-data"},

    { 1005, 0, "keytype"},      /* EncryptionKey */
    { 1005, 1, "keyvalue"},

    { 1006, 0, "cksumtype"},    /* Checksum */
    { 1006, 1, "checksum"},

    { 1007, 0, "kdc-options"},  /* KDC-REQ-BODY */
    { 1007, 1, "cname", 1000},
    { 1007, 2, "realm"},
    { 1007, 3, "sname", 1000},
    { 1007, 4, "from"},
    { 1007, 5, "till"},
    { 1007, 6, "rtime"},
    { 1007, 7, "nonce"},
    { 1007, 8, "etype"},
    { 1007, 9, "addresses", 1003},
    { 1007, 10, "enc-authorization-data", 1001},
    { 1007, 11, "additional-tickets"},

    { 1008, 1, "padata-type"},  /* PA-DATA */
    { 1008, 2, "pa-data"},

    { 1009, 0, "user-data"},    /* KRB-SAFE-BODY */
    { 1009, 1, "timestamp"},
    { 1009, 2, "usec"},
    { 1009, 3, "seq-number"},
    { 1009, 4, "s-address", 1002},
    { 1009, 5, "r-address", 1002},

    { 1010, 0, "lr-type"},      /* LastReq */
    { 1010, 1, "lr-value"},

    { 1011, 0, "key", 1005},    /* KRB-CRED-INFO */
    { 1011, 1, "prealm"},
    { 1011, 2, "pname", 1000},
    { 1011, 3, "flags"},
    { 1011, 4, "authtime"},
    { 1011, 5, "startime"},
    { 1011, 6, "endtime"},
    { 1011, 7, "renew-till"},
    { 1011, 8, "srealm"},
    { 1011, 9, "sname", 1000},
    { 1011, 10, "caddr", 1002},

    { 1, 0, "tkt-vno"}, /* Ticket */
    { 1, 1, "realm"},
    { 1, 2, "sname", 1000},
    { 1, 3, "tkt-enc-part", 1001},

    { 2, 0, "authenticator-vno"}, /* Authenticator */
    { 2, 1, "crealm"},
    { 2, 2, "cname", 1000},
    { 2, 3, "cksum", 1006},
    { 2, 4, "cusec"},
    { 2, 5, "ctime"},
    { 2, 6, "subkey", 1005},
    { 2, 7, "seq-number"},
    { 2, 8, "authorization-data", 1004},

    { 3, 0, "flags"}, /* EncTicketPart */
    { 3, 1, "key", 1005},
    { 3, 2, "crealm"},
    { 3, 3, "cname", 1000},
    { 3, 4, "transited"},
    { 3, 5, "authtime"},
    { 3, 6, "starttime"},
    { 3, 7, "endtime"},
    { 3, 8, "renew-till"},
    { 3, 9, "caddr", 1003},
    { 3, 10, "authorization-data", 1004},

    { 10, 1, "pvno"},   /* AS-REQ */
    { 10, 2, "msg-type"},
    { 10, 3, "padata", 1008},
    { 10, 4, "req-body", 1007},

    { 11, 0, "pvno"},   /* AS-REP */
    { 11, 1, "msg-type"},
    { 11, 2, "padata", 1008},
    { 11, 3, "crealm"},
    { 11, 4, "cname", 1000},
    { 11, 5, "ticket"},
    { 11, 6, "enc-part", 1001},

    { 12, 1, "pvno"},   /* TGS-REQ */
    { 12, 2, "msg-type"},
    { 12, 3, "padata", 1008},
    { 12, 4, "req-body", 1007},

    { 13, 0, "pvno"},   /* TGS-REP */
    { 13, 1, "msg-type"},
    { 13, 2, "padata", 1008},
    { 13, 3, "crealm"},
    { 13, 4, "cname", 1000},
    { 13, 5, "ticket"},
    { 13, 6, "enc-part", 1001},

    { 14, 0, "pvno"},   /* AP-REQ */
    { 14, 1, "msg-type"},
    { 14, 2, "ap-options"},
    { 14, 3, "ticket"},
    { 14, 4, "authenticator", 1001},

    { 15, 0, "pvno"},   /* AP-REP */
    { 15, 1, "msg-type"},
    { 15, 2, "enc-part", 1001},

    { 20, 0, "pvno"},   /* KRB-SAFE */
    { 20, 1, "msg-type"},
    { 20, 2, "safe-body", 1009},
    { 20, 3, "cksum", 1006},

    { 21, 0, "pvno"},   /* KRB-PRIV */
    { 21, 1, "msg-type"},
    { 21, 2, "enc-part", 1001},

    { 22, 0, "pvno"},   /* KRB-CRED */
    { 22, 1, "msg-type"},
    { 22, 2, "tickets"},
    { 22, 3, "enc-part", 1001},

    { 25, 0, "key", 1005},      /* EncASRepPart */
    { 25, 1, "last-req", 1010},
    { 25, 2, "nonce"},
    { 25, 3, "key-expiration"},
    { 25, 4, "flags"},
    { 25, 5, "authtime"},
    { 25, 6, "starttime"},
    { 25, 7, "enddtime"},
    { 25, 8, "renew-till"},
    { 25, 9, "srealm"},
    { 25, 10, "sname", 1000},
    { 25, 11, "caddr", 1003},

    { 26, 0, "key", 1005},      /* EncTGSRepPart */
    { 26, 1, "last-req", 1010},
    { 26, 2, "nonce"},
    { 26, 3, "key-expiration"},
    { 26, 4, "flags"},
    { 26, 5, "authtime"},
    { 26, 6, "starttime"},
    { 26, 7, "enddtime"},
    { 26, 8, "renew-till"},
    { 26, 9, "srealm"},
    { 26, 10, "sname", 1000},
    { 26, 11, "caddr", 1003},

    { 27, 0, "ctime"},  /* EncApRepPart */
    { 27, 1, "cusec"},
    { 27, 2, "subkey", 1005},
    { 27, 3, "seq-number"},

    { 28, 0, "user-data"},      /* EncKrbPrivPart */
    { 28, 1, "timestamp"},
    { 28, 2, "usec"},
    { 28, 3, "seq-number"},
    { 28, 4, "s-address", 1002},
    { 28, 5, "r-address", 1002},

    { 29, 0, "ticket-info", 1011},      /* EncKrbCredPart */
    { 29, 1, "nonce"},
    { 29, 2, "timestamp"},
    { 29, 3, "usec"},
    { 29, 4, "s-address", 1002},
    { 29, 5, "r-address", 1002},

    { 30, 0, "pvno"},   /* KRB-ERROR */
    { 30, 1, "msg-type"},
    { 30, 2, "ctime"},
    { 30, 3, "cusec"},
    { 30, 4, "stime"},
    { 30, 5, "susec"},
    { 30, 6, "error-code"},
    { 30, 7, "crealm"},
    { 30, 8, "cname", 1000},
    { 30, 9, "realm"},
    { 30, 10, "sname", 1000},
    { 30, 11, "e-text"},
    { 30, 12, "e-data"},

    { -1, -1, 0}
};
#endif

void print_tag_type(fp, eid, lev)
    FILE *fp;
    int     eid;
    int     lev;
{
    int tag = eid & ID_TAG;
    int do_space = 1;
    char        *str;

    fprintf(fp, "[");

    switch(eid & ID_CLASS) {
    case CLASS_UNIV:
        if (print_types && print_skip_tagnum)
            do_space = 0;
        else
            fprintf(fp, "UNIV %d", tag);
        break;
    case CLASS_APPL:
        current_appl_type = tag;
#ifdef KRB5
        if (print_krb5_types) {
            str = lookup_typestring(krb5_types, tag, -1);
            if (str) {
                fputs(str, fp);
                break;
            }
        }
#endif
        fprintf(fp, "APPL %d", tag);
        break;
    case CLASS_CONT:
#ifdef KRB5
        if (print_krb5_types && current_appl_type) {
            str = lookup_typestring(krb5_fields,
                                    current_appl_type, tag);
            if (str) {
                fputs(str, fp);
                break;
            }
        }
#endif
        if (print_skip_context && lev)
            fprintf(fp, "%d", tag);
        else
            fprintf(fp, "CONT %d", tag);
        break;
    case CLASS_PRIV:
        fprintf(fp, "PRIV %d", tag);
        break;
    }

    if (print_types && ((eid & ID_CLASS) == CLASS_UNIV)) {
        if (do_space)
            fputs(" ", fp);
        str = lookup_typestring(univ_types, eid & ID_TAG, -1);
        if (str)
            fputs(str, fp);
        else
            fprintf(fp, "UNIV %d???", eid & ID_TAG);
    }

    fprintf(fp, "]");

}

/*****************************************************************************/
