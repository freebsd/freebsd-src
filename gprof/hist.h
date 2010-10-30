/* hist.h

   Copyright 2000, 2001, 2002, 2004, 2005 Free Software Foundation, Inc.

This file is part of GNU Binutils.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef hist_h
#define hist_h

typedef struct histogram
{
  bfd_vma lowpc;
  bfd_vma highpc;
  unsigned int num_bins;
  int *sample;           /* Histogram samples (shorts in the file!).  */
} histogram;

histogram *histograms;
unsigned num_histograms;

/* Scale factor converting samples to pc values:
   each sample covers HIST_SCALE bytes.  */
extern double hist_scale;

extern void hist_read_rec        (FILE *, const char *);
extern void hist_write_hist      (FILE *, const char *);
extern void hist_assign_samples  (void);
extern void hist_print           (void);

/* Checks if ADDRESS is within the range of addresses for which
   we have histogram data.  Returns 1 if so and 0 otherwise.  */
extern int hist_check_address (unsigned address);

/* Given a range of addresses for a symbol, find a histogram record 
   that intersects with this range, and clips the range to that
   histogram record, modifying *P_LOWPC and *P_HIGHPC.
   
   If no intersection is found, *P_LOWPC and *P_HIGHPC will be set to
   one unspecified value.  If more that one intersection is found,
   an error is emitted.  */
extern void hist_clip_symbol_address (bfd_vma *p_lowpc, bfd_vma *p_highpc);

#endif /* hist_h */
