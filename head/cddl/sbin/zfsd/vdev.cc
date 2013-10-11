/*-
 * Copyright (c) 2011 Spectra Logic Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * Authors: Justin T. Gibbs     (Spectra Logic Corporation)
 *
 * $FreeBSD$
 */

/**
 * \file vdev.cc
 *
 * Implementation of the Vdev class.
 */
#include <sys/cdefs.h>

#include <sstream>

#include "vdev.h"
#include "zfsd.h"
#include "zfsd_exception.h"

__FBSDID("$FreeBSD$");
/*============================ Namespace Control =============================*/
using std::stringstream;

/*=========================== Class Implementations ==========================*/
/*----------------------------------- Guid -----------------------------------*/
std::ostream& operator<< (std::ostream& out, Guid g){
	if (g.isValid())
		out << (uint64_t) g;
	else
		out << "None";
	return (out);
}


/*----------------------------------- Vdev -----------------------------------*/
Vdev::Vdev(zpool_handle_t *pool, nvlist_t *config)
 : m_poolConfig(zpool_get_config(pool, NULL)),
   m_config(config)
{
	uint64_t raw_guid;
	if (nvlist_lookup_uint64(m_poolConfig, ZPOOL_CONFIG_POOL_GUID,
				 &raw_guid) != 0)
		throw ZfsdException("Unable to extract pool GUID "
				    "from pool handle.");
	m_poolGUID = raw_guid;

	if (nvlist_lookup_uint64(m_config, ZPOOL_CONFIG_GUID, &raw_guid) != 0)
		throw ZfsdException("Unable to extract vdev GUID "
				    "from vdev config data.");
	m_vdevGUID = raw_guid;
}

Vdev::Vdev(nvlist_t *poolConfig, nvlist_t *config)
 : m_poolConfig(poolConfig),
   m_config(config)
{
	uint64_t raw_guid;
	if (nvlist_lookup_uint64(m_poolConfig, ZPOOL_CONFIG_POOL_GUID,
				 &raw_guid) != 0)
		throw ZfsdException("Unable to extract pool GUID "
				    "from pool handle.");
	m_poolGUID = raw_guid;

	if (nvlist_lookup_uint64(m_config, ZPOOL_CONFIG_GUID, &raw_guid) != 0)
		throw ZfsdException("Unable to extract vdev GUID "
				    "from vdev config data.");
	m_vdevGUID = raw_guid;
}

Vdev::Vdev(nvlist_t *labelConfig)
 : m_poolConfig(labelConfig)
{
	uint64_t raw_guid;
	if (nvlist_lookup_uint64(labelConfig, ZPOOL_CONFIG_POOL_GUID,
				 &raw_guid) != 0)
		m_vdevGUID = Guid();
	else
		m_poolGUID = raw_guid;

	if (nvlist_lookup_uint64(labelConfig, ZPOOL_CONFIG_GUID,
				 &raw_guid) != 0)
		throw ZfsdException("Unable to extract vdev GUID "
				    "from vdev label data.");
	m_vdevGUID = raw_guid;

	try {
		m_config = VdevIterator(labelConfig).Find(m_vdevGUID);
	} catch (const ZfsdException &exp) {
		/*
		 * When reading a spare's label, it is normal not to find
		 * a list of vdevs
		 */
		m_config = NULL;
	}
}

vdev_state
Vdev::State() const
{
	vdev_stat_t *vs;
	uint_t       vsc;

	if (m_config == NULL) {
		/*
		 * If we couldn't find the list of vdevs, that normally means
		 * that this is an available hotspare.  In that case, we will
		 * presume it to be healthy.  Even if this spare had formerly
		 * been in use, been degraded, and been replaced, the act of
		 * replacement wipes the degraded bit from the label.  So we
		 * have no choice but to presume that it is healthy.
		 */
		return (VDEV_STATE_HEALTHY);
	}

	if (nvlist_lookup_uint64_array(m_config, ZPOOL_CONFIG_VDEV_STATS,
					(uint64_t **)&vs, &vsc) == 0)
		return (static_cast<vdev_state>(vs->vs_state));

	/*
	 * Stats are not available.  This vdev was created from a label.
	 * Synthesize a state based on available data.
	 */
	uint64_t faulted(0);
	uint64_t degraded(0);
	(void)nvlist_lookup_uint64(m_config, ZPOOL_CONFIG_FAULTED, &faulted);
	(void)nvlist_lookup_uint64(m_config, ZPOOL_CONFIG_DEGRADED, &degraded);
	if (faulted)
		return (VDEV_STATE_FAULTED);
	if (degraded)
		return (VDEV_STATE_DEGRADED);
	return (VDEV_STATE_HEALTHY);
}

string
Vdev::GUIDString() const
{
	stringstream vdevGUIDString;

	vdevGUIDString << GUID();
	return (vdevGUIDString.str());
}

string
Vdev::Path() const
{
	char *path(NULL);

	if ((m_config != NULL)
	    && (nvlist_lookup_string(m_config, ZPOOL_CONFIG_PATH, &path) == 0))
		return (path);

	return ("");
}

string
Vdev::PhysicalPath() const
{
	char *path(NULL);

	if ((m_config != NULL) && (nvlist_lookup_string(m_config,
				    ZPOOL_CONFIG_PHYS_PATH, &path) == 0))
		return (path);

	return ("");
}
