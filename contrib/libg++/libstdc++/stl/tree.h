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

#ifndef TREE_H
#define TREE_H

/*

Red-black tree class, designed for use in implementing STL
associative containers (set, multiset, map, and multimap). The
insertion and deletion algorithms are based on those in Cormen,
Leiserson, and Rivest, Introduction to Algorithms (MIT Press, 1990),
except that

(1) the header cell is maintained with links not only to the root
but also to the leftmost node of the tree, to enable constant time
begin(), and to the rightmost node of the tree, to enable linear time
performance when used with the generic set algorithms (set_union,
etc.);

(2) when a node being deleted has two children its successor node is
relinked into its place, rather than copied, so that the only
iterators invalidated are those referring to the deleted node.

*/

#include <algobase.h>
#include <iterator.h>
#include <function.h>
#ifndef __GNUG__
#include <bool.h>
#endif
#include <projectn.h>

#ifndef rb_tree 
#define rb_tree rb_tree
#endif

enum __rb_color_type {red, black};

struct __rb_tree_node_base {
  enum __rb_color_type color_field;
  void* parent_link;
  void* left_link;
  void* right_link;
};

extern __rb_tree_node_base __rb_NIL;

template <class Key, class Value, class KeyOfValue, class Compare>
class rb_tree {
protected:
    typedef enum __rb_color_type color_type;
    typedef Allocator<void>::pointer void_pointer;
    struct rb_tree_node;
    friend rb_tree_node;
    struct rb_tree_node  : public __rb_tree_node_base {
        Value value_field;
    };
#ifndef __GNUG__
    static Allocator<rb_tree_node> rb_tree_node_allocator;
    static Allocator<Value> value_allocator;
#endif
public:
    typedef Key key_type;
    typedef Value value_type;
    typedef Allocator<Value>::pointer pointer;
    typedef Allocator<Value>::reference reference;
    typedef Allocator<Value>::const_reference const_reference;
    typedef Allocator<rb_tree_node> rb_tree_node_allocator_type;
    typedef Allocator<rb_tree_node>::pointer link_type;
    typedef Allocator<rb_tree_node>::size_type size_type;
    typedef Allocator<rb_tree_node>::difference_type difference_type;
protected:
#ifndef __GNUG__
    size_type buffer_size() {
        return rb_tree_node_allocator.init_page_size();
    }
#endif
    struct rb_tree_node_buffer;
    friend rb_tree_node_buffer;
    struct rb_tree_node_buffer {
        void_pointer next_buffer;
        link_type buffer;
    };
public:
    typedef Allocator<rb_tree_node_buffer> buffer_allocator_type;
    typedef Allocator<rb_tree_node_buffer>::pointer buffer_pointer;     
protected:
#ifdef __GNUG__
    static Allocator<rb_tree_node_buffer> buffer_allocator;
    static buffer_pointer buffer_list;
    static link_type free_list;
    static link_type next_avail;
    static link_type last;
    link_type get_node() { return (link_type) operator new (sizeof (rb_tree_node)); }
    void put_node(link_type p) { operator delete (p); }
#else
    void add_new_buffer() {
        buffer_pointer tmp = buffer_allocator.allocate((size_type)1);
        tmp->buffer = rb_tree_node_allocator.allocate(buffer_size());
        tmp->next_buffer = buffer_list;
        buffer_list = tmp;
        next_avail = buffer_list->buffer;
        last = next_avail + buffer_size();
    }
    static size_type number_of_trees;
    void deallocate_buffers();
    link_type get_node() {
        link_type tmp = free_list;
        return free_list ? 
            (free_list = (link_type)(free_list->right_link), tmp) 
                : (next_avail == last ? (add_new_buffer(), next_avail++) 
                   : next_avail++);
        // ugly code for inlining - avoids multiple returns
    }
    void put_node(link_type p) {
        p->right_link = free_list;
        free_list = p;
    }
#endif
protected:
    link_type header;  
    link_type& root() { return parent(header); }
    link_type& root() const { return parent(header); }
    link_type& leftmost() { return left(header); }
    link_type& leftmost() const { return left(header); }
    link_type& rightmost() { return right(header); }
    link_type& rightmost() const { return right(header); }
    size_type node_count; // keeps track of size of tree
    bool insert_always;  // controls whether an element already in the
                         // tree is inserted again
//public:
    Compare key_compare;
    static link_type& left(link_type x) { 
        return (link_type&)((*x).left_link);
    }
    static link_type& right(link_type x) {
        return (link_type&)((*x).right_link); 
    }
    static link_type& parent(link_type x) {
        return (link_type&)((*x).parent_link);
    }
    static reference value(link_type x) { return (*x).value_field; }
    static Allocator<Key>::const_reference key(link_type x) {
        return KeyOfValue()(value(x));
    }
    static color_type& color(link_type x) { 
        return (color_type&)(*x).color_field; }
    static link_type minimum(link_type x) {
        while (left(x) != &__rb_NIL)
            x = left(x);
        return x;
    }
    static link_type maximum(link_type x) {
        while (right(x) != &__rb_NIL)
            x = right(x);
        return x;
    }
public:
    class iterator;
    friend iterator;
    class const_iterator;
    friend const_iterator;
    class iterator : public bidirectional_iterator<Value, difference_type> {
    friend class rb_tree<Key, Value, KeyOfValue, Compare>;
    friend class const_iterator;
/*      
    friend bool operator==(const iterator& x, const iterator& y) {
        return x.node == y.node;
    }
*/
    protected:
        link_type node;
        iterator(link_type x) : node(x) {}
    public:
        iterator() {}
        bool operator==(const iterator& y) const { return node == y.node; }
        reference operator*() const { return value(node); }
        iterator& operator++() {
            if (right(node) != &__rb_NIL) {
                node = right(node);
                while (left(node) != &__rb_NIL)
                    node = left(node);
            } else {
                link_type y = parent(node);
                while (node == right(y)) {
                    node = y;
                    y = parent(y);
                }
                if (right(node) != y) // necessary because of rightmost 
                    node = y;
            }
            return *this;
        }
        iterator operator++(int) {
            iterator tmp = *this;
            ++*this;
            return tmp;
        }
        iterator& operator--() {
            if (color(node) == red && parent(parent(node)) == node)  
                // check for header
                node = right(node);   // return rightmost
            else if (left(node) != &__rb_NIL) {
                link_type y = left(node);
                while (right(y) != &__rb_NIL)
                    y = right(y);
                node = y;
            } else {
                link_type y = parent(node);
                while (node == left(y)) {
                    node = y;
                    y = parent(y);
                }
                node = y;
            }
            return *this;
        }
        iterator operator--(int) {
            iterator tmp = *this;
            --*this;
            return tmp;
        }
    };
    class const_iterator 
        : public bidirectional_iterator<Value,difference_type> {
    friend class rb_tree<Key, Value, KeyOfValue, Compare>;
    friend class iterator;
/*      
    friend bool operator==(const const_iterator& x, const const_iterator& y) {
        return x.node == y.node;
    }
*/
    protected:
        link_type node;
        const_iterator(link_type x) : node(x) {}
    public:
        const_iterator() {}
        const_iterator(const iterator& x) : node(x.node) {}
        bool operator==(const const_iterator& y) const { 
            return node == y.node; 
        }
        bool operator!=(const const_iterator& y) const { 
            return node != y.node; 
        }
        const_reference operator*() const { return value(node); }
        const_iterator& operator++() {
            if (right(node) != &__rb_NIL) {
                node = right(node);
                while (left(node) != &__rb_NIL)
                    node = left(node);
            } else {
                link_type y = parent(node);
                while (node == right(y)) {
                    node = y;
                    y = parent(y);
                }
                if (right(node) != y) // necessary because of rightmost 
                    node = y;
            }
            return *this;
        }
        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++*this;
            return tmp;
        }
        const_iterator& operator--() {
            if (color(node) == red && parent(parent(node)) == node)  
                // check for header
                node = right(node);   // return rightmost
            else if (left(node) != &__rb_NIL) {
                link_type y = left(node);
                while (right(y) != &__rb_NIL)
                    y = right(y);
                node = y;
            } else {
                link_type y = parent(node);
                while (node == left(y)) {
                    node = y;
                    y = parent(y);
                }
                node = y;
            }
            return *this;
        }
        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --*this;
            return tmp;
        }
    };
    typedef reverse_bidirectional_iterator<iterator, value_type, reference,
                                           difference_type>
        reverse_iterator; 
    typedef reverse_bidirectional_iterator<const_iterator, value_type,
                                           const_reference, difference_type>
	const_reverse_iterator;
