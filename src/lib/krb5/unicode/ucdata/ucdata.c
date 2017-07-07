/*
 * Copyright 1998-2008 The OpenLDAP Foundation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */
/* Copyright 2001 Computing Research Labs, New Mexico State University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COMPUTING RESEARCH LAB OR NEW MEXICO STATE UNIVERSITY BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * This work is part of OpenLDAP Software <http://www.openldap.org/>.
 * $OpenLDAP: pkg/ldap/libraries/liblunicode/ucdata/ucdata.c,v 1.36 2008/01/07 23:20:05 kurt Exp $
 * $Id: ucdata.c,v 1.4 2001/01/02 18:46:20 mleisher Exp $"
 */

#include "k5-int.h"
#include "k5-utf8.h"
#include "k5-unicode.h"

#include "ucdata.h"

#ifndef HARDCODE_DATA
#define	HARDCODE_DATA	1
#endif

#if HARDCODE_DATA
#include "uctable.h"
#endif

/**************************************************************************
 *
 * Miscellaneous types, data, and support functions.
 *
 **************************************************************************/

typedef struct {
    krb5_ui_2 bom;
    krb5_ui_2 cnt;
    union {
        krb5_ui_4 bytes;
        krb5_ui_2 len[2];
    } size;
} _ucheader_t;

/*
 * A simple array of 32-bit masks for lookup.
 */
static krb5_ui_4 masks32[32] = {
	0x00000001UL, 0x00000002UL, 0x00000004UL, 0x00000008UL,
	0x00000010UL, 0x00000020UL, 0x00000040UL, 0x00000080UL,
	0x00000100UL, 0x00000200UL, 0x00000400UL, 0x00000800UL,
	0x00001000UL, 0x00002000UL, 0x00004000UL, 0x00008000UL,
	0x00010000UL, 0x00020000UL, 0x00040000UL, 0x00080000UL,
	0x00100000UL, 0x00200000UL, 0x00400000UL, 0x00800000UL,
	0x01000000UL, 0x02000000UL, 0x04000000UL, 0x08000000UL,
	0x10000000UL, 0x20000000UL, 0x40000000UL, 0x80000000UL
};

#define endian_short(cc) (((cc) >> 8) | (((cc) & 0xff) << 8))
#define endian_long(cc) ((((cc) & 0xff) << 24)|((((cc) >> 8) & 0xff) << 16)|\
                        ((((cc) >> 16) & 0xff) << 8)|((cc) >> 24))

#if !HARDCODE_DATA
static FILE *
_ucopenfile(char *paths, char *filename, char *mode)
{
    FILE *f;
    char *fp, *dp, *pp, path[BUFSIZ];

    if (filename == 0 || *filename == 0)
      return 0;

    dp = paths;
    while (dp && *dp) {
        pp = path;
        while (*dp && *dp != ':')
          *pp++ = *dp++;
        *pp++ = *LDAP_DIRSEP;

        fp = filename;
        while (*fp)
          *pp++ = *fp++;
        *pp = 0;

        if ((f = fopen(path, mode)) != 0)
          return f;

        if (*dp == ':')
          dp++;
    }

    return 0;
}
#endif

/**************************************************************************
 *
 * Support for the character properties.
 *
 **************************************************************************/

#if !HARDCODE_DATA

static krb5_ui_4 _ucprop_size;
static krb5_ui_2 *_ucprop_offsets;
static krb5_ui_4 *_ucprop_ranges;

/*
 * Return -1 on error, 0 if okay
 */
static int
_ucprop_load(char *paths, int reload)
{
    FILE *in;
    krb5_ui_4 size, i;
    _ucheader_t hdr;

    if (_ucprop_size > 0) {
        if (!reload)
          /*
           * The character properties have already been loaded.
           */
          return 0;

        /*
         * Unload the current character property data in preparation for
         * loading a new copy.  Only the first array has to be deallocated
         * because all the memory for the arrays is allocated as a single
         * block.
         */
        free((char *) _ucprop_offsets);
        _ucprop_size = 0;
    }

    if ((in = _ucopenfile(paths, "ctype.dat", "rb")) == 0)
      return -1;

    /*
     * Load the header.
     */
    fread((char *) &hdr, sizeof(_ucheader_t), 1, in);

    if (hdr.bom == 0xfffe) {
        hdr.cnt = endian_short(hdr.cnt);
        hdr.size.bytes = endian_long(hdr.size.bytes);
    }

    if ((_ucprop_size = hdr.cnt) == 0) {
        fclose(in);
        return -1;
    }

    /*
     * Allocate all the storage needed for the lookup table.
     */
    _ucprop_offsets = (krb5_ui_2 *) malloc(hdr.size.bytes);

    /*
     * Calculate the offset into the storage for the ranges.  The offsets
     * array is on a 4-byte boundary and one larger than the value provided in
     * the header count field.  This means the offset to the ranges must be
     * calculated after aligning the count to a 4-byte boundary.
     */
    if ((size = ((hdr.cnt + 1) * sizeof(krb5_ui_2))) & 3)
      size += 4 - (size & 3);
    size >>= 1;
    _ucprop_ranges = (krb5_ui_4 *) (_ucprop_offsets + size);

    /*
     * Load the offset array.
     */
    fread((char *) _ucprop_offsets, sizeof(krb5_ui_2), size, in);

    /*
     * Do an endian swap if necessary.  Don't forget there is an extra node on
     * the end with the final index.
     */
    if (hdr.bom == 0xfffe) {
        for (i = 0; i <= _ucprop_size; i++)
          _ucprop_offsets[i] = endian_short(_ucprop_offsets[i]);
    }

    /*
     * Load the ranges.  The number of elements is in the last array position
     * of the offsets.
     */
    fread((char *) _ucprop_ranges, sizeof(krb5_ui_4),
          _ucprop_offsets[_ucprop_size], in);

    fclose(in);

    /*
     * Do an endian swap if necessary.
     */
    if (hdr.bom == 0xfffe) {
        for (i = 0; i < _ucprop_offsets[_ucprop_size]; i++)
          _ucprop_ranges[i] = endian_long(_ucprop_ranges[i]);
    }
    return 0;
}

