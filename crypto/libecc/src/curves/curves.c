/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/curves/curves.h>

/*
 * From a null-terminated string 'ec_name' of exact length 'ec_name_len'
 * (including final null character), the function returns a pointer
 * to the parameters for that curve via 'ec_params'. The function returns
 * -1 on error or if the search was unsuccessful. It returns 0 on success.
 * 'ec_params' is not meaningful on error.
 */
int ec_get_curve_params_by_name(const u8 *ec_name, u8 ec_name_len,
				const ec_str_params **ec_s_params)
{
	const ec_str_params *params;
	u8 comp_len, name_len;
	u32 len;
	const ec_mapping *map;
	const u8 *name;
	unsigned int i;
	int ret, check;

	MUST_HAVE((ec_name != NULL), ret, err);
	MUST_HAVE((ec_s_params != NULL), ret, err);
	MUST_HAVE(((ec_name_len > 2) && (ec_name_len <= MAX_CURVE_NAME_LEN)), ret, err);

	/*
	 * User has been warned ec_name_len is expected to include final
	 * null character.
	 */
	ret = local_strnlen((const char *)ec_name, ec_name_len, &len); EG(ret, err);
	comp_len = (u8)len;
	MUST_HAVE(((comp_len + 1) == ec_name_len), ret, err);

	/* Iterate on our list of curves */
	ret = -1;
	for (i = 0; i < EC_CURVES_NUM; i++) {
		map = &ec_maps[i];
		params = map->params;

		MUST_HAVE((params != NULL), ret, err);
		MUST_HAVE((params->name != NULL), ret, err);
		MUST_HAVE((params->name->buf != NULL), ret, err);

		name = params->name->buf;
		name_len = params->name->buflen;

		if (name_len != ec_name_len) {
			continue;
		}

		if ((!are_str_equal((const char *)ec_name, (const char *)name, &check)) && check) {
			(*ec_s_params) = params;
			ret = 0;
			break;
		}
	}

 err:
	return ret;
}

/*
 * From given curve type 'ec_type', the function provides a pointer to the
 * parameters for that curve if it is known, using 'ec_params' out parameter.
 * On error, or if the curve is unknown, the function returns -1, in which
 * case 'ec_params' is not meaningful. The function returns 0 on success.
 */
int ec_get_curve_params_by_type(ec_curve_type ec_type,
				const ec_str_params **ec_s_params)
{
	const ec_str_params *params;
	const ec_mapping *map;
	const u8 *name;
	u32 len;
	u8 name_len;
	unsigned int i;
	int ret;

	MUST_HAVE((ec_s_params != NULL), ret, err);

	ret = -1;
	for (i = 0; i < EC_CURVES_NUM; i++) {
		map = &ec_maps[i];
		params = map->params;

		MUST_HAVE((params != NULL), ret, err);

		if (ec_type == map->type) {
			/* Do some sanity check before returning */
			MUST_HAVE((params->name != NULL), ret, err);
			MUST_HAVE((params->name->buf != NULL), ret, err);

			name = params->name->buf;
			ret = local_strlen((const char *)name, &len); EG(ret, err);
			MUST_HAVE(len < 256, ret, err);
			name_len = (u8)len;

			MUST_HAVE((params->name->buflen == (name_len + 1)), ret, err);

			(*ec_s_params) = params;
			ret = 0;
			break;
		}
	}

err:
	return ret;
}

/*
 * From a null-terminated string 'ec_name' of exact length 'ec_name_len'
 * (including final null character), the function returns the curve type
 * via 'ec_type'. The function returns -1 on error or if the search was
 * unsuccessful. It returns 0 on success. 'ec_types' is not meaningful
 * on error.
 */
