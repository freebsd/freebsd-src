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
spdxtool_simplelicensing_licenseExpression_new(pkgconf_client_t *client, char *license)
{
	spdxtool_simplelicensing_license_expression_t *expression = NULL;

	if(!client || !license)
	{
		return NULL;
	}

	expression = calloc(1, sizeof(spdxtool_simplelicensing_license_expression_t));

	if(!expression)
	{
		pkgconf_error(client, "Memory exhausted! Can't create simplelicense_expression struct!");
		return NULL;
	}

	expression->type = "simplelicensing_LicenseExpression";
	expression->license_expression = strdup(license);
	expression->spdx_id = spdxtool_util_get_spdx_id_string(client, expression->type, license);

	return expression;
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_simplelicensing_licenseExpression_free(spdxtool_simplelicensing_license_expression_t *expression_struct)
 *
 *    Free /SimpleLicensing/SimpleLicensingText struct
 *
 *    :param spdxtool_simplelicensing_license_expression_t *expression_struct: SimpleLicensingText struct to be freed.
 *    :return: nothing
 */
void
spdxtool_simplelicensing_licenseExpression_free(spdxtool_simplelicensing_license_expression_t *expression_struct)
{
	if(!expression_struct)
	{
		return;
	}

	if(expression_struct->spdx_id)
	{
		free(expression_struct->spdx_id);
		expression_struct->spdx_id = NULL;
	}

	if(expression_struct->license_expression)
	{
		free(expression_struct->license_expression);
		expression_struct->license_expression = NULL;
	}

	free(expression_struct);
}

/*
 * !doc
 *
 * .. c:function:: void spdxtool_simplelicensing_licenseExpression_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_spdx_document_t *spdx_struct, bool last)
 *
 *    Serialize /SimpleLicensing/SimpleLicensingText struct to JSON
 *
 *    :param pkgconf_client_t *client: The pkgconf client being accessed.
 *    :param pkgconf_buffer_t *buffer: Buffer where struct is serialized
 *    :param spdxtool_core_spdx_document_t *spdx_struct: SimpleLicensingText struct to be serialized
 *    :param bool last: Is this last CreationInfo struct or does it need comma at the end. True comma False not
 *    :return: nothing
 */
void
spdxtool_simplelicensing_licenseExpression_serialize(pkgconf_client_t *client, pkgconf_buffer_t *buffer, spdxtool_core_spdx_document_t *spdx_struct, bool last)
{
	pkgconf_node_t *iter = NULL;

	(void) last;

	PKGCONF_FOREACH_LIST_ENTRY(spdx_struct->licenses.head, iter)
	{
		spdxtool_simplelicensing_license_expression_t *expression = iter->data;

		spdxtool_serialize_obj_start(buffer, 2);
		spdxtool_serialize_parm_and_string(buffer, "@type", "simplelicensing_LicenseExpression", 3, true);
		spdxtool_serialize_parm_and_string(buffer, "creationInfo", spdx_struct->creation_info, 3, true);
		spdxtool_serialize_parm_and_string(buffer, "spdxId",expression->spdx_id, 3, true);
		spdxtool_serialize_parm_and_string(buffer, "simplelicensing_licenseExpression",  expression->license_expression, 3, false);
		spdxtool_serialize_obj_end(buffer, 2, true);

		spdxtool_core_spdx_document_add_element(client, spdx_struct, strdup(expression->spdx_id));
	}
}
