/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <ctf_impl.h>

void
ctf_get_ctt_index(const ctf_file_t *fp, const void *v, uint_t *indexp,
    uint_t *typep, int *ischildp)
{
	uint_t index, type;
	int ischild;

	if (fp->ctf_version == CTF_VERSION_2) {
		const struct ctf_type_v2 *ctt = v;

		type = ctt->ctt_type;
		index = CTF_V2_TYPE_TO_INDEX(ctt->ctt_type);
		ischild = CTF_V2_TYPE_ISCHILD(ctt->ctt_type);
	} else {
		const struct ctf_type_v3 *ctt = v;

		type = ctt->ctt_type;
		index = CTF_V3_TYPE_TO_INDEX(ctt->ctt_type);
		ischild = CTF_V3_TYPE_ISCHILD(ctt->ctt_type);
	}

	if (indexp != NULL)
		*indexp = index;
	if (typep != NULL)
		*typep = type;
	if (ischildp != NULL)
		*ischildp = ischild;
}

void
ctf_get_ctt_info(const ctf_file_t *fp, const void *v, uint_t *kindp,
    uint_t *vlenp, int *isrootp)
{
	uint_t kind, vlen;
	int isroot;

	if (fp->ctf_version == CTF_VERSION_2) {
		const struct ctf_type_v2 *ctt = v;

		kind = CTF_V2_INFO_KIND(ctt->ctt_info);
		vlen = CTF_V2_INFO_VLEN(ctt->ctt_info);
		isroot = CTF_V2_INFO_ISROOT(ctt->ctt_info);
	} else {
		const struct ctf_type_v3 *ctt = v;

		kind = CTF_V3_INFO_KIND(ctt->ctt_info);
		vlen = CTF_V3_INFO_VLEN(ctt->ctt_info);
		isroot = CTF_V3_INFO_ISROOT(ctt->ctt_info);
	}

	if (kindp != NULL)
		*kindp = kind;
	if (vlenp != NULL)
		*vlenp = vlen;
	if (isrootp != NULL)
		*isrootp = isroot;
}

ssize_t
ctf_get_ctt_size(const ctf_file_t *fp, const void *v, ssize_t *sizep,
    ssize_t *incrementp)
{
	ssize_t size, increment;

	if (fp->ctf_version == CTF_VERSION_2) {
		const struct ctf_type_v2 *ctt = v;

		if (ctt->ctt_size == CTF_V2_LSIZE_SENT) {
			size = (size_t)CTF_TYPE_LSIZE(ctt);
			increment = sizeof (struct ctf_type_v2);
		} else {
			size = ctt->ctt_size;
			increment = sizeof (struct ctf_stype_v2);
		}
	} else {
		const struct ctf_type_v3 *ctt = v;

		if (ctt->ctt_size == CTF_V3_LSIZE_SENT) {
			size = (size_t)CTF_TYPE_LSIZE(ctt);
			increment = sizeof (struct ctf_type_v3);
		} else {
			size = ctt->ctt_size;
			increment = sizeof (struct ctf_stype_v3);
		}
	}

	if (sizep)
		*sizep = size;
	if (incrementp)
		*incrementp = increment;

	return (size);
}

/*
 * Fetch info for a struct or union member.
 */
void
ctf_get_ctm_info(const ctf_file_t *fp, const void *v, size_t size,
    size_t *incrementp, uint_t *typep, ulong_t *offsetp, const char **namep)
{
	size_t increment;
	ulong_t offset;
	uint_t name, type;

	if (fp->ctf_version == CTF_VERSION_2) {
		if (size < CTF_V2_LSTRUCT_THRESH) {
			const struct ctf_member_v2 *ctm = v;

			name = ctm->ctm_name;
			type = ctm->ctm_type;
			offset = ctm->ctm_offset;
			increment = sizeof(*ctm);
		} else {
			const struct ctf_lmember_v2 *ctlm = v;

			name = ctlm->ctlm_name;
			type = ctlm->ctlm_type;
			offset = (ulong_t)CTF_LMEM_OFFSET(ctlm);
			increment = sizeof(*ctlm);
		}
	} else {
		if (size < CTF_V3_LSTRUCT_THRESH) {
			const struct ctf_member_v3 *ctm = v;

			name = ctm->ctm_name;
			type = ctm->ctm_type;
			offset = ctm->ctm_offset;
			increment = sizeof(*ctm);
		} else {
			const struct ctf_lmember_v3 *ctlm = v;

			name = ctlm->ctlm_name;
			type = ctlm->ctlm_type;
			offset = (ulong_t)CTF_LMEM_OFFSET(ctlm);
			increment = sizeof(*ctlm);
		}
	}

	if (incrementp != NULL)
		*incrementp = increment;
	if (typep != NULL)
		*typep = type;
	if (offsetp != NULL)
		*offsetp = offset;
	if (namep != NULL)
		*namep = ctf_strraw(fp, name);
}

