#ifndef IF_ATHN_USB_FW_H
#define IF_ATHN_USB_FW_H

struct athn_usb_softc;

int	 athn_usb_load_firmware(struct athn_usb_softc *);
const struct firmware* athn_usb_unload_firmware();

#endif	/* IF_ATHN_USB_FW_H */