static void
_ucprop_unload(void)
{
    if (_ucprop_size == 0)
      return;

    /*
     * Only need to free the offsets because the memory is allocated as a
     * single block.
     */
    free((char *) _ucprop_offsets);
    _ucprop_size = 0;
}
#endif

static int
_ucprop_lookup(krb5_ui_4 code, krb5_ui_4 n)
{
    long l, r, m;

    if (_ucprop_size == 0)
      return 0;

    /*
     * There is an extra node on the end of the offsets to allow this routine
     * to work right.  If the index is 0xffff, then there are no nodes for the
     * property.
     */
    if ((l = _ucprop_offsets[n]) == 0xffff)
      return 0;

    /*
     * Locate the next offset that is not 0xffff.  The sentinel at the end of
     * the array is the max index value.
     */
    for (m = 1;
         n + m < _ucprop_size && _ucprop_offsets[n + m] == 0xffff; m++) ;

    r = _ucprop_offsets[n + m] - 1;

    while (l <= r) {
        /*
         * Determine a "mid" point and adjust to make sure the mid point is at
         * the beginning of a range pair.
         */
        m = (l + r) >> 1;
        m -= (m & 1);
        if (code > _ucprop_ranges[m + 1])
          l = m + 2;
        else if (code < _ucprop_ranges[m])
          r = m - 2;
        else if (code >= _ucprop_ranges[m] && code <= _ucprop_ranges[m + 1])
          return 1;
    }
    return 0;
}

int
ucisprop(krb5_ui_4 code, krb5_ui_4 mask1, krb5_ui_4 mask2)
{
    krb5_ui_4 i;

    if (mask1 == 0 && mask2 == 0)
      return 0;

    for (i = 0; mask1 && i < 32; i++) {
        if ((mask1 & masks32[i]) && _ucprop_lookup(code, i))
          return 1;
    }

    for (i = 32; mask2 && i < _ucprop_size; i++) {
        if ((mask2 & masks32[i & 31]) && _ucprop_lookup(code, i))
          return 1;
    }

    return 0;
}

/**************************************************************************
 *
 * Support for case mapping.
 *
 **************************************************************************/

#if !HARDCODE_DATA

/* These record the number of slots in the map.
 * There are 3 words per slot.
 */
static krb5_ui_4 _uccase_size;
static krb5_ui_2 _uccase_len[2];
static krb5_ui_4 *_uccase_map;

/*
 * Return -1 on error, 0 if okay
 */
static int
_uccase_load(char *paths, int reload)
{
    FILE *in;
    krb5_ui_4 i;
    _ucheader_t hdr;

    if (_uccase_size > 0) {
        if (!reload)
          /*
           * The case mappings have already been loaded.
           */
          return 0;

        free((char *) _uccase_map);
        _uccase_size = 0;
    }

    if ((in = _ucopenfile(paths, "case.dat", "rb")) == 0)
      return -1;

    /*
     * Load the header.
     */
    fread((char *) &hdr, sizeof(_ucheader_t), 1, in);

    if (hdr.bom == 0xfffe) {
        hdr.cnt = endian_short(hdr.cnt);
        hdr.size.len[0] = endian_short(hdr.size.len[0]);
        hdr.size.len[1] = endian_short(hdr.size.len[1]);
    }

    /*
     * Set the node count and lengths of the upper and lower case mapping
     * tables.
     */
    _uccase_size = hdr.cnt;
    _uccase_len[0] = hdr.size.len[0];
    _uccase_len[1] = hdr.size.len[1];

    _uccase_map = (krb5_ui_4 *)
        malloc(_uccase_size * 3 * sizeof(krb5_ui_4));

    /*
     * Load the case mapping table.
     */
    fread((char *) _uccase_map, sizeof(krb5_ui_4), _uccase_size * 3, in);

    /*
     * Do an endian swap if necessary.
     */
    if (hdr.bom == 0xfffe) {
        for (i = 0; i < _uccase_size * 3; i++)
          _uccase_map[i] = endian_long(_uccase_map[i]);
    }
    fclose(in);
    return 0;
}

static void
_uccase_unload(void)
{
    if (_uccase_size == 0)
      return;

    free((char *) _uccase_map);
    _uccase_size = 0;
}
#endif

