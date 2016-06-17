/***************************************************************************
 *
 *  drivers/s390/char/tape3480.h
 *    tape device discipline for 3480 tapes.
 *
 *  S390 and zSeries version
 *    Copyright (C) 2001 IBM Corporation
 *    Author(s): Carsten Otte <cotte@de.ibm.com>
 *               Tuan Ngo-Anh <ngoanh@de.ibm.com>
 *
 ****************************************************************************
 */

#ifndef _TAPE3480_H

#define _TAPE3480_H


typedef struct _tape3480_disc_data_t {
    __u8 modeset_byte;
} tape3480_disc_data_t  __attribute__ ((packed, aligned(8)));
tape_discipline_t * tape3480_init (int);
#endif // _TAPE3480_H
