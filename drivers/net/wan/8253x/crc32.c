/* -*- linux-c -*- */
/******************************************************************************
 *  FILE:      crc32.c
 *
 *  Copyright: Telford Tools, Inc.
 *             1996
 *
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 ******************************************************************************
 */
/******************************************************************************
 *  REVISION HISTORY: (Most Recent First)
 *  -------------------------------------
 *  2-Apr-91    ALEX    Introduced "fn_calc_novram_crc32()".
 ******************************************************************************
 */

/****************************************************/
/*		    header files		    */
/****************************************************/

#include "crc32dcl.h"
#include "endian.h"

/****************************************************/
/*		    constants			    */
/****************************************************/

#define k_poly		    ((unsigned int)(0xedb88320))
#define k_crc_table_size    (256)

#if defined (AMD29K)
pragma Code ("rkernel");
pragma Off(cross_jump);
#endif

/****************************************************/
/*			static data		    */
/****************************************************/

#if defined(AMD29K)
pragma Data (Export,"fastbss");
#endif // defined(AMD29K)

unsigned int gg_a_crc_table[k_crc_table_size];

#if defined(AMD29K)
pragma Data;
#endif // defined(AMD29K)


/****************************************************/
/*		    global procedures		    */
/****************************************************/

void
fn_init_crc_table()
{
	short	i_table;
	
	for (i_table = 0; i_table < k_crc_table_size; i_table++)
	{
		unsigned int	result = 0;
		short	i_bit;
		
		for (i_bit = 0; i_bit < 8; i_bit++)
		{
			unsigned int    bit = ((i_table  & (1 << i_bit)) != 0);
			
			if ((bit ^ (result & 1)) != 0)
				result = (result >> 1) ^ k_poly;
			else
				result >>= 1;
		}
		
		gg_a_crc_table[i_table] = result;
	}
	
} /* end of fn_init_crc_table */

/****************************************************/

static unsigned int
fn_calc_memory_chunk_crc32(void *p, unsigned int n_bytes, unsigned int crc)
{
	unsigned char    *p_uc   = (unsigned char*)p;
	unsigned int   result  = ~crc;
	
	while (n_bytes-- > 0)
	{
		result = (result >> 8) ^ gg_a_crc_table[(result ^ *p_uc++) & 0xff];
	}
	
	return(~result);
	
} /* end of fn_calc_memory_chunk_crc32 */

/****************************************************/

unsigned int
fn_calc_memory_crc32(void *p, unsigned int n_bytes)
{
	fnm_assert_stmt(n_bytes > 4);
	
	return(fn_calc_memory_chunk_crc32(p, n_bytes, k_initial_crc_value));
	
} /* end of fn_calc_memory_crc32 */

/****************************************************/

unsigned int
fn_check_memory_crc32(void *p, unsigned int n_bytes, unsigned int crc)
{
	return(fn_calc_memory_crc32(p, n_bytes) == crc);
	
} /* end of fn_check_memory_crc32 */


/****************************************************/
/* Adds current longword to the crc value and       */
/* returns that value.                              */
unsigned int
fn_update_crc(char *val, unsigned int crcval)
{
	long i;
	
	/* ----< break long into bytes >---- */
	/* ----< put bytes into crc >---- */
	for (i = 0; i < 4; i++)
	{
		crcval = gg_a_crc_table[(crcval ^ val[i]) & 0xff] ^
			((crcval >> 8) & 0x00ffffff);
	}
	return(crcval);
}					/* endfunc--fn_update_crc */


/****************************************************/
/****************************************************/
/*	    End source file "crc32.c"		    */
/****************************************************/
/****************************************************/
