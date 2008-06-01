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

#ifndef VECTOR_H
#define VECTOR_H

#include <function.h>
#include <algobase.h>
#ifndef __GNUG__
#include <bool.h>
#endif

#ifndef Allocator
#define Allocator allocator
#include <defalloc.h>
#endif

#ifndef vector
#define vector vector
#endif

template <class T>
class vector {
public:
    
    typedef Allocator<T> vector_allocator;
    typedef T value_type;
    typedef vector_allocator::pointer pointer;
    typedef vector_allocator::pointer iterator;
    typedef vector_allocator::const_pointer const_iterator;
    typedef vector_allocator::reference reference;
    typedef vector_allocator::const_reference const_reference;
    typedef vector_allocator::size_type size_type;
    typedef vector_allocator::difference_type difference_type;
    typedef reverse_iterator<const_iterator, value_type, const_reference, 
                             difference_type>  const_reverse_iterator;
    typedef reverse_iterator<iterator, value_type, reference, difference_type>
        reverse_iterator;
protected:
    static Allocator<T> static_allocator;
    iterator start;
    iterator finish;
    iterator end_of_storage;
#ifdef __GNUG__
    void insert_aux(iterator position, const T& x) {
	insert_aux(vector_iterator<T>(position), x);
    }
    void insert_aux(vector_iterator<T> position, const T& x);
#else
    void insert_aux(iterator position, const T& x);
#endif
public:
    iterator begin() { return start; }
    const_iterator begin() const { return start; }
    iterator end() { return finish; }
    const_iterator end() const { return finish; }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { 
        return const_reverse_iterator(end()); 
    }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { 
        return const_reverse_iterator(begin()); 
    }
    size_type size() const { return size_type(end() - begin()); }
    size_type max_size() const { return static_allocator.max_size(); }
    size_type capacity() const { return size_type(end_of_storage - begin()); }
    bool empty() const { return begin() == end(); }
    reference operator[](size_type n) { return *(begin() + n); }
    const_reference operator[](size_type n) const { return *(begin() + n); }
    vector() : start(0), finish(0), end_of_storage(0) {}
    vector(size_type n, const T& value = T()) {
	start = static_allocator.allocate(n);
	uninitialized_fill_n(start, n, value);
	finish = start + n;
	end_of_storage = finish;
    }
    vector(const vector<T>& x) {
	start = static_allocator.allocate(x.end() - x.begin());
	finish = uninitialized_copy(x.begin(), x.end(), start);
	end_of_storage = finish;
    }
    vector(const_iterator first, const_iterator last) {
	size_type n = 0;
	distance(first, last, n);
	start = static_allocator.allocate(n);
	finish = uninitialized_copy(first, last, start);
	end_of_storage = finish;
    }
    ~vector() { 
	destroy(start, finish);
	static_allocator.deallocate(start);
    }
    vector<T>& operator=(const vector<T>& x);
    void reserve(size_type n) {
	if (capacity() < n) {
	    iterator tmp = static_allocator.allocate(n);
	    uninitialized_copy(begin(), end(), tmp);
	    destroy(start, finish);
	    static_allocator.deallocate(start);
	    finish = tmp + size();
	    start = tmp;
	    end_of_storage = begin() + n;
	}
    }
    reference front() { return *begin(); }
    const_reference front() const { return *begin(); }
    reference back() { return *(end() - 1); }
    const_reference back() const { return *(end() - 1); }
    void push_back(const T& x) {
	if (finish != end_of_storage) {
	    /* Borland bug */
	    construct(finish, x);
	    finish++;
	} else
	    insert_aux(end(), x);
    }
    void swap(vector<T>& x) {
	::swap(start, x.start);
	::swap(finish, x.finish);
	::swap(end_of_storage, x.end_of_storage);
    }
    iterator insert(iterator position, const T& x) {
	size_type n = position - begin();
	if (finish != end_of_storage && position == end()) {
	    /* Borland bug */
	    construct(finish, x);
	    finish++;
	} else
	    insert_aux(position, x);
	return begin() + n;
    }
#ifdef __GNUG__
    void insert (iterator position, const_iterator first, 
		 const_iterator last) {
        insert(vector_iterator<T>(position),
	       vector_const_iterator<T>(first),
	       vector_const_iterator<T>(last));
    }
    void insert (vector_iterator<T> position, vector_const_iterator<T> first, 
		 vector_const_iterator<T> last);
    void insert (iterator position, size_type n, const T& x) {
	insert(vector_iterator<T>(position), n, x);
    }
    void insert (vector_iterator<T> position, size_type n, const T& x);
#else
    void insert (iterator position, const_iterator first, 
		 const_iterator last);
    void insert (iterator position, size_type n, const T& x);
#endif
    void pop_back() {
	/* Borland bug */
        --finish;
        destroy(finish);
    }
    void erase(iterator position) {
	if (position + 1 != end())
	    copy(position + 1, end(), position);
	/* Borland bug */
	--finish;
	destroy(finish);
    }
    void erase(iterator first, iterator last) {
	vector<T>::iterator i = copy(last, end(), first);
	destroy(i, finish);
	// work around for destroy(copy(last, end(), first), finish);
	finish = finish - (last - first); 
    }
};