/*
 * Iterate over the members of a STRUCT or UNION.  We pass the name, member
 * type, and offset of each member to the specified callback function.
 */
int
ctf_member_iter(ctf_file_t *fp, ctf_id_t type, ctf_member_f *func, void *arg)
{
	ctf_file_t *ofp = fp;
	const void *tp;
	ssize_t size, increment;
	uint_t kind, n, vlen;
	int rc;

	if ((type = ctf_type_resolve(fp, type)) == CTF_ERR)
		return (CTF_ERR); /* errno is set for us */

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (CTF_ERR); /* errno is set for us */

	(void) ctf_get_ctt_size(fp, tp, &size, &increment);
	ctf_get_ctt_info(fp, tp, &kind, &vlen, NULL);

	if (kind != CTF_K_STRUCT && kind != CTF_K_UNION)
		return (ctf_set_errno(ofp, ECTF_NOTSOU));

	const char *mp = (const char *)((uintptr_t)tp + increment);

	for (n = vlen; n != 0; n--, mp += increment) {
		const char *name;
		ulong_t offset;
		uint_t type;

		ctf_get_ctm_info(fp, mp, size, &increment, &type, &offset,
		    &name);
		if ((rc = func(name, type, offset, arg)) != 0)
			return (rc);
	}

	return (0);
}

/*
 * Iterate over the members of an ENUM.  We pass the string name and associated
 * integer value of each enum element to the specified callback function.
 */
int
ctf_enum_iter(ctf_file_t *fp, ctf_id_t type, ctf_enum_f *func, void *arg)
{
	ctf_file_t *ofp = fp;
	const void *tp;
	const ctf_enum_t *ep;
	ssize_t increment;
	uint_t kind, n, vlen;
	int rc;

	if ((type = ctf_type_resolve(fp, type)) == CTF_ERR)
		return (CTF_ERR); /* errno is set for us */

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (CTF_ERR); /* errno is set for us */

	ctf_get_ctt_info(fp, tp, &kind, &vlen, NULL);
	if (kind != CTF_K_ENUM)
		return (ctf_set_errno(ofp, ECTF_NOTENUM));

	(void) ctf_get_ctt_size(fp, tp, NULL, &increment);

	ep = (const ctf_enum_t *)((uintptr_t)tp + increment);

	for (n = vlen; n != 0; n--, ep++) {
		const char *name = ctf_strptr(fp, ep->cte_name);
		if ((rc = func(name, ep->cte_value, arg)) != 0)
			return (rc);
	}

	return (0);
}

/*
 * Iterate over every root (user-visible) type in the given CTF container.
 * We pass the type ID of each type to the specified callback function.
 */
int
ctf_type_iter(ctf_file_t *fp, ctf_type_f *func, void *arg)
{
	ctf_id_t id, max = fp->ctf_typemax;
	int rc, child = (fp->ctf_flags & LCTF_CHILD);
	int isroot;

	for (id = 1; id <= max; id++) {
		const void *tp = LCTF_INDEX_TO_TYPEPTR(fp, id);
		ctf_get_ctt_info(fp, tp, NULL, NULL, &isroot);
		if (isroot &&
		    (rc = func(LCTF_INDEX_TO_TYPE(fp, id, child), arg)) != 0)
			return (rc);
	}

	return (0);
}

/*
 * Follow a given type through the graph for TYPEDEF, VOLATILE, CONST, and
 * RESTRICT nodes until we reach a "base" type node.  This is useful when
 * we want to follow a type ID to a node that has members or a size.  To guard
 * against infinite loops, we implement simplified cycle detection and check
 * each link against itself, the previous node, and the topmost node.
 */
