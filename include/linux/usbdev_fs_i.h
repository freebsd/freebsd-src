struct usb_device;
struct usb_bus;

struct usbdev_inode_info {
	struct list_head dlist;
	struct list_head slist;
	union {
		struct usb_device *dev;
		struct usb_bus *bus;
	} p;
};
