//    -*- C++ -*- 
//    This is part of the iostream library, providing parametrized manipulators
//    Written by Heinz G. Seidl, Copyright (C) 1992 Cygnus Support
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifdef __GNUG__
//#pragma implementation
#endif

#include "iomanip.h"


// Those functions are called through a pointer, 
// thus it does not make sense, to inline them.

ios & __iomanip_setbase (ios& i, int n)
{
    ios::fmtflags b;
    switch (n)
      {
	case  8: 
	  b = ios::oct; break;
	case 10: 
	  b = ios::dec; break;
	case 16: 
	  b = ios::hex; break;
	default:
	  b = 0;
      }
    i.setf(b, ios::basefield);
    return i;
}

ios & __iomanip_setfill (ios& i, int n)
{
    //FIXME if ( i.flags() & ios::widechar )
      i.fill( (char) n);
    //FIXME else
    //FIXME   i.fill( (wchar) n);
    return i;
}   

ios &  __iomanip_setprecision (ios& i, int n)
{
    i.precision(n);
    return i;
}
ios &  __iomanip_setw (ios& i, int n)
{
    i.width(n);
    return i;
}

ios & __iomanip_setiosflags (ios& i, ios::fmtflags n)
{
    i.setf(n,n);
    return i;
}

ios & __iomanip_resetiosflags (ios& i, ios::fmtflags n)
{
    i.setf(0,n);
    return i;
}