static krb5_ui_4
_uccase_lookup(krb5_ui_4 code, long l, long r, int field)
{
    long m;
	const krb5_ui_4 *tmp;

    /*
     * Do the binary search.
     */
    while (l <= r) {
        /*
         * Determine a "mid" point and adjust to make sure the mid point is at
         * the beginning of a case mapping triple.
         */
        m = (l + r) >> 1;
		tmp = &_uccase_map[m*3];
        if (code > *tmp)
          l = m + 1;
        else if (code < *tmp)
          r = m - 1;
        else if (code == *tmp)
          return tmp[field];
    }

    return code;
}

krb5_ui_4
uctoupper(krb5_ui_4 code)
{
    int field;
    long l, r;

    if (ucisupper(code))
      return code;

    if (ucislower(code)) {
        /*
         * The character is lower case.
         */
        field = 2;
        l = _uccase_len[0];
        r = (l + _uccase_len[1]) - 1;
    } else {
        /*
         * The character is title case.
         */
        field = 1;
        l = _uccase_len[0] + _uccase_len[1];
        r = _uccase_size - 1;
    }
    return _uccase_lookup(code, l, r, field);
}

krb5_ui_4
uctolower(krb5_ui_4 code)
{
    int field;
    long l, r;

    if (ucislower(code))
      return code;

    if (ucisupper(code)) {
        /*
         * The character is upper case.
         */
        field = 1;
        l = 0;
        r = _uccase_len[0] - 1;
    } else {
        /*
         * The character is title case.
         */
        field = 2;
        l = _uccase_len[0] + _uccase_len[1];
        r = _uccase_size - 1;
    }
    return _uccase_lookup(code, l, r, field);
}

krb5_ui_4
uctotitle(krb5_ui_4 code)
{
    int field;
    long l, r;

    if (ucistitle(code))
      return code;

    /*
     * The offset will always be the same for converting to title case.
     */
    field = 2;

    if (ucisupper(code)) {
        /*
         * The character is upper case.
         */
        l = 0;
        r = _uccase_len[0] - 1;
    } else {
        /*
         * The character is lower case.
         */
        l = _uccase_len[0];
        r = (l + _uccase_len[1]) - 1;
    }
    return _uccase_lookup(code, l, r, field);
}

/**************************************************************************
 *
 * Support for compositions.
 *
 **************************************************************************/

#if !HARDCODE_DATA

static krb5_ui_4  _uccomp_size;
static krb5_ui_4 *_uccomp_data;

/*
 * Return -1 on error, 0 if okay
 */
static int
_uccomp_load(char *paths, int reload)
{
    FILE *in;
    krb5_ui_4 size, i;
    _ucheader_t hdr;

    if (_uccomp_size > 0) {
        if (!reload)
            /*
             * The compositions have already been loaded.
             */
            return 0;

        free((char *) _uccomp_data);
        _uccomp_size = 0;
    }

    if ((in = _ucopenfile(paths, "comp.dat", "rb")) == 0)
        return -1;

    /*
     * Load the header.
     */
    fread((char *) &hdr, sizeof(_ucheader_t), 1, in);

    if (hdr.bom == 0xfffe) {
        hdr.cnt = endian_short(hdr.cnt);
        hdr.size.bytes = endian_long(hdr.size.bytes);
    }

    _uccomp_size = hdr.cnt;
    _uccomp_data = (krb5_ui_4 *) malloc(hdr.size.bytes);

    /*
     * Read the composition data in.
     */
    size = hdr.size.bytes / sizeof(krb5_ui_4);
    fread((char *) _uccomp_data, sizeof(krb5_ui_4), size, in);

    /*
     * Do an endian swap if necessary.
     */
    if (hdr.bom == 0xfffe) {
        for (i = 0; i < size; i++)
            _uccomp_data[i] = endian_long(_uccomp_data[i]);
    }

    /*
     * Assume that the data is ordered on count, so that all compositions
     * of length 2 come first. Only handling length 2 for now.
     */
    for (i = 1; i < size; i += 4)
      if (_uccomp_data[i] != 2)
        break;
    _uccomp_size = i - 1;

    fclose(in);
    return 0;
}

static void
_uccomp_unload(void)
{
    if (_uccomp_size == 0)
        return;

    free((char *) _uccomp_data);
    _uccomp_size = 0;
}
#endif

int
uccomp(krb5_ui_4 node1, krb5_ui_4 node2, krb5_ui_4 *comp)
{
    int l, r, m;

    l = 0;
    r = _uccomp_size - 1;

    while (l <= r) {
        m = ((r + l) >> 1);
        m -= m & 3;
        if (node1 > _uccomp_data[m+2])
          l = m + 4;
        else if (node1 < _uccomp_data[m+2])
          r = m - 4;
        else if (node2 > _uccomp_data[m+3])
          l = m + 4;
        else if (node2 < _uccomp_data[m+3])
          r = m - 4;
        else {
            *comp = _uccomp_data[m];
            return 1;
        }
    }
    return 0;
}

