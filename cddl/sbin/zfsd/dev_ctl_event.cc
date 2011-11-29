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
 */

/**
 * \file dev_ctl_event.cc
 *
 * Implementation of the class hierarchy used to express events
 * received via the devctl API.
 */
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <inttypes.h>
#include <iostream>
#include <sstream>
#include <syslog.h>

#include "case_file.h"
#include "dev_ctl_event.h"
#include "vdev.h"
#include "zfsd.h"
#include "zfsd_exception.h"
#include "zpool_list.h"

/*============================ Namespace Control =============================*/
using std::cerr;
using std::cout;
using std::endl;
using std::stringstream;

/*=========================== Class Implementations ==========================*/
/*------------------------------ ParseException ------------------------------*/
//- ParseException Public Methods ----------------------------------------------
string
ParseException::ToString(const string &parsedBuffer) const
{
	stringstream result;

	result << "Parsing ";

	switch(Type()) {
	case INVALID_FORMAT:
		result << "invalid format ";
		break;
	case DISCARDED_EVENT_TYPE:
		result << "discarded event ";
		break;
	case UNKNOWN_EVENT_TYPE:
		result << "unknown event ";
		break;
	default:
		break;
	}
	result << "exception on buffer: \'";
	if (GetOffset() == 0) {
		result << parsedBuffer << '\'' << endl;
	} else {
		string markedBuffer(parsedBuffer);

		markedBuffer.insert(GetOffset(), "<HERE-->");
		result << markedBuffer << '\'' << endl;
	}

	return (result.str());
}

void
ParseException::Log(const string &parsedBuffer) const
{
	int priority(LOG_ERR);

	if (Type() == DISCARDED_EVENT_TYPE)
		priority = LOG_INFO;

	syslog(priority, "%s", ToString(parsedBuffer).c_str());
}

/*-------------------------------- DevCtlEvent -------------------------------*/
//- DevCtlEvent Static Protected Data ------------------------------------------
const string DevCtlEvent::s_theEmptyString;

DevCtlEvent::EventTypeRecord DevCtlEvent::s_typeTable[] =
{
	{ DevCtlEvent::NOTIFY,  "Notify" },
	{ DevCtlEvent::NOMATCH, "No Driver Match" },
	{ DevCtlEvent::ATTACH,  "Attach" },
	{ DevCtlEvent::DETACH,  "Detach" }
};

DevCtlEvent::EventFactoryRecord DevCtlEvent::s_factoryTable[] =
{
	{ DevCtlEvent::NOTIFY, "DEVFS", &DevfsEvent::DevfsEventFactory },
	{ DevCtlEvent::NOTIFY, "ZFS",   &ZfsEvent::ZfsEventFactory}
};

DevCtlEvent::EventFactoryRegistry DevCtlEvent::s_factoryRegistry;

//- DevCtlEvent Static Public Methods ------------------------------------------
void
DevCtlEvent::Init()
{
	EventFactoryRecord *rec(s_factoryTable);
	EventFactoryRecord *lastRec(s_factoryTable
				  + NUM_ELEMENTS(s_factoryTable) - 1);
		
	for (; rec <= lastRec; rec++) {
		EventFactoryKey key(rec->m_type, rec->m_subsystem);

		s_factoryRegistry[key] = rec->m_method;
	}
}

DevCtlEvent *
DevCtlEvent::CreateEvent(const string &eventString)
{
	NVPairMap &nvpairs(*new NVPairMap);
	Type       type(static_cast<DevCtlEvent::Type>(eventString[0]));

	try {
		ParseEventString(type, eventString, nvpairs);
	} catch (const ParseException &exp) {
		if (exp.GetType() == ParseException::INVALID_FORMAT)
			exp.Log(eventString);
		return (NULL);
	}

	/*
	 * Allow entries in our table for events with no system specified.
	 * These entries should specify the string "none".
	 */
	NVPairMap::iterator system_item(nvpairs.find("system"));
	if (system_item == nvpairs.end())
		nvpairs["system"] = "none";

	EventFactoryKey key(type, nvpairs["system"]);
	EventFactoryRegistry::iterator foundMethod(s_factoryRegistry.find(key));
	if (foundMethod == s_factoryRegistry.end())
		return (NULL);
	return ((foundMethod->second)(type, nvpairs, eventString));
}

