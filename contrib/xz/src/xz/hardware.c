///////////////////////////////////////////////////////////////////////////////
//
/// \file       hardware.c
/// \brief      Detection of available hardware resources
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"
#include "tuklib_cpucores.h"


/// Maximum number of free *coder* threads. This can be set with
/// the --threads=NUM command line option.
static uint32_t threadlimit;

/// Memory usage limit
static uint64_t memlimit;

/// Total amount of physical RAM
static uint64_t total_ram;


extern void
hardware_threadlimit_set(uint32_t new_threadlimit)
{
	if (new_threadlimit == 0) {
		// The default is the number of available CPU cores.
		threadlimit = tuklib_cpucores();
		if (threadlimit == 0)
			threadlimit = 1;
	} else {
		threadlimit = new_threadlimit;
	}

	return;
}


extern uint32_t
hardware_threadlimit_get(void)
{
	return threadlimit;
}


extern void
hardware_memlimit_set(uint64_t new_memlimit)
{
	if (new_memlimit != 0) {
		memlimit = new_memlimit;
	} else {
		// The default depends on the amount of RAM but so that
		// on "low-memory" systems the relative limit is higher
		// to make it more likely that files created with "xz -9"
		// will still decompress without overriding the limit
		// manually.
		//
		// If 40 % of RAM is 80 MiB or more, use 40 % of RAM as
		// the limit.
		memlimit = 40 * total_ram / 100;
		if (memlimit < UINT64_C(80) * 1024 * 1024) {
			// If 80 % of RAM is less than 80 MiB,
			// use 80 % of RAM as the limit.
			memlimit = 80 * total_ram / 100;
			if (memlimit > UINT64_C(80) * 1024 * 1024) {
				// Otherwise use 80 MiB as the limit.
				memlimit = UINT64_C(80) * 1024 * 1024;
			}
		}
	}

	return;
}


extern void
hardware_memlimit_set_percentage(uint32_t percentage)
{
	assert(percentage > 0);
	assert(percentage <= 100);

	memlimit = percentage * total_ram / 100;
	return;
}


extern uint64_t
hardware_memlimit_get(void)
{
	return memlimit;
}


extern void
hardware_init(void)
{
	// Get the amount of RAM. If we cannot determine it,
	// use the assumption defined by the configure script.
	total_ram = lzma_physmem();
	if (total_ram == 0)
		total_ram = (uint64_t)(ASSUME_RAM) * 1024 * 1024;

	// Set the defaults.
	hardware_memlimit_set(0);
	hardware_threadlimit_set(0);
	return;
}
