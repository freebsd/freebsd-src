/* Public domain. */

#ifndef _LINUXKPI_LINUX_CC_PLATFORM_H_
#define	_LINUXKPI_LINUX_CC_PLATFORM_H_

enum cc_attr {
	CC_ATTR_MEM_ENCRYPT,
};

static inline bool
cc_platform_has(enum cc_attr attr __unused)
{

	return (false);
}

#endif /* _LINUXKPI_LINUX_CC_PLATFORM_H_ */
