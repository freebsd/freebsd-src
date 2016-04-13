/*-
 * Copyright (c) 2016 Spectra Logic Corporation
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
 * Authors: Alan Somers         (Spectra Logic Corporation)
 */

#include <gtest/gtest.h>

#include <list>
#include <map>
#include <string>

#include <devdctl/guid.h>
#include <devdctl/event.h>
#include <devdctl/event_factory.h>

using namespace DevdCtl;
using namespace std;
using namespace testing;

#define	NUM_ELEMENTS(x) (sizeof(x) / sizeof(*x))

class IsDiskDevTest : public TestWithParam<pair<bool, const char*> >{
protected:
	virtual void SetUp()
	{
		m_factory = new EventFactory();
	}

	virtual void TearDown()
	{
		if (m_ev) delete m_ev;
		if (m_factory) delete m_factory;
	}

	EventFactory *m_factory;
	Event *m_ev;
	static EventFactory::Record s_registry[];
};

DevdCtl::EventFactory::Record IsDiskDevTest::s_registry[] = {
	{ Event::NOTIFY, "DEVFS", &DevfsEvent::Builder }
};

TEST_P(IsDiskDevTest, TestIsDiskDev) {
	pair<bool, const char*> param = GetParam();
	DevfsEvent *devfs_ev;

	m_factory->UpdateRegistry(s_registry, NUM_ELEMENTS(s_registry));
	string evString(param.second);
	m_ev = Event::CreateEvent(*m_factory, evString);
	devfs_ev = dynamic_cast<DevfsEvent*>(m_ev);
	ASSERT_NE(nullptr, devfs_ev);
	EXPECT_EQ(param.first, devfs_ev->IsDiskDev());
}

INSTANTIATE_TEST_CASE_P(IsDiskDevTestInstantiation, IsDiskDevTest, Values(
	pair<bool, const char*>(true,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=da6\n"),
	pair<bool, const char*>(false,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=cuau0\n"),
	pair<bool, const char*>(true,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=ada6\n"),
	pair<bool, const char*>(true,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=da6p1\n"),
	pair<bool, const char*>(true,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=ada6p1\n"),
	pair<bool, const char*>(true,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=da6s0p1\n"),
	pair<bool, const char*>(true,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=ada6s0p1\n"),
	/* 
	 * Test physical path nodes.  These are currently all set to false since
	 * physical path nodes are implemented with symlinks, and most CAM and
	 * ZFS operations can't use symlinked device nodes
	 */
	/* A SpectraBSD-style physical path node*/
	pair<bool, const char*>(false,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=enc@50030480019f53fd/elmtype@array_device/slot@18/da\n"),
	pair<bool, const char*>(false,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=enc@50030480019f53fd/elmtype@array_device/slot@18/pass\n"),
	/* A FreeBSD-style physical path node */
	pair<bool, const char*>(true,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=enc@n50030480019f53fd/type@0/slot@18/elmdesc@ArrayDevice18/da6\n"),
	pair<bool, const char*>(false,
		"!system=DEVFS subsystem=CDEV type=CREATE cdev=enc@n50030480019f53fd/type@0/slot@18/elmdesc@ArrayDevice18/pass6\n"));
);