int
uccomp_hangul(krb5_ui_4 *str, int len)
{
    const int SBase = 0xAC00, LBase = 0x1100,
        VBase = 0x1161, TBase = 0x11A7,
        LCount = 19, VCount = 21, TCount = 28,
        NCount = VCount * TCount,   /* 588 */
        SCount = LCount * NCount;   /* 11172 */

    int i, rlen;
    krb5_ui_4 ch, last, lindex, sindex;

    last = str[0];
    rlen = 1;
    for ( i = 1; i < len; i++ ) {
        ch = str[i];

        /* check if two current characters are L and V */
        lindex = last - LBase;
        if (lindex < (krb5_ui_4) LCount) {
            krb5_ui_4 vindex = ch - VBase;
            if (vindex < (krb5_ui_4) VCount) {
                /* make syllable of form LV */
                last = SBase + (lindex * VCount + vindex) * TCount;
                str[rlen-1] = last; /* reset last */
                continue;
            }
        }

        /* check if two current characters are LV and T */
        sindex = last - SBase;
        if (sindex < (krb5_ui_4) SCount
			&& (sindex % TCount) == 0)
		{
            krb5_ui_4 tindex = ch - TBase;
            if (tindex <= (krb5_ui_4) TCount) {
                /* make syllable of form LVT */
                last += tindex;
                str[rlen-1] = last; /* reset last */
                continue;
            }
        }

        /* if neither case was true, just add the character */
        last = ch;
        str[rlen] = ch;
        rlen++;
    }
    return rlen;
}

int
uccanoncomp(krb5_ui_4 *str, int len)
{
    int i, stpos, copos;
    krb5_ui_4 cl, prevcl, st, ch, co;

    st = str[0];
    stpos = 0;
    copos = 1;
    prevcl = uccombining_class(st) == 0 ? 0 : 256;

    for (i = 1; i < len; i++) {
        ch = str[i];
        cl = uccombining_class(ch);
        if (uccomp(st, ch, &co) && (prevcl < cl || prevcl == 0))
          st = str[stpos] = co;
        else {
            if (cl == 0) {
                stpos = copos;
                st = ch;
            }
            prevcl = cl;
            str[copos++] = ch;
        }
    }

    return uccomp_hangul(str, copos);
}

/**************************************************************************
 *
 * Support for decompositions.
 *
 **************************************************************************/

#if !HARDCODE_DATA

static krb5_ui_4  _ucdcmp_size;
static krb5_ui_4 *_ucdcmp_nodes;
static krb5_ui_4 *_ucdcmp_decomp;

static krb5_ui_4  _uckdcmp_size;
static krb5_ui_4 *_uckdcmp_nodes;
static krb5_ui_4 *_uckdcmp_decomp;

/*
 * Return -1 on error, 0 if okay
 */
static int
_ucdcmp_load(char *paths, int reload)
{
    FILE *in;
    krb5_ui_4 size, i;
    _ucheader_t hdr;

    if (_ucdcmp_size > 0) {
        if (!reload)
            /*
             * The decompositions have already been loaded.
             */
          return 0;

        free((char *) _ucdcmp_nodes);
        _ucdcmp_size = 0;
    }

    if ((in = _ucopenfile(paths, "decomp.dat", "rb")) == 0)
        return -1;

    /*
     * Load the header.
     */
    fread((char *) &hdr, sizeof(_ucheader_t), 1, in);

    if (hdr.bom == 0xfffe) {
        hdr.cnt = endian_short(hdr.cnt);
        hdr.size.bytes = endian_long(hdr.size.bytes);
    }

    _ucdcmp_size = hdr.cnt << 1;
    _ucdcmp_nodes = (krb5_ui_4 *) malloc(hdr.size.bytes);
    _ucdcmp_decomp = _ucdcmp_nodes + (_ucdcmp_size + 1);

    /*
     * Read the decomposition data in.
     */
    size = hdr.size.bytes / sizeof(krb5_ui_4);
    fread((char *) _ucdcmp_nodes, sizeof(krb5_ui_4), size, in);

    /*
     * Do an endian swap if necessary.
     */
    if (hdr.bom == 0xfffe) {
        for (i = 0; i < size; i++)
            _ucdcmp_nodes[i] = endian_long(_ucdcmp_nodes[i]);
    }
    fclose(in);
    return 0;
}

/*
 * Return -1 on error, 0 if okay
 */
static int
_uckdcmp_load(char *paths, int reload)
{
    FILE *in;
    krb5_ui_4 size, i;
    _ucheader_t hdr;

    if (_uckdcmp_size > 0) {
        if (!reload)
            /*
             * The decompositions have already been loaded.
             */
          return 0;

        free((char *) _uckdcmp_nodes);
        _uckdcmp_size = 0;
    }

    if ((in = _ucopenfile(paths, "kdecomp.dat", "rb")) == 0)
        return -1;

    /*
     * Load the header.
     */
    fread((char *) &hdr, sizeof(_ucheader_t), 1, in);

    if (hdr.bom == 0xfffe) {
        hdr.cnt = endian_short(hdr.cnt);
        hdr.size.bytes = endian_long(hdr.size.bytes);
    }

    _uckdcmp_size = hdr.cnt << 1;
    _uckdcmp_nodes = (krb5_ui_4 *) malloc(hdr.size.bytes);
    _uckdcmp_decomp = _uckdcmp_nodes + (_uckdcmp_size + 1);

    /*
     * Read the decomposition data in.
     */
    size = hdr.size.bytes / sizeof(krb5_ui_4);
    fread((char *) _uckdcmp_nodes, sizeof(krb5_ui_4), size, in);

    /*
     * Do an endian swap if necessary.
     */
    if (hdr.bom == 0xfffe) {
        for (i = 0; i < size; i++)
            _uckdcmp_nodes[i] = endian_long(_uckdcmp_nodes[i]);
    }
    fclose(in);
    return 0;
}

