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

/***************************************************************
 *
 * HEADER: udat_config.h
 *
 * PURPOSE: provides uDAPL configuration information.
 *
 * Description: Header file for "uDAPL: User Direct Access Programming
 *              Library, Version: 2.0"
 *
 ***************************************************************/
#ifndef _UDAT_CONFIG_H_
#define _UDAT_CONFIG_H_

#define DAT_VERSION_MAJOR 2
#define DAT_VERSION_MINOR 0

/*
 * The official header files will default DAT_THREADSAFE to DAT_TRUE. If
 * your project does not wish to use this default, you must ensure that
 * DAT_THREADSAFE will be set to DAT_FALSE. This may be done by an
 * explicit #define in a common project header file that is included
 * before any DAT header files, or through command line directives to the
 * compiler (presumably controlled by the make environment).
 */

/*
 * A site, project or platform may consider setting an alternate default
 * via their make rules, but are discouraged from doing so by editing
 * the official header files.
 */

/*
 * The Reference Implementation is not Thread Safe. The Reference
 * Implementation has chosen to go with the first method and define it
 * explicitly in the header file.
 */
#define DAT_THREADSAFE DAT_FALSE

#ifndef DAT_THREADSAFE
#define DAT_THREADSAFE DAT_TRUE
#endif /* DAT_THREADSAFE */

/*
 * Enable DAT  Extensions
 */
#ifndef DAT_EXTENSIONS
#define DAT_EXTENSIONS 1
#endif

#endif /* _UDAT_CONFIG_H_ */