ctf_id_t
ctf_type_resolve(ctf_file_t *fp, ctf_id_t type)
{
	ctf_id_t prev = type, otype = type;
	ctf_file_t *ofp = fp;
	const void *tp;
	uint_t kind, ctype;

	while ((tp = ctf_lookup_by_id(&fp, type)) != NULL) {
		ctf_get_ctt_info(fp, tp, &kind, NULL, NULL);
		switch (kind) {
		case CTF_K_TYPEDEF:
		case CTF_K_VOLATILE:
		case CTF_K_CONST:
		case CTF_K_RESTRICT:
			ctf_get_ctt_index(fp, tp, NULL, &ctype, NULL);
			if (ctype == type || ctype == otype || ctype == prev) {
				ctf_dprintf("type %ld cycle detected\n", otype);
				return (ctf_set_errno(ofp, ECTF_CORRUPT));
			}
			prev = type;
			type = ctype;
			break;
		default:
			return (type);
		}
	}

	return (CTF_ERR); /* errno is set for us */
}

/*
 * Lookup the given type ID and print a string name for it into buf.  Return
 * the actual number of bytes (not including \0) needed to format the name.
 */
static ssize_t
ctf_type_qlname(ctf_file_t *fp, ctf_id_t type, char *buf, size_t len,
    const char *qname)
{
	ctf_decl_t cd;
	ctf_decl_node_t *cdp;
	ctf_decl_prec_t prec, lp, rp;
	int ptr, arr;
	uint_t k;

	if (fp == NULL && type == CTF_ERR)
		return (-1); /* simplify caller code by permitting CTF_ERR */

	ctf_decl_init(&cd, buf, len);
	ctf_decl_push(&cd, fp, type);

	if (cd.cd_err != 0) {
		ctf_decl_fini(&cd);
		return (ctf_set_errno(fp, cd.cd_err));
	}

	/*
	 * If the type graph's order conflicts with lexical precedence order
	 * for pointers or arrays, then we need to surround the declarations at
	 * the corresponding lexical precedence with parentheses.  This can
	 * result in either a parenthesized pointer (*) as in int (*)() or
	 * int (*)[], or in a parenthesized pointer and array as in int (*[])().
	 */
	ptr = cd.cd_order[CTF_PREC_POINTER] > CTF_PREC_POINTER;
	arr = cd.cd_order[CTF_PREC_ARRAY] > CTF_PREC_ARRAY;

	rp = arr ? CTF_PREC_ARRAY : ptr ? CTF_PREC_POINTER : -1;
	lp = ptr ? CTF_PREC_POINTER : arr ? CTF_PREC_ARRAY : -1;

	k = CTF_K_POINTER; /* avoid leading whitespace (see below) */

	for (prec = CTF_PREC_BASE; prec < CTF_PREC_MAX; prec++) {
		for (cdp = ctf_list_next(&cd.cd_nodes[prec]);
		    cdp != NULL; cdp = ctf_list_next(cdp)) {

			ctf_file_t *rfp = fp;
			const void *tp = ctf_lookup_by_id(&rfp, cdp->cd_type);
			const char *name = ctf_type_rname(rfp, tp);

			if (k != CTF_K_POINTER && k != CTF_K_ARRAY)
				ctf_decl_sprintf(&cd, " ");

			if (lp == prec) {
				ctf_decl_sprintf(&cd, "(");
				lp = -1;
			}

			switch (cdp->cd_kind) {
			case CTF_K_INTEGER:
			case CTF_K_FLOAT:
			case CTF_K_TYPEDEF:
				if (qname != NULL)
					ctf_decl_sprintf(&cd, "%s`", qname);
				ctf_decl_sprintf(&cd, "%s", name);
				break;
			case CTF_K_POINTER:
				ctf_decl_sprintf(&cd, "*");
				break;
			case CTF_K_ARRAY:
				ctf_decl_sprintf(&cd, "[%u]", cdp->cd_n);
				break;
			case CTF_K_FUNCTION:
				ctf_decl_sprintf(&cd, "()");
				break;
			case CTF_K_STRUCT:
			case CTF_K_FORWARD:
				ctf_decl_sprintf(&cd, "struct ");
				if (qname != NULL)
					ctf_decl_sprintf(&cd, "%s`", qname);
				ctf_decl_sprintf(&cd, "%s", name);
				break;
			case CTF_K_UNION:
				ctf_decl_sprintf(&cd, "union ");
				if (qname != NULL)
					ctf_decl_sprintf(&cd, "%s`", qname);
				ctf_decl_sprintf(&cd, "%s", name);
				break;
			case CTF_K_ENUM:
				ctf_decl_sprintf(&cd, "enum ");
				if (qname != NULL)
					ctf_decl_sprintf(&cd, "%s`", qname);
				ctf_decl_sprintf(&cd, "%s", name);
				break;
			case CTF_K_VOLATILE:
				ctf_decl_sprintf(&cd, "volatile");
				break;
			case CTF_K_CONST:
				ctf_decl_sprintf(&cd, "const");
				break;
			case CTF_K_RESTRICT:
				ctf_decl_sprintf(&cd, "restrict");
				break;
			}

			k = cdp->cd_kind;
		}

		if (rp == prec)
			ctf_decl_sprintf(&cd, ")");
	}

	if (cd.cd_len >= len)
		(void) ctf_set_errno(fp, ECTF_NAMELEN);

	ctf_decl_fini(&cd);
	return (cd.cd_len);
}

