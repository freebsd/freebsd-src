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
 * \file dev_ctl_event.h
 *
 * \brief Class hierarchy used to express events received via
 *        the devctl API.
 */

#ifndef _DEV_CTL_EVENT_H_
#define	_DEV_CTL_EVENT_H_

#include <string>
#include <list>
#include <map>

#include <sys/fs/zfs.h>
#include <libzfs.h>

/*============================ Namespace Control =============================*/
using std::map;
using std::pair;
using std::string;

/*============================= Class Definitions ============================*/
/*------------------------------ ParseException ------------------------------*/
/**
 * Exception thrown when an event string is not converted to an actionable
 * DevCtlEvent object.
 */
class ParseException
{
public:
	enum Type
	{
		/** Improperly formatted event string encounterd. */
		INVALID_FORMAT,

		/** No handlers for this event type. */
		DISCARDED_EVENT_TYPE,

		/** Unhandled event type. */
		UNKNOWN_EVENT_TYPE
	};

	/**
	 * Constructor
	 *
	 * \param type    The type of this exception.
	 * \param offset  The location in the parse buffer where this
	 *                exception occurred.
	 */
	ParseException(Type type, size_t offset = 0);

	/**
	 * Accessor
	 *
	 * \return  The classification for this exception.
	 */
	Type   GetType()			    const;

	/**
	 * Accessor
	 *
	 * \return  The offset into the event string where this exception
	 *	    occurred.
	 */
	size_t GetOffset()			    const;

	/**
	 * Convert an exception into a human readable string.
	 *
	 * \param parsedBuffer  The event buffer that caused the failure.
	 */
	string ToString(const string &parsedBuffer) const;

	/**
	 * Log exception data to syslog.
	 *
	 * \param parsedBuffer  The event buffer that caused the failure.
	 */
	void   Log(const string &parsedBuffer)      const;

private:
	/** The type of this exception. */
	Type   m_type;

	/**
	 * The offset into the event string buffer from where this
	 * exception was triggered.
	 */
	size_t m_offset;
};

//- ParseException Inline Public Methods ---------------------------------------
inline
ParseException::ParseException(Type type, size_t offset)
 : m_type(type),
   m_offset(offset)
{
}

//- ParseException Inline Const Public Methods ---------------------------------
inline ParseException::Type  
ParseException::GetType() const
{
	return (m_type);
}

inline size_t
ParseException::GetOffset() const
{
	return (m_offset);
}

/*-------------------------------- NVPairMap ---------------------------------*/
/**
 * NVPairMap is a specialization of the standard map STL container.
 */
typedef map<string, string> NVPairMap;

/*-------------------------------- DevCtlEvent -------------------------------*/
/**
 * \brief Container for the name => value pairs that comprise the content of
 *        a device control event.
 *
 * All name => value data for events can be accessed via the Contains()
 * and Value() methods.  name => value pairs for data not explicitly
 * recieved as a a name => value pair are synthesized during parsing.  For
 * example, ATTACH and DETACH events have "device-name" and "parent"
 * name => value pairs added.
 */
class DevCtlEvent
{
public:
	/** Event type */
	enum Type {
		/** Generic event notification. */
		NOTIFY  = '!',

		/** A driver was not found for this device. */
		NOMATCH = '?',

		/** A bus device instance has been added. */
		ATTACH  = '+',

		/** A bus device instance has been removed. */
		DETACH  = '-'
	};

	/** Prepare the DevCtlEvent class for processing of events. */
	static void	    Init();

	/**
	 * Factory method for creating events.
	 *
	 * \param buffer  String representing a single event received
	 *                from devd.
	 *
	 * \note Init() must be invoked prior to the first call to
	 *       CreateEvent().
	 */
	static DevCtlEvent *CreateEvent(const string &buffer);

	/**
	 * Provide a user friendly string representation of an
	 * event type.
	 *
	 * \param type  The type of event to map to a string.
	 *
	 * \return  A user friendly string representing the input type.
	 */
	static const char  *TypeToString(Type type);

