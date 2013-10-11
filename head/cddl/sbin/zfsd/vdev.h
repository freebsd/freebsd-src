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
 * \file vdev.h
 *
 * Definition of the Vdev class.
 */
#ifndef	_VDEV_H_
#define	_VDEV_H_

#include <ostream>
#include <string>

#include <sys/fs/zfs.h>
#include <libzfs.h>


/**
 * \brief Object that represents guids.
 *
 * It can generally be manipulated as a uint64_t, but with a special value
 * "None" that does not equal any valid guid.
 *
 * As of this writing, spa_generate_guid() in spa_misc.c explicitly refuses to
 * return a guid of 0.  So this class uses 0 as a flag value for "None".  In the
 * future, if 0 is allowed to be a valid guid, the implementation of this class
 * must change.
 */
class Guid
{
public:
	/* Constructors */
	Guid(uint64_t guid) : m_GUID(guid) {};
	Guid() 				{ m_GUID = NONE_FLAG; };

	/* Assignment */
	Guid& operator=(const uint64_t& other) {
		m_GUID = other;
		return (*this);
	};

	/* Test the validity of this guid. */
	bool isValid() const		{ return ((bool)m_GUID);	};

	/* Comparison to other Guid operators */
	bool operator==(const Guid& other) const {
		return (m_GUID == other.m_GUID);
	};
	bool operator!=(const Guid& other) const {
		return (m_GUID != other.m_GUID);
	};

	/* Integer conversion operators */
	operator uint64_t() const	{ return (m_GUID);		};
	operator bool() const		{ return (m_GUID != NONE_FLAG);	};

protected:
	const static uint64_t NONE_FLAG = 0;
	/* The stored value.  0 is a flag for "None" */
	uint64_t  m_GUID;
};


/** Convert the GUID into its string representation */
std::ostream& operator<< (std::ostream& out, Guid g);


/**
 * \brief Wrapper class for a vdev's name/value configuration list
 *        simplifying access to commonly used vdev attributes.
 */
class Vdev
{
public:
	/**
	 * \brief Instantiate a vdev object for a vdev that is a member
	 *        of an imported pool.
	 *
	 * \param pool        The pool object containing the vdev with
	 *                    configuration data provided in vdevConfig.
	 * \param vdevConfig  Vdev configuration data.
	 *
	 * This method should be used whenever dealing with vdev's
	 * enumerated via the ZpoolList class.  The in-core configuration
	 * data for a vdev does not contain all of the items found in
	 * the on-disk label.  This requires the vdev class to augment
	 * the data in vdevConfig with data found in the pool object.
	 */
	Vdev(zpool_handle_t *pool, nvlist_t *vdevConfig);

	/**
	 * \brief Instantiate a vdev object for a vdev that is a member
	 *        of a pool configuration.
	 *
	 * \param poolConfig  The pool configuration containing the vdev
	 *                    configuration data provided in vdevConfig.
	 * \param vdevConfig  Vdev configuration data.
	 *
	 * This method should be used whenever dealing with vdev's
	 * enumerated via the ZpoolList class.  The in-core configuration
	 * data for a vdev does not contain all of the items found in
	 * the on-disk label.  This requires the vdev class to augment
	 * the data in vdevConfig with data found in the pool object.
	 */
	Vdev(nvlist_t *poolConfig, nvlist_t *vdevConfig);

	/**
	 * \brief Instantiate a vdev object from a ZFS label stored on
	 *        the device.
	 *
	 * \param vdevConfig  The name/value list retrieved by reading
	 *                    the label information on a leaf vdev.
	 */
	Vdev(nvlist_t *vdevConfig);

	Guid		 GUID()		const;
	Guid		 PoolGUID()	const;
	vdev_state	 State()	const;
	std::string	 Path()		const;
	std::string	 PhysicalPath()	const;
	std::string	 GUIDString()	const;
	nvlist_t	*PoolConfig()	const;
	nvlist_t	*Config()	const;

private:
	Guid	  m_poolGUID;
	Guid	  m_vdevGUID;
	nvlist_t *m_poolConfig;
	nvlist_t *m_config;
};

inline Guid
Vdev::PoolGUID() const
{
	return (m_poolGUID);
}

inline Guid
Vdev::GUID() const
{
	return (m_vdevGUID);
}

inline nvlist_t *
Vdev::PoolConfig() const
{
	return (m_poolConfig);
}

inline nvlist_t *
Vdev::Config() const
{
	return (m_config);
}

#endif /* _VDEV_H_ */
