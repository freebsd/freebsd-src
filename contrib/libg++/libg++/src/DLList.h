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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/


#ifndef _DLList_h
#ifdef __GNUG__
//#pragma interface
#endif
#define _DLList_h 1

#undef OK

#include <Pix.h>

struct BaseDLNode {
    BaseDLNode *bk;
    BaseDLNode *fd;
    void *item() {return (void*)(this+1);} //Return ((DLNode<T>*)this)->hd
};

template<class T>
class DLNode : public BaseDLNode
{
  public:
    T hd;
    DLNode() { }
    DLNode(const T& h, DLNode* p = 0, DLNode* n = 0)
        : hd(h) { bk = p; fd = n; }
    ~DLNode() { }
};

class BaseDLList {
  protected:
    BaseDLNode *h;

    BaseDLList() { h = 0; }
    void copy(const BaseDLList&);
    BaseDLList& operator= (const BaseDLList& a);
    virtual void delete_node(BaseDLNode*node) = 0;
    virtual BaseDLNode* copy_node(const void* datum) = 0;
    virtual void copy_item(void *dst, void *src) = 0;
    virtual ~BaseDLList() { }

    Pix                   prepend(const void*);
    Pix                   append(const void*);
    Pix ins_after(Pix p, const void *datum);
    Pix ins_before(Pix p, const void *datum);
    void remove_front(void *dst);
    void remove_rear(void *dst);
    void join(BaseDLList&);

  public:
    int                   empty() const { return h == 0; }
    int                   length() const;
    void                  clear();
    void                  error(const char* msg) const;
    int                   owns(Pix p) const;
    int                   OK() const;
    void                  del(Pix& p, int dir = 1);
    void                  del_after(Pix& p);
    void                  del_front();
    void                  del_rear();
};

template <class T>
class DLList : public BaseDLList {
    //friend class          <T>DLListTrav;

    virtual void delete_node(BaseDLNode *node) { delete (DLNode<T>*)node; }
    virtual BaseDLNode* copy_node(const void *datum)
	{ return new DLNode<T>(*(const T*)datum); }
    virtual void copy_item(void *dst, void *src) { *(T*)dst = *(T*)src; }

  public:
    DLList() : BaseDLList() { }
    DLList(const DLList<T>& a) : BaseDLList() { copy(a); }

    DLList<T>&            operator = (const DLList<T>& a)
	{ BaseDLList::operator=((const BaseDLList&) a); return *this; }
    virtual ~DLList() { clear(); }

    Pix prepend(const T& item) {return BaseDLList::prepend(&item);}
    Pix append(const T& item) {return BaseDLList::append(&item);}

    void join(DLList<T>& a) { BaseDLList::join(a); }

    T& front() {
	if (h == 0) error("front: empty list");
	return ((DLNode<T>*)h)->hd; }
    T& rear() {
	if (h == 0) error("rear: empty list");
	return ((DLNode<T>*)h->bk)->hd;
    }
    const T& front() const {
	if (h == 0) error("front: empty list");
	return ((DLNode<T>*)h)->hd; }
    const T& rear() const {
	if (h == 0) error("rear: empty list");
	return ((DLNode<T>*)h->bk)->hd;
    }
    T remove_front() { T dst; BaseDLList::remove_front(&dst); return dst; }
    T remove_rear() { T dst; BaseDLList::remove_rear(&dst); return dst; }

    T&                  operator () (Pix p) {
	if (p == 0) error("null Pix");
	return ((DLNode<T>*)p)->hd;
    }
    const T&              operator () (Pix p) const {
	if (p == 0) error("null Pix");
	return ((DLNode<T>*)p)->hd;
    }
    Pix                   first() const { return Pix(h); }
    Pix                   last()  const { return (h == 0) ? 0 : Pix(h->bk); }
    void                  next(Pix& p) const
	{ p = (p == 0 || p == h->bk)? 0 : Pix(((DLNode<T>*)p)->fd); }
    void                  prev(Pix& p) const
	{ p = (p == 0 || p == h)? 0 : Pix(((DLNode<T>*)p)->bk); }
    Pix ins_after(Pix p, const T& item)
      {return BaseDLList::ins_after(p, &item); }
    Pix ins_before(Pix p, const T& item)
      {return BaseDLList::ins_before(p, &item);}
};

#endif