ssize_t
ctf_type_lname(ctf_file_t *fp, ctf_id_t type, char *buf, size_t len)
{
	return (ctf_type_qlname(fp, type, buf, len, NULL));
}

/*
 * Lookup the given type ID and print a string name for it into buf.  If buf
 * is too small, return NULL: the ECTF_NAMELEN error is set on 'fp' for us.
 */
char *
ctf_type_name(ctf_file_t *fp, ctf_id_t type, char *buf, size_t len)
{
	ssize_t rv = ctf_type_qlname(fp, type, buf, len, NULL);
	return (rv >= 0 && rv < len ? buf : NULL);
}

char *
ctf_type_qname(ctf_file_t *fp, ctf_id_t type, char *buf, size_t len,
    const char *qname)
{
	ssize_t rv = ctf_type_qlname(fp, type, buf, len, qname);
	return (rv >= 0 && rv < len ? buf : NULL);
}

const char *
ctf_type_rname(ctf_file_t *fp, const void *v)
{
	uint_t name;

	if (fp->ctf_version == CTF_VERSION_2) {
		const struct ctf_type_v2 *ctt = v;

		name = ctt->ctt_name;
	} else {
		const struct ctf_type_v3 *ctt = v;

		name = ctt->ctt_name;
	}

	return (ctf_strptr(fp, name));
}

/*
 * Resolve the type down to a base type node, and then return the size
 * of the type storage in bytes.
 */
ssize_t
ctf_type_size(ctf_file_t *fp, ctf_id_t type)
{
	const void *tp;
	ssize_t size;
	ctf_arinfo_t ar;
	uint_t kind;

	if ((type = ctf_type_resolve(fp, type)) == CTF_ERR)
		return (-1); /* errno is set for us */

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (-1); /* errno is set for us */

	ctf_get_ctt_info(fp, tp, &kind, NULL, NULL);

	switch (kind) {
	case CTF_K_POINTER:
		return (fp->ctf_dmodel->ctd_pointer);

	case CTF_K_FUNCTION:
		return (0); /* function size is only known by symtab */

	case CTF_K_ENUM:
		return (fp->ctf_dmodel->ctd_int);

	case CTF_K_ARRAY:
		/*
		 * Array size is not directly returned by stabs data.  Instead,
		 * it defines the element type and requires the user to perform
		 * the multiplication.  If ctf_get_ctt_size() returns zero, the
		 * current version of ctfconvert does not compute member sizes
		 * and we compute the size here on its behalf.
		 */
		if ((size = ctf_get_ctt_size(fp, tp, NULL, NULL)) > 0)
			return (size);

		if (ctf_array_info(fp, type, &ar) == CTF_ERR ||
		    (size = ctf_type_size(fp, ar.ctr_contents)) == CTF_ERR)
			return (-1); /* errno is set for us */

		return (size * ar.ctr_nelems);

	default:
		return (ctf_get_ctt_size(fp, tp, NULL, NULL));
	}
}