static void
_ucdcmp_unload(void)
{
    if (_ucdcmp_size == 0)
      return;

    /*
     * Only need to free the offsets because the memory is allocated as a
     * single block.
     */
    free((char *) _ucdcmp_nodes);
    _ucdcmp_size = 0;
}

static void
_uckdcmp_unload(void)
{
    if (_uckdcmp_size == 0)
      return;

    /*
     * Only need to free the offsets because the memory is allocated as a
     * single block.
     */
    free((char *) _uckdcmp_nodes);
    _uckdcmp_size = 0;
}
#endif

int
ucdecomp(krb5_ui_4 code, krb5_ui_4 *num, krb5_ui_4 **decomp)
{
    long l, r, m;

    if (code < _ucdcmp_nodes[0]) {
	return 0;
    }

    l = 0;
    r = _ucdcmp_nodes[_ucdcmp_size] - 1;

    while (l <= r) {
        /*
         * Determine a "mid" point and adjust to make sure the mid point is at
         * the beginning of a code+offset pair.
         */
        m = (l + r) >> 1;
        m -= (m & 1);
        if (code > _ucdcmp_nodes[m])
          l = m + 2;
        else if (code < _ucdcmp_nodes[m])
          r = m - 2;
        else if (code == _ucdcmp_nodes[m]) {
            *num = _ucdcmp_nodes[m + 3] - _ucdcmp_nodes[m + 1];
            *decomp = (krb5_ui_4*)&_ucdcmp_decomp[_ucdcmp_nodes[m + 1]];
            return 1;
        }
    }
    return 0;
}

int
uckdecomp(krb5_ui_4 code, krb5_ui_4 *num, krb5_ui_4 **decomp)
{
    long l, r, m;

    if (code < _uckdcmp_nodes[0]) {
	return 0;
    }

    l = 0;
    r = _uckdcmp_nodes[_uckdcmp_size] - 1;

    while (l <= r) {
        /*
         * Determine a "mid" point and adjust to make sure the mid point is at
         * the beginning of a code+offset pair.
         */
        m = (l + r) >> 1;
        m -= (m & 1);
        if (code > _uckdcmp_nodes[m])
          l = m + 2;
        else if (code < _uckdcmp_nodes[m])
          r = m - 2;
        else if (code == _uckdcmp_nodes[m]) {
            *num = _uckdcmp_nodes[m + 3] - _uckdcmp_nodes[m + 1];
            *decomp = (krb5_ui_4*)&_uckdcmp_decomp[_uckdcmp_nodes[m + 1]];
            return 1;
        }
    }
    return 0;
}

int
ucdecomp_hangul(krb5_ui_4 code, krb5_ui_4 *num, krb5_ui_4 decomp[])
{
    if (!ucishangul(code))
      return 0;

    code -= 0xac00;
    decomp[0] = 0x1100 + (krb5_ui_4) (code / 588);
    decomp[1] = 0x1161 + (krb5_ui_4) ((code % 588) / 28);
    decomp[2] = 0x11a7 + (krb5_ui_4) (code % 28);
    *num = (decomp[2] != 0x11a7) ? 3 : 2;

    return 1;
}

/* mode == 0 for canonical, mode == 1 for compatibility */
static int
uccanoncompatdecomp(const krb5_ui_4 *in, int inlen,
		    krb5_ui_4 **out, int *outlen, short mode)
{
    int l, size;
	unsigned i, j, k;
    krb5_ui_4 num, class, *decomp, hangdecomp[3];

    size = inlen * 2;
    *out = (krb5_ui_4 *) malloc(size * sizeof(**out));
    if (*out == NULL)
        return *outlen = -1;

    i = 0;
    for (j = 0; j < (unsigned) inlen; j++) {
	if (mode ? uckdecomp(in[j], &num, &decomp) : ucdecomp(in[j], &num, &decomp)) {
            if ( size - i < num) {
                size = inlen + i - j + num - 1;
                *out = (krb5_ui_4 *) realloc(*out, size * sizeof(**out));
                if (*out == NULL)
                    return *outlen = -1;
            }
            for (k = 0; k < num; k++) {
                class = uccombining_class(decomp[k]);
                if (class == 0) {
                    (*out)[i] = decomp[k];
                } else {
                    for (l = i; l > 0; l--)
                        if (class >= uccombining_class((*out)[l-1]))
                            break;
                    memcpy(*out + l + 1, *out + l, (i - l) * sizeof(**out));
                    (*out)[l] = decomp[k];
                }
                i++;
            }
        } else if (ucdecomp_hangul(in[j], &num, hangdecomp)) {
            if (size - i < num) {
                size = inlen + i - j + num - 1;
                *out = (krb5_ui_4 *) realloc(*out, size * sizeof(**out));
                if (*out == NULL)
                    return *outlen = -1;
            }
            for (k = 0; k < num; k++) {
                (*out)[i] = hangdecomp[k];
                i++;
            }
        } else {
            if (size - i < 1) {
                size = inlen + i - j;
                *out = (krb5_ui_4 *) realloc(*out, size * sizeof(**out));
                if (*out == NULL)
                    return *outlen = -1;
            }
            class = uccombining_class(in[j]);
            if (class == 0) {
                (*out)[i] = in[j];
            } else {
                for (l = i; l > 0; l--)
                    if (class >= uccombining_class((*out)[l-1]))
                        break;
                memcpy(*out + l + 1, *out + l, (i - l) * sizeof(**out));
                (*out)[l] = in[j];
            }
            i++;
        }
    }
    return *outlen = i;
}