	/**
	 * Determine the availability of a name => value pair by name.
	 *
	 * \param name  The key name to search for in this event instance.
	 *
	 * \return  true if the specified key is available in this
	 *          event, otherwise false.
	 */
	bool Contains(const string &name)	const;

	/**
	 * \param key  The name of the key for which to retrieve its
	 *             associated value.
	 *
	 * \return  A const reference to the string representing the
	 *	    value associated with key.
	 *
	 * \note  For key's with no registered value, the empty string
	 *        is returned.
	 */
	const string &Value(const string &key) const;

	/**
	 * Get the type of this event instance.
	 *
	 * \return  The type of this event instance.
	 */
	Type GetType()				const;

	/**
	 * Get the orginal DevCtl event string for this event.
	 *
	 * \return  The DevCtl event string.
	 */
	const string &GetEventString()		const;

	/**
	 * Convert the event instance into a string suitable for
	 * printing to the console or emitting to syslog.
	 *
	 * \return  A string of formatted event data.
	 */
	string ToString()			const;

	/**
	 * Pretty-print this event instance to cout.
	 */
	void Print()				const;

	/**
	 * Pretty-print this event instance to syslog.
	 *
	 * \param priority  The logging priority/facility.
	 *                  See syslog(3).
	 */
	void Log(int priority)			const;

	/** 
	 * Create and return a fully independent clone
	 * of this event.
	 */
	virtual DevCtlEvent *DeepCopy() 	const = 0;

	/** Destructor */
	virtual ~DevCtlEvent();

	/**
	 * Interpret and perform any actions necessary to
	 * consume the event.
	 */
	virtual void Process()			const;

protected:
	/** Table entries used to map a type to a user friendly string. */
	struct EventTypeRecord
	{
		Type         m_type;
		const char  *m_typeName;
	};

	/**
	 * Event creation handlers are matched by event type and a
	 * string representing the system emitting the event.
	 */
	typedef pair<Type, string> EventFactoryKey;

	/**
	 * Event factory methods construct a DevCtlEvent given
	 * the type of event and an NVPairMap populated from
	 * the event string received from devd.
	 */
	typedef DevCtlEvent* (EventFactory)(Type, NVPairMap &, const string &);

	/** Map type for Factory method lookups. */
	typedef map<EventFactoryKey, EventFactory *> EventFactoryRegistry;

	/** Table record of factory methods to add to our registry. */
	struct EventFactoryRecord
	{
		Type          m_type;
		const char   *m_subsystem;
		EventFactory *m_method;
	};

	/**
	 * Constructor
	 *
	 * \param type  The type of event to create.
	 */
	DevCtlEvent(Type type, NVPairMap &map, const string &eventString);

	/** Deep copy constructor. */
	DevCtlEvent(const DevCtlEvent &src);

	/** Always empty string returned when NVPairMap lookups fail. */
	static const string         s_theEmptyString;

	/** Unsorted table of event types. */
	static EventTypeRecord      s_typeTable[];

	/** Unsorted table of factory methods to add to our registry. */
	static EventFactoryRecord   s_factoryTable[];

	/** Registry of event factory methods providing O(log(n)) lookup. */
	static EventFactoryRegistry s_factoryRegistry;

	/** The type of this event. */
	const Type                  m_type;

	/**
	 * Event attribute storage.
	 *
	 * \note Although stored by reference (since m_nvPairs can
	 *       never be NULL), the NVPairMap referenced by this field
	 *       is dynamically allocated and owned by this event object.
	 *       m_nvPairs must be deleted at event desctruction.
	 */
	NVPairMap                  &m_nvPairs;

	/**
	 * The unaltered event string, as received from devd, used to
	 * create this event object.
	 */
	string                      m_eventString;

private:
	/**
	 * Ingest event data from the supplied string.
	 *
	 * \param eventString  The string of devd event data to parse.
	 */
	static void ParseEventString(Type type, const string &eventString,
				     NVPairMap &nvpairs);
};

