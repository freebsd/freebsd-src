/* USB Scanner IOCTLS */

/* read vendor and product IDs from the scanner */
#define SCANNER_IOCTL_VENDOR _IOR('U', 0x20, int)
#define SCANNER_IOCTL_PRODUCT _IOR('U', 0x21, int)
/* send/recv a control message to the scanner */
#define SCANNER_IOCTL_CTRLMSG _IOWR('U', 0x22, struct usb_ctrlrequest )


