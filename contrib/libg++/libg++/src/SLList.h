// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988, 1992 Free Software Foundation
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef _SLList_h
#ifdef __GNUG__
//#pragma interface
#endif
#define _SLList_h 1

#undef OK

#include <Pix.h>

struct BaseSLNode
{
   union {
     struct BaseSLNode *tl;
     double dummy;  /* To force correct alignment */
   };
   void *item() {return (void*)(this+1);} // Return ((SLNode<T>*)this)->hd
};

template<class T>
class SLNode : public BaseSLNode
{
  public:
    T                    hd; // Data part of node
                         SLNode() { }
                         SLNode(const T& h, SLNode* t = 0)
			     : hd(h) { tl = t; }
                         ~SLNode() { }
};

extern int __SLListLength(BaseSLNode *ptr);

class BaseSLList {
  protected:
    BaseSLNode *last;
    virtual void delete_node(BaseSLNode*node) = 0;
    virtual BaseSLNode* copy_node(const void* datum) = 0;
    virtual void copy_item(void *dst, void *src) = 0;
    virtual ~BaseSLList() { }
    BaseSLList() { last = 0; }
    void copy(const BaseSLList&);
    BaseSLList& operator = (const BaseSLList& a);
    Pix ins_after(Pix p, const void *datum);
    Pix prepend(const void *datum);
    Pix append(const void *datum);
    int remove_front(void *dst, int signal_error = 0);
    void join(BaseSLList&);
  public:
    int length() const;
    int empty() const { return last == 0; }
    void clear();
    Pix                   prepend(BaseSLNode*);
    Pix                   append(BaseSLNode*);
    int                   OK() const;
    void                  error(const char* msg) const;
    void                  del_after(Pix p);
    int                   owns(Pix p) const;
    void                  del_front();
};

template <class T>
class SLList : public BaseSLList
{
  private:
    virtual void delete_node(BaseSLNode *node) { delete (SLNode<T>*)node; }
    virtual BaseSLNode* copy_node(const void *datum)
	{ return new SLNode<T>(*(const T*)datum); }
    virtual void copy_item(void *dst, void *src) { *(T*)dst = *(T*)src; }

public:
    SLList() : BaseSLList() { }
    SLList(const SLList<T>& a) : BaseSLList() { copy(a); }
    SLList<T>&            operator = (const SLList<T>& a)
	{ BaseSLList::operator=((const BaseSLList&) a); return *this; }
    virtual ~SLList() { clear(); }

    Pix prepend(const T& item) {return BaseSLList::prepend(&item);}
    Pix append(const T& item) {return BaseSLList::append(&item);}
    Pix prepend(SLNode<T>* node) {return BaseSLList::prepend(node);}
    Pix append(SLNode<T>* node) {return BaseSLList::append(node);}

    T& operator () (Pix p) {
	if (p == 0) error("null Pix");
	return ((SLNode<T>*)(p))->hd; }
    const T& operator () (Pix p) const {
	if (p == 0) error("null Pix");
	return ((SLNode<T>*)(p))->hd; }
    inline Pix first() const { return (last == 0) ? 0 : Pix(last->tl); }
    void next(Pix& p) const
	{ p = (p == 0 || p == last) ? 0 : Pix(((SLNode<T>*)(p))->tl); }
    Pix ins_after(Pix p, const T& item)
      { return BaseSLList::ins_after(p, &item); }
    void join(SLList<T>& a) { BaseSLList::join(a); }
    
    T& front() {
	if (last == 0) error("front: empty list");
	return ((SLNode<T>*)last->tl)->hd; }
    T& rear() {
	if (last == 0) error("rear: empty list");
	return ((SLNode<T>*)last)->hd; }
    const T& front() const {
	if (last == 0) error("front: empty list");
	return ((SLNode<T>*)last->tl)->hd; }
    const T& rear() const {
	if (last == 0) error("rear: empty list");
	return ((SLNode<T>*)last)->hd; }
    int remove_front(T& x) { return BaseSLList::remove_front(&x); }
    T remove_front() { T dst; BaseSLList::remove_front(&dst, 1); return dst; }
};

#endif
