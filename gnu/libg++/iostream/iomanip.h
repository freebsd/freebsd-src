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

#ifndef _IOMANIP_H
//
// Not specifying `pragma interface' causes the compiler to emit the 
// template definitions in the files, where they are used.
//
//#ifdef __GNUG__
//#pragma interface
//#endif
#define _IOMANIP_H

#include <_G_config.h>

#ifndef _G_NO_TEMPLATES

#include <iostream.h>

//-----------------------------------------------------------------------------
//	Parametrized Manipulators as specified by ANSI draft
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//	Stream Manipulators
//-----------------------------------------------------------------------------
//
template<class TP> class smanip; // TP = Type Param

template<class TP> class sapp {
    ios& (*_f)(ios&, TP);
public: 
    sapp(ios& (*f)(ios&, TP)) : _f(f) {}
    //
    smanip<TP> operator()(TP a) 
      { return smanip<TP>(_f, a); }
};

template <class TP> class smanip {
    ios& (*_f)(ios&, TP);
    TP _a;
public:
    smanip(ios& (*f)(ios&, TP), TP a) : _f(f), _a(a) {}
    //
    friend 
      istream& operator>>(istream& i, const smanip<TP>& m);
    friend
      ostream& operator<<(ostream& o, const smanip<TP>& m);
};

template<class TP>
inline istream& operator>>(istream& i, const smanip<TP>& m)
	{ (*m._f)(i, m._a); return i; }

template<class TP>
inline ostream& operator<<(ostream& o, const smanip<TP>& m)
	{ (*m._f)(o, m._a); return o;}

//-----------------------------------------------------------------------------
//	Input-Stream Manipulators
//-----------------------------------------------------------------------------
//
template<class TP> class imanip; 

template<class TP> class iapp {
    istream& (*_f)(istream&, TP);
public: 
    iapp(ostream& (*f)(istream&,TP)) : _f(f) {}
    //
    imanip<TP> operator()(TP a)
       { return imanip<TP>(_f, a); }
};

template <class TP> class imanip {
    istream& (*_f)(istream&, TP);
    TP _a;
public:
    imanip(istream& (*f)(istream&, TP), TP a) : _f(f), _a(a) {}
    //
    friend 
      istream& operator>>(istream& i, const imanip<TP>& m)
	{ return (*m._f)( i, m._a); }
};


//-----------------------------------------------------------------------------
//	Output-Stream Manipulators
//-----------------------------------------------------------------------------
//
template<class TP> class omanip; 

template<class TP> class oapp {
    ostream& (*_f)(ostream&, TP);
public: 
    oapp(ostream& (*f)(ostream&,TP)) : _f(f) {}
    //
    omanip<TP> operator()(TP a)
      { return omanip<TP>(_f, a); }
};

template <class TP> class omanip {
    ostream& (*_f)(ostream&, TP);
    TP _a;
public:
    omanip(ostream& (*f)(ostream&, TP), TP a) : _f(f), _a(a) {}
    //
    friend
      ostream& operator<<(ostream& o, omanip<TP>& m)
	{ return (*m._f)(o, m._a); }
};


//-----------------------------------------------------------------------------
//	Available Manipulators
//-----------------------------------------------------------------------------

//
// Macro to define an iomanip function, with one argument
// The underlying function is `__iomanip_<name>' 
//
#define __DEFINE_IOMANIP_FN1(type,param,function)         \
	extern ios& __iomanip_##function (ios&, param); \
	inline type<param> function (param n)           \
		        { return type<param> (__iomanip_##function, n); }

__DEFINE_IOMANIP_FN1( smanip, int, setbase)
__DEFINE_IOMANIP_FN1( smanip, int, setfill)
__DEFINE_IOMANIP_FN1( smanip, int, setprecision)
__DEFINE_IOMANIP_FN1( smanip, int, setw)

__DEFINE_IOMANIP_FN1( smanip, ios::fmtflags, resetiosflags)
__DEFINE_IOMANIP_FN1( smanip, ios::fmtflags, setiosflags)

#endif /*!_G_NO_TEMPLATES*/
#endif /*!_IOMANIP_H*/
