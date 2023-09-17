/* Public domain. */

#ifndef _LINUXKPI_LINUX_IOMMU_H_
#define	_LINUXKPI_LINUX_IOMMU_H_

#include <linux/device.h>

#define	__IOMMU_DOMAIN_PAGING	(1U << 0)
#define	__IOMMU_DOMAIN_DMA_API	(1U << 1)
#define	__IOMMU_DOMAIN_PT	(1U << 2)
#define	__IOMMU_DOMAIN_DMA_FQ	(1U << 3)

#define	IOMMU_DOMAIN_BLOCKED	(0U)
#define	IOMMU_DOMAIN_IDENTITY	(__IOMMU_DOMAIN_PT)
#define	IOMMU_DOMAIN_UNMANAGED	(__IOMMU_DOMAIN_PAGING)
#define	IOMMU_DOMAIN_DMA	(__IOMMU_DOMAIN_PAGING | __IOMMU_DOMAIN_DMA_API)
#define	IOMMU_DOMAIN_DMA_FQ	(__IOMMU_DOMAIN_PAGING | __IOMMU_DOMAIN_DMA_API | __IOMMU_DOMAIN_DMA_FQ)

struct iommu_domain {
	unsigned int type;
};

static inline struct iommu_domain *
iommu_get_domain_for_dev(struct device *dev __unused)
{
	return (NULL);
}

#endif /* _LINUXKPI_LINUX_IOMMU_H_ */