int
uccanondecomp(const krb5_ui_4 *in, int inlen,
              krb5_ui_4 **out, int *outlen)
{
    return uccanoncompatdecomp(in, inlen, out, outlen, 0);
}

int
uccompatdecomp(const krb5_ui_4 *in, int inlen,
	       krb5_ui_4 **out, int *outlen)
{
    return uccanoncompatdecomp(in, inlen, out, outlen, 1);
}

/**************************************************************************
 *
 * Support for combining classes.
 *
 **************************************************************************/

#if !HARDCODE_DATA
static krb5_ui_4  _uccmcl_size;
static krb5_ui_4 *_uccmcl_nodes;

/*
 * Return -1 on error, 0 if okay
 */
static int
_uccmcl_load(char *paths, int reload)
{
    FILE *in;
    krb5_ui_4 i;
    _ucheader_t hdr;

    if (_uccmcl_size > 0) {
        if (!reload)
            /*
             * The combining classes have already been loaded.
             */
            return 0;

        free((char *) _uccmcl_nodes);
        _uccmcl_size = 0;
    }

    if ((in = _ucopenfile(paths, "cmbcl.dat", "rb")) == 0)
        return -1;

    /*
     * Load the header.
     */
    fread((char *) &hdr, sizeof(_ucheader_t), 1, in);

    if (hdr.bom == 0xfffe) {
        hdr.cnt = endian_short(hdr.cnt);
        hdr.size.bytes = endian_long(hdr.size.bytes);
    }

    _uccmcl_size = hdr.cnt * 3;
    _uccmcl_nodes = (krb5_ui_4 *) malloc(hdr.size.bytes);

    /*
     * Read the combining classes in.
     */
    fread((char *) _uccmcl_nodes, sizeof(krb5_ui_4), _uccmcl_size, in);

    /*
     * Do an endian swap if necessary.
     */
    if (hdr.bom == 0xfffe) {
        for (i = 0; i < _uccmcl_size; i++)
            _uccmcl_nodes[i] = endian_long(_uccmcl_nodes[i]);
    }
    fclose(in);
    return 0;
}

static void
_uccmcl_unload(void)
{
    if (_uccmcl_size == 0)
      return;

    free((char *) _uccmcl_nodes);
    _uccmcl_size = 0;
}
#endif

krb5_ui_4
uccombining_class(krb5_ui_4 code)
{
    long l, r, m;

    l = 0;
    r = _uccmcl_size - 1;

    while (l <= r) {
        m = (l + r) >> 1;
        m -= (m % 3);
        if (code > _uccmcl_nodes[m + 1])
          l = m + 3;
        else if (code < _uccmcl_nodes[m])
          r = m - 3;
        else if (code >= _uccmcl_nodes[m] && code <= _uccmcl_nodes[m + 1])
          return _uccmcl_nodes[m + 2];
    }
    return 0;
}

/**************************************************************************
 *
 * Support for numeric values.
 *
 **************************************************************************/

#if !HARDCODE_DATA
static krb5_ui_4 *_ucnum_nodes;
static krb5_ui_4 _ucnum_size;
static short *_ucnum_vals;

/*
 * Return -1 on error, 0 if okay
 */
static int
_ucnumb_load(char *paths, int reload)
{
    FILE *in;
    krb5_ui_4 size, i;
    _ucheader_t hdr;

    if (_ucnum_size > 0) {
        if (!reload)
          /*
           * The numbers have already been loaded.
           */
          return 0;

        free((char *) _ucnum_nodes);
        _ucnum_size = 0;
    }

    if ((in = _ucopenfile(paths, "num.dat", "rb")) == 0)
      return -1;

    /*
     * Load the header.
     */
    fread((char *) &hdr, sizeof(_ucheader_t), 1, in);

    if (hdr.bom == 0xfffe) {
        hdr.cnt = endian_short(hdr.cnt);
        hdr.size.bytes = endian_long(hdr.size.bytes);
    }

    _ucnum_size = hdr.cnt;
    _ucnum_nodes = (krb5_ui_4 *) malloc(hdr.size.bytes);
    _ucnum_vals = (short *) (_ucnum_nodes + _ucnum_size);

    /*
     * Read the combining classes in.
     */
    fread((char *) _ucnum_nodes, sizeof(unsigned char), hdr.size.bytes, in);

    /*
     * Do an endian swap if necessary.
     */
    if (hdr.bom == 0xfffe) {
        for (i = 0; i < _ucnum_size; i++)
          _ucnum_nodes[i] = endian_long(_ucnum_nodes[i]);

        /*
         * Determine the number of values that have to be adjusted.
         */
        size = (hdr.size.bytes -
                (_ucnum_size * (sizeof(krb5_ui_4) << 1))) /
            sizeof(short);

        for (i = 0; i < size; i++)
          _ucnum_vals[i] = endian_short(_ucnum_vals[i]);
    }
    fclose(in);
    return 0;
}

