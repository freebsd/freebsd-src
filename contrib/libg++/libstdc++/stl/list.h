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

#ifndef LIST_H
#define LIST_H

#include <function.h>
#include <algobase.h>
#include <iterator.h>
#ifndef __GNUG__
#include <bool.h>
#endif
    
#ifndef Allocator
#define Allocator allocator
#include <defalloc.h>
#endif

#ifndef list 
#define list list
#endif

template <class T>
class list {
protected:
    typedef Allocator<void>::pointer void_pointer;
    struct list_node;
    friend list_node;
    struct list_node {
	void_pointer next;
	void_pointer prev;
	T data;
    };
#ifndef __GNUG__
    static Allocator<list_node> list_node_allocator;
    static Allocator<T> value_allocator;
#endif
public:      
    typedef T value_type;
    typedef Allocator<T> value_allocator_type;
    typedef Allocator<T>::pointer pointer;
    typedef Allocator<T>::reference reference;
    typedef Allocator<T>::const_reference const_reference;
    typedef Allocator<list_node> list_node_allocator_type;
    typedef Allocator<list_node>::pointer link_type;
    typedef Allocator<list_node>::size_type size_type;
    typedef Allocator<list_node>::difference_type difference_type;
protected:
#ifdef __GNUG__
    link_type get_node() { return (link_type)(::operator new(sizeof(list_node))); }
    void put_node(link_type p) { ::operator delete(p); }
#else
    size_type buffer_size() {
	return list_node_allocator.init_page_size();
    }
    struct list_node_buffer;
    friend list_node_buffer;
    struct list_node_buffer {
	void_pointer next_buffer;
	link_type buffer;
    };
public:
    typedef Allocator<list_node_buffer> buffer_allocator_type;
    typedef Allocator<list_node_buffer>::pointer buffer_pointer;     
protected:
    static Allocator<list_node_buffer> buffer_allocator;
    static buffer_pointer buffer_list;
    static link_type free_list;
    static link_type next_avail;
    static link_type last;
    void add_new_buffer() {
	buffer_pointer tmp = buffer_allocator.allocate((size_type)1);
	tmp->buffer = list_node_allocator.allocate(buffer_size());
	tmp->next_buffer = buffer_list;
	buffer_list = tmp;
	next_avail = buffer_list->buffer;
	last = next_avail + buffer_size();
    }
    static size_type number_of_lists;
    void deallocate_buffers();
    link_type get_node() {
	link_type tmp = free_list;
	return free_list ? (free_list = (link_type)(free_list->next), tmp) 
	    : (next_avail == last ? (add_new_buffer(), next_avail++) 
		: next_avail++);
	// ugly code for inlining - avoids multiple returns
    }
    void put_node(link_type p) {
	p->next = free_list;
	free_list = p;
    }
#endif

