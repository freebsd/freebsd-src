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

#ifndef ALGOBASE_H
#define ALGOBASE_H

#include <pair.h>
#include <iterator.h>

template <class ForwardIterator1, class ForwardIterator2, class T>
inline void __iter_swap(ForwardIterator1 a, ForwardIterator2 b, T*) {
    T tmp = *a;
    *a = *b;
    *b = tmp;
}

template <class ForwardIterator1, class ForwardIterator2>
inline void iter_swap(ForwardIterator1 a, ForwardIterator2 b) {
    __iter_swap(a, b, value_type(a));
}

template <class T>
inline void swap(T& a, T& b) {
    T tmp = a;
    a = b;
    b = tmp;
}

template <class T>
inline const T& min(const T& a, const T& b) {
    return b < a ? b : a;
}

template <class T, class Compare>
inline const T& min(const T& a, const T& b, Compare comp) {
    return comp(b, a) ? b : a;
}

template <class T>
inline const T& max(const T& a, const T& b) {
    return  a < b ? b : a;
}

template <class T, class Compare>
inline const T& max(const T& a, const T& b, Compare comp) {
    return comp(a, b) ? b : a;
}

template <class InputIterator, class Distance>
void __distance(InputIterator first, InputIterator last, Distance& n, 
		input_iterator_tag) {
    while (first != last) { ++first; ++n; }
}

template <class ForwardIterator, class Distance>
void __distance(ForwardIterator first, ForwardIterator last, Distance& n, 
		forward_iterator_tag) {
    while (first != last) { ++first; ++n; }
}

template <class BidirectionalIterator, class Distance>
void __distance(BidirectionalIterator first, BidirectionalIterator last, 
		Distance& n, bidirectional_iterator_tag) {
    while (first != last) { ++first; ++n; }
}

template <class RandomAccessIterator, class Distance>
inline void __distance(RandomAccessIterator first, RandomAccessIterator last, 
		       Distance& n, random_access_iterator_tag) {
    n += last - first;
}

template <class InputIterator, class Distance>
inline void distance(InputIterator first, InputIterator last, Distance& n) {
    __distance(first, last, n, iterator_category(first));
}

template <class InputIterator, class Distance>
void __advance(InputIterator& i, Distance n, input_iterator_tag) {
    while (n--) ++i;
}

template <class ForwardIterator, class Distance>
void __advance(ForwardIterator& i, Distance n, forward_iterator_tag) {
    while (n--) ++i;
}

template <class BidirectionalIterator, class Distance>
void __advance(BidirectionalIterator& i, Distance n, 
	       bidirectional_iterator_tag) {
    if (n >= 0)
	while (n--) ++i;
    else
	while (n++) --i;
}

template <class RandomAccessIterator, class Distance>
inline void __advance(RandomAccessIterator& i, Distance n, 
		      random_access_iterator_tag) {
    i += n;
}

template <class InputIterator, class Distance>
inline void advance(InputIterator& i, Distance n) {
    __advance(i, n, iterator_category(i));
}

template <class ForwardIterator>
void destroy(ForwardIterator first, ForwardIterator last) {
    while (first != last) {
	/* Borland bug */
	destroy(&*first);
	++first;
	//destroy(&*first++);
    }
}

template <class InputIterator, class ForwardIterator>
ForwardIterator uninitialized_copy(InputIterator first, InputIterator last,
				   ForwardIterator result) {
    while (first != last) construct(&*result++, *first++);
    return result;
}

template <class ForwardIterator, class T>
void uninitialized_fill(ForwardIterator first, ForwardIterator last, 
			const T& x) {
    while (first != last) construct(&*first++, x);
}

template <class ForwardIterator, class Size, class T>
ForwardIterator uninitialized_fill_n(ForwardIterator first, Size n,
				     const T& x) {
    while (n--) construct(&*first++, x);
    return first;
}

template <class InputIterator, class OutputIterator>
OutputIterator copy(InputIterator first, InputIterator last,
		    OutputIterator result) {
    while (first != last) *result++ = *first++;
    return result;
}

template <class BidirectionalIterator1, class BidirectionalIterator2>
BidirectionalIterator2 copy_backward(BidirectionalIterator1 first, 
				     BidirectionalIterator1 last, 
				     BidirectionalIterator2 result) {
    while (first != last) *--result = *--last;
    return result;
}

template <class ForwardIterator, class T>
void fill(ForwardIterator first, ForwardIterator last, const T& value) {
    while (first != last) *first++ = value;
}

template <class OutputIterator, class Size, class T>
OutputIterator fill_n(OutputIterator first, Size n, const T& value) {
    while (n-- > 0) *first++ = value;
    return first;
}

template <class InputIterator1, class InputIterator2>
pair<InputIterator1, InputIterator2> mismatch(InputIterator1 first1,
					      InputIterator1 last1,
					      InputIterator2 first2) {
    while (first1 != last1 && *first1 == *first2) {
	++first1;
	++first2;
    }
    return pair<InputIterator1, InputIterator2>(first1, first2);
}

template <class InputIterator1, class InputIterator2, class BinaryPredicate>
pair<InputIterator1, InputIterator2> mismatch(InputIterator1 first1,
					      InputIterator1 last1,
					      InputIterator2 first2,
					      BinaryPredicate binary_pred) {
    while (first1 != last1 && binary_pred(*first1, *first2)) {
	++first1;
	++first2;
    }
    return pair<InputIterator1, InputIterator2>(first1, first2);
}

template <class InputIterator1, class InputIterator2>
inline bool equal(InputIterator1 first1, InputIterator1 last1,
		  InputIterator2 first2) {
    return mismatch(first1, last1, first2).first == last1;
}

template <class InputIterator1, class InputIterator2, class BinaryPredicate>
inline bool equal(InputIterator1 first1, InputIterator1 last1,
		  InputIterator2 first2, BinaryPredicate binary_pred) {
    return mismatch(first1, last1, first2, binary_pred).first == last1;
}

template <class InputIterator1, class InputIterator2>
bool lexicographical_compare(InputIterator1 first1, InputIterator1 last1,
			     InputIterator2 first2, InputIterator2 last2) {
    while (first1 != last1 && first2 != last2) {
	if (*first1 < *first2) return true;
	if (*first2++ < *first1++) return false;
    }
    return first1 == last1 && first2 != last2;
}

template <class InputIterator1, class InputIterator2, class Compare>
bool lexicographical_compare(InputIterator1 first1, InputIterator1 last1,
			     InputIterator2 first2, InputIterator2 last2,
			     Compare comp) {
    while (first1 != last1 && first2 != last2) {
	if (comp(*first1, *first2)) return true;
	if (comp(*first2++, *first1++)) return false;
    }
    return first1 == last1 && first2 != last2;
}

#endif