static void
_ucnumb_unload(void)
{
    if (_ucnum_size == 0)
      return;

    free((char *) _ucnum_nodes);
    _ucnum_size = 0;
}
#endif

int
ucnumber_lookup(krb5_ui_4 code, struct ucnumber *num)
{
    long l, r, m;
    short *vp;

    l = 0;
    r = _ucnum_size - 1;
    while (l <= r) {
        /*
         * Determine a "mid" point and adjust to make sure the mid point is at
         * the beginning of a code+offset pair.
         */
        m = (l + r) >> 1;
        m -= (m & 1);
        if (code > _ucnum_nodes[m])
          l = m + 2;
        else if (code < _ucnum_nodes[m])
          r = m - 2;
        else {
            vp = (short *)_ucnum_vals + _ucnum_nodes[m + 1];
            num->numerator = (int) *vp++;
            num->denominator = (int) *vp;
            return 1;
        }
    }
    return 0;
}

int
ucdigit_lookup(krb5_ui_4 code, int *digit)
{
    long l, r, m;
    short *vp;

    l = 0;
    r = _ucnum_size - 1;
    while (l <= r) {
        /*
         * Determine a "mid" point and adjust to make sure the mid point is at
         * the beginning of a code+offset pair.
         */
        m = (l + r) >> 1;
        m -= (m & 1);
        if (code > _ucnum_nodes[m])
          l = m + 2;
        else if (code < _ucnum_nodes[m])
          r = m - 2;
        else {
            vp = (short *)_ucnum_vals + _ucnum_nodes[m + 1];
            if (*vp == *(vp + 1)) {
              *digit = *vp;
              return 1;
            }
            return 0;
        }
    }
    return 0;
}

struct ucnumber
ucgetnumber(krb5_ui_4 code)
{
    struct ucnumber num;

    /*
     * Initialize with some arbitrary value, because the caller simply cannot
     * tell for sure if the code is a number without calling the ucisnumber()
     * macro before calling this function.
     */
    num.numerator = num.denominator = -111;

    (void) ucnumber_lookup(code, &num);

    return num;
}

int
ucgetdigit(krb5_ui_4 code)
{
    int dig;

    /*
     * Initialize with some arbitrary value, because the caller simply cannot
     * tell for sure if the code is a number without calling the ucisdigit()
     * macro before calling this function.
     */
    dig = -111;

    (void) ucdigit_lookup(code, &dig);

    return dig;
}

/**************************************************************************
 *
 * Setup and cleanup routines.
 *
 **************************************************************************/

#if HARDCODE_DATA
int ucdata_load(char *paths, int masks) { return 0; }
void ucdata_unload(int masks) { }
int ucdata_reload(char *paths, int masks) { return 0; }
#else
/*
 * Return 0 if okay, negative on error
 */
int
ucdata_load(char *paths, int masks)
{
    int error = 0;

    if (masks & UCDATA_CTYPE)
      error |= _ucprop_load(paths, 0) < 0 ? UCDATA_CTYPE : 0;
    if (masks & UCDATA_CASE)
      error |= _uccase_load(paths, 0) < 0 ? UCDATA_CASE : 0;
    if (masks & UCDATA_DECOMP)
      error |= _ucdcmp_load(paths, 0) < 0 ? UCDATA_DECOMP : 0;
    if (masks & UCDATA_CMBCL)
      error |= _uccmcl_load(paths, 0) < 0 ? UCDATA_CMBCL : 0;
    if (masks & UCDATA_NUM)
      error |= _ucnumb_load(paths, 0) < 0 ? UCDATA_NUM : 0;
    if (masks & UCDATA_COMP)
      error |= _uccomp_load(paths, 0) < 0 ? UCDATA_COMP : 0;
    if (masks & UCDATA_KDECOMP)
      error |= _uckdcmp_load(paths, 0) < 0 ? UCDATA_KDECOMP : 0;

    return -error;
}

void
ucdata_unload(int masks)
{
    if (masks & UCDATA_CTYPE)
      _ucprop_unload();
    if (masks & UCDATA_CASE)
      _uccase_unload();
    if (masks & UCDATA_DECOMP)
      _ucdcmp_unload();
    if (masks & UCDATA_CMBCL)
      _uccmcl_unload();
    if (masks & UCDATA_NUM)
      _ucnumb_unload();
    if (masks & UCDATA_COMP)
      _uccomp_unload();
    if (masks & UCDATA_KDECOMP)
      _uckdcmp_unload();
}

/*
 * Return 0 if okay, negative on error
 */