const char *
DevCtlEvent::TypeToString(DevCtlEvent::Type type)
{
	EventTypeRecord *rec(s_typeTable);
	EventTypeRecord *lastRec(s_typeTable + NUM_ELEMENTS(s_typeTable) - 1);

	for (; rec <= lastRec; rec++) {
		if (rec->m_type == type)
			return (rec->m_typeName);
	}
	return ("Unknown");
}

//- DevCtlEvent Public Methods -------------------------------------------------
const string &
DevCtlEvent::Value(const string &varName) const
{
	NVPairMap::const_iterator item(m_nvPairs.find(varName));
	if (item == m_nvPairs.end())
		return (s_theEmptyString);

	return (item->second);
}

bool
DevCtlEvent::Contains(const string &varName) const
{
	return (m_nvPairs.find(varName) != m_nvPairs.end());
}

string
DevCtlEvent::ToString() const
{
	stringstream result;

	NVPairMap::const_iterator devName(m_nvPairs.find("device-name"));
	if (devName != m_nvPairs.end())
		result << devName->second << ": ";

	NVPairMap::const_iterator systemName(m_nvPairs.find("system"));
	if (systemName != m_nvPairs.end()
	 && systemName->second != "none")
		result << systemName->second << ": ";

	result << TypeToString(GetType()) << ' ';

	for (NVPairMap::const_iterator curVar = m_nvPairs.begin();
	     curVar != m_nvPairs.end(); curVar++) {
		if (curVar == devName || curVar == systemName)
			continue;

		result << ' '
		     << curVar->first << "=" << curVar->second;
	}
	result << endl;

	return (result.str());
}

void
DevCtlEvent::Print() const
{
	cout << ToString() << std::flush;
}

void
DevCtlEvent::Log(int priority) const
{
	syslog(priority, "%s", ToString().c_str());
}

//- DevCtlEvent Virtual Public Methods -----------------------------------------
DevCtlEvent::~DevCtlEvent()
{
	delete &m_nvPairs;
}

void
DevCtlEvent::Process() const
{
}

//- DevCtlEvent Protected Methods ----------------------------------------------
DevCtlEvent::DevCtlEvent(Type type, NVPairMap &map, const string &eventString)
 : m_type(type),
   m_nvPairs(map),
   m_eventString(eventString)
{
}

DevCtlEvent::DevCtlEvent(const DevCtlEvent &src)
 : m_type(src.m_type),
   m_nvPairs(*new NVPairMap(src.m_nvPairs)),
   m_eventString(src.m_eventString)
{
}

void
DevCtlEvent::ParseEventString(DevCtlEvent::Type type,
			      const string &eventString,
			      NVPairMap& nvpairs)
{
	size_t start;
	size_t end;

	switch (type) {
	case ATTACH:
	case DETACH:

		/*
		 * <type><device-name><unit> <pnpvars> \
		 *                        at <location vars> <pnpvars> \
		 *                        on <parent>
		 *
		 * Handle all data that doesn't conform to the
		 * "name=value" format, and let the generic parser
		 * below handle the rest.
		 *
		 * Type is a single char.  Skip it.
		 */
		start = 1;
		end = eventString.find_first_of(" \t\n", start);
		if (end == string::npos)
			throw ParseException(ParseException::INVALID_FORMAT,
					     start);

		nvpairs["device-name"] = eventString.substr(start, end - start);

		start = eventString.find(" on ", end);
		if (end == string::npos)
			throw ParseException(ParseException::INVALID_FORMAT,
					     start);
		start += 4;
		end = eventString.find_first_of(" \t\n", start);
		nvpairs["parent"] = eventString.substr(start, end);
		if (end == string::npos)
			break;

		/*
		 * The parent field should terminate the event with the
		 * exception of trailing whitespace.
		 */
		end = eventString.find_first_not_of(" \t\n", end);
		if (end != string::npos)
			throw ParseException(ParseException::INVALID_FORMAT,
					     end);
		break;
	case NOTIFY:
		break;
	case NOMATCH:
		throw ParseException(ParseException::DISCARDED_EVENT_TYPE);
	default:
		throw ParseException(ParseException::UNKNOWN_EVENT_TYPE);
	}

	/* Process common "key=value" format. */
	for (start = 1; start < eventString.length(); start = end + 1) {

		/* Find the '=' in the middle of the key/value pair. */
		end = eventString.find('=', start);
		if (end == string::npos)
			break;

		/*
		 * Find the start of the key by backing up until
		 * we hit whitespace or '!' (event type "notice").
		 * Due to the devctl format, all key/value pair must
		 * start with one of these two characters.
		 */
		start = eventString.find_last_of("! \t\n", end);
		if (start == string::npos)
			throw ParseException(ParseException::INVALID_FORMAT,
					     end);
		start++;
		string key(eventString.substr(start, end - start));

		/*
		 * Walk forward from the '=' until either we exhaust
		 * the buffer or we hit whitespace.
		 */
		start = end + 1;
		if (start >= eventString.length())
			throw ParseException(ParseException::INVALID_FORMAT,
					     end);
		end = eventString.find_first_of(" \t\n", start);
		if (end == string::npos)
			end = eventString.length() - 1;
		string value(eventString.substr(start, end - start));

		nvpairs[key] = value;
	}
}

