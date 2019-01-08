/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
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
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl_proto.h"

/*
 * Parse a QOS argument into a DAT_QOS.
 *
 * Returns no errors: defaults to best effort.
 */
DAT_QOS DT_ParseQoS(char *arg)
{
	if (0 == strcmp(arg, "HT")) {
		return (DAT_QOS_HIGH_THROUGHPUT);
	}

	if (0 == strcmp(arg, "LL")) {
		return (DAT_QOS_LOW_LATENCY);
	}

	if (0 == strcmp(arg, "EC")) {
		return (DAT_QOS_ECONOMY);
	}

	if (0 == strcmp(arg, "PM")) {
		return (DAT_QOS_PREMIUM);
	}
	/*
	 * Default to "BE" so no point in checking further
	 */
	return (DAT_QOS_BEST_EFFORT);
}