private:
#ifdef __GNUC__
     rb_tree_iterator<Key, Value, KeyOfValue, Compare> __insert(void* x, void* y, const value_type& v);
    link_type __copy(link_type x, link_type p) {
        return (link_type) __copy_hack (x, p);
    }
private:
    void * __copy_hack (void *, void *);
public:
    void __erase(void* x);
#else
    iterator __insert(link_type x, link_type y, const value_type& v);
    link_type __copy(link_type x, link_type p);
    void __erase(link_type x);
#endif
    void init() {
#ifndef __GNUG__
        ++number_of_trees;
#endif
        header = get_node();
        color(header) = red;  // used to distinguish header from root,
                              // in iterator.operator++
        header->parent_link = &__rb_NIL;
        leftmost() = header;
        rightmost() = header;
    }
public:
    
// allocation/deallocation
    
    rb_tree(const Compare& comp = Compare(), bool always = true) 
           : node_count(0), insert_always(always), key_compare(comp) { 
        init();
    }
    rb_tree(const value_type* first, const value_type* last, 
            const Compare& comp = Compare(), bool always = true)
          : node_count(0), insert_always(always), key_compare(comp) { 
        init();
        insert(first, last);
    }
    rb_tree(const rb_tree<Key, Value, KeyOfValue, Compare>& x, 
            bool always = true) : node_count(x.node_count), 
                 insert_always(always), key_compare(x.key_compare) { 
#ifndef __GNUG__
        ++number_of_trees;
#endif
        header = get_node();
        color(header) = red;
        root() = __copy(x.root(), header);
        if (root() == &__rb_NIL) {
            leftmost() = header;
            rightmost() = header;
        } else {
	    leftmost() = minimum(root());
            rightmost() = maximum(root());
        }
    }
    ~rb_tree() {
        erase(begin(), end());
        put_node(header);
#ifndef __GNUG__
        if (--number_of_trees == 0) {
            deallocate_buffers();
            free_list = 0;    
            next_avail = 0;
            last = 0;
        }
#endif
    }
    rb_tree<Key, Value, KeyOfValue, Compare>& 
        operator=(const rb_tree<Key, Value, KeyOfValue, Compare>& x);
    
