/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <dom/core/typeinfo.h>
#include <dom/core/string.h>

#include "utils/utils.h"

/* TypeInfo object */
struct dom_type_info {
	struct lwc_string_s *type;	/**< Type name */
	struct lwc_string_s *namespace;	/**< Type namespace */
};

/**
 * Get the type name of this dom_type_info
 *
 * \param ti   The dom_type_info
 * \param ret  The name
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 *
 * We don't support this API now, so this function call always
 * return DOM_NOT_SUPPORTED_ERR.
 */
dom_exception _dom_type_info_get_type_name(dom_type_info *ti, 
		dom_string **ret)
{
	UNUSED(ti);
	UNUSED(ret);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Get the namespace of this type info
 *
 * \param ti   The dom_type_info
 * \param ret  The namespace
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 *
 * We don't support this API now, so this function call always
 * return DOM_NOT_SUPPORTED_ERR.
 */
dom_exception _dom_type_info_get_type_namespace(dom_type_info *ti,
		dom_string **ret)
{
	UNUSED(ti);
	UNUSED(ret);
	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Whether this type info is derived from another one
 *
 * \param ti         The dom_type_info
 * \param namespace  The namespace of name
 * \param name       The name of the base typeinfo
 * \param method     The deriving method
 * \param ret        The return value
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 *
 * We don't support this API now, so this function call always
 * return DOM_NOT_SUPPORTED_ERR.
 */
dom_exception _dom_type_info_is_derived(dom_type_info *ti,
		dom_string *namespace, dom_string *name, 
		dom_type_info_derivation_method method, bool *ret)
{
	UNUSED(ti);
	UNUSED(namespace);
	UNUSED(name);
	UNUSED(method);
	UNUSED(ret);
	return DOM_NOT_SUPPORTED_ERR;
}