/*-------------------------------- DevfsEvent --------------------------------*/
//- DevfsEvent Static Public Methods -------------------------------------------
DevCtlEvent *
DevfsEvent::DevfsEventFactory(DevCtlEvent::Type type, NVPairMap &nvPairs,
			      const string &eventString)
{
	return (new DevfsEvent(type, nvPairs, eventString));
}

//- DevfsEvent Static Protected Methods ----------------------------------------
bool
DevfsEvent::IsDiskDev(const string &devName)
{
	static const char *diskDevNames[] =
	{
		"da",
		"ada"
	};

	const char **diskName(diskDevNames);
	const char **lastDiskName(diskDevNames
				+ NUM_ELEMENTS(diskDevNames) - 1);
	size_t find_start = devName.rfind('/');
	if (find_start == string::npos) {
		find_start = 0;
	} else {
		/* Just after the last '/'. */
		find_start++;
	}

	for (; diskName <= lastDiskName; diskName++) {

		size_t loc(devName.find(*diskName, find_start));
		if (loc == find_start) {
			size_t prefixLen(strlen(*diskName));

			if (devName.length() - find_start >= prefixLen
			 && isdigit(devName[find_start + prefixLen]))
				return (true);
		}
	}

	return (false);
}

bool
DevfsEvent::IsWholeDev(const string &devName)
{
	string::const_iterator i(devName.begin());

	size_t start = devName.rfind('/');
	if (start == string::npos) {
		start = 0;
	} else {
		/* Just after the last '/'. */
		start++;
	}
	i += start;

	/* alpha prefix followed only by digits. */
	for (; i < devName.end() && !isdigit(*i); i++)
		;
 
	if (i == devName.end())
		return (false);
	
	for (; i < devName.end() && isdigit(*i); i++)
		;

	return (i == devName.end());
}

nvlist_t *
DevfsEvent::ReadLabel(int devFd, bool &inUse, bool &degraded)
{
	pool_state_t poolState;
	char        *poolName;
	boolean_t    b_inuse;

	inUse    = false;
	degraded = false;
	poolName = NULL;
	if (zpool_in_use(g_zfsHandle, devFd, &poolState,
			 &poolName, &b_inuse) == 0) {
		nvlist_t *devLabel;

		inUse = b_inuse == B_TRUE;
		if (poolName != NULL)
			free(poolName);

		if (zpool_read_label(devFd, &devLabel) != 0
		 || devLabel == NULL)
			return (NULL);

		try {
			Vdev vdev(devLabel);
			degraded = vdev.State() != VDEV_STATE_HEALTHY;
			return (devLabel);
		} catch (ZfsdException &exp) {
			string devName = fdevname(devFd);
			string devPath = _PATH_DEV + devName;
			string context("DevfsEvent::ReadLabel: " + devPath + ": ");

			exp.GetString().insert(0, context);
			exp.Log();
		}
	}
	return (NULL);
}

