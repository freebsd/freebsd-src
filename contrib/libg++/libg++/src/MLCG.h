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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef _MLCG_h
#define _MLCG_h 1 
#ifdef __GNUG__
#pragma interface
#endif

#include <RNG.h>
#include <math.h>

//
//	Multiplicative Linear Conguential Generator
//

class MLCG : public RNG {
    _G_int32_t initialSeedOne;
    _G_int32_t initialSeedTwo;
    _G_int32_t seedOne;
    _G_int32_t seedTwo;

protected:

public:
    MLCG(_G_int32_t seed1 = 0, _G_int32_t seed2 = 1);
    //
    // Return a long-words word of random bits
    //
    virtual _G_uint32_t asLong();
    virtual void reset();
    _G_int32_t seed1();
    void seed1(_G_int32_t);
    _G_int32_t seed2();
    void seed2(_G_int32_t);
    void reseed(_G_int32_t, _G_int32_t);
};

inline _G_int32_t
MLCG::seed1()
{
    return(seedOne);
}

inline void
MLCG::seed1(_G_int32_t s)
{
    initialSeedOne = s;
    reset();
}

inline _G_int32_t
MLCG::seed2()
{
    return(seedTwo);
}

inline void
MLCG::seed2(_G_int32_t s)
{
    initialSeedTwo = s;
    reset();
}

inline void
MLCG::reseed(_G_int32_t s1, _G_int32_t s2)
{
    initialSeedOne = s1;
    initialSeedTwo = s2;
    reset();
}

#endif
