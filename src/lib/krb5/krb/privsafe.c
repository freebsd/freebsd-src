/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/privsafe.c - Shared logic for KRB-SAFE and KRB-PRIV messages */
/*
 * Copyright (C) 2011 by the Massachusetts Institute of Technology.
 * All rights reserved.
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

#include "k5-int.h"
#include "int-proto.h"
#include "auth_con.h"

/*
 * k5_privsafe_check_seqnum
 *
 * We use a somewhat complex heuristic for validating received
 * sequence numbers.  We must accommodate both our older
 * implementation, which sends negative sequence numbers, and the
 * broken Heimdal implementation (at least as of 0.5.2), which
 * violates X.690 BER for integer encodings.  The requirement of
 * handling negative sequence numbers removes one of easier means of
 * detecting a Heimdal implementation, so we resort to this mess
 * here.
 *
 * X.690 BER (and consequently DER, which are the required encoding
 * rules in RFC1510) encode all integer types as signed integers.
 * This means that the MSB being set on the first octet of the
 * contents of the encoding indicates a negative value.  Heimdal does
 * not prepend the required zero octet to unsigned integer encodings
 * which would otherwise have the MSB of the first octet of their
 * encodings set.
 *
 * Our ASN.1 library implements a special decoder for sequence
 * numbers, accepting both negative and positive 32-bit numbers but
 * mapping them both into the space of positive unsigned 32-bit
 * numbers in the obvious bit-pattern-preserving way.  This maintains
 * compatibility with our older implementations.  This also means that
 * encodings emitted by Heimdal are ambiguous.
 *
 * Heimdal counter value        received uint32 value
 *
 * 0x00000080                   0xFFFFFF80
 * 0x000000FF                   0xFFFFFFFF
 * 0x00008000                   0xFFFF8000
 * 0x0000FFFF                   0xFFFFFFFF
 * 0x00800000                   0xFF800000
 * 0x00FFFFFF                   0xFFFFFFFF
 * 0xFF800000                   0xFF800000
 * 0xFFFFFFFF                   0xFFFFFFFF
 *
 * We use two auth_context flags, SANE_SEQ and HEIMDAL_SEQ, which are
 * only set after we can unambiguously determine the sanity of the
 * sending implementation.  Once one of these flags is set, we accept
 * only the sequence numbers appropriate to the remote implementation
 * type.  We can make the determination in two different ways.  The
 * first is to note the receipt of a "negative" sequence number when a
 * "positive" one was expected.  The second is to note the receipt of
 * a sequence number that wraps through "zero" in a weird way.  The
 * latter corresponds to the receipt of an initial sequence number in
 * the ambiguous range.
 *
 * There are 2^7 + 2^15 + 2^23 + 2^23 = 16810112 total ambiguous
 * initial Heimdal counter values, but we receive them as one of 2^23
 * possible values.  There is a ~1/256 chance of a Heimdal
 * implementation sending an intial sequence number in the ambiguous
 * range.
 *
 * We have to do special treatment when receiving sequence numbers
 * between 0xFF800000..0xFFFFFFFF, or when wrapping through zero
 * weirdly (due to ambiguous initial sequence number).  If we are
 * expecting a value corresponding to an ambiguous Heimdal counter
 * value, and we receive an exact match, we can mark the remote end as
 * sane.
 */

static krb5_boolean
chk_heimdal_seqnum(krb5_ui_4 exp_seq, krb5_ui_4 in_seq)
{
    if (( exp_seq & 0xFF800000) == 0x00800000
        && (in_seq & 0xFF800000) == 0xFF800000
        && (in_seq & 0x00FFFFFF) == exp_seq)
        return 1;
    else if ((  exp_seq & 0xFFFF8000) == 0x00008000
             && (in_seq & 0xFFFF8000) == 0xFFFF8000
             && (in_seq & 0x0000FFFF) == exp_seq)
        return 1;
    else if ((  exp_seq & 0xFFFFFF80) == 0x00000080
             && (in_seq & 0xFFFFFF80) == 0xFFFFFF80
             && (in_seq & 0x000000FF) == exp_seq)
        return 1;
    else
        return 0;
}

