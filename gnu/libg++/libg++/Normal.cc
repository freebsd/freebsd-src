/* 
Copyright (C) 1988 Free Software Foundation
    written by Dirk Grunwald (grunwald@cs.uiuc.edu)

This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#ifdef __GNUG__
#pragma implementation
#endif
#include <builtin.h>
#include <Random.h>
#include <Normal.h>
//
//	See Simulation, Modelling & Analysis by Law & Kelton, pp259
//
//	This is the ``polar'' method.
// 

double Normal::operator()()
{
    
    if (haveCachedNormal == 1) {
	haveCachedNormal = 0;
	return(cachedNormal * pStdDev + pMean );
    } else {
	
	for(;;) {
	    double u1 = pGenerator -> asDouble();
	    double u2 = pGenerator -> asDouble();
	    double v1 = 2 * u1 - 1;
	    double v2 = 2 * u2 - 1;
	    double w = (v1 * v1) + (v2 * v2);
	    
//
//	We actually generate two IID normal distribution variables.
//	We cache the one & return the other.
// 
	    if (w <= 1) {
		double y = sqrt( (-2 * log(w)) / w);
		double x1 = v1 * y;
		double x2 = v2 * y;
		
		haveCachedNormal = 1;
		cachedNormal = x2;
		return(x1 * pStdDev + pMean);
	    }
	}
    }
}