/*
 * Resolve the type down to a base type node, and then return the alignment
 * needed for the type storage in bytes.
 */
ssize_t
ctf_type_align(ctf_file_t *fp, ctf_id_t type)
{
	const void *tp;
	ctf_arinfo_t r;
	uint_t kind, vlen;

	if ((type = ctf_type_resolve(fp, type)) == CTF_ERR)
		return (-1); /* errno is set for us */

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (-1); /* errno is set for us */

	ctf_get_ctt_info(fp, tp, &kind, &vlen, NULL);

	switch (kind) {
	case CTF_K_POINTER:
	case CTF_K_FUNCTION:
		return (fp->ctf_dmodel->ctd_pointer);

	case CTF_K_ARRAY:
		if (ctf_array_info(fp, type, &r) == CTF_ERR)
			return (-1); /* errno is set for us */
		return (ctf_type_align(fp, r.ctr_contents));

	case CTF_K_STRUCT:
	case CTF_K_UNION: {
		uint_t n = vlen;
		ssize_t size, increment;
		size_t align = 0;
		const void *vmp;

		(void) ctf_get_ctt_size(fp, tp, &size, &increment);
		vmp = (uchar_t *)tp + increment;

		if (kind == CTF_K_STRUCT)
			n = MIN(n, 1); /* only use first member for structs */

		for (const char *mp = vmp; n != 0; n--, mp += increment) {
			uint_t type;

			ctf_get_ctm_info(fp, mp, size, &increment, &type,
			    NULL, NULL);
			ssize_t am = ctf_type_align(fp, type);
			align = MAX(align, am);
		}

		return (align);
	}

	case CTF_K_ENUM:
		return (fp->ctf_dmodel->ctd_int);

	default:
		return (ctf_get_ctt_size(fp, tp, NULL, NULL));
	}
}

/*
 * Return the kind (CTF_K_* constant) for the specified type ID.
 */
int
ctf_type_kind(ctf_file_t *fp, ctf_id_t type)
{
	const void *tp;
	uint_t kind;

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (CTF_ERR); /* errno is set for us */

	ctf_get_ctt_info(fp, tp, &kind, NULL, NULL);

	return (kind);
}

/*
 * If the type is one that directly references another type (such as POINTER),
 * then return the ID of the type to which it refers.
 */
ctf_id_t
ctf_type_reference(ctf_file_t *fp, ctf_id_t type)
{
	ctf_file_t *ofp = fp;
	const void *tp;
	uint_t ctype, kind;

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (CTF_ERR); /* errno is set for us */

	ctf_get_ctt_info(fp, tp, &kind, NULL, NULL);

	switch (kind) {
	case CTF_K_POINTER:
	case CTF_K_TYPEDEF:
	case CTF_K_VOLATILE:
	case CTF_K_CONST:
	case CTF_K_RESTRICT:
		ctf_get_ctt_index(fp, tp, NULL, &ctype, NULL);
		return (ctype);
	default:
		return (ctf_set_errno(ofp, ECTF_NOTREF));
	}
}

/*
 * Find a pointer to type by looking in fp->ctf_ptrtab.  If we can't find a
 * pointer to the given type, see if we can compute a pointer to the type
 * resulting from resolving the type down to its base type and use that
 * instead.  This helps with cases where the CTF data includes "struct foo *"
 * but not "foo_t *" and the user accesses "foo_t *" in the debugger.
 */
ctf_id_t
ctf_type_pointer(ctf_file_t *fp, ctf_id_t type)
{
	ctf_file_t *ofp = fp;
	ctf_id_t ntype;

	if (ctf_lookup_by_id(&fp, type) == NULL)
		return (CTF_ERR); /* errno is set for us */

	if ((ntype = fp->ctf_ptrtab[LCTF_TYPE_TO_INDEX(fp, type)]) != 0)
		return (LCTF_INDEX_TO_TYPE(fp, ntype, (fp->ctf_flags & LCTF_CHILD)));

	if ((type = ctf_type_resolve(fp, type)) == CTF_ERR)
		return (ctf_set_errno(ofp, ECTF_NOTYPE));

	if (ctf_lookup_by_id(&fp, type) == NULL)
		return (ctf_set_errno(ofp, ECTF_NOTYPE));

	if ((ntype = fp->ctf_ptrtab[LCTF_TYPE_TO_INDEX(fp, type)]) != 0)
		return (LCTF_INDEX_TO_TYPE(fp, ntype, (fp->ctf_flags & LCTF_CHILD)));

	return (ctf_set_errno(ofp, ECTF_NOTYPE));
}

