/*
 * Copyright (c) 2004-2006 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *    Implementation of osm_mcast_tbl_t.
 * This object represents an multicast forwarding table.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>
#include <complib/cl_math.h>
#include <iba/ib_types.h>
#include <opensm/osm_mcast_tbl.h>

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osm_mcast_tbl_init(IN osm_mcast_tbl_t * const p_tbl,
		   IN uint8_t const num_ports, IN uint16_t const capacity)
{
	CL_ASSERT(p_tbl);
	CL_ASSERT(num_ports);

	memset(p_tbl, 0, sizeof(*p_tbl));

	p_tbl->max_block_in_use = -1;

	if (capacity == 0) {
		/*
		   This switch apparently doesn't support multicast.
		   Everything is initialized to zero already, so return.
		 */
		return (IB_SUCCESS);
	}

	p_tbl->num_entries = capacity;
	p_tbl->num_ports = num_ports;
	p_tbl->max_position =
	    (uint8_t) ((ROUNDUP(num_ports, IB_MCAST_MASK_SIZE) /
			IB_MCAST_MASK_SIZE) - 1);

	p_tbl->max_block = (uint16_t) ((ROUNDUP(p_tbl->num_entries,
						IB_MCAST_BLOCK_SIZE) /
					IB_MCAST_BLOCK_SIZE) - 1);

	p_tbl->max_mlid_ho = (uint16_t) (IB_LID_MCAST_START_HO + capacity - 1);

	/*
	   The number of bytes needed in the mask table is:
	   The (maximum bit mask 'position' + 1) times the
	   number of bytes in each bit mask times the
	   number of MLIDs supported by the table.

	   We must always allocate the array with the maximum position
	   since it is (and must be) defined that way the table structure
	   in order to create a pointer to a two dimensional array.
	 */
	p_tbl->p_mask_tbl = malloc(p_tbl->num_entries *
				   (IB_MCAST_POSITION_MAX +
				    1) * IB_MCAST_MASK_SIZE / 8);

	if (p_tbl->p_mask_tbl == NULL)
		return (IB_INSUFFICIENT_MEMORY);

	memset(p_tbl->p_mask_tbl, 0,
	       p_tbl->num_entries * (IB_MCAST_POSITION_MAX +
				     1) * IB_MCAST_MASK_SIZE / 8);
	return (IB_SUCCESS);
}

/**********************************************************************
 **********************************************************************/
void osm_mcast_tbl_destroy(IN osm_mcast_tbl_t * const p_tbl)
{
	free(p_tbl->p_mask_tbl);
}

/**********************************************************************
 **********************************************************************/
void
osm_mcast_tbl_set(IN osm_mcast_tbl_t * const p_tbl,
		  IN const uint16_t mlid_ho, IN const uint8_t port)
{
	uintn_t mlid_offset;
	uintn_t mask_offset;
	uintn_t bit_mask;
	int16_t block_num;

	CL_ASSERT(p_tbl);
	CL_ASSERT(mlid_ho >= IB_LID_MCAST_START_HO);
	CL_ASSERT(mlid_ho <= p_tbl->max_mlid_ho);
	CL_ASSERT(p_tbl->p_mask_tbl);

	mlid_offset = mlid_ho - IB_LID_MCAST_START_HO;
	mask_offset = port / IB_MCAST_MASK_SIZE;
	bit_mask = cl_ntoh16((uint16_t) (1 << (port % IB_MCAST_MASK_SIZE)));
	(*p_tbl->p_mask_tbl)[mlid_offset][mask_offset] |= bit_mask;

	block_num = (int16_t) (mlid_offset / IB_MCAST_BLOCK_SIZE);

	if (block_num > p_tbl->max_block_in_use)
		p_tbl->max_block_in_use = (uint16_t) block_num;
}

/**********************************************************************
 **********************************************************************/
boolean_t
osm_mcast_tbl_is_port(IN const osm_mcast_tbl_t * const p_tbl,
		      IN const uint16_t mlid_ho, IN const uint8_t port_num)
{
	uintn_t mlid_offset;
	uintn_t mask_offset;
	uintn_t bit_mask;

	CL_ASSERT(p_tbl);

	if (p_tbl->p_mask_tbl) {
		CL_ASSERT(port_num <=
			  (p_tbl->max_position + 1) * IB_MCAST_MASK_SIZE);
		CL_ASSERT(mlid_ho >= IB_LID_MCAST_START_HO);
		CL_ASSERT(mlid_ho <= p_tbl->max_mlid_ho);

		mlid_offset = mlid_ho - IB_LID_MCAST_START_HO;
		mask_offset = port_num / IB_MCAST_MASK_SIZE;
		bit_mask = cl_ntoh16((uint16_t)
				     (1 << (port_num % IB_MCAST_MASK_SIZE)));
		return (((*p_tbl->
			  p_mask_tbl)[mlid_offset][mask_offset] & bit_mask) ==
			bit_mask);
	}

	return (FALSE);
}

