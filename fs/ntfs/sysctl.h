/*
 * sysctl.h - Header file for sysctl.c
 *
 * Copyright (C) 1997 Martin von Löwis
 * Copyright (C) 1997 Régis Duchesne
 */

#ifdef DEBUG
	extern int ntdebug;

	void ntfs_sysctl(int add);

	#define SYSCTL(x)	ntfs_sysctl(x)
#else
	#define SYSCTL(x)
#endif /* DEBUG */