int ec_get_curve_type_by_name(const u8 *ec_name, u8 ec_name_len,
			      ec_curve_type *ec_type)
{
	const ec_str_params *params;
	u32 len;
	u8 name_len, comp_len;
	const ec_mapping *map;
	const u8 *name;
	unsigned int i;
	int ret, check;

	/* No need to bother w/ obvious crap */
	MUST_HAVE(((ec_name_len > 2) && (ec_name_len <= MAX_CURVE_NAME_LEN)), ret, err);
	MUST_HAVE((ec_type != NULL), ret, err);
	MUST_HAVE((ec_name != NULL), ret, err);

	/*
	 * User has been warned ec_name_len is expected to include final
	 * null character.
	 */
	ret = local_strnlen((const char *)ec_name, ec_name_len, &len); EG(ret, err);
	MUST_HAVE(len < 256, ret, err);
	comp_len = (u8)len;
	MUST_HAVE(((comp_len + 1) == ec_name_len), ret, err);

	/* Iterate on our list of curves */
	ret = -1;
	for (i = 0; i < EC_CURVES_NUM; i++) {
		map = &ec_maps[i];
		params = map->params;

		MUST_HAVE((params != NULL), ret, err);
		MUST_HAVE((params->name != NULL), ret, err);
		MUST_HAVE((params->name->buf != NULL), ret, err);

		name = params->name->buf;
		name_len = params->name->buflen;

		if (name_len != ec_name_len) {
			continue;
		}

		if((!are_str_equal((const char *)ec_name, (const char *)name, &check)) && check) {
			(*ec_type) = map->type;
			ret = 0;
			break;
		}
	}

 err:
	return ret;
}

/*
 * Given a curve type, the function finds the curve described by given type
 * and write its name (null terminated string) to given output buffer 'out'
 * of length 'outlen'. 0 is returned on success, -1 otherwise.
 */
int ec_get_curve_name_by_type(const ec_curve_type ec_type, u8 *out, u8 outlen)
{
	const ec_str_params *by_type;
	const u8 *name;
	u8 name_len;
	int ret;

	MUST_HAVE((out != NULL), ret, err);

	/* Let's first do the lookup by type */
	ret =  ec_get_curve_params_by_type(ec_type, &by_type); EG(ret, err);

	/* Found a curve for that type. Let's check name matches. */
	MUST_HAVE((by_type != NULL), ret, err);
	MUST_HAVE((by_type->name != NULL), ret, err);
	MUST_HAVE((by_type->name->buf != NULL), ret, err);

	name_len = by_type->name->buflen;
	name = by_type->name->buf;

	/* Not enough room to copy curve name? */
	MUST_HAVE((name_len <= outlen), ret, err);

	ret = local_memcpy(out, name, name_len);

 err:
	return ret;
}

/*
 * The function verifies the coherency between given curve type value and
 * associated name 'ec_name' of length 'ec_name_len' (including final
 * null character). The function returns 0 if the curve type is known
 * and provided name matches expected one. The function returns -1
 * otherwise.
 */
int ec_check_curve_type_and_name(const ec_curve_type ec_type,
				 const u8 *ec_name, u8 ec_name_len)
{
	const ec_str_params *by_type;
	const u8 *name;
	u8 name_len;
	int ret, check;

	/* No need to bother w/ obvious crap */
	MUST_HAVE((ec_name != NULL), ret, err);
	MUST_HAVE(((ec_name_len > 2) && (ec_name_len <= MAX_CURVE_NAME_LEN)), ret, err);

	/* Let's first do the lookup by type */
	ret = ec_get_curve_params_by_type(ec_type, &by_type); EG(ret, err);

	/* Found a curve for that type. Let's check name matches. */
	MUST_HAVE((by_type != NULL), ret, err);
	MUST_HAVE((by_type->name != NULL), ret, err);
	MUST_HAVE((by_type->name->buf != NULL), ret, err);

	name = by_type->name->buf;
	name_len = by_type->name->buflen;

	MUST_HAVE((name_len == ec_name_len), ret, err);

	if ((!are_str_equal((const char *)ec_name, (const char *)name, &check)) && (!check)) {
		ret = -1;
	}

err:
	return ret;
}