#ifdef __GNUG__
template <class T>
struct vector_iterator {
    vector<T>::iterator it;
    vector_iterator(vector<T>::iterator i) : it(i) {}
    operator vector<T>::iterator() {
	return it;
    }
};

template <class T>
inline T* value_type(const vector_iterator<T>&) {
    return (T*)(0);
}


template <class T>
struct vector_const_iterator {
    vector<T>::const_iterator it;
    vector_const_iterator(vector<T>::const_iterator i) : it(i) {}
    operator vector<T>::const_iterator() {
	return it;
    }
};
#endif

template <class T>
inline bool operator==(const vector<T>& x, const vector<T>& y) {
    return x.size() == y.size() && equal(x.begin(), x.end(), y.begin());
}

template <class T>
inline bool operator<(const vector<T>& x, const vector<T>& y) {
    return lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());
}

#ifndef __GNUG__
template <class T>
vector<T>::vector_allocator vector<T>::static_allocator;
#endif

template <class T>
vector<T>& vector<T>::operator=(const vector<T>& x) {
    if (&x == this) return *this;
    if (x.size() > capacity()) {
	destroy(start, finish);
	static_allocator.deallocate(start);
	start = static_allocator.allocate(x.end() - x.begin());
	end_of_storage = uninitialized_copy(x.begin(), x.end(), start);
    } else if (size() >= x.size()) {
	vector<T>::iterator i = copy(x.begin(), x.end(), begin());
	destroy(i, finish);
	// work around for destroy(copy(x.begin(), x.end(), begin()), finish);
    } else {
	copy(x.begin(), x.begin() + size(), begin());
	uninitialized_copy(x.begin() + size(), x.end(), begin() + size());
    }
    finish = begin() + x.size();
    return *this;
}

template <class T>
#ifdef __GNUG__
void vector<T>::insert_aux(vector_iterator<T> posn, const T& x) {
    iterator position = posn;
#else
void vector<T>::insert_aux(iterator position, const T& x) {
#endif
    if (finish != end_of_storage) {
	construct(finish, *(finish - 1));
	copy_backward(position, finish - 1, finish);
	*position = x;
	++finish;
    } else {
	size_type len = size() ? 2 * size() 
	    : static_allocator.init_page_size();
	iterator tmp = static_allocator.allocate(len);
	uninitialized_copy(begin(), position, tmp);
	construct(tmp + (position - begin()), x);
	uninitialized_copy(position, end(), tmp + (position - begin()) + 1); 
	destroy(begin(), end());
	static_allocator.deallocate(begin());
	end_of_storage = tmp + len;
	finish = tmp + size() + 1;
	start = tmp;
    }
}

template <class T>
#ifdef __GNUG__
void vector<T>::insert(vector_iterator<T> posn,
		       size_t n,
		       const T& x) {
    iterator position = posn;
#else
void vector<T>::insert(iterator position, size_type n, const T& x) {
#endif
    if (n == 0) return;
    if ((size_type) (end_of_storage - finish) >= n) {
	if ((size_type) (end() - position) > n) {
	    uninitialized_copy(end() - n, end(), end());
	    copy_backward(position, end() - n, end());
	    fill(position, position + n, x);
	} else {
	    uninitialized_copy(position, end(), position + n);
	    fill(position, end(), x);
	    uninitialized_fill_n(end(), n - (end() - position), x);
	}
	finish += n;
    } else {
	size_type len = size() + max(size(), n);
	iterator tmp = static_allocator.allocate(len);
	uninitialized_copy(begin(), position, tmp);
	uninitialized_fill_n(tmp + (position - begin()), n, x);
	uninitialized_copy(position, end(), tmp + (position - begin() + n));
	destroy(begin(), end());
	static_allocator.deallocate(begin());
	end_of_storage = tmp + len;
	finish = tmp + size() + n;
	start = tmp;
    }
}

template <class T>
#ifdef __GNUG__
void vector<T>::insert(vector_iterator<T> posn, 
		       vector_const_iterator<T> fi, 
		       vector_const_iterator<T> la) {
    iterator position = posn;
    const_iterator first = fi;
    const_iterator last = la;
#else
void vector<T>::insert(iterator position, 
		       const_iterator first, 
		       const_iterator last) {
#endif
    if (first == last) return;
    size_type n = 0;
    distance(first, last, n);
    if ((size_type) (end_of_storage - finish) >= n) {
	if ((size_type) (end() - position) > n) {
	    uninitialized_copy(end() - n, end(), end());
	    copy_backward(position, end() - n, end());
	    copy(first, last, position);
	} else {
	    uninitialized_copy(position, end(), position + n);
	    copy(first, first + (end() - position), position);
	    uninitialized_copy(first + (end() - position), last, end());
	}
	finish += n;
    } else {
	size_type len = size() + max(size(), n);
	iterator tmp = static_allocator.allocate(len);
	uninitialized_copy(begin(), position, tmp);
	uninitialized_copy(first, last, tmp + (position - begin()));
	uninitialized_copy(position, end(), tmp + (position - begin() + n));
	destroy(begin(), end());
	static_allocator.deallocate(begin());
	end_of_storage = tmp + len;
	finish = tmp + size() + n;
	start = tmp;
    }
}

#undef Allocator
#undef vector

#endif

 



