/*
 *
 * Copyright (c) 1994
 * Hewlett-Packard Company
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Hewlett-Packard Company makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#ifndef STACK_H
#define STACK_H

#ifndef __GNUG__
#include <bool.h>
#endif
#include <heap.h>

template <class Container>
class stack {
friend bool operator==(const stack<Container>& x, const stack<Container>& y);
friend bool operator<(const stack<Container>& x, const stack<Container>& y);
public:
    typedef Container::value_type value_type;
    typedef Container::size_type size_type;
protected:
    Container c;
public:
    bool empty() const { return c.empty(); }
    size_type size() const { return c.size(); }
    value_type& top() { return c.back(); }
    const value_type& top() const { return c.back(); }
    void push(const value_type& x) { c.push_back(x); }
    void pop() { c.pop_back(); }
};

template <class Container>
bool operator==(const stack<Container>& x, const stack<Container>& y) {
    return x.c == y.c;
}

template <class Container>
bool operator<(const stack<Container>& x, const stack<Container>& y) {
    return x.c < y.c;
}

template <class Container>
class queue {
friend bool operator==(const queue<Container>& x, const queue<Container>& y);
friend bool operator<(const queue<Container>& x, const queue<Container>& y);
public:
    typedef Container::value_type value_type;
    typedef Container::size_type size_type;
protected:
    Container c;
public:
    bool empty() const { return c.empty(); }
    size_type size() const { return c.size(); }
    value_type& front() { return c.front(); }
    const value_type& front() const { return c.front(); }
    value_type& back() { return c.back(); }
    const value_type& back() const { return c.back(); }
    void push(const value_type& x) { c.push_back(x); }
    void pop() { c.pop_front(); }
};

template <class Container>
bool operator==(const queue<Container>& x, const queue<Container>& y) {
    return x.c == y.c;
}

template <class Container>
bool operator<(const queue<Container>& x, const queue<Container>& y) {
    return x.c < y.c;
}

template <class Container, class Compare> 
// Compare = less<Container::value_type> >
class  priority_queue {
public:
    typedef Container::value_type value_type;
    typedef Container::size_type size_type;
protected:
    Container c;
    Compare comp;
public:
    priority_queue(const Compare& x = Compare()) :  c(), comp(x) {}
    priority_queue(const value_type* first, const value_type* last, 
		   const Compare& x = Compare()) : c(first, last), comp(x) {
	make_heap(c.begin(), c.end(), comp);
    }
/*
    template <class InputIterator>
    priority_queue(InputIterator first, InputIterator last, 
		   const Compare& x = Compare()) : c(first, last), comp(x) {
	make_heap(c.begin(), c.end(), comp);
    }
*/
    bool empty() const { return c.empty(); }
    size_type size() const { return c.size(); }
    value_type& top() { return c.front(); }
    const value_type& top() const { return c.front(); }
    void push(const value_type& x) { 
	c.push_back(x); 
	push_heap(c.begin(), c.end(), comp);
    }
    void pop() { 
	pop_heap(c.begin(), c.end(), comp);
	c.pop_back(); 
    }
};

// no equality is provided

#endif