    link_type node;
    size_type length;
public:
    class iterator;
    class const_iterator;
    class iterator : public bidirectional_iterator<T, difference_type> {
    friend class list<T>;
    friend class const_iterator;
//  friend bool operator==(const iterator& x, const iterator& y);
    protected:
	link_type node;
	iterator(link_type x) : node(x) {}
    public:
	iterator() {}
	bool operator==(const iterator& x) const { return node == x.node; }
	reference operator*() const { return (*node).data; }
	iterator& operator++() { 
	    node = (link_type)((*node).next);
	    return *this;
	}
	iterator operator++(int) { 
	    iterator tmp = *this;
	    ++*this;
	    return tmp;
	}
	iterator& operator--() { 
	    node = (link_type)((*node).prev);
	    return *this;
	}
	iterator operator--(int) { 
	    iterator tmp = *this;
	    --*this;
	    return tmp;
	}
    };
    class const_iterator : public bidirectional_iterator<T, difference_type> {
    friend class list<T>;
    protected:
	link_type node;
	const_iterator(link_type x) : node(x) {}
    public:     
	const_iterator() {}
	const_iterator(const iterator& x) : node(x.node) {}
	bool operator==(const const_iterator& x) const { return node == x.node; } 
	const_reference operator*() const { return (*node).data; }
	const_iterator& operator++() { 
	    node = (link_type)((*node).next);
	    return *this;
	}
	const_iterator operator++(int) { 
	    const_iterator tmp = *this;
	    ++*this;
	    return tmp;
	}
	const_iterator& operator--() { 
	    node = (link_type)((*node).prev);
	    return *this;
	}
	const_iterator operator--(int) { 
	    const_iterator tmp = *this;
	    --*this;
	    return tmp;
	}
    };
    typedef reverse_bidirectional_iterator<const_iterator, value_type,
                                           const_reference, difference_type>
	const_reverse_iterator;
    typedef reverse_bidirectional_iterator<iterator, value_type, reference,
                                           difference_type>
        reverse_iterator; 
    list() : length(0) {
#ifndef __GNUG__
	++number_of_lists;
#endif
	node = get_node();
	(*node).next = node;
	(*node).prev = node;
    }
    iterator begin() { return (link_type)((*node).next); }
    const_iterator begin() const { return (link_type)((*node).next); }
    iterator end() { return node; }
    const_iterator end() const { return node; }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { 
        return const_reverse_iterator(end()); 
    }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { 
        return const_reverse_iterator(begin());
    } 
    bool empty() const { return length == 0; }
    size_type size() const { return length; }
#ifndef __GNUG__
    size_type max_size() const { return list_node_allocator.max_size(); }
#endif
    reference front() { return *begin(); }
    const_reference front() const { return *begin(); }
    reference back() { return *(--end()); }
    const_reference back() const { return *(--end()); }
    void swap(list<T>& x) {
	::swap(node, x.node);
	::swap(length, x.length);
    }
    iterator insert(iterator position, const T& x) {
	link_type tmp = get_node();
#ifdef __GNUG__
	construct(&(*tmp).data, x);
#else
	construct(value_allocator.address((*tmp).data), x);
#endif
	(*tmp).next = position.node;
	(*tmp).prev = (*position.node).prev;
	(*(link_type((*position.node).prev))).next = tmp;
	(*position.node).prev = tmp;
	++length;
	return tmp;
    }
#ifdef __GNUG__
    void insert(iterator position, const T* first, const T* last) {
	while (first != last) insert(position, *first++);
    }
    void insert(iterator position, const_iterator first,
		const_iterator last) {
        while (first != last) insert(position, *first++);
    }
    void insert(iterator position, size_type n, const T& x) {
	while (n--) insert(position, x);
    }
#else
    void insert(iterator position, const T* first, const T* last);
    void insert(iterator position, const_iterator first,
		const_iterator last);
    void insert(iterator position, size_type n, const T& x);
#endif
    void push_front(const T& x) { insert(begin(), x); }
    void push_back(const T& x) { insert(end(), x); }
    void erase(iterator position) {
	(*(link_type((*position.node).prev))).next = (*position.node).next;
	(*(link_type((*position.node).next))).prev = (*position.node).prev;
#ifdef __GNUG__
	destroy(&(*position.node).data);
#else
	destroy(value_allocator.address((*position.node).data));
#endif
	put_node(position.node);
	--length;
    }
#ifdef __GNUG__
    void erase(iterator first, iterator last) {
	while (first != last) erase(first++);
    }
#else
    void erase(iterator first, iterator last);
#endif
    void pop_front() { erase(begin()); }
    void pop_back() { 
	iterator tmp = end();
	erase(--tmp);
    }
    list(size_type n, const T& value = T()) : length(0) {
#ifndef __GNUG__
	++number_of_lists;
#endif
	node = get_node();
	(*node).next = node;
	(*node).prev = node;
	insert(begin(), n, value);
    }
    list(const T* first, const T* last) : length(0) {
#ifndef __GNUG__
	++number_of_lists;
#endif
	node = get_node();
	(*node).next = node;
	(*node).prev = node;
	insert(begin(), first, last);
    }
    list(const list<T>& x) : length(0) {
#ifndef __GNUG__
	++number_of_lists;
#endif
	node = get_node();
	(*node).next = node;
	(*node).prev = node;
	insert(begin(), x.begin(), x.end());
    }
    ~list() {
	erase(begin(), end());
	put_node(node);
#ifndef __GNUG__
	if (--number_of_lists == 0) deallocate_buffers();
#endif
    }
    list<T>& operator=(const list<T>& x);
protected:
    void transfer(iterator position, iterator first, iterator last) {
	(*(link_type((*last.node).prev))).next = position.node;
	(*(link_type((*first.node).prev))).next = last.node;
	(*(link_type((*position.node).prev))).next = first.node;  
	link_type tmp = link_type((*position.node).prev);
	(*position.node).prev = (*last.node).prev;
	(*last.node).prev = (*first.node).prev; 
	(*first.node).prev = tmp;
    }
public:
    void splice(iterator position, list<T>& x) {
	if (!x.empty()) {
	    transfer(position, x.begin(), x.end());
	    length += x.length;
	    x.length = 0;
	}
    }
    void splice(iterator position, list<T>& x, iterator i) {
	iterator j = i;
	if (position == i || position == ++j) return;
	transfer(position, i, j);
	++length;
	--x.length;
    }
    void splice(iterator position, list<T>& x, iterator first, iterator last) {
	if (first != last) {
	    if (&x != this) {
		difference_type n = 0;
	    	distance(first, last, n);
	    	x.length -= n;
	    	length += n;
	    }
	    transfer(position, first, last);
	}
    }
    void remove(const T& value);
    void unique();
    void merge(list<T>& x);
    void reverse();
    void sort();
#ifdef __GNUG__
    friend difference_type* distance_type(const iterator&) {
	return (difference_type*)(0);
    }
    friend T* value_type(const iterator&) {
	return (T*)(0);
    }
    friend bidirectional_iterator_tag iterator_category(iterator) {
	return bidirectional_iterator_tag();
    }
#endif
};

