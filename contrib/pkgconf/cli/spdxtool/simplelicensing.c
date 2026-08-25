/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 *​ Copyright (c) 2025 The FreeBSD Foundation
 *​
 *​ Portions of this software were developed by
 * Tuukka Pasanen <tuukka.pasanen@ilmi.fi> under sponsorship from
 * the FreeBSD Foundation
 */

#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "serialize.h"
#include "software.h"
#include "core.h"
#include "simplelicensing.h"

/*
 * !doc
 *
 * .. c:function:: spdxtool_simplelicensing_license_expression_t *spdxtool_simplelicensing_licenseExpression_new(pkgconf_client_t *client, char *license)
 *
 *    Create new /SimpleLicensing/SimpleLicensingText struct
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param char *license: SPDX name of license
 *    :return: NULL if some problem occurs and SimpleLicensingText struct if not
 */
spdxtool_simplelicensing_license_expression_t *
spdxtool_simplelicensing_licenseExpression_new(pkgconf_client_t *client, const char *license)
{
	if (!client || !license)
		return NULL;

	spdxtool_simplelicensing_license_expression_t *expression = calloc(1, sizeof(spdxtool_simplelicensing_license_expression_t));
	if (!expression)
		goto err;

	expression->type = "simplelicensing_LicenseExpression";
	expression->license_expression = strdup(license);
	expression->spdx_id = spdxtool_util_get_spdx_id_string(client, expression->type, license);

	if (!expression->license_expression || !expression->spdx_id)
		goto err;

	return expression;

err:
	pkgconf_error(client, "spdxtool_simplelicensing_licenseExpression_new: out of memory");
	spdxtool_simplelicensing_licenseExpression_free(expression);
	return NULL;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_simplelicensing_licenseExpression_free(spdxtool_simplelicensing_license_expression_t *expression)
 *
 *    Free /SimpleLicensing/SimpleLicensingText struct
 *
 *    :param spdxtool_simplelicensing_license_expression_t *expression: SimpleLicensingText struct to be freed.
 *    :return: nothing
 */
void
spdxtool_simplelicensing_licenseExpression_free(spdxtool_simplelicensing_license_expression_t *expression)
{
	if(!expression)
		return;

	free(expression->spdx_id);
	free(expression->license_expression);

	free(expression);
}

/*
 * !doc
 *
 * .. c:function:: spdxtool_serialize_value_t *spdxtool_simplelicensing_licenseExpression_to_object(const char *creation_info, const spdxtool_simplelicensing_license_expression_t *expression)
 *
 *    Serialize /SimpleLicensing/LicenseExpression struct to a JSON value tree.
 *
 *    :param const char *creation_info: The creationInfo ID string to embed in the object.
 *    :param const spdxtool_simplelicensing_license_expression_t *expression: LicenseExpression struct to be serialized.
 *    :return: spdxtool_serialize_value_t * representing the LicenseExpression object.
 */
spdxtool_serialize_value_t *
spdxtool_simplelicensing_licenseExpression_to_object(pkgconf_client_t *client, const char *creation_info, const spdxtool_simplelicensing_license_expression_t *expression)
{
	spdxtool_serialize_value_t *ret = NULL;
	spdxtool_serialize_object_list_t *object_list = spdxtool_serialize_object_list_new();
	if (!object_list)
		goto err;

	if (!(spdxtool_serialize_object_add_string(object_list, "type", "simplelicensing_LicenseExpression") &&
		spdxtool_serialize_object_add_string(object_list, "creationInfo", creation_info) &&
		spdxtool_serialize_object_add_string(object_list, "spdxId", expression->spdx_id) &&
		spdxtool_serialize_object_add_string(object_list, "simplelicensing_licenseExpression", expression->license_expression)))
	{
		goto err;
	}

	ret = spdxtool_serialize_value_object(object_list);
	object_list = NULL;

err:
	if (!ret)
		pkgconf_error(client, "spdxtool_simplelicensing_licenseExpression_to_object: out of memory");

	spdxtool_serialize_object_list_free(object_list);
	return ret;
}