int
ucdata_reload(char *paths, int masks)
{
    int error = 0;

    if (masks & UCDATA_CTYPE)
        error |= _ucprop_load(paths, 1) < 0 ? UCDATA_CTYPE : 0;
    if (masks & UCDATA_CASE)
        error |= _uccase_load(paths, 1) < 0 ? UCDATA_CASE : 0;
    if (masks & UCDATA_DECOMP)
        error |= _ucdcmp_load(paths, 1) < 0 ? UCDATA_DECOMP : 0;
    if (masks & UCDATA_CMBCL)
        error |= _uccmcl_load(paths, 1) < 0 ? UCDATA_CMBCL : 0;
    if (masks & UCDATA_NUM)
        error |= _ucnumb_load(paths, 1) < 0 ? UCDATA_NUM : 0;
    if (masks & UCDATA_COMP)
        error |= _uccomp_load(paths, 1) < 0 ? UCDATA_COMP : 0;
    if (masks & UCDATA_KDECOMP)
        error |= _uckdcmp_load(paths, 1) < 0 ? UCDATA_KDECOMP : 0;

    return -error;
}
#endif

#ifdef TEST

void
main(void)
{
    int dig;
    krb5_ui_4 i, lo, *dec;
    struct ucnumber num;

/*    ucdata_setup("."); */

    if (ucisweak(0x30))
      printf("WEAK\n");
    else
      printf("NOT WEAK\n");

    printf("LOWER 0x%04lX\n", uctolower(0xff3a));
    printf("UPPER 0x%04lX\n", uctoupper(0xff5a));

    if (ucisalpha(0x1d5))
      printf("ALPHA\n");
    else
      printf("NOT ALPHA\n");

    if (ucisupper(0x1d5)) {
        printf("UPPER\n");
        lo = uctolower(0x1d5);
        printf("0x%04lx\n", lo);
        lo = uctotitle(0x1d5);
        printf("0x%04lx\n", lo);
    } else
      printf("NOT UPPER\n");

    if (ucistitle(0x1d5))
      printf("TITLE\n");
    else
      printf("NOT TITLE\n");

    if (uciscomposite(0x1d5))
      printf("COMPOSITE\n");
    else
      printf("NOT COMPOSITE\n");

    if (ucdecomp(0x1d5, &lo, &dec)) {
        for (i = 0; i < lo; i++)
          printf("0x%04lx ", dec[i]);
        putchar('\n');
    }

    if ((lo = uccombining_class(0x41)) != 0)
      printf("0x41 CCL %ld\n", lo);

    if (ucisxdigit(0xfeff))
      printf("0xFEFF HEX DIGIT\n");
    else
      printf("0xFEFF NOT HEX DIGIT\n");

    if (ucisdefined(0x10000))
      printf("0x10000 DEFINED\n");
    else
      printf("0x10000 NOT DEFINED\n");

    if (ucnumber_lookup(0x30, &num)) {
        if (num.denominator != 1)
          printf("UCNUMBER: 0x30 = %d/%d\n", num.numerator, num.denominator);
        else
          printf("UCNUMBER: 0x30 = %d\n", num.numerator);
    } else
      printf("UCNUMBER: 0x30 NOT A NUMBER\n");

    if (ucnumber_lookup(0xbc, &num)) {
        if (num.denominator != 1)
          printf("UCNUMBER: 0xbc = %d/%d\n", num.numerator, num.denominator);
        else
          printf("UCNUMBER: 0xbc = %d\n", num.numerator);
    } else
      printf("UCNUMBER: 0xbc NOT A NUMBER\n");


    if (ucnumber_lookup(0xff19, &num)) {
        if (num.denominator != 1)
          printf("UCNUMBER: 0xff19 = %d/%d\n", num.numerator, num.denominator);
        else
          printf("UCNUMBER: 0xff19 = %d\n", num.numerator);
    } else
      printf("UCNUMBER: 0xff19 NOT A NUMBER\n");

    if (ucnumber_lookup(0x4e00, &num)) {
        if (num.denominator != 1)
          printf("UCNUMBER: 0x4e00 = %d/%d\n", num.numerator, num.denominator);
        else
          printf("UCNUMBER: 0x4e00 = %d\n", num.numerator);
    } else
      printf("UCNUMBER: 0x4e00 NOT A NUMBER\n");

    if (ucdigit_lookup(0x06f9, &dig))
      printf("UCDIGIT: 0x6f9 = %d\n", dig);
    else
      printf("UCDIGIT: 0x6f9 NOT A NUMBER\n");

    dig = ucgetdigit(0x0969);
    printf("UCGETDIGIT: 0x969 = %d\n", dig);

    num = ucgetnumber(0x30);
    if (num.denominator != 1)
      printf("UCGETNUMBER: 0x30 = %d/%d\n", num.numerator, num.denominator);
    else
      printf("UCGETNUMBER: 0x30 = %d\n", num.numerator);

    num = ucgetnumber(0xbc);
    if (num.denominator != 1)
      printf("UCGETNUMBER: 0xbc = %d/%d\n", num.numerator, num.denominator);
    else
      printf("UCGETNUMBER: 0xbc = %d\n", num.numerator);

    num = ucgetnumber(0xff19);
    if (num.denominator != 1)
      printf("UCGETNUMBER: 0xff19 = %d/%d\n", num.numerator, num.denominator);
    else
      printf("UCGETNUMBER: 0xff19 = %d\n", num.numerator);

/*    ucdata_cleanup(); */
    exit(0);
}

#endif /* TEST */