#ifndef __GNUG__
template <class T>
list<T>::buffer_pointer list<T>::buffer_list = 0;

template <class T>
list<T>::link_type list<T>::free_list = 0;

template <class T>
list<T>::link_type list<T>::next_avail = 0;

template <class T>
list<T>::link_type list<T>::last = 0;

template <class T>
list<T>::size_type list<T>::number_of_lists = 0;

template <class T>
list<T>::list_node_allocator_type list<T>::list_node_allocator;

template <class T>
list<T>::value_allocator_type list<T>::value_allocator;

template <class T>
list<T>::buffer_allocator_type list<T>::buffer_allocator;
#endif

/* 
 * currently the following does not work - made into a member function

template <class T>
inline bool operator==(const list<T>::iterator& x, const list<T>::iterator& y) { 
    return x.node == y.node; 
}
*/

template <class T>
inline bool operator==(const list<T>& x, const list<T>& y) {
    return x.size() == y.size() && equal(x.begin(), x.end(), y.begin());
}

template <class T>
inline bool operator<(const list<T>& x, const list<T>& y) {
    return lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());
}

#ifndef __GNUG__
template <class T>
void list<T>::deallocate_buffers() {
    while (buffer_list) {
	buffer_pointer tmp = buffer_list;
	buffer_list = (buffer_pointer)(buffer_list->next_buffer);
	list_node_allocator.deallocate(tmp->buffer);
	buffer_allocator.deallocate(tmp);
    }
    free_list = 0;
    next_avail = 0;
    last = 0;
}
#endif

#ifndef __GNUG__
template <class T>
void list<T>::insert(iterator position, const T* first, const T* last) {
    while (first != last) insert(position, *first++);
}
	 
template <class T>
void list<T>::insert(iterator position, const_iterator first,
		     const_iterator last) {
    while (first != last) insert(position, *first++);
}

template <class T>
void list<T>::insert(iterator position, size_type n, const T& x) {
    while (n--) insert(position, x);
}

template <class T>
void list<T>::erase(iterator first, iterator last) {
    while (first != last) erase(first++);
}
#endif

template <class T>
list<T>& list<T>::operator=(const list<T>& x) {
    if (this != &x) {
	iterator first1 = begin();
	iterator last1 = end();
	const_iterator first2 = x.begin();
	const_iterator last2 = x.end();
	while (first1 != last1 && first2 != last2) *first1++ = *first2++;
	if (first2 == last2)
	    erase(first1, last1);
	else
	    insert(last1, first2, last2);
    }
    return *this;
}

template <class T>
void list<T>::remove(const T& value) {
    iterator first = begin();
    iterator last = end();
    while (first != last) {
	iterator next = first;
	++next;
	if (*first == value) erase(first);
	first = next;
    }
}

template <class T>
void list<T>::unique() {
    iterator first = begin();
    iterator last = end();
    if (first == last) return;
    iterator next = first;
    while (++next != last) {
	if (*first == *next)
	    erase(next);
	else
	    first = next;
	next = first;
    }
}

template <class T>
void list<T>::merge(list<T>& x) {
    iterator first1 = begin();
    iterator last1 = end();
    iterator first2 = x.begin();
    iterator last2 = x.end();
    while (first1 != last1 && first2 != last2)
	if (*first2 < *first1) {
	    iterator next = first2;
	    transfer(first1, first2, ++next);
	    first2 = next;
	} else
	    ++first1;
    if (first2 != last2) transfer(last1, first2, last2);
    length += x.length;
    x.length= 0;
}

template <class T>
void list<T>::reverse() {
    if (size() < 2) return;
    for (iterator first = ++begin(); first != end();) {
	iterator old = first++;
	transfer(begin(), old, first);
    }
}    

template <class T>
void list<T>::sort() {
    if (size() < 2) return;
    list<T> carry;
    list<T> counter[64];
    int fill = 0;
    while (!empty()) {
	carry.splice(carry.begin(), *this, begin());
	int i = 0;
	while(i < fill && !counter[i].empty()) {
	    counter[i].merge(carry);
	    carry.swap(counter[i++]);
	}
	carry.swap(counter[i]);         
	if (i == fill) ++fill;
    } 

    for (int i = 1; i < fill; ++i) counter[i].merge(counter[i-1]);
    swap(counter[fill-1]);
}

#undef Allocator
#undef list

#endif