/*
 * Return the encoding for the specified INTEGER or FLOAT.
 */
int
ctf_type_encoding(ctf_file_t *fp, ctf_id_t type, ctf_encoding_t *ep)
{
	ctf_file_t *ofp = fp;
	const void *tp;
	ssize_t increment;
	uint_t data, kind;

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (CTF_ERR); /* errno is set for us */

	(void) ctf_get_ctt_size(fp, tp, NULL, &increment);
	ctf_get_ctt_info(fp, tp, &kind, NULL, NULL);

	switch (kind) {
	case CTF_K_INTEGER:
		data = *(const uint_t *)((uintptr_t)tp + increment);
		ep->cte_format = CTF_INT_ENCODING(data);
		ep->cte_offset = CTF_INT_OFFSET(data);
		ep->cte_bits = CTF_INT_BITS(data);
		break;
	case CTF_K_FLOAT:
		data = *(const uint_t *)((uintptr_t)tp + increment);
		ep->cte_format = CTF_FP_ENCODING(data);
		ep->cte_offset = CTF_FP_OFFSET(data);
		ep->cte_bits = CTF_FP_BITS(data);
		break;
	default:
		return (ctf_set_errno(ofp, ECTF_NOTINTFP));
	}

	return (0);
}

int
ctf_type_cmp(ctf_file_t *lfp, ctf_id_t ltype, ctf_file_t *rfp, ctf_id_t rtype)
{
	int rval;

	if (ltype < rtype)
		rval = -1;
	else if (ltype > rtype)
		rval = 1;
	else
		rval = 0;

	if (lfp == rfp)
		return (rval);

	if (LCTF_TYPE_ISPARENT(lfp, ltype) && lfp->ctf_parent != NULL)
		lfp = lfp->ctf_parent;

	if (LCTF_TYPE_ISPARENT(rfp, rtype) && rfp->ctf_parent != NULL)
		rfp = rfp->ctf_parent;

	if (lfp < rfp)
		return (-1);

	if (lfp > rfp)
		return (1);

	return (rval);
}

/*
 * Return a boolean value indicating if two types are compatible integers or
 * floating-pointer values.  This function returns true if the two types are
 * the same, or if they have the same ASCII name and encoding properties.
 * This function could be extended to test for compatibility for other kinds.
 */
int
ctf_type_compat(ctf_file_t *lfp, ctf_id_t ltype,
    ctf_file_t *rfp, ctf_id_t rtype)
{
	const void *ltp, *rtp;
	ctf_encoding_t le, re;
	ctf_arinfo_t la, ra;
	uint_t lkind, rkind;

	if (ctf_type_cmp(lfp, ltype, rfp, rtype) == 0)
		return (1);

	ltype = ctf_type_resolve(lfp, ltype);
	lkind = ctf_type_kind(lfp, ltype);

	rtype = ctf_type_resolve(rfp, rtype);
	rkind = ctf_type_kind(rfp, rtype);

	if (lkind != rkind ||
	    (ltp = ctf_lookup_by_id(&lfp, ltype)) == NULL ||
	    (rtp = ctf_lookup_by_id(&rfp, rtype)) == NULL ||
	    strcmp(ctf_type_rname(lfp, ltp), ctf_type_rname(rfp, rtp)) != 0)
		return (0);

	switch (lkind) {
	case CTF_K_INTEGER:
	case CTF_K_FLOAT:
		return (ctf_type_encoding(lfp, ltype, &le) == 0 &&
		    ctf_type_encoding(rfp, rtype, &re) == 0 &&
		    bcmp(&le, &re, sizeof (ctf_encoding_t)) == 0);
	case CTF_K_POINTER:
		return (ctf_type_compat(lfp, ctf_type_reference(lfp, ltype),
		    rfp, ctf_type_reference(rfp, rtype)));
	case CTF_K_ARRAY:
		return (ctf_array_info(lfp, ltype, &la) == 0 &&
		    ctf_array_info(rfp, rtype, &ra) == 0 &&
		    la.ctr_nelems == ra.ctr_nelems && ctf_type_compat(
		    lfp, la.ctr_contents, rfp, ra.ctr_contents) &&
		    ctf_type_compat(lfp, la.ctr_index, rfp, ra.ctr_index));
	case CTF_K_STRUCT:
	case CTF_K_UNION:
		return (ctf_type_size(lfp, ltype) == ctf_type_size(rfp, rtype));
	case CTF_K_ENUM:
	case CTF_K_FORWARD:
		return (1); /* no other checks required for these type kinds */
	default:
		return (0); /* should not get here since we did a resolve */
	}
}

