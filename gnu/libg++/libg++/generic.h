// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

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

#ifndef generic_h
#define generic_h 1

/*
 *	See the CPP manual, argument prescan section for explanation
 */

#define name2(a,b) gEnErIc2(a,b)
#define gEnErIc2(a,b) a ## b

#define name3(a,b,c) gEnErIc3(a,b,c)
#define gEnErIc3(a,b,c) a ## b ## c

#define name4(a,b,c,d) gEnErIc4(a,b,c,d)
#define gEnErIc4(a,b,c,d) a ## b ## c ## d

#define GENERIC_STRING(a) gEnErIcStRiNg(a)
#define gEnErIcStRiNg(a) #a

#define declare(clas,t)        name2(clas,declare)(t)
#define declare2(clas,t1,t2)   name2(clas,declare2)(t1,t2)

#define implement(clas,t)      name2(clas,implement)(t)
#define implement2(clas,t1,t2) name2(clas,implement2)(t1,t2)

//extern genericerror(int,char*);
typedef int (*GPT)(int,char*);

#define set_handler(gen,type,x) name4(set_,type,gen,_handler)(x)

#define errorhandler(gen,type)  name3(type,gen,handler)

#define callerror(gen,type,a,b) (*errorhandler(gen,type))(a,b)


#endif generic_h