inline DevCtlEvent::Type
DevCtlEvent::GetType() const
{
	return (m_type);
}

inline const string &
DevCtlEvent::GetEventString() const
{
	return (m_eventString);
}

/*------------------------------ DevCtlEventList -----------------------------*/
/**
 * DevCtlEventList is a specialization of the standard list STL container.
 */
typedef std::list<DevCtlEvent *> DevCtlEventList;

/*-------------------------------- DevfsEvent --------------------------------*/
class DevfsEvent : public DevCtlEvent
{
public:
	/** Specialized DevCtlEvent object factory for Devfs events. */
	static EventFactory DevfsEventFactory;

	virtual DevCtlEvent *DeepCopy() const;

	/**
	 * Interpret and perform any actions necessary to
	 * consume the event.
	 */
	virtual void Process() const;

protected:
	/**
	 * Determine if the given device name references a potential
	 * ZFS leaf vdev.
	 *
	 * \param devName  The device name to test.
	 */
	static bool         IsDiskDev(const string &devName);

	/**
	 * Given the device name of a disk, determine if the device
	 * represents the whole device, not just a partition.
	 *
	 * \param devName  Device name of disk device to test.
	 *
	 * \return  True if the device name represents the whole device.
	 *          Otherwise false.
	 */
	static bool         IsWholeDev(const string &devName);

	/**
	 * \brief Read and return label information for a device.
	 *
	 * \param devFd     The device from which to read ZFS label information.
	 * \param inUse     The device is part of an active or potentially
	 *                  active configuration.
	 * \param degraded  The device label indicates the vdev is not healthy.
	 *
	 * \return  If label information is available, an nvlist describing
	 *          the vdev configuraiton found on the device specified by
	 *          devFd.  Otherwise NULL.
	 */
	static nvlist_t    *ReadLabel(int devFd, bool &inUse, bool &degraded);

	/**
	 * Attempt to match the ZFS labeled device at devPath with an active
	 * CaseFile for a missing vdev.  If a CaseFile is found, attempt
	 * to re-integrate the device with its pool.
	 *
	 * \param devPath    The devfs path to the potential leaf vdev.
	 * \param physPath   The physical path string reported by the device
	 *                   at devPath.
	 * \param devConfig  The ZFS label information found on the device
	 *                   at devPath.
	 *
	 * \return  true if the event that caused the online action can
	 *          be considered consumed.
	 */
	static bool	    OnlineByLabel(const string &devPath,
					  const string& physPath,
					  nvlist_t *devConfig);

	/** DeepCopy Constructor. */
	DevfsEvent(const DevfsEvent &src);

	/** Constructor */
	DevfsEvent(Type, NVPairMap &, const string &);
};

/*--------------------------------- ZfsEvent ---------------------------------*/
class ZfsEvent : public DevCtlEvent
{
public:
	/** Specialized DevCtlEvent object factory for ZFS events. */
	static EventFactory ZfsEventFactory;

	virtual DevCtlEvent *DeepCopy() const;

	/**
	 * Interpret and perform any actions necessary to
	 * consume the event.
	 */
	virtual void Process()	        const;

	const string &PoolName()        const;
	uint64_t      PoolGUID()        const;
	uint64_t      VdevGUID()        const;

protected:
	/** Constructor */
	ZfsEvent(Type, NVPairMap &, const string &);

	/** Deep copy constructor. */
	ZfsEvent(const ZfsEvent &src);

	void ProcessPoolEvent()         const;

	uint64_t m_poolGUID;
	uint64_t m_vdevGUID;
};

//- ZfsEvent Inline Public Methods --------------------------------------------
inline const string&
ZfsEvent::PoolName() const
{
	/* The pool name is reported as the subsystem of ZFS events. */
	return (Value("subsystem"));
}

inline uint64_t
ZfsEvent::PoolGUID() const
{
	return (m_poolGUID);
}

inline uint64_t
ZfsEvent::VdevGUID() const
{
	return (m_vdevGUID);
}

#endif /*_DEV_CTL_EVENT_H_ */