static int
_ctf_member_info(ctf_file_t *fp, ctf_id_t type, const char *name, ulong_t off,
    ctf_membinfo_t *mip)
{
	ctf_file_t *ofp = fp;
	const void *tp;
	ssize_t size, increment;
	uint_t kind, n, vlen;

	if ((type = ctf_type_resolve(fp, type)) == CTF_ERR)
		return (CTF_ERR); /* errno is set for us */

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (CTF_ERR); /* errno is set for us */

	(void) ctf_get_ctt_size(fp, tp, &size, &increment);
	ctf_get_ctt_info(fp, tp, &kind, &vlen, NULL);

	if (kind != CTF_K_STRUCT && kind != CTF_K_UNION)
		return (ctf_set_errno(ofp, ECTF_NOTSOU));

	const char *mp = (const char *)((uintptr_t)tp + increment);

	for (n = vlen; n != 0; n--, mp += increment) {
		const char *name1;
		ulong_t offset;
		uint_t type;

		ctf_get_ctm_info(fp, mp, size, &increment, &type, &offset,
		    &name1);
		if (name1[0] == '\0' &&
		    _ctf_member_info(fp, type, name, offset + off, mip) == 0)
			return (0);
		if (strcmp(name1, name) == 0) {
			mip->ctm_type = type;
			mip->ctm_offset = offset + off;
			return (0);
		}
	}

	return (ctf_set_errno(ofp, ECTF_NOMEMBNAM));
}

/*
 * Return the type and offset for a given member of a STRUCT or UNION.
 */
int
ctf_member_info(ctf_file_t *fp, ctf_id_t type, const char *name,
    ctf_membinfo_t *mip)
{

	return (_ctf_member_info(fp, type, name, 0, mip));
}

/*
 * Return the array type, index, and size information for the specified ARRAY.
 */
int
ctf_array_info(ctf_file_t *fp, ctf_id_t type, ctf_arinfo_t *arp)
{
	ctf_file_t *ofp = fp;
	const void *ap, *tp;
	ssize_t increment;
	uint_t kind;

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (CTF_ERR); /* errno is set for us */

	ctf_get_ctt_info(fp, tp, &kind, NULL, NULL);

	if (kind != CTF_K_ARRAY)
		return (ctf_set_errno(ofp, ECTF_NOTARRAY));

	(void) ctf_get_ctt_size(fp, tp, NULL, &increment);

	ap = (const void *)((uintptr_t)tp + increment);
	if (fp->ctf_version == CTF_VERSION_2) {
		const struct ctf_array_v2 *ap2 = ap;

		arp->ctr_contents = ap2->cta_contents;
		arp->ctr_index = ap2->cta_index;
		arp->ctr_nelems = ap2->cta_nelems;
	} else {
		const struct ctf_array_v3 *ap3 = ap;

		arp->ctr_contents = ap3->cta_contents;
		arp->ctr_index = ap3->cta_index;
		arp->ctr_nelems = ap3->cta_nelems;
	}

	return (0);
}

/*
 * Convert the specified value to the corresponding enum member name, if a
 * matching name can be found.  Otherwise NULL is returned.
 */
