/* Macro's to cope with the differences between NetBSD and FreeBSD
 */

/*
 * NetBSD
 *
 */

#if defined(__NetBSD__)
#include "opt_usbverbose.h"

#define DEVICE_NAME(bdev)	\
	printf("%s: ", (bdev).dv_xname)

typedef struct device bdevice;			/* base device */



/*
 * FreeBSD
 *
 */

#elif defined(__FreeBSD__)
#include "opt_usb.h"
#define DEVICE_NAME(bdev)	\
		printf("%s%d: ",	\
			device_get_name(bdev), device_get_unit(bdev))

/* XXX Change this when FreeBSD has memset
 */
#define memset(d, v, s)	\
		do{			\
		if ((v) == 0)		\
			bzero((d), (s));	\
		else			\
			panic("Non zero filler for memset, cannot handle!"); \
		} while (0)

/* XXX can't we put this somehow into a typedef? */
#define bdevice	device_t			/* base device */

#define USB_MODULE(name, driver, devclass)				\
	DRIVER_MODULE((name), "usb", (driver), (devclass), usb_driver_load, 0)
#endif




/*
 * General
 *
 */

#define DEVICE_MSG(bdev, s)	(DEVICE_NAME(bdev), printf s)
#define DEVICE_ERROR(bdev, s)	DEVICE_MSG(bdev, s)


/* Returns from attach for NetBSD vs. FreeBSD
 */

/* Error returns */
#if defined(__NetBSD__)
#define ATTACH_ERROR_RETURN	return
#define ATTACH_SUCCESS_RETURN	return
#elif defined(__FreeBSD__)
#define ATTACH_ERROR_RETURN	return ENXIO
#define ATTACH_SUCCESS_RETURN	return 0
#endif


/*
 * The debugging subsystem
 */

/* XXX to be filled in
 */

