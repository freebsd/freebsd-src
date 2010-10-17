/*
 * Copyright (c) 2002-2006, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under all of the following licenses:
 * 
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see 
 *    http://www.opensource.org/licenses/cpl.php.
 * 
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 * 
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is in the file LICENSE3.txt in the root directory. The
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 * 
 * Licensee has the right to choose one of the above licenses.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain both the above copyright
 * notice and one of the license notices.
 * 
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of Network Appliance, Inc. nor the names of other DAT
 * Collaborative contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 */

/****************************************************************
 *
 * HEADER: dat_registry.h
 *
 * PURPOSE: DAT registration API signatures
 *
 * Description: Header file for "DAPL: Direct Access Programming
 *		Library, Version: 2.0"
 *
 * 		Contains registration external reference signatures
 * 		for dat registry functions. This file is *only*
 * 		included by providers, not consumers.
 *
 * Mapping rules:
 * 	All global symbols are prepended with DAT_ or dat_
 * 	All DAT objects have an 'api' tag which, such as 'ep' or 'lmr'
 * 	The method table is in the provider definition structure.
 *
 **********************************************************/
#ifndef _DAT_REGISTRY_H_
#define _DAT_REGISTRY_H_

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_UDAT_H_)
#include <dat2/udat_redirection.h>
#elif defined(_KDAT_H_)
#include <dat2/kdat_redirection.h>
#else
#error Must include udat.h or kdat.h
#endif

/*
 * dat registration API.
 *
 * Technically the dat_ia_open is part of the registration API. This
 * is so the registration module can map the device name to a provider
 * structure and then call the provider dat_ia_open function.
 * dat_is_close is also part of the registration API so that the
 * registration code can be aware when an ia is no longer in use.
 *
 */

extern DAT_RETURN DAT_API dat_registry_add_provider (
	IN  const DAT_PROVIDER *,               /* provider          */
	IN  const DAT_PROVIDER_INFO* );         /* provider info     */

extern DAT_RETURN DAT_API dat_registry_remove_provider (
	IN  const DAT_PROVIDER *,               /* provider          */
	IN  const DAT_PROVIDER_INFO* );         /* provider info     */

/*
 * Provider initialization APIs.
 *
 * Providers that support being automatically loaded by the Registry must
 * implement these APIs and export them as public symbols.
 */

#define DAT_PROVIDER_INIT_FUNC_NAME  dat_provider_init
#define DAT_PROVIDER_FINI_FUNC_NAME  dat_provider_fini

#define DAT_PROVIDER_INIT_FUNC_STR   "dat_provider_init"
#define DAT_PROVIDER_FINI_FUNC_STR   "dat_provider_fini"

typedef void ( DAT_API *DAT_PROVIDER_INIT_FUNC) (
	IN const DAT_PROVIDER_INFO *,           /* provider info     */
	IN const char *);                       /* instance data     */

typedef void ( DAT_API *DAT_PROVIDER_FINI_FUNC) (
	IN const DAT_PROVIDER_INFO *);          /* provider info     */

typedef enum dat_ha_relationship
{
	DAT_HA_FALSE,		/* two IAs are not related		*/
	DAT_HA_TRUE,		/* two IAs are related			*/
	DAT_HA_UNKNOWN,		/* relationship is not known		*/
	DAT_HA_CONFLICTING,	/* 2 IAs do not agree on the relationship */
	DAT_HA_EXTENSION_BASE
} DAT_HA_RELATIONSHIP;

extern DAT_RETURN DAT_API dat_registry_providers_related (
	IN      const DAT_NAME_PTR,
	IN      const DAT_NAME_PTR,
	OUT     DAT_HA_RELATIONSHIP * );

#ifdef __cplusplus
}
#endif

#endif /* _DAT_REGISTRY_H_ */
