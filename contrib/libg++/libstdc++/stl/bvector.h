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

// vector<bool> is replaced by bit_vector at present because bool is not
// implemented.  

#ifndef BVECTOR_H
#define BVECTOR_H

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

#define __WORD_BIT (int(CHAR_BIT*sizeof(unsigned int)))

class bit_vector {
public:
    typedef Allocator<unsigned int> vector_allocator;
    typedef bool value_type;
    typedef vector_allocator::size_type size_type;
    typedef vector_allocator::difference_type difference_type; 

    class iterator;
    class const_iterator;

    class reference {
    friend class iterator;
    friend class const_iterator;
    protected:
	unsigned int* p;
	unsigned int mask;
	reference(unsigned int* x, unsigned int y) : p(x), mask(y) {}
    public:
	reference() : p(0), mask(0) {}
	operator bool() const { return !(!(*p & mask)); }
	reference& operator=(bool x) {
	    if (x)      
		*p |= mask;
	    else 
		*p &= ~mask;
	    return *this;
	}
	reference& operator=(const reference& x) { return *this = bool(x); }
	bool operator==(const reference& x) const {
	    return bool(*this) == bool(x);
	}
	bool operator<(const reference& x) const {
	    return bool(*this) < bool(x);
	}
	void flip() { *p ^= mask; }
    };
    typedef bool const_reference;
    class iterator : public random_access_iterator<bool, difference_type> {
    friend class bit_vector;
    friend class const_iterator;
    protected:
	unsigned int* p;
	unsigned int offset;
	void bump_up() {
	    if (offset++ == __WORD_BIT - 1) {
		offset = 0;
		++p;
	    }
	}
    void bump_down() {
	if (offset-- == 0) {
	    offset = __WORD_BIT - 1;
	    --p;
	}
    }
    public:
	iterator() : p(0), offset(0) {}
	iterator(unsigned int* x, unsigned int y) : p(x), offset(y) {}
	reference operator*() const { return reference(p, 1U << offset); }
	iterator& operator++() {
	    bump_up();
	    return *this;
	}
	iterator operator++(int) {
	    iterator tmp = *this;
	    bump_up();
	    return tmp;
	}
	iterator& operator--() {
	    bump_down();
	    return *this;
	}
	iterator operator--(int) {
	    iterator tmp = *this;
	    bump_down();
	    return tmp;
	}
	iterator& operator+=(difference_type i) {
	    difference_type n = i + offset;
	    p += n / __WORD_BIT;
	    n = n % __WORD_BIT;
	    if (n < 0) {
		offset = n + __WORD_BIT;
		--p;
	    } else
		offset = n;
	    return *this;
	}
	iterator& operator-=(difference_type i) {
	    *this += -i;
	    return *this;
	}
	iterator operator+(difference_type i) const {
	    iterator tmp = *this;
	    return tmp += i;
	}
	iterator operator-(difference_type i) const {
	    iterator tmp = *this;
	    return tmp -= i;
	}
	difference_type operator-(iterator x) const {
	    return __WORD_BIT * (p - x.p) + offset - x.offset;
	}
	reference operator[](difference_type i) { return *(*this + i); }
	bool operator==(const iterator& x) const {
	    return p == x.p && offset == x.offset;
	}
	bool operator<(iterator x) const {
	    return p < x.p || (p == x.p && offset < x.offset);
	}
    };

    class const_iterator 
	: public random_access_iterator<bool, difference_type> {
    friend class bit_vector;
    protected:
	unsigned int* p;
	unsigned int offset;
	void bump_up() {
	    if (offset++ == __WORD_BIT - 1) {
		offset = 0;
		++p;
	    }
	}
    void bump_down() {
	if (offset-- == 0) {
	    offset = __WORD_BIT - 1;
	    --p;
	}
    }
    public:
	const_iterator() : p(0), offset(0) {}
	const_iterator(unsigned int* x, unsigned int y) : p(x), offset(y) {}
	const_iterator(const iterator& x) : p(x.p), offset(x.offset) {}
	const_reference operator*() const {
	    return reference(p, 1U << offset);
	}
	const_iterator& operator++() {
	    bump_up();
	    return *this;
	}
	const_iterator operator++(int) {
	    const_iterator tmp = *this;
	    bump_up();
	    return tmp;
	}
	const_iterator& operator--() {
	    bump_down();
	    return *this;
	}
	const_iterator operator--(int) {
	    const_iterator tmp = *this;
	    bump_down();
	    return tmp;
	}
	const_iterator& operator+=(difference_type i) {
	    difference_type n = i + offset;
	    p += n / __WORD_BIT;
	    n = n % __WORD_BIT;
	    if (n < 0) {
		offset = n + __WORD_BIT;
		--p;
	    } else
		offset = n;
	    return *this;
	}
	const_iterator& operator-=(difference_type i) {
	    *this += -i;
	    return *this;
	}
	const_iterator operator+(difference_type i) const {
	    const_iterator tmp = *this;
	    return tmp += i;
	}
	const_iterator operator-(difference_type i) const {
	    const_iterator tmp = *this;
	    return tmp -= i;
	}
	difference_type operator-(const_iterator x) const {
	    return __WORD_BIT * (p - x.p) + offset - x.offset;
	}
	const_reference operator[](difference_type i) { 
	    return *(*this + i); 
	}
	bool operator==(const const_iterator& x) const {
	    return p == x.p && offset == x.offset;
	}
	bool operator<(const_iterator x) const {
	    return p < x.p || (p == x.p && offset < x.offset);
	}
    };

