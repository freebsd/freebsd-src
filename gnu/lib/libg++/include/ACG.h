// This may look like C code, but it is really -*- C++ -*-
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
#ifndef _ACG_h
#define _ACG_h 1 

#include <RNG.h>
#include <math.h>
#ifdef __GNUG__
#pragma interface
#endif

//
//	Additive number generator. This method is presented in Volume II
//	of The Art of Computer Programming by Knuth. I've coded the algorithm
//	and have added the extensions by Andres Nowatzyk of CMU to randomize
//	the result of algorithm M a bit	by using an LCG & a spatial
//	permutation table.
//
//	The version presented uses the same constants for the LCG that Andres
//	uses (chosen by trial & error). The spatial permutation table is
//	the same size (it's based on word size). This is for 32-bit words.
//
//	The ``auxillary table'' used by the LCG table varies in size, and
//	is chosen to be the the smallest power of two which is larger than
//	twice the size of the state table.
//

class ACG : public RNG {

    unsigned long initialSeed;	// used to reset generator
    int initialTableEntry;

    unsigned long *state;
    unsigned long *auxState;
    short stateSize;
    short auxSize;
    unsigned long lcgRecurr;
    short j;
    short k;

protected:

public:
    ACG(unsigned long seed = 0, int size = 55);
    virtual ~ACG();
    //
    // Return a long-words word of random bits
    //
    virtual unsigned long asLong();
    virtual void reset();
};

#endif
