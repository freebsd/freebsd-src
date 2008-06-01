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

#ifndef FUNCTION_H
#define FUNCTION_H

#ifndef __GNUG__
#include <bool.h>
#endif

template <class T1, class T2>
inline bool operator!=(const T1& x, const T2& y) {
    return !(x == y);
}

template <class T1, class T2>
inline bool operator>(const T1& x, const T2& y) {
    return y < x;
}

template <class T1, class T2>
inline bool operator<=(const T1& x, const T2& y) {
    return !(y < x);
}

template <class T1, class T2>
inline bool operator>=(const T1& x, const T2& y) {
    return !(x < y);
}

template <class Arg, class Result>
struct unary_function {
    typedef Arg argument_type;
    typedef Result result_type;
};

template <class Arg1, class Arg2, class Result>
struct binary_function {
    typedef Arg1 first_argument_type;
    typedef Arg2 second_argument_type;
    typedef Result result_type;
};      

template <class T>
struct plus : binary_function<T, T, T> {
    T operator()(const T& x, const T& y) const { return x + y; }
};

template <class T>
struct minus : binary_function<T, T, T> {
    T operator()(const T& x, const T& y) const { return x - y; }
};

template <class T>
struct times : binary_function<T, T, T> {
    T operator()(const T& x, const T& y) const { return x * y; }
};

template <class T>
struct divides : binary_function<T, T, T> {
    T operator()(const T& x, const T& y) const { return x / y; }
};

template <class T>
#ifdef __GNU__
struct modulus {
    typedef T first_argument_type;
    typedef T second_argument_type;
    typedef T result_type;
    T operator()(const T& x, const T& y) const { return x % y; }
};
#else
struct modulus : binary_function<T, T, T> {
    T operator()(const T& x, const T& y) const { return x % y; }
};
#endif

template <class T>
struct negate : unary_function<T, T> {
    T operator()(const T& x) const { return -x; }
};

template <class T>
struct equal_to : binary_function<T, T, bool> {
    bool operator()(const T& x, const T& y) const { return x == y; }
};

template <class T>
struct not_equal_to : binary_function<T, T, bool> {
    bool operator()(const T& x, const T& y) const { return x != y; }
};

template <class T>
struct greater : binary_function<T, T, bool> {
    bool operator()(const T& x, const T& y) const { return x > y; }
};

template <class T>
struct less : binary_function<T, T, bool> {
    bool operator()(const T& x, const T& y) const { return x < y; }
};

template <class T>
struct greater_equal : binary_function<T, T, bool> {
    bool operator()(const T& x, const T& y) const { return x >= y; }
};

template <class T>
struct less_equal : binary_function<T, T, bool> {
    bool operator()(const T& x, const T& y) const { return x <= y; }
};

template <class T>
struct logical_and : binary_function<T, T, bool> {
    bool operator()(const T& x, const T& y) const { return x && y; }
};

template <class T>
struct logical_or : binary_function<T, T, bool> {
    bool operator()(const T& x, const T& y) const { return x || y; }
};

template <class T>
struct logical_not : unary_function<T, bool> {
    bool operator()(const T& x) const { return !x; }
};

template <class Predicate>
class unary_negate : public unary_function<Predicate::argument_type, bool> {
protected:
    Predicate pred;
public:
    unary_negate(const Predicate& x) : pred(x) {}
    bool operator()(const argument_type& x) const { return !pred(x); }
};

template <class Predicate>
unary_negate<Predicate> not1(const Predicate& pred) {
    return unary_negate<Predicate>(pred);
}

template <class Predicate> 
class binary_negate 
    : public binary_function<Predicate::first_argument_type,
			     Predicate::second_argument_type, bool> {
protected:
    Predicate pred;
public:
    binary_negate(const Predicate& x) : pred(x) {}
    bool operator()(const first_argument_type& x, 
		    const second_argument_type& y) const {
	return !pred(x, y); 
    }
};