const char *
ctf_enum_name(ctf_file_t *fp, ctf_id_t type, int value)
{
	ctf_file_t *ofp = fp;
	const void *tp;
	const ctf_enum_t *ep;
	ssize_t increment;
	uint_t kind, n, vlen;

	if ((type = ctf_type_resolve(fp, type)) == CTF_ERR)
		return (NULL); /* errno is set for us */

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (NULL); /* errno is set for us */

	ctf_get_ctt_info(fp, tp, &kind, &vlen, NULL);

	if (kind != CTF_K_ENUM) {
		(void) ctf_set_errno(ofp, ECTF_NOTENUM);
		return (NULL);
	}

	(void) ctf_get_ctt_size(fp, tp, NULL, &increment);

	ep = (const ctf_enum_t *)((uintptr_t)tp + increment);

	for (n = vlen; n != 0; n--, ep++) {
		if (ep->cte_value == value)
			return (ctf_strptr(fp, ep->cte_name));
	}

	(void) ctf_set_errno(ofp, ECTF_NOENUMNAM);
	return (NULL);
}

/*
 * Convert the specified enum tag name to the corresponding value, if a
 * matching name can be found.  Otherwise CTF_ERR is returned.
 */
int
ctf_enum_value(ctf_file_t *fp, ctf_id_t type, const char *name, int *valp)
{
	ctf_file_t *ofp = fp;
	const void *tp;
	const ctf_enum_t *ep;
	ssize_t size, increment;
	uint_t kind, n, vlen;

	if ((type = ctf_type_resolve(fp, type)) == CTF_ERR)
		return (CTF_ERR); /* errno is set for us */

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (CTF_ERR); /* errno is set for us */

	ctf_get_ctt_info(fp, tp, &kind, &vlen, NULL);

	if (kind != CTF_K_ENUM) {
		(void) ctf_set_errno(ofp, ECTF_NOTENUM);
		return (CTF_ERR);
	}

	(void) ctf_get_ctt_size(fp, tp, &size, &increment);

	ep = (const ctf_enum_t *)((uintptr_t)tp + increment);

	for (n = vlen; n != 0; n--, ep++) {
		if (strcmp(ctf_strptr(fp, ep->cte_name), name) == 0) {
			if (valp != NULL)
				*valp = ep->cte_value;
			return (0);
		}
	}

	(void) ctf_set_errno(ofp, ECTF_NOENUMNAM);
	return (CTF_ERR);
}

/*
 * Recursively visit the members of any type.  This function is used as the
 * engine for ctf_type_visit, below.  We resolve the input type, recursively
 * invoke ourself for each type member if the type is a struct or union, and
 * then invoke the callback function on the current type.  If any callback
 * returns non-zero, we abort and percolate the error code back up to the top.
 */
static int
ctf_type_rvisit(ctf_file_t *fp, ctf_id_t type, ctf_visit_f *func, void *arg,
    const char *name, ulong_t offset, int depth)
{
	ctf_id_t otype = type;
	const void *tp;
	ssize_t size, increment;
	uint_t kind, n, vlen;
	int rc;

	if ((type = ctf_type_resolve(fp, type)) == CTF_ERR)
		return (CTF_ERR); /* errno is set for us */

	if ((tp = ctf_lookup_by_id(&fp, type)) == NULL)
		return (CTF_ERR); /* errno is set for us */

	if ((rc = func(name, otype, offset, depth, arg)) != 0)
		return (rc);

	ctf_get_ctt_info(fp, tp, &kind, &vlen, NULL);

	if (kind != CTF_K_STRUCT && kind != CTF_K_UNION)
		return (0);

	(void) ctf_get_ctt_size(fp, tp, &size, &increment);

	const char *mp = (const char *)((uintptr_t)tp + increment);
	for (n = vlen; n != 0; n--, mp += increment) {
		const char *name;
		ulong_t offset1;
		uint_t type;

		ctf_get_ctm_info(fp, mp, size, &increment, &type, &offset1,
		    &name);
		if ((rc = ctf_type_rvisit(fp, type, func, arg, name,
		    offset + offset1, depth + 1)) != 0)
			return (rc);
	}

	return (0);
}

/*
 * Recursively visit the members of any type.  We pass the name, member
 * type, and offset of each member to the specified callback function.
 */
int
ctf_type_visit(ctf_file_t *fp, ctf_id_t type, ctf_visit_f *func, void *arg)
{
	return (ctf_type_rvisit(fp, type, func, arg, "", 0, 0));
}