// accessors:

    Compare key_comp() const { return key_compare; }
    iterator begin() { return leftmost(); }
    const_iterator begin() const { return leftmost(); }
    iterator end() { return header; }
    const_iterator end() const { return header; }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { 
        return const_reverse_iterator(end()); 
    }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { 
        return const_reverse_iterator(begin());
    } 
    bool empty() const { return node_count == 0; }
    size_type size() const { return node_count; }
#ifndef __GNUG__
    size_type max_size() const { 
        return rb_tree_node_allocator.max_size(); 
    }
#else
    size_type max_size() const { 
        return rb_tree_node_allocator_type::max_size(); 
    }
#endif
    void swap(rb_tree<Key, Value, KeyOfValue, Compare>& t) {
        ::swap(header, t.header);
        ::swap(node_count, t.node_count);
        ::swap(insert_always, t.insert_always);
        ::swap(key_compare, t.key_compare);
    }
    
// insert/erase

    typedef  pair<iterator, bool> pair_iterator_bool; 
    // typedef done to get around compiler bug
#ifdef __GNUG__
    pair_iterator_bool insert(const value_type& x) {
	return insert_hack(x);
    }
private:
    rb_tree_pair_iterator_bool<Key, Value, KeyOfValue, Compare>
	insert_hack(const Value& v);
public:
    iterator insert(iterator position, const value_type& x) {
        return insert_hack(position, x);
    }
private:
    rb_tree_iterator<Key, Value, KeyOfValue, Compare>
    insert_hack(rb_tree_iterator<Key, Value, KeyOfValue, Compare> posn,
						      const Value& v);
public:
    void insert(iterator first, iterator last) {
        while (first != last) insert(*first++);
    }
    void insert(const value_type* first, const value_type* last){
	while (first != last) insert(*first++);
    }
    void erase(iterator position) {
	erase_hack(position);
    }
private:
    void erase_hack(rb_tree_iterator<Key, Value, KeyOfValue, Compare> position);
public:
    size_type erase(const key_type& x);
    void erase(iterator first, iterator last) {
	while (first != last) erase(first++);
    }
#else
    pair_iterator_bool insert(const value_type& x);
    iterator insert(iterator position, const value_type& x);
    void insert(iterator first, iterator last);
    void insert(const value_type* first, const value_type* last);
    void erase(iterator position);
    size_type erase(const key_type& x);
    void erase(iterator first, iterator last);
#endif
    void erase(const key_type* first, const key_type* last);

// set operations:

#ifdef __GNUG__
    iterator find(const key_type& x) {
	return find_hack(x);
    }
    const_iterator find(const key_type& x) const {
	return find_hack(x);
    }
private:
    rb_tree_iterator<Key, Value, KeyOfValue, Compare>
        find_hack(const key_type& x);
    rb_tree_const_iterator<Key, Value, KeyOfValue, Compare>
	find_hack(const Key& k) const;
public:
    
    size_type count(const key_type& x) const;
    iterator lower_bound(const key_type& x) {
	return lower_bound_hack(x);
    }
    const_iterator lower_bound(const key_type& x) const {
	return lower_bound_hack(x);
    }
    iterator upper_bound(const key_type& x) {
	return upper_bound_hack(x);
    }
    const_iterator upper_bound(const key_type& x) const {
	return upper_bound_hack(x);
    }
private:
    rb_tree_iterator<Key, Value, KeyOfValue, Compare>
        lower_bound_hack(const key_type& x);
    rb_tree_const_iterator<Key, Value, KeyOfValue, Compare>
	lower_bound_hack(const Key& k) const;
    rb_tree_iterator<Key, Value, KeyOfValue, Compare>
        upper_bound_hack(const key_type& x);
    rb_tree_const_iterator<Key, Value, KeyOfValue, Compare>
	upper_bound_hack(const Key& k) const;
