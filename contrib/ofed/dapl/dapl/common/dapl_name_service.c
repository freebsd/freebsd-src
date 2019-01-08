/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
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

/**********************************************************************
 *
 * MODULE: dapl_name_service.c
 *
 * PURPOSE: Provide simple, file base name services in the absence
 *          of DNS hooks for a particular transport type. If an
 *          InfiniBand implementation supports IPoIB, this should
 *	    not be used.
 *
 * Description: Interfaces in this file are completely described in
 *		dapl_name_service.h
 *
 * $Id:$
 **********************************************************************/

/*
 * Include files for setting up a network name
 */
#include "dapl.h"
#include "dapl_name_service.h"

/*
 * Prototypes
 */
#ifdef IBHOSTS_NAMING
DAT_RETURN dapli_ns_create_gid_map(void);

DAT_RETURN dapli_ns_add_address(IN DAPL_GID_MAP * gme);
#endif				/* IBHOSTS_NAMING */

/*
 * dapls_ns_init
 *
 * Initialize naming services
 *
 * Input:
 *	none
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN dapls_ns_init(void)
{
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;
#ifdef IBHOSTS_NAMING
	dat_status = dapli_ns_create_gid_map();
#endif				/* IBHOSTS_NAMING */

	return dat_status;
}

#ifdef IBHOSTS_NAMING

#define	MAX_GID_ENTRIES		32
DAPL_GID_MAP g_gid_map_table[MAX_GID_ENTRIES];

#ifdef _WIN32
#define MAP_FILE        "c:/WINNT/system32/drivers/etc/ibhosts"
#else
#define MAP_FILE	"/etc/dapl/ibhosts"
#endif				/* !defined WIN32 */

/*
 * dapli_ns_create_gid_map()
 *
 * Read the MAP_FILE to obtain host names and GIDs.
 * Create a table containing IP addresses and GIDs which can
 * be used for lookups.
 *
 * This implementation is a simple method providing name services
 * when more advanced mechanisms do not exist. The proper way
 * to obtain these mappings is to use a name service such as is
 * provided by IPoIB on InfiniBand.
 *
 * Input:
 *	device_name		Name of device as reported by the provider
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	char * to string number
 */
DAT_RETURN dapli_ns_create_gid_map(void)
{
	FILE *f;
	ib_gid_t gid;
	char hostname[128];
	int rc;
	struct addrinfo *addr;
	struct sockaddr_in *si;
	DAPL_GID_MAP gmt;

	f = fopen(MAP_FILE, "r");
	if (f == NULL) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "ERROR: Must have file <%s> for IP/GID mappings\n",
			     MAP_FILE);
		return DAT_ERROR(DAT_INTERNAL_ERROR, 0);
	}

	rc = fscanf(f, "%s " F64x " " F64x, hostname, &gid.gid_prefix,
		    &gid.guid);
	while (rc != EOF) {
		rc = dapls_osd_getaddrinfo(hostname, &addr);

		if (rc != 0) {
			/*
			 * hostname not registered in DNS, provide a dummy value
			 */
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "WARNING: <%s> not registered in DNS, using dummy IP value\n",
				     hostname);
			/*dapl_os_memcpy(hca_ptr->hca_address.sa_data, "0x01020304", 4); */
			gmt.ip_address = 0x01020304;
		} else {
			/*
			 * Load into the ip/gid mapping table
			 */
			si = (struct sockaddr_in *)addr->ai_addr;
			if (AF_INET == addr->ai_addr->sa_family) {
				gmt.ip_address = si->sin_addr.s_addr;
			} else {
				dapl_dbg_log(DAPL_DBG_TYPE_WARN,
					     "WARNING: <%s> Address family not supported, using dummy IP value\n",
					     hostname);
				gmt.ip_address = 0x01020304;
			}
			dapls_osd_freeaddrinfo(addr);
		}
		gmt.gid.gid_prefix = gid.gid_prefix;
		gmt.gid.guid = gid.guid;

		dapli_ns_add_address(&gmt);
		rc = fscanf(f, "%s " F64x " " F64x, hostname, &gid.gid_prefix,
			    &gid.guid);
	}

	fclose(f);

	return DAT_SUCCESS;
}

/*
 * dapli_ns_add_address
 *
 * Add a table entry to the  gid_map_table.
 *
 * Input:
 *	remote_ia_address	remote IP address
 *	gid			pointer to output gid
 *
 * Output:
 * 	gid			filled in GID
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN dapli_ns_add_address(IN DAPL_GID_MAP * gme)
{
	DAPL_GID_MAP *gmt;
	int count;

	gmt = g_gid_map_table;

	for (count = 0, gmt = g_gid_map_table; gmt->ip_address; gmt++) {
		count++;
	}

	if (count > MAX_GID_ENTRIES) {
		return DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, 0);
	}

	*gmt = *gme;

	return DAT_SUCCESS;
}

/*
 * dapls_ns_lookup_address
 *
 * Look up the provided IA_ADDRESS in the gid_map_table. Return
 * the gid if found.
 *
 * Input:
 *	remote_ia_address	remote IP address
 *	gid			pointer to output gid
 *
 * Output:
 * 	gid			filled in GID
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 */
DAT_RETURN
dapls_ns_lookup_address(IN DAPL_IA * ia_ptr,
			IN DAT_IA_ADDRESS_PTR remote_ia_address,
			OUT ib_gid_t * gid)
{
	DAPL_GID_MAP *gmt;
	struct sockaddr_in *si;

	ia_ptr = ia_ptr;	/* unused here */

	si = (struct sockaddr_in *)remote_ia_address;

	for (gmt = g_gid_map_table; gmt->ip_address; gmt++) {
		if (gmt->ip_address == si->sin_addr.s_addr) {
			gid->guid = gmt->gid.guid;
			gid->gid_prefix = gmt->gid.gid_prefix;

			return DAT_SUCCESS;
		}
	}

	return DAT_ERROR(DAT_INVALID_PARAMETER, 0);
}

#endif				/* IBHOSTS_NAMING */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
