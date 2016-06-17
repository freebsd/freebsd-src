/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */ 

#include <linux/types.h>
#include <linux/slab.h>
#include <asm/sn/sgi.h>
#include <asm/sn/io.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/router.h>
#include <asm/sn/module.h>
#include <asm/sn/ksys/l1.h>
#include <asm/sn/nodepda.h>
#include <asm/sn/clksupport.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/sn_sal.h>
#include <linux/ctype.h>

/* elsc_display_line writes up to 12 characters to either the top or bottom
 * line of the L1 display.  line points to a buffer containing the message
 * to be displayed.  The zero-based line number is specified by lnum (so
 * lnum == 0 specifies the top line and lnum == 1 specifies the bottom).
 * Lines longer than 12 characters, or line numbers not less than
 * L1_DISPLAY_LINES, cause elsc_display_line to return an error.
 */
int elsc_display_line(nasid_t nasid, char *line, int lnum)
{
    return 0;
}


/*
 * iobrick routines
 */

/* iobrick_rack_bay_type_get fills in the three int * arguments with the
 * rack number, bay number and brick type of the L1 being addressed.  Note
 * that if the L1 operation fails and this function returns an error value, 
 * garbage may be written to brick_type.
 */


int iobrick_rack_bay_type_get( nasid_t nasid, uint *rack, 
			       uint *bay, uint *brick_type )
{
	int result = 0;

	if ( ia64_sn_sysctl_iobrick_module_get(nasid, &result) )
		return( ELSC_ERROR_CMD_SEND );

	*rack = (result & MODULE_RACK_MASK) >> MODULE_RACK_SHFT;
	*bay = (result & MODULE_BPOS_MASK) >> MODULE_BPOS_SHFT;
	*brick_type = (result & MODULE_BTYPE_MASK) >> MODULE_BTYPE_SHFT;
	return 0;
}


int iomoduleid_get(nasid_t nasid)
{
	int result = 0;

	if ( ia64_sn_sysctl_iobrick_module_get(nasid, &result) )
		return( ELSC_ERROR_CMD_SEND );

	return result;
}

int iobrick_module_get(nasid_t nasid)
{
    uint rnum, rack, bay, brick_type, t;
    int ret;

    /* construct module ID from rack and slot info */

    if ((ret = iobrick_rack_bay_type_get(nasid, &rnum, &bay, &brick_type)) < 0)
        return ret;

    if (bay > MODULE_BPOS_MASK >> MODULE_BPOS_SHFT)
        return ELSC_ERROR_MODULE;

    /* Build a moduleid_t-compatible rack number */

    rack = 0;           
    t = rnum / 100;             /* rack class (CPU/IO) */
    if (t > RACK_CLASS_MASK(rack) >> RACK_CLASS_SHFT(rack))
        return ELSC_ERROR_MODULE;
    RACK_ADD_CLASS(rack, t);
    rnum %= 100;

    t = rnum / 10;              /* rack group */
    if (t > RACK_GROUP_MASK(rack) >> RACK_GROUP_SHFT(rack))
        return ELSC_ERROR_MODULE;
    RACK_ADD_GROUP(rack, t);

    t = rnum % 10;              /* rack number (one-based) */
    if (t-1 > RACK_NUM_MASK(rack) >> RACK_NUM_SHFT(rack))
        return ELSC_ERROR_MODULE;
    RACK_ADD_NUM(rack, t);

    switch( brick_type ) {
      case L1_BRICKTYPE_IX: 
	brick_type = MODULE_IXBRICK; break;
      case L1_BRICKTYPE_PX: 
	brick_type = MODULE_PXBRICK; break;
      case L1_BRICKTYPE_OPUS: 
	brick_type = MODULE_OPUSBRICK; break;
      case L1_BRICKTYPE_I: 
	brick_type = MODULE_IBRICK; break;
      case L1_BRICKTYPE_P:
	brick_type = MODULE_PBRICK; break;
      case L1_BRICKTYPE_X:
	brick_type = MODULE_XBRICK; break;
      case L1_BRICKTYPE_CHI_CG:
	brick_type = MODULE_CGBRICK; break;
    }

    ret = RBT_TO_MODULE(rack, bay, brick_type);

    return ret;
}

/*
 * iobrick_module_get_nasid() returns a module_id which has the brick
 * type encoded in bits 15-12, but this is not the true brick type...
 * The module_id returned by iobrick_module_get_nasid() is modified
 * to make a PEBRICKs & PXBRICKs look like a PBRICK.  So this routine
 * iobrick_type_get_nasid() returns the true unmodified brick type.
 */
int
iobrick_type_get_nasid(nasid_t nasid)
{
    uint rack, bay, type;
    int t, ret;
    extern char brick_types[];

    if ((ret = iobrick_rack_bay_type_get(nasid, &rack, &bay, &type)) < 0) {
        return ret;
    }

    /* convert brick_type to lower case */
    if ((type >= 'A') && (type <= 'Z'))
        type = type - 'A' + 'a';

    /* convert to a module.h brick type */
    for( t = 0; t < MAX_BRICK_TYPES; t++ ) {
        if( brick_types[t] == type ) {
            return t;
	}
    }

    return -1;    /* unknown brick */
}

int iobrick_module_get_nasid(nasid_t nasid)
{
    int io_moduleid;

    io_moduleid = iobrick_module_get(nasid);
    return io_moduleid;
}

/*
 * given a L1 bricktype, return a bricktype string.  This string is the
 * string that will be used in the hwpath for I/O bricks
 */
char *
iobrick_L1bricktype_to_name(int type)
{
    switch (type)
    {
    default:
        return("Unknown");

    case L1_BRICKTYPE_X:
        return(EDGE_LBL_XBRICK);

    case L1_BRICKTYPE_I:
        return(EDGE_LBL_IBRICK);

    case L1_BRICKTYPE_P:
        return(EDGE_LBL_PBRICK);

    case L1_BRICKTYPE_PX:
        return(EDGE_LBL_PXBRICK);

    case L1_BRICKTYPE_OPUS:
        return(EDGE_LBL_OPUSBRICK);

    case L1_BRICKTYPE_IX:
        return(EDGE_LBL_IXBRICK);

    case L1_BRICKTYPE_C:
        return("Cbrick");

    case L1_BRICKTYPE_R:
        return("Rbrick");

    case L1_BRICKTYPE_CHI_CG:
        return(EDGE_LBL_CGBRICK);
    }
}