krb5_boolean
k5_privsafe_check_seqnum(krb5_context ctx, krb5_auth_context ac,
                         krb5_ui_4 in_seq)
{
    krb5_ui_4 exp_seq;

    exp_seq = ac->remote_seq_number;

    /*
     * If sender is known to be sane, accept _only_ exact matches.
     */
    if (ac->auth_context_flags & KRB5_AUTH_CONN_SANE_SEQ)
        return in_seq == exp_seq;

    /*
     * If sender is not known to be sane, first check the ambiguous
     * range of received values, 0xFF800000..0xFFFFFFFF.
     */
    if ((in_seq & 0xFF800000) == 0xFF800000) {
        /*
         * If expected sequence number is in the range
         * 0xFF800000..0xFFFFFFFF, then we can't make any
         * determinations about the sanity of the sending
         * implementation.
         */
        if ((exp_seq & 0xFF800000) == 0xFF800000 && in_seq == exp_seq)
            return 1;
        /*
         * If sender is not known for certain to be a broken Heimdal
         * implementation, check for exact match.
         */
        if (!(ac->auth_context_flags & KRB5_AUTH_CONN_HEIMDAL_SEQ)
            && in_seq == exp_seq)
            return 1;
        /*
         * Now apply hairy algorithm for matching sequence numbers
         * sent by broken Heimdal implementations.  If it matches, we
         * know for certain it's a broken Heimdal sender.
         */
        if (chk_heimdal_seqnum(exp_seq, in_seq)) {
            ac->auth_context_flags |= KRB5_AUTH_CONN_HEIMDAL_SEQ;
            return 1;
        }
        return 0;
    }

    /*
     * Received value not in the ambiguous range?  If the _expected_
     * value is in the range of ambiguous Hemidal counter values, and
     * it matches the received value, sender is known to be sane.
     */
    if (in_seq == exp_seq) {
        if ((   exp_seq & 0xFFFFFF80) == 0x00000080
            || (exp_seq & 0xFFFF8000) == 0x00008000
            || (exp_seq & 0xFF800000) == 0x00800000)
            ac->auth_context_flags |= KRB5_AUTH_CONN_SANE_SEQ;
        return 1;
    }

    /*
     * Magic wraparound for the case where the intial sequence number
     * is in the ambiguous range.  This means that the sender's
     * counter is at a different count than ours, so we correct ours,
     * and mark the sender as being a broken Heimdal implementation.
     */
    if (exp_seq == 0
        && !(ac->auth_context_flags & KRB5_AUTH_CONN_HEIMDAL_SEQ)) {
        switch (in_seq) {
        case 0x100:
        case 0x10000:
        case 0x1000000:
            ac->auth_context_flags |= KRB5_AUTH_CONN_HEIMDAL_SEQ;
            exp_seq = in_seq;
            return 1;
        default:
            return 0;
        }
    }
    return 0;
}

/*
 * Verify the sender and receiver addresses from a KRB-SAFE or KRB-PRIV message
 * against the auth context.  msg_r_addr may be NULL, but msg_s_addr must not
 * be.  The auth context's remote addr must be set.
 */
krb5_error_code
k5_privsafe_check_addrs(krb5_context context, krb5_auth_context ac,
                        krb5_address *msg_s_addr, krb5_address *msg_r_addr)
{
    krb5_error_code ret = 0;
    krb5_address **our_addrs = NULL;
    const krb5_address *local_addr, *remote_addr;
    krb5_address local_fulladdr, remote_fulladdr;

    local_fulladdr.contents = remote_fulladdr.contents = NULL;

    /* Determine the remote comparison address. */
    if (ac->remote_addr != NULL) {
        if (ac->remote_port != NULL) {
            ret = krb5_make_fulladdr(context, ac->remote_addr, ac->remote_port,
                                     &remote_fulladdr);
            if (ret)
                goto cleanup;
            remote_addr = &remote_fulladdr;
        } else
            remote_addr = ac->remote_addr;
    } else
        remote_addr = NULL;

    /* Determine the local comparison address (possibly NULL). */
    if (ac->local_addr != NULL) {
        if (ac->local_port != NULL) {
            ret = krb5_make_fulladdr(context, ac->local_addr, ac->local_port,
                                     &local_fulladdr);
            if (ret)
                goto cleanup;
            local_addr = &local_fulladdr;
        } else
            local_addr = ac->local_addr;
    } else
        local_addr = NULL;

    /* Check the remote address against the message's sender address. */
    if (remote_addr != NULL &&
        !krb5_address_compare(context, remote_addr, msg_s_addr)) {
        ret = KRB5KRB_AP_ERR_BADADDR;
        goto cleanup;
    }

    /* Receiver address is optional; only check it if supplied. */
    if (msg_r_addr == NULL)
        goto cleanup;

    /* Check the message's receiver address against the local address, or
     * against all local addresses if no specific local address is set. */
    if (local_addr != NULL) {
        if (!krb5_address_compare(context, local_addr, msg_r_addr)) {
            ret = KRB5KRB_AP_ERR_BADADDR;
            goto cleanup;
        }
    } else {
        ret = krb5_os_localaddr(context, &our_addrs);
        if (ret)
            goto cleanup;

        if (!krb5_address_search(context, msg_r_addr, our_addrs)) {
            ret = KRB5KRB_AP_ERR_BADADDR;
            goto cleanup;
        }
    }

cleanup:
    free(local_fulladdr.contents);
    free(remote_fulladdr.contents);
    krb5_free_addresses(context, our_addrs);
    return ret;
}
