#ifndef _IEEE1394_HOTPLUG_H
#define _IEEE1394_HOTPLUG_H

#include <linux/types.h>

#define IEEE1394_MATCH_VENDOR_ID	0x0001
#define IEEE1394_MATCH_MODEL_ID		0x0002
#define IEEE1394_MATCH_SPECIFIER_ID	0x0004
#define IEEE1394_MATCH_VERSION		0x0008

/*
 * Unit spec id and sw version entry for some protocols
 */
#define AVC_UNIT_SPEC_ID_ENTRY					0x0000A02D
#define AVC_SW_VERSION_ENTRY					0x00010001
#define CAMERA_UNIT_SPEC_ID_ENTRY				0x0000A02D
#define CAMERA_SW_VERSION_ENTRY					0x00000100

struct ieee1394_device_id {
	u32 match_flags;
	u32 vendor_id;
	u32 model_id;
	u32 specifier_id;
	u32 version;
	void *driver_data;
};

#endif /* _IEEE1394_HOTPLUG_H */
