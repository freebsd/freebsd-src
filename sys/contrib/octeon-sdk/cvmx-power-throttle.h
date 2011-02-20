/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/

/**
 * @file
 *
 * Interface to power-throttle control, measurement, and debugging
 * facilities.
 *
 * <hr>$Revision<hr>
 *
 */

#ifndef __CVMX_POWER_THROTTLE_H__
#define __CVMX_POWER_THROTTLE_H__
#ifdef	__cplusplus
extern "C" {
#endif

/**
 * a field of the POWTHROTTLE register
 */
static struct cvmx_power_throttle_rfield_t {
	char	name[16];	/* the field's name */
	int32_t	pos;		/* position of the field's LSb */
	int32_t	len;		/* the field's length */
} cvmx_power_throttle_rfield[] = {
#define CVMX_PTH_INDEX_MAXPOW	0
	{"MAXPOW",   56,  8},
#define CVMX_PTH_INDEX_POWER		1
	{"POWER" ,   48,  8},
#define CVMX_PTH_INDEX_THROTT	2
	{"THROTT",   40,  8},
#define CVMX_PTH_INDEX_RESERVED	3
	{"Reserved", 28, 12},
#define CVMX_PTH_INDEX_DISTAG	4
	{"DISTAG",   27,  1},
#define CVMX_PTH_INDEX_PERIOD	5
	{"PERIOD",   24,  3},
#define CVMX_PTH_INDEX_POWLIM	6
	{"POWLIM",   16,  8},
#define CVMX_PTH_INDEX_MAXTHR	7
	{"MAXTHR",    8,  8},
#define CVMX_PTH_INDEX_MINTHR	8
	{"MINTHR",    0,  8}
#define CVMX_PTH_INDEX_MAX		9
};

#define CVMX_PTH_GET_MASK(len, pos) \
	((((uint64_t)1 << (len)) - 1) << (pos))

/**
 * Get the i'th field of power-throttle register r.
 */
static inline uint64_t cvmx_power_throttle_get_field(int i, uint64_t r)
{
    if (OCTEON_IS_MODEL(OCTEON_CN6XXX))
    {
        uint64_t m;
        struct cvmx_power_throttle_rfield_t *p;

        assert((i >= 0) && (i < CVMX_PTH_INDEX_MAX));

        p = &cvmx_power_throttle_rfield[i];
        m = CVMX_PTH_GET_MASK(p->len, p->pos);

        return((r & m) >> p->pos);
    }
    return 0;
}

/**
 * Set the i'th field of power-throttle register r to v.
 */
static inline int cvmx_power_throttle_set_field(int i, uint64_t r, uint64_t v)
{
    if (OCTEON_IS_MODEL(OCTEON_CN6XXX))
    {
        uint64_t m;
        struct cvmx_power_throttle_rfield_t *p;

        assert((i >= 0) && (i < CVMX_PTH_INDEX_MAX));

        p = &cvmx_power_throttle_rfield[i];
        m = CVMX_PTH_GET_MASK(p->len, p->pos);

        return((~m & r) | ((v << p->pos) & m));
    }
    return 0;
}

/**
 * API Function Prototypes
 */
extern int cvmx_power_throttle_self(uint8_t percentage);
extern int cvmx_power_throttle(uint8_t percentage, uint64_t coremask);

#ifdef	__cplusplus
}
#endif
#endif /* __CVMX_POWER_THROTTLE_H__ */
