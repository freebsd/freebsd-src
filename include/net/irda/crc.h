/*********************************************************************
 *                
 * Filename:      crc.h
 * Version:       
 * Description:   CRC routines
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Mon Aug  4 20:40:53 1997
 * Modified at:   Sun May  2 20:25:23 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 ********************************************************************/

#ifndef IRDA_CRC_H
#define IRDA_CRC_H

#include <linux/types.h>

#define INIT_FCS  0xffff   /* Initial FCS value */
#define GOOD_FCS  0xf0b8   /* Good final FCS value */

extern __u16 const irda_crc16_table[];

/* Recompute the FCS with one more character appended. */
static inline __u16 irda_fcs(__u16 fcs, __u8 c)
{
	return (((fcs) >> 8) ^ irda_crc16_table[((fcs) ^ (c)) & 0xff]);
}

/* Recompute the FCS with len bytes appended. */
unsigned short irda_calc_crc16( __u16 fcs, __u8 const *buf, size_t len);

#endif