bool
DevfsEvent::OnlineByLabel(const string &devPath, const string& physPath,
			  nvlist_t *devConfig)
{
	try {
		/*
		 * A device with ZFS label information has been
		 * inserted.  If it matches a device for which we
		 * have a case, see if we can solve that case.
		 */
		syslog(LOG_INFO, "Interrogating VDEV label for %s\n",
		       devPath.c_str());
		Vdev vdev(devConfig);
		CaseFile *caseFile(CaseFile::Find(vdev.PoolGUID(),
						  vdev.GUID()));
		if (caseFile != NULL)
			return (caseFile->ReEvaluate(devPath, physPath, &vdev));

	} catch (ZfsdException &exp) {
		string context("DevfsEvent::OnlineByLabel: " + devPath + ": ");

		exp.GetString().insert(0, context);
		exp.Log();
	}
	return (false);
}

//- DevfsEvent Virtual Public Methods ------------------------------------------
DevCtlEvent *
DevfsEvent::DeepCopy() const
{
	return (new DevfsEvent(*this));
}

void
DevfsEvent::Process() const
{
	/*
	 * We are only concerned with newly discovered
	 * devices that can be ZFS vdevs.
	 */
	string devName(Value("cdev"));
	if (Value("type") != "CREATE"
	 || Value("subsystem") != "CDEV"
	 || !IsDiskDev(devName))
		return;

	/* Log the event since it is of interest. */
	Log(LOG_INFO);

	string devPath(_PATH_DEV + devName);
	int devFd(open(devPath.c_str(), O_RDONLY));
	if (devFd == -1)
		return;

	/* Normalize the device name in case the DEVFS event is for a link. */
	devName = fdevname(devFd);
	devPath = _PATH_DEV + devName;

	bool inUse;
	bool degraded;
	nvlist_t *devLabel(ReadLabel(devFd, inUse, degraded));

	char physPath[MAXPATHLEN];
	physPath[0] = '\0';
	bool havePhysPath(ioctl(devFd, DIOCGPHYSPATH, physPath) == 0);

	close(devFd);

	if (inUse && devLabel != NULL) {
		OnlineByLabel(devPath, physPath, devLabel);
	} else if (degraded) {
		syslog(LOG_INFO, "%s is marked degraded.  Ignoring "
		       "as a replace by physical path candidate.\n",
		       devName.c_str());
	} else if (havePhysPath && IsWholeDev(devName)) {
		syslog(LOG_INFO, "Searching for CaseFile by Physical Path\n");
		CaseFile *caseFile(CaseFile::Find(physPath));
		if (caseFile != NULL) {
			syslog(LOG_INFO,
			       "Found CaseFile(%s:%s:%s) - ReEvaluating\n",
			       caseFile->PoolGUIDString().c_str(),
			       caseFile->VdevGUIDString().c_str(),
			       zpool_state_to_name(caseFile->VdevState(),
						   VDEV_AUX_NONE));
			caseFile->ReEvaluate(devPath, physPath, /*vdev*/NULL);
		}
	}
	if (devLabel != NULL)
		nvlist_free(devLabel);
}

//- DevfsEvent Protected Methods -----------------------------------------------
DevfsEvent::DevfsEvent(DevCtlEvent::Type type, NVPairMap &nvpairs,
		       const string &eventString)
 : DevCtlEvent(type, nvpairs, eventString)
{
}

DevfsEvent::DevfsEvent(const DevfsEvent &src)
 : DevCtlEvent(src)
{
}

/*--------------------------------- ZfsEvent ---------------------------------*/
//- ZfsEvent Static Public Methods ---------------------------------------------
DevCtlEvent *
ZfsEvent::ZfsEventFactory(DevCtlEvent::Type type, NVPairMap &nvpairs,
			  const string &eventString)
{
	return (new ZfsEvent(type, nvpairs, eventString));
}

//- ZfsEvent Virtual Public Methods --------------------------------------------
DevCtlEvent *
ZfsEvent::DeepCopy() const
{
	return (new ZfsEvent(*this));
}

