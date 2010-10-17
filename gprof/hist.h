/* hist.h

   Copyright 2000, 2001 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef hist_h
#define hist_h

extern bfd_vma s_lowpc;		/* Lowpc from the profile file.  */
extern bfd_vma s_highpc;	/* Highpc from the profile file.  */
extern bfd_vma lowpc, highpc;	/* Range profiled, in UNIT's.  */
extern int hist_num_bins;	/* Number of histogram bins.  */
extern int *hist_sample;	/* Code histogram.  */

/* Scale factor converting samples to pc values:
   each sample covers HIST_SCALE bytes.  */
extern double hist_scale;


extern void hist_read_rec        PARAMS ((FILE *, const char *));
extern void hist_write_hist      PARAMS ((FILE *, const char *));
extern void hist_assign_samples  PARAMS ((void));
extern void hist_print           PARAMS ((void));

#endif /* hist_h */
