/*
 * Data structures and definitions for linking CAM into the autoconf system.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id$
 */

#ifndef _CAM_CAM_CONF_H
#define _CAM_CAM_CONF_H 1

#ifdef KERNEL

#define CAMCONF_UNSPEC 255
#define CAMCONF_ANY 254

/*
 * Macro that lets us know something is specified.
 */
#define IS_SPECIFIED(ARG) (ARG != CAMCONF_UNSPEC && ARG != CAMCONF_ANY)

struct cam_sim_config
{
	int	pathid;
	char	*sim_name;
	int	sim_unit; 
	int	sim_bus;
}; 

struct cam_periph_config       
{
	char	*periph_name;
	int	periph_unit;	/* desired device unit */
	int	pathid;		/* Controller unit */
	int	target;
	int	lun;
	int	flags;		/* Flags from config */
};

extern struct cam_sim_config cam_sinit[];
extern struct cam_periph_config cam_pinit[];

#endif /* KERNEL */

#endif /* _CAM_CAM_CONF_H */