void
ZfsEvent::Process() const
{
	string logstr("");

	if (!Contains("class") && !Contains("type")) {
		syslog(LOG_ERR,
		       "ZfsEvent::Process: Missing class or type data.");
		return;
	}

	/* On config syncs, replay any queued events first. */
	if (Value("type").find("ESC_ZFS_config_sync") == 0)
		ZfsDaemon::ReplayUnconsumedEvents();

	Log(LOG_INFO);

	if (Value("subsystem").find("misc.fs.zfs.") == 0) {
		/* Configuration changes, resilver events, etc. */
		ProcessPoolEvent();
		return;
	}

	if (!Contains("pool_guid") || !Contains("vdev_guid")) {
		/* Only currently interested in Vdev related events. */
		return;
	}

	CaseFile *caseFile(CaseFile::Find(PoolGUID(), VdevGUID()));
	if (caseFile != NULL) {
		syslog(LOG_INFO, "Evaluating existing case file\n");
		caseFile->ReEvaluate(*this);
		return;
	}

	/* Skip events that can't be handled. */
	uint64_t poolGUID(PoolGUID());
	/* If there are no replicas for a pool, then it's not manageable. */
	if (Value("class").find("fs.zfs.vdev.no_replicas") == 0) {
		syslog(LOG_INFO, "No replicas available for pool %ju"
		    ", ignoring\n", (uintmax_t)poolGUID);
		return;
	}

	/*
	 * Create a case file for this vdev, and have it
	 * evaluate the event.
	 */
	ZpoolList zpl(ZpoolList::ZpoolByGUID, &poolGUID);
	if (zpl.empty()) {
		bool queued = ZfsDaemon::SaveEvent(*this);
		int priority = queued ? LOG_INFO : LOG_ERR;
		syslog(priority,
		    "ZfsEvent::Process: Event for unknown pool %ju %s",
		    (uintmax_t)poolGUID, queued ? "queued" : "dropped");
		return;
	}

	nvlist_t *vdevConfig = VdevIterator(zpl.front()).Find(VdevGUID());
	if (vdevConfig == NULL) {
		bool queued = ZfsDaemon::SaveEvent(*this);
		int priority = queued ? LOG_INFO : LOG_ERR;
		syslog(priority,
		    "ZfsEvent::Process: Event for unknown vdev %ju %s",
		    (uintmax_t)poolGUID, queued ? "queued" : "dropped");
		return;
	}

	Vdev vdev(zpl.front(), vdevConfig);
	caseFile = &CaseFile::Create(vdev);
	caseFile->ReEvaluate(*this);
}

//- ZfsEvent Protected Methods -------------------------------------------------
ZfsEvent::ZfsEvent(DevCtlEvent::Type type, NVPairMap &nvpairs,
		   const string &eventString)
 : DevCtlEvent(type, nvpairs, eventString)
{
	/*
	 * These are zero on conversion failure as will happen if
	 * Value returns the empty string.
	 */
	m_poolGUID = (uint64_t)strtoumax(Value("pool_guid").c_str(), NULL, 0);
	m_vdevGUID = (uint64_t)strtoumax(Value("vdev_guid").c_str(), NULL, 0);
}

ZfsEvent::ZfsEvent(const ZfsEvent &src)
 : DevCtlEvent(src),
   m_poolGUID(src.m_poolGUID),
   m_vdevGUID(src.m_vdevGUID)
{
}

void
ZfsEvent::ProcessPoolEvent() const
{
	bool degradedDevice(false);

	CaseFile *caseFile(CaseFile::Find(PoolGUID(), VdevGUID()));
	if (caseFile != NULL) {
		if (caseFile->VdevState() != VDEV_STATE_UNKNOWN
		 && caseFile->VdevState() < VDEV_STATE_HEALTHY)
			degradedDevice = true;

		caseFile->ReEvaluate(*this);
	}

	/* XXX Needs to be changed. */
	if (Value("type") == "ESC_ZFS_vdev_remove"
	 && degradedDevice == false) {
		/* See if any other cases can make use of this device. */
		ZfsDaemon::RequestSystemRescan();
	}
}