    typedef reverse_iterator<const_iterator, value_type, const_reference, 
                             difference_type> const_reverse_iterator;
    typedef reverse_iterator<iterator, value_type, reference, difference_type>
        reverse_iterator;

protected:
    static Allocator<unsigned int> static_allocator;
    iterator start;
    iterator finish;
    unsigned int* end_of_storage;
    unsigned int* bit_alloc(size_type n) {
	return static_allocator.allocate((n + __WORD_BIT - 1)/__WORD_BIT);
    }
    void initialize(size_type n) {
	unsigned int* q = bit_alloc(n);
	end_of_storage = q + (n + __WORD_BIT - 1)/__WORD_BIT;
	start = iterator(q, 0);
	finish = start + n;
    }
    void insert_aux(iterator position, bool x);
    typedef bit_vector self;
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
    size_type capacity() const {
	return size_type(const_iterator(end_of_storage, 0) - begin());
    }
    bool empty() const { return begin() == end(); }
    reference operator[](size_type n) { return *(begin() + n); }
    const_reference operator[](size_type n) const { return *(begin() + n); }
    bit_vector() : start(iterator()), finish(iterator()), end_of_storage(0) {}
    bit_vector(size_type n, bool value = bool()) {
	initialize(n);
	fill(start.p, end_of_storage, value ? ~0 : 0);
    }
    bit_vector(const self& x) {
	initialize(x.size());
	copy(x.begin(), x.end(), start);
    }
    bit_vector(const_iterator first, const_iterator last) {
	size_type n = 0;
	distance(first, last, n);
	initialize(n);
	copy(first, last, start);
    }
    ~bit_vector() { static_allocator.deallocate(start.p); }
    self& operator=(const self& x) {
	if (&x == this) return *this;
	if (x.size() > capacity()) {
	    static_allocator.deallocate(start.p); 
	    initialize(x.size());
	}
	copy(x.begin(), x.end(), begin());
	finish = begin() + x.size();
	return *this;
    }
    void reserve(size_type n) {
	if (capacity() < n) {
	    unsigned int* q = bit_alloc(n);
	    finish = copy(begin(), end(), iterator(q, 0));
	    static_allocator.deallocate(start.p);
	    start = iterator(q, 0);
	    end_of_storage = q + (n + __WORD_BIT - 1)/__WORD_BIT;
	}
    }
    reference front() { return *begin(); }
    const_reference front() const { return *begin(); }
    reference back() { return *(end() - 1); }
    const_reference back() const { return *(end() - 1); }
    void push_back(bool x) {
	if (finish.p != end_of_storage)
	    *finish++ = x;
	else
	    insert_aux(end(), x);
    }
    void swap(bit_vector& x) {
	::swap(start, x.start);
	::swap(finish, x.finish);
	::swap(end_of_storage, x.end_of_storage);
    }
    iterator insert(iterator position, bool x) {
	size_type n = position - begin();
	if (finish.p != end_of_storage && position == end())
	    *finish++ = x;
	else
	    insert_aux(position, x);
	return begin() + n;
    }
    void insert(iterator position, const_iterator first, 
		const_iterator last);
    void insert(iterator position, size_type n, bool x);
    void pop_back() { --finish; }
    void erase(iterator position) {
	if (position + 1 != end())
	    copy(position + 1, end(), position);
	--finish;
    }
    void erase(iterator first, iterator last) {
	finish = copy(last, end(), first);
    }
};

Allocator<unsigned int> bit_vector::static_allocator;

inline bool operator==(const bit_vector& x, const bit_vector& y) {
    return x.size() == y.size() && equal(x.begin(), x.end(), y.begin());
}

inline bool operator<(const bit_vector& x, const bit_vector& y) {
    return lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());
}

void swap(bit_vector::reference x, bit_vector::reference y) {
    bool tmp = x;
    x = y;
    y = tmp;
}

void bit_vector::insert_aux(iterator position, bool x) {
    if (finish.p != end_of_storage) {
	copy_backward(position, finish - 1, finish);
	*position = x;
	++finish;
    } else {
	size_type len = size() ? 2 * size() : __WORD_BIT;
	unsigned int* q = bit_alloc(len);
	iterator i = copy(begin(), position, iterator(q, 0));
	*i++ = x;
	finish = copy(position, end(), i);
	static_allocator.deallocate(start.p);
	end_of_storage = q + (len + __WORD_BIT - 1)/__WORD_BIT;
	start = iterator(q, 0);
    }
}

void bit_vector::insert(iterator position, size_type n, bool x) {
    if (n == 0) return;
    if (capacity() - size() >= n) {
	copy_backward(position, end(), finish + n);
	fill(position, position + n, x);
	finish += n;
    } else {
	size_type len = size() + max(size(), n);
	unsigned int* q = bit_alloc(len);
	iterator i = copy(begin(), position, iterator(q, 0));
	fill_n(i, n, x);
	finish = copy(position, end(), i + n);
	static_allocator.deallocate(start.p);
	end_of_storage = q + (n + __WORD_BIT - 1)/__WORD_BIT;
	start = iterator(q, 0);
    }
}

void bit_vector::insert(iterator position, const_iterator first, 
			const_iterator last) {
    if (first == last) return;
    size_type n = 0;
    distance(first, last, n);
    if (capacity() - size() >= n) {
	copy_backward(position, end(), finish + n);
	copy(first, last, position);
	finish += n;
    } else {
	size_type len = size() + max(size(), n);
	unsigned int* q = bit_alloc(len);
	iterator i = copy(begin(), position, iterator(q, 0));
	i = copy(first, last, i);
	finish = copy(position, end(), i);
	static_allocator.deallocate(start.p);
	end_of_storage = q + (len + __WORD_BIT - 1)/__WORD_BIT;
	start = iterator(q, 0);
    }
}

#undef Allocator
#undef __WORD_BIT

#endif

 