public:
    typedef  pair<iterator, iterator> pair_iterator_iterator; 
    // typedef done to get around compiler bug
    pair_iterator_iterator equal_range(const key_type& x) {
	return pair_iterator_iterator(lower_bound(x), upper_bound(x));
    }
    typedef  pair<const_iterator, const_iterator> pair_citerator_citerator;
    
    // typedef done to get around compiler bug
    pair_citerator_citerator equal_range(const key_type& x) const {
	return pair_citerator_citerator(lower_bound(x), upper_bound(x));
    }
    inline void rotate_left(link_type x) {
	link_type y = right(x);
	right(x) = left(y);
	if (left(y) != &__rb_NIL)
	    parent(left(y)) = x;
	parent(y) = parent(x);
	if (x == root())
	    root() = y;
	else if (x == left(parent(x)))
	    left(parent(x)) = y;
	else
	    right(parent(x)) = y;
	left(y) = x;
	parent(x) = y;
    }

    inline void rotate_right(link_type x) {
	link_type y = left(x);
	left(x) = right(y);
	if (right(y) != &__rb_NIL)
	    parent(right(y)) = x;
	parent(y) = parent(x);
	if (x == root())
	    root() = y;
	else if (x == right(parent(x)))
	    right(parent(x)) = y;
	else
	    left(parent(x)) = y;
	right(y) = x;
	parent(x) = y;
    }
    friend bidirectional_iterator_tag iterator_category(iterator) {
	return bidirectional_iterator_tag();
    }
    friend bidirectional_iterator_tag iterator_category(const_iterator) {
	return bidirectional_iterator_tag();
    }
#else
    iterator find(const key_type& x);
    const_iterator find(const key_type& x) const;
    size_type count(const key_type& x) const;
    iterator lower_bound(const key_type& x);
    const_iterator lower_bound(const key_type& x) const;
    iterator upper_bound(const key_type& x);
    const_iterator upper_bound(const key_type& x) const;
    typedef  pair<iterator, iterator> pair_iterator_iterator; 
    // typedef done to get around compiler bug
    pair_iterator_iterator equal_range(const key_type& x);
    typedef  pair<const_iterator, const_iterator> pair_citerator_citerator; 
    // typedef done to get around compiler bug
    pair_citerator_citerator equal_range(const key_type& x) const;
    inline void rotate_left(link_type x);
    inline void rotate_right(link_type x);
#endif
};

#ifndef __GNUG__
template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::buffer_pointer 
        rb_tree<Key, Value, KeyOfValue, Compare>::buffer_list = 0;

template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::link_type 
        rb_tree<Key, Value, KeyOfValue, Compare>::free_list = 0;

template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::link_type 
        rb_tree<Key, Value, KeyOfValue, Compare>::next_avail = 0;

template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::link_type 
        rb_tree<Key, Value, KeyOfValue, Compare>::last = 0;

template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::size_type 
        rb_tree<Key, Value, KeyOfValue, Compare>::number_of_trees = 0;

template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::rb_tree_node_allocator_type 
        rb_tree<Key, Value, KeyOfValue, Compare>::rb_tree_node_allocator;

template <class Key, class Value, class KeyOfValue, class Compare>
Allocator<Value> rb_tree<Key, Value, KeyOfValue, Compare>::value_allocator;

template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::buffer_allocator_type 
        rb_tree<Key, Value, KeyOfValue, Compare>::buffer_allocator;

template <class Key, class Value, class KeyOfValue, class Compare>
void rb_tree<Key, Value, KeyOfValue, Compare>::deallocate_buffers() {
    while (buffer_list) {
        buffer_pointer tmp = buffer_list;
        buffer_list = (buffer_pointer)(buffer_list->next_buffer);
        rb_tree_node_allocator.deallocate(tmp->buffer);
        buffer_allocator.deallocate(tmp);
    }
}
#endif

#ifdef __GNUC__
template <class Key, class Value, class KeyOfValue, class Compare>
struct rb_tree_iterator {
  rb_tree<Key, Value, KeyOfValue, Compare>::iterator it;
  rb_tree_iterator(rb_tree<Key, Value, KeyOfValue, Compare>::iterator i) : it(i) {}
  operator rb_tree<Key, Value, KeyOfValue, Compare>::iterator() {
    return it;
  }
};

template <class Key, class Value, class KeyOfValue, class Compare>
inline Value* value_type(const rb_tree_iterator<Key, Value, KeyOfValue, Compare>&) {
    return (Value*)(0);
}

template <class Key, class Value, class KeyOfValue, class Compare>
struct rb_tree_const_iterator {
    rb_tree<Key, Value, KeyOfValue, Compare>::const_iterator it;
    rb_tree_const_iterator(rb_tree<Key, Value, KeyOfValue, Compare>::const_iterator i) : it(i) {}
    operator rb_tree<Key, Value, KeyOfValue, Compare>::const_iterator() {
	return it;
    }
};

