
INTERFACE gic;

HEADER {
	struct intr_irqsrc;
};

METHOD void reserve_msi_range {
	device_t	dev;
	u_int		mbi_start;
	u_int		mbi_count;
};

METHOD int alloc_msi {
	device_t	dev;
	u_int		mbi_start;
	u_int		mbi_count;
	int		count;
	int		maxcount;
	struct intr_irqsrc **isrc;
};

METHOD int release_msi {
	device_t	dev;
	int		count;
	struct intr_irqsrc **isrc;
};

METHOD int alloc_msix {
	device_t	dev;
	u_int		mbi_start;
	u_int		mbi_count;
	struct intr_irqsrc **isrc;
};

METHOD int release_msix {
	device_t	dev;
	struct intr_irqsrc *isrc;
};
