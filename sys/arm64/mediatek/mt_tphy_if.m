INTERFACE mt_tphy;

METHOD int power_on {
	device_t dev;
	int lane; /* 0 = SATA, others reserved for USB if present */
};

METHOD void power_off {
	device_t dev;
	int lane;
};