template <class Key, class Value, class KeyOfValue, class Compare>
inline Value* value_type(const rb_tree_const_iterator<Key, Value, KeyOfValue, Compare>&) {
    return (Value*)(0);
}

template <class Key, class Value, class KeyOfValue, class Compare>
struct rb_tree_pair_iterator_bool {
    rb_tree<Key, Value, KeyOfValue, Compare>::pair_iterator_bool it;
    rb_tree_pair_iterator_bool(rb_tree<Key, Value, KeyOfValue, Compare>::pair_iterator_bool i) : it(i) {}
    operator rb_tree<Key, Value, KeyOfValue, Compare>::pair_iterator_bool() {
	return it;
    }
};

template <class Key, class Value, class KeyOfValue, class Compare>
inline Value* value_type(rb_tree_pair_iterator_bool<Key, Value, KeyOfValue, Compare>&) {
    return (Value*)(0);
}
#endif

template <class Key, class Value, class KeyOfValue, class Compare>
inline bool operator==(const rb_tree<Key, Value, KeyOfValue, Compare>& x, 
                       const rb_tree<Key, Value, KeyOfValue, Compare>& y) {
    return x.size() == y.size() && equal(x.begin(), x.end(), y.begin());
}

template <class Key, class Value, class KeyOfValue, class Compare>
inline bool operator<(const rb_tree<Key, Value, KeyOfValue, Compare>& x, 
                      const rb_tree<Key, Value, KeyOfValue, Compare>& y) {
    return lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());
}

template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>& 
rb_tree<Key, Value, KeyOfValue, Compare>::
operator=(const rb_tree<Key, Value, KeyOfValue, Compare>& x) {
    if (this != &x) {
        // can't be done as in list because Key may be a constant type
        erase(begin(), end());
        root() = __copy(x.root(), header);
        if (root() == &__rb_NIL) {
            leftmost() = header;
            rightmost() = header;
        } else {
	    leftmost() = minimum(root());
            rightmost() = maximum(root());
        }
        node_count = x.node_count;
    }
    return *this;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
rb_tree_iterator<Key, Value, KeyOfValue, Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::__insert
(void* xa, void* ya, const Value& v) {
    link_type x = (link_type)xa;
    link_type y = (link_type)ya;
#else
rb_tree<Key, Value, KeyOfValue, Compare>::iterator
rb_tree<Key, Value, KeyOfValue, Compare>::
__insert(link_type x, link_type y, const Value& v) {
#endif
    ++node_count;
    link_type z = get_node();
#ifdef __GNUG__
    construct(&(value(z)), v);
#else
    construct(value_allocator.address(value(z)), v);
#endif
    if (y == header || x != &__rb_NIL || key_compare(KeyOfValue()(v), key(y))) {
        left(y) = z;  // also makes leftmost() = z when y == header
        if (y == header) {
            root() = z;
            rightmost() = z;
        } else if (y == leftmost())
            leftmost() = z;   // maintain leftmost() pointing to minimum node
    } else {
        right(y) = z;
        if (y == rightmost())
            rightmost() = z;   // maintain rightmost() pointing to maximum node
    }
    parent(z) = y;
    z->left_link = &__rb_NIL;
    z->right_link = &__rb_NIL;
    x = z;  // recolor and rebalance the tree
    color(x) = red;
    while (x != root() && color(parent(x)) == red) 
        if (parent(x) == left(parent(parent(x)))) {
            y = right(parent(parent(x)));
            if (color(y) == red) {
                color(parent(x)) = black;
                color(y) = black;
                color(parent(parent(x))) = red;
                x = parent(parent(x));
            } else {
                if (x == right(parent(x))) {
                    x = parent(x);
                    rotate_left(x);
                }
                color(parent(x)) = black;
                color(parent(parent(x))) = red;
                rotate_right(parent(parent(x)));
            }
        } else {
            y = left(parent(parent(x)));
            if (color(y) == red) {
                color(parent(x)) = black;
                color(y) = black;
                color(parent(parent(x))) = red;
                x = parent(parent(x));
            } else {
                if (x == left(parent(x))) {
                    x = parent(x);
                    rotate_right(x);
                }
                color(parent(x)) = black;
                color(parent(parent(x))) = red;
                rotate_left(parent(parent(x)));
            }
        }
    color(root()) = black;
    return iterator(z);
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
rb_tree_pair_iterator_bool<Key, Value, KeyOfValue, Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::insert_hack(const Value& v) {
#else
rb_tree<Key, Value, KeyOfValue, Compare>::pair_iterator_bool
rb_tree<Key, Value, KeyOfValue, Compare>::insert(const Value& v) {
#endif
    link_type y = header;
    link_type x = root();
    bool comp = true;
    while (x != &__rb_NIL) {
        y = x;
        comp = key_compare(KeyOfValue()(v), key(x));
        x = comp ? left(x) : right(x);
    }
    if (insert_always)
        return pair_iterator_bool(__insert(x, y, v), true);
    iterator j = iterator(y);   
    if (comp)
        if (j == begin())     
            return pair_iterator_bool(__insert(x, y, v), true);
        else
            --j;
    if (key_compare(key(j.node), KeyOfValue()(v)))
        return pair_iterator_bool(__insert(x, y, v), true);
    return pair_iterator_bool(j, false);
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
rb_tree_iterator<Key, Value, KeyOfValue, Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::insert_hack(rb_tree_iterator<Key, Value, KeyOfValue, Compare> posn,
                                                 const Value& v) {
    iterator position = posn;
#else
rb_tree<Key, Value, KeyOfValue, Compare>::iterator 
rb_tree<Key, Value, KeyOfValue, Compare>::insert(iterator position,
                                                 const Value& v) {
#endif
    if (position == iterator(begin()))
        if (size() > 0 && key_compare(KeyOfValue()(v), key(position.node)))
            return __insert(position.node, position.node, v);
            // first argument just needs to be non-&__rb_NIL 
        else
            return insert(v).first;
    else if (position == iterator(end()))
        if (key_compare(key(rightmost()), KeyOfValue()(v)))
            return __insert(&__rb_NIL, rightmost(), v);
        else
            return insert(v).first;
    else {
        iterator before = --position;
        if (key_compare(key(before.node), KeyOfValue()(v))
            && key_compare(KeyOfValue()(v), key(position.node)))
            if (right(before.node) == &__rb_NIL)
                return __insert(&__rb_NIL, before.node, v); 
            else
                return __insert(position.node, position.node, v);
                // first argument just needs to be non-&__rb_NIL 
        else
            return insert(v).first;
    }
}

#ifndef __GNUC__
template <class Key, class Value, class KeyOfValue, class Compare>
void rb_tree<Key, Value, KeyOfValue, Compare>::insert(iterator first, 
                                                      iterator last) {
    while (first != last) insert(*first++);
}

template <class Key, class Value, class KeyOfValue, class Compare>
void rb_tree<Key, Value, KeyOfValue, Compare>::insert(const Value* first, 
                                                      const Value* last) {
    while (first != last) insert(*first++);
}
#endif
         
template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
void rb_tree<Key, Value, KeyOfValue, Compare>::erase_hack(
    rb_tree_iterator<Key, Value, KeyOfValue, Compare> posn) {
    iterator position = posn;
#else
void rb_tree<Key, Value, KeyOfValue, Compare>::erase(iterator position) {
#endif
    link_type z = position.node;
    link_type y = z;
    link_type x;
    if (left(y) == &__rb_NIL)
        x = right(y);
    else
        if (right(y) == &__rb_NIL) 
            x = left(y);
        else {
            y = right(y);
            while (left(y) != &__rb_NIL)
                y = left(y);
            x = right(y);
        }
    if (y != z) { // relink y in place of z
        parent(left(z)) = y; 
        left(y) = left(z);
        if (y != right(z)) {
            parent(x) = parent(y); // possibly x == &__rb_NIL
            left(parent(y)) = x;   // y must be a left child
            right(y) = right(z);
            parent(right(z)) = y;
        } else
            parent(x) = y;  // needed in case x == &__rb_NIL
        if (root() == z)
            root() = y;
        else if (left(parent(z)) == z)
            left(parent(z)) = y;
        else 
            right(parent(z)) = y;
        parent(y) = parent(z);
        ::swap(color(y), color(z));
        ::swap(y, z);  
                       // y points to node to be actually deleted,
                       // z points to old z's former successor
    } else {  // y == z
        parent(x) = parent(y);   // possibly x == &__rb_NIL
        if (root() == z)
            root() = x;
        else 
            if (left(parent(z)) == z)
                left(parent(z)) = x;
            else
                right(parent(z)) = x;
        if (leftmost() == z) 
            if (right(z) == &__rb_NIL)  // left(z) must be &__rb_NIL also
                leftmost() = parent(z);
                // makes leftmost() == header if z == root()
        else
            leftmost() = minimum(x);
        if (rightmost() == z)  
            if (left(z) == &__rb_NIL) // right(z) must be &__rb_NIL also
                rightmost() = parent(z);  
                // makes rightmost() == header if z == root()
        else  // x == left(z)
            rightmost() = maximum(x);
    }
    if (color(y) != red) { 
        while (x != root() && color(x) == black)
            if (x == left(parent(x))) {
                link_type w = right(parent(x));
                if (color(w) == red) {
                    color(w) = black;
                    color(parent(x)) = red;
                    rotate_left(parent(x));
                    w = right(parent(x));
                }
                if (color(left(w)) == black && color(right(w)) == black) {
                    color(w) = red;
                    x = parent(x);
                } else {
                    if (color(right(w)) == black) {
                        color(left(w)) = black;
                        color(w) = red;
                        rotate_right(w);
                        w = right(parent(x));
                    }
                    color(w) = color(parent(x));
                    color(parent(x)) = black;
                    color(right(w)) = black;
                    rotate_left(parent(x));
                    break;
                }
            } else {  // same as then clause with "right" and "left" exchanged
                link_type w = left(parent(x));
                if (color(w) == red) {
                    color(w) = black;
                    color(parent(x)) = red;
                    rotate_right(parent(x));
                    w = left(parent(x));
                }
                if (color(right(w)) == black && color(left(w)) == black) {
                    color(w) = red;
                    x = parent(x);
                } else {
                    if (color(left(w)) == black) {
                        color(right(w)) = black;
                        color(w) = red;
                        rotate_left(w);
                        w = left(parent(x));
                    }
                    color(w) = color(parent(x));
                    color(parent(x)) = black;
                    color(left(w)) = black;
                    rotate_right(parent(x));
                    break;
                }
            }
        color(x) = black;
    }
#ifdef __GNUG__
    delete y;
#else
    destroy(value_allocator.address(value(y)));
    put_node(y);
#endif
    --node_count;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
#ifndef __SIZE_TYPE__
#define __SIZE_TYPE__ long unsigned int
#endif
__SIZE_TYPE__
#else
rb_tree<Key, Value, KeyOfValue, Compare>::size_type 
#endif
rb_tree<Key, Value, KeyOfValue, Compare>::erase(const Key& x) {
    pair_iterator_iterator p = equal_range(x);
    size_type n = 0;
    distance(p.first, p.second, n);
    erase(p.first, p.second);
    return n;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUG__
void *
rb_tree<Key, Value, KeyOfValue, Compare>::__copy_hack(void* xa, void* pa) {
   link_type x = (link_type)xa;
   link_type p = (link_type)pa;
#else
rb_tree<Key, Value, KeyOfValue, Compare>::link_type 
rb_tree<Key, Value, KeyOfValue, Compare>::__copy(link_type x, link_type p) {
#endif
   // structural copy
   link_type r = x;
   while (x != &__rb_NIL) {
      link_type y = get_node();
      if (r == x) r = y;  // save for return value
#ifdef __GNUG__
      construct(&(value(y)), value(x));
#else
      construct(value_allocator.address(value(y)), value(x));
#endif
      left(p) = y;
      parent(y) = p;
      color(y) = color(x);
      right(y) = __copy(right(x), y);
      p = y;
      x = left(x);
   }
   left(p) = (link_type)&__rb_NIL;
   return r;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUG__
void rb_tree<Key, Value, KeyOfValue, Compare>::__erase(void* xa) {
    link_type x = (link_type)xa;
#else
void rb_tree<Key, Value, KeyOfValue, Compare>::__erase(link_type x) {
#endif
    // erase without rebalancing
    while (x != &__rb_NIL) {
       __erase(right(x));
       link_type y = left(x);
#ifdef __GNUG__
       delete x;
#else
       destroy(value_allocator.address(value(x)));
       put_node(x);
#endif
       x = y;
    }
}

#ifndef __GNUC__
template <class Key, class Value, class KeyOfValue, class Compare>
void rb_tree<Key, Value, KeyOfValue, Compare>::erase(iterator first, 
                                                     iterator last) {
    if (first == begin() && last == end() && node_count != 0) {
        __erase(root());
        leftmost() = header;
        root() = NIL;
        rightmost() = header;
        node_count = 0;
    } else
        while (first != last) erase(first++);
}
#endif

template <class Key, class Value, class KeyOfValue, class Compare>
void rb_tree<Key, Value, KeyOfValue, Compare>::erase(const Key* first, 
                                                     const Key* last) {
    while (first != last) erase(*first++);
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
rb_tree_iterator<Key, Value, KeyOfValue, Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::find_hack(const Key& k) {
#else
rb_tree<Key, Value, KeyOfValue, Compare>::iterator 
rb_tree<Key, Value, KeyOfValue, Compare>::find(const Key& k) {
#endif
    link_type y = header;
    link_type x = root();
    bool comp = false;
    while (x != &__rb_NIL) {
        y = x;
        comp = key_compare(key(x), k);
        x = comp ? right(x) : left(x);
    }
    iterator j = iterator(y);   
    if (comp) ++j;
    return (j == end() || key_compare(k, key(j.node))) ? end() : j;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
rb_tree_const_iterator<Key, Value, KeyOfValue, Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::find_hack(const Key& k) const {
#else
rb_tree<Key, Value, KeyOfValue, Compare>::const_iterator 
rb_tree<Key, Value, KeyOfValue, Compare>::find(const Key& k) const {
#endif
    link_type y = header;
    link_type x = root();
    bool comp = false;
    while (x != &__rb_NIL) {
        y = x;
        comp = key_compare(key(x), k);
        x = comp ? right(x) : left(x);
    }
    const_iterator j = const_iterator(y);   
    if (comp) ++j;
    return (j == end() || key_compare(k, key(j.node))) ? end() : j;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUG__
__SIZE_TYPE__
#else
rb_tree<Key, Value, KeyOfValue, Compare>::size_type 
#endif
rb_tree<Key, Value, KeyOfValue, Compare>::count(const Key& k) const {
    pair<const_iterator, const_iterator> p = equal_range(k);
    size_type n = 0;
    distance(p.first, p.second, n);
    return n;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
rb_tree_iterator<Key, Value, KeyOfValue, Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::lower_bound_hack(const Key& k) {
#else
rb_tree<Key, Value, KeyOfValue, Compare>::iterator 
rb_tree<Key, Value, KeyOfValue, Compare>::lower_bound(const Key& k) {
#endif
    link_type y = header;
    link_type x = root();
    bool comp = false;
    while (x != &__rb_NIL) {
        y = x;
        comp = key_compare(key(x), k);
        x = comp ? right(x) : left(x);
    }
    iterator j = iterator(y);   
    return comp ? ++j : j;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
rb_tree_const_iterator<Key, Value, KeyOfValue, Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::lower_bound_hack(const Key& k) const {
#else
rb_tree<Key, Value, KeyOfValue, Compare>::const_iterator 
rb_tree<Key, Value, KeyOfValue, Compare>::lower_bound(const Key& k) const {
#endif
    link_type y = header;
    link_type x = root();
    bool comp = false;
    while (x != &__rb_NIL) {
        y = x;
        comp = key_compare(key(x), k);
        x = comp ? right(x) : left(x);
    }
    const_iterator j = const_iterator(y);   
    return comp ? ++j : j;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
rb_tree_iterator<Key, Value, KeyOfValue, Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::upper_bound_hack(const Key& k) {
#else
rb_tree<Key, Value, KeyOfValue, Compare>::iterator 
rb_tree<Key, Value, KeyOfValue, Compare>::upper_bound(const Key& k) {
#endif
    link_type y = header;
    link_type x = root();
    bool comp = true;
    while (x != &__rb_NIL) {
        y = x;
        comp = key_compare(k, key(x));
        x = comp ? left(x) : right(x);
    }
    iterator j = iterator(y);   
    return comp ? j : ++j;
}

template <class Key, class Value, class KeyOfValue, class Compare>
#ifdef __GNUC__
rb_tree_const_iterator<Key, Value, KeyOfValue, Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::upper_bound_hack(const Key& k) const {
#else
rb_tree<Key, Value, KeyOfValue, Compare>::const_iterator 
rb_tree<Key, Value, KeyOfValue, Compare>::upper_bound(const Key& k) const {
#endif
    link_type y = header;
    link_type x = root();
    bool comp = true;
    while (x != &__rb_NIL) {
        y = x;
        comp = key_compare(k, key(x));
        x = comp ? left(x) : right(x);
    }
    const_iterator j = const_iterator(y);   
    return comp ? j : ++j;
}


#ifndef __GNUC__
template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::pair_iterator_iterator 
rb_tree<Key, Value, KeyOfValue, Compare>::equal_range(const Key& k) {
    return pair_iterator_iterator(lower_bound(k), upper_bound(k));
}

template <class Key, class Value, class KeyOfValue, class Compare>
rb_tree<Key, Value, KeyOfValue, Compare>::pair_citerator_citerator 
rb_tree<Key, Value, KeyOfValue, Compare>::equal_range(const Key& k) const {
    return pair_citerator_citerator(lower_bound(k), upper_bound(k));
}

template <class Key, class Value, class KeyOfValue, class Compare>
inline void 
rb_tree<Key, Value, KeyOfValue, Compare>::rotate_left(link_type x) {
    link_type y = right(x);
    right(x) = left(y);
    if (left(y) != &__rb_NIL)
        parent(left(y)) = x;
    parent(y) = parent(x);
    if (x == root())
        root() = y;
    else if (x == left(parent(x)))
        left(parent(x)) = y;
    else
        right(parent(x)) = y;
    left(y) = x;
    parent(x) = y;
}

template <class Key, class Value, class KeyOfValue, class Compare>
inline void 
rb_tree<Key, Value, KeyOfValue, Compare>::rotate_right(link_type x) {
    link_type y = left(x);
    left(x) = right(y);
    if (right(y) != &__rb_NIL)
        parent(right(y)) = x;
    parent(y) = parent(x);
    if (x == root())
        root() = y;
    else if (x == right(parent(x)))
        right(parent(x)) = y;
    else
        left(parent(x)) = y;
    right(y) = x;
    parent(x) = y;
}
#endif

#endif