template <class Predicate>
binary_negate<Predicate> not2(const Predicate& pred) {
    return binary_negate<Predicate>(pred);
}

template <class Operation> 
class binder1st : public unary_function<Operation::second_argument_type,
					Operation::result_type> {
protected:
    Operation op;
    Operation::first_argument_type value;
public:
    binder1st(const Operation& x, const Operation::first_argument_type& y)
	: op(x), value(y) {}
    result_type operator()(const argument_type& x) const {
	return op(value, x); 
    }
};

template <class Operation, class T>
binder1st<Operation> bind1st(const Operation& op, const T& x) {
    return binder1st<Operation>(op, Operation::first_argument_type(x));
}

template <class Operation> 
class binder2nd : public unary_function<Operation::first_argument_type,
					Operation::result_type> {
protected:
    Operation op;
    Operation::second_argument_type value;
public:
    binder2nd(const Operation& x, const Operation::second_argument_type& y) 
	: op(x), value(y) {}
    result_type operator()(const argument_type& x) const {
	return op(x, value); 
    }
};

template <class Operation, class T>
binder2nd<Operation> bind2nd(const Operation& op, const T& x) {
    return binder2nd<Operation>(op, Operation::second_argument_type(x));
}

template <class Operation1, class Operation2>
class unary_compose : public unary_function<Operation2::argument_type,
                                            Operation1::result_type> {
protected:
    Operation1 op1;
    Operation2 op2;
public:
    unary_compose(const Operation1& x, const Operation2& y) : op1(x), op2(y) {}
    result_type operator()(const argument_type& x) const {
	return op1(op2(x));
    }
};

template <class Operation1, class Operation2>
unary_compose<Operation1, Operation2> compose1(const Operation1& op1, 
					       const Operation2& op2) {
    return unary_compose<Operation1, Operation2>(op1, op2);
}

template <class Operation1, class Operation2, class Operation3>
class binary_compose : public unary_function<Operation2::argument_type,
                                             Operation1::result_type> {
protected:
    Operation1 op1;
    Operation2 op2;
    Operation3 op3;
public:
    binary_compose(const Operation1& x, const Operation2& y, 
		   const Operation3& z) : op1(x), op2(y), op3(z) { }
    result_type operator()(const argument_type& x) const {
	return op1(op2(x), op3(x));
    }
};

template <class Operation1, class Operation2, class Operation3>
binary_compose<Operation1, Operation2, Operation3> 
compose2(const Operation1& op1, const Operation2& op2, const Operation3& op3) {
    return binary_compose<Operation1, Operation2, Operation3>(op1, op2, op3);
}

template <class Arg, class Result>
class pointer_to_unary_function : public unary_function<Arg, Result> {
protected:
    Result (*ptr)(Arg);
public:
    pointer_to_unary_function() {}
    pointer_to_unary_function(Result (*x)(Arg)) : ptr(x) {}
    Result operator()(Arg x) const { return ptr(x); }
};

template <class Arg, class Result>
pointer_to_unary_function<Arg, Result> ptr_fun(Result (*x)(Arg)) {
    return pointer_to_unary_function<Arg, Result>(x);
}

template <class Arg1, class Arg2, class Result>
class pointer_to_binary_function : public binary_function<Arg1, Arg2, Result> {
protected:
    Result (*ptr)(Arg1, Arg2);
public:
    pointer_to_binary_function() {}
    pointer_to_binary_function(Result (*x)(Arg1, Arg2)) : ptr(x) {}
    Result operator()(Arg1 x, Arg2 y) const { return ptr(x, y); }
};

template <class Arg1, class Arg2, class Result>
pointer_to_binary_function<Arg1, Arg2, Result> 
ptr_fun(Result (*x)(Arg1, Arg2)) {
    return pointer_to_binary_function<Arg1, Arg2, Result>(x);
}

#endif