/**********************************************************************
 **********************************************************************/
boolean_t
osm_mcast_tbl_is_any_port(IN const osm_mcast_tbl_t * const p_tbl,
			  IN const uint16_t mlid_ho)
{
	uintn_t mlid_offset;
	uint8_t position;
	uint16_t result = 0;

	CL_ASSERT(p_tbl);

	if (p_tbl->p_mask_tbl) {
		CL_ASSERT(mlid_ho >= IB_LID_MCAST_START_HO);
		CL_ASSERT(mlid_ho <= p_tbl->max_mlid_ho);

		mlid_offset = mlid_ho - IB_LID_MCAST_START_HO;

		for (position = 0; position <= p_tbl->max_position; position++)
			result |= (*p_tbl->p_mask_tbl)[mlid_offset][position];
	}

	return (result != 0);
}

/**********************************************************************
 **********************************************************************/
ib_api_status_t
osm_mcast_tbl_set_block(IN osm_mcast_tbl_t * const p_tbl,
			IN const ib_net16_t * const p_block,
			IN const int16_t block_num, IN const uint8_t position)
{
	uint32_t i;
	uint16_t mlid_start_ho;

	CL_ASSERT(p_tbl);
	CL_ASSERT(p_block);

	if (block_num > p_tbl->max_block)
		return (IB_INVALID_PARAMETER);

	if (position > p_tbl->max_position)
		return (IB_INVALID_PARAMETER);

	mlid_start_ho = (uint16_t) (block_num * IB_MCAST_BLOCK_SIZE);

	if (mlid_start_ho + IB_MCAST_BLOCK_SIZE - 1 > p_tbl->max_mlid_ho)
		return (IB_INVALID_PARAMETER);

	for (i = 0; i < IB_MCAST_BLOCK_SIZE; i++)
		(*p_tbl->p_mask_tbl)[mlid_start_ho + i][position] = p_block[i];

	if (block_num > p_tbl->max_block_in_use)
		p_tbl->max_block_in_use = (uint16_t) block_num;

	return (IB_SUCCESS);
}

/**********************************************************************
 **********************************************************************/
void
osm_mcast_tbl_clear_mlid(IN osm_mcast_tbl_t * const p_tbl,
			 IN const uint16_t mlid_ho)
{
	uint8_t i;
	uintn_t mlid_offset;

	CL_ASSERT(p_tbl);
	CL_ASSERT(mlid_ho >= IB_LID_MCAST_START_HO);

	if (p_tbl->p_mask_tbl && (mlid_ho <= p_tbl->max_mlid_ho)) {
		mlid_offset = mlid_ho - IB_LID_MCAST_START_HO;
		for (i = 0; i <= p_tbl->max_position; i++)
			(*p_tbl->p_mask_tbl)[mlid_offset][i] = 0;
	}
}

/**********************************************************************
 **********************************************************************/
boolean_t
osm_mcast_tbl_get_block(IN osm_mcast_tbl_t * const p_tbl,
			IN int16_t const block_num,
			IN uint8_t const position,
			OUT ib_net16_t * const p_block)
{
	uint32_t i;
	uint16_t mlid_start_ho;

	CL_ASSERT(p_tbl);
	CL_ASSERT(p_block);

	if (block_num > p_tbl->max_block_in_use)
		return (FALSE);

	if (position > p_tbl->max_position) {
		/*
		   Caller shouldn't do this for efficiency's sake...
		 */
		memset(p_block, 0, IB_SMP_DATA_SIZE);
		return (TRUE);
	}

	mlid_start_ho = (uint16_t) (block_num * IB_MCAST_BLOCK_SIZE);

	if (mlid_start_ho + IB_MCAST_BLOCK_SIZE - 1 > p_tbl->max_mlid_ho)
		return (IB_INVALID_PARAMETER);

	for (i = 0; i < IB_MCAST_BLOCK_SIZE; i++)
		p_block[i] = (*p_tbl->p_mask_tbl)[mlid_start_ho + i][position];

	return (TRUE);
}
