<!---

SPDX-License-Identifier: BSD-2-Clause

Copyright (c) 2018-2020 Gavin D. Howard and contributors.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

-->

# NAME

bcl - library of arbitrary precision decimal arithmetic

# SYNOPSIS

## Use

*#include <bcl.h>*

Link with *-lbcl*.

## Signals

This procedure will allow clients to use signals to interrupt computations
running in bcl(3).

**void bcl_handleSignal(***void***);**

**bool bcl_running(***void***);**

## Setup

These items allow clients to set up bcl(3).

**BclError bcl_init(***void***);**

**void bcl_free(***void***);**

**bool bcl_abortOnFatalError(***void***);**

**void bcl_setAbortOnFatalError(bool** *abrt***);**

**void bcl_gc(***void***);**

## Contexts

These items will allow clients to handle contexts, which are isolated from each
other. This allows more than one client to use bcl(3) in the same program.

**struct BclCtxt;**

**typedef struct BclCtxt\* BclContext;**

**BclContext bcl_ctxt_create(***void***);**

**void bcl_ctxt_free(BclContext** *ctxt***);**

**BclError bcl_pushContext(BclContext** *ctxt***);**

**void bcl_popContext(***void***);**

**BclContext bcl_context(***void***);**

**void bcl_ctxt_freeNums(BclContext** *ctxt***);**

**size_t bcl_ctxt_scale(BclContext** *ctxt***);**

**void bcl_ctxt_setScale(BclContext** *ctxt***, size_t** *scale***);**

**size_t bcl_ctxt_ibase(BclContext** *ctxt***);**

**void bcl_ctxt_setIbase(BclContext** *ctxt***, size_t** *ibase***);**

**size_t bcl_ctxt_obase(BclContext** *ctxt***);**

**void bcl_ctxt_setObase(BclContext** *ctxt***, size_t** *obase***);**

## Errors

These items allow clients to handle errors.

**typedef enum BclError BclError;**

**BclError bcl_err(BclNumber** *n***);**

## Numbers

These items allow clients to manipulate and query the arbitrary-precision
numbers managed by bcl(3).

**typedef struct { size_t i; } BclNumber;**

**BclNumber bcl_num_create(***void***);**

**void bcl_num_free(BclNumber** *n***);**

**bool bcl_num_neg(BclNumber** *n***);**

**void bcl_num_setNeg(BclNumber** *n***, bool** *neg***);**

**size_t bcl_num_scale(BclNumber** *n***);**

**BclError bcl_num_setScale(BclNumber** *n***, size_t** *scale***);**

**size_t bcl_num_len(BclNumber** *n***);**

## Conversion

These items allow clients to convert numbers into and from strings and integers.

**BclNumber bcl_parse(const char \*restrict** *val***);**

**char\* bcl_string(BclNumber** *n***);**

**BclError bcl_bigdig(BclNumber** *n***, BclBigDig \****result***);**

**BclNumber bcl_bigdig2num(BclBigDig** *val***);**

## Math

These items allow clients to run math on numbers.

**BclNumber bcl_add(BclNumber** *a***, BclNumber** *b***);**

**BclNumber bcl_sub(BclNumber** *a***, BclNumber** *b***);**

**BclNumber bcl_mul(BclNumber** *a***, BclNumber** *b***);**

**BclNumber bcl_div(BclNumber** *a***, BclNumber** *b***);**

**BclNumber bcl_mod(BclNumber** *a***, BclNumber** *b***);**

**BclNumber bcl_pow(BclNumber** *a***, BclNumber** *b***);**

**BclNumber bcl_lshift(BclNumber** *a***, BclNumber** *b***);**

**BclNumber bcl_rshift(BclNumber** *a***, BclNumber** *b***);**

**BclNumber bcl_sqrt(BclNumber** *a***);**

**BclError bcl_divmod(BclNumber** *a***, BclNumber** *b***, BclNumber \****c***, BclNumber \****d***);**

**BclNumber bcl_modexp(BclNumber** *a***, BclNumber** *b***, BclNumber** *c***);**

## Miscellaneous

These items are miscellaneous.

**void bcl_zero(BclNumber** *n***);**

**void bcl_one(BclNumber** *n***);**

**ssize_t bcl_cmp(BclNumber** *a***, BclNumber** *b***);**

**BclError bcl_copy(BclNumber** *d***, BclNumber** *s***);**

**BclNumber bcl_dup(BclNumber** *s***);**

## Pseudo-Random Number Generator

These items allow clients to manipulate the seeded pseudo-random number
generator in bcl(3).

**#define BCL_SEED_ULONGS**

**#define BCL_SEED_SIZE**

**typedef unsigned long BclBigDig;**

**typedef unsigned long BclRandInt;**

**BclNumber bcl_irand(BclNumber** *a***);**

**BclNumber bcl_frand(size_t** *places***);**

**BclNumber bcl_ifrand(BclNumber** *a***, size_t** *places***);**

**BclError bcl_rand_seedWithNum(BclNumber** *n***);**

**BclError bcl_rand_seed(unsigned char** *seed***[***BC_SEED_SIZE***]);**

**void bcl_rand_reseed(***void***);**

**BclNumber bcl_rand_seed2num(***void***);**

**BclRandInt bcl_rand_int(***void***);**

**BclRandInt bcl_rand_bounded(BclRandInt** *bound***);**

# DESCRIPTION

bcl(3) is a library that implements arbitrary-precision decimal math, as
[standardized by POSIX][1] in bc(1).

bcl(3) is async-signal-safe if **bcl_handleSignal(***void***)** is used
properly. (See the **SIGNAL HANDLING** section.)

All of the items in its interface are described below. See the documentation for
each function for what each function can return.

## Signals

**void bcl_handleSignal(***void***)**

:   An async-signal-safe function that can be called from a signal handler. If
    called from a signal handler on the same thread as any executing bcl(3)
    functions, it will interrupt the functions and force them to return early.
    It is undefined behavior if this function is called from a thread that is
    *not* executing any bcl(3) functions while any bcl(3) functions are
    executing.

    If execution *is* interrupted, **bcl_handleSignal(***void***)** does *not*
    return to its caller.

    See the **SIGNAL HANDLING** section.

**bool bcl_running(***void***)**

:   An async-signal-safe function that can be called from a signal handler. It
    will return **true** if any bcl(3) procedures are running, which means it is
    safe to call **bcl_handleSignal(***void***)**. Otherwise, it returns
    **false**.

    See the **SIGNAL HANDLING** section.

## Setup

**BclError bcl_init(***void***)**

:   Initializes this library. This function can be called multiple times, but
    each call must be matched by a call to **bcl_free(***void***)**. This is to
    make it possible for multiple libraries and applications to initialize
    bcl(3) without problem.

    If there was no error, **BCL_ERROR_NONE** is returned. Otherwise, this
    function can return:

    * **BCL_ERROR_FATAL_ALLOC_ERR**

    This function must be the first one clients call. Calling any other
    function without calling this one first is undefined behavior.

**void bcl_free(***void***)**

:   Decrements bcl(3)'s reference count and frees the data associated with it if
    the reference count is **0**.

    This function must be the last one clients call. Calling this function
    before calling any other function is undefined behavior.

**bool bcl_abortOnFatalError(***void***)**

:   Queries and returns the current state of calling **abort()** on fatal
    errors. If **true** is returned, bcl(3) will cause a **SIGABRT** if a fatal
    error occurs.

    If activated, clients do not need to check for fatal errors.

**void bcl_setAbortOnFatalError(bool** *abrt***)**

:   Sets the state of calling **abort()** on fatal errors. If *abrt* is
    **false**, bcl(3) will not cause a **SIGABRT** on fatal errors after the
    call. If *abrt* is **true**, bcl(3) will cause a **SIGABRT** on fatal errors
    after the call.

    If activated, clients do not need to check for fatal errors.

**void bcl_gc(***void***)**

:   Garbage collects cached instances of arbitrary-precision numbers. This only
    frees the memory of numbers that are *not* in use, so it is safe to call at
    any time.

## Contexts

All procedures that take a **BclContext** parameter a require a valid context as
an argument.

**struct BclCtxt**

:   A forward declaration for a hidden **struct** type. Clients cannot access
    the internals of the **struct** type directly. All interactions with the
    type are done through pointers. See **BclContext** below.

**BclContext**

:   A typedef to a pointer of **struct BclCtxt**. This is the only handle
    clients can get to **struct BclCtxt**.

    A **BclContext** contains the values **scale**, **ibase**, and **obase**, as
    well as a list of numbers.

    **scale** is a value used to control how many decimal places calculations
    should use. A value of **0** means that calculations are done on integers
    only, where applicable, and a value of 20, for example, means that all
    applicable calculations return results with 20 decimal places. The default
    is **0**.

    **ibase** is a value used to control the input base. The minimum **ibase**
    is **2**, and the maximum is **36**. If **ibase** is **2**, numbers are
    parsed as though they are in binary, and any digits larger than **1** are
    clamped. Likewise, a value of **10** means that numbers are parsed as though
    they are decimal, and any larger digits are clamped. The default is **10**.

    **obase** is a value used to control the output base. The minimum **obase**
    is **0** and the maximum is **BC_BASE_MAX** (see the **LIMITS** section).

    Numbers created in one context are not valid in another context. It is
    undefined behavior to use a number created in a different context. Contexts
    are meant to isolate the numbers used by different clients in the same
    application.

**BclContext bcl_ctxt_create(***void***)**

:   Creates a context and returns it. Returns **NULL** if there was an error.

**void bcl_ctxt_free(BclContext** *ctxt***)**

:   Frees *ctxt*, after which it is no longer valid. It is undefined behavior to
    attempt to use an invalid context.

**BclError bcl_pushContext(BclContext** *ctxt***)**

:   Pushes *ctxt* onto bcl(3)'s stack of contexts. *ctxt* must have been created
    with **bcl_ctxt_create(***void***)**.

    If there was no error, **BCL_ERROR_NONE** is returned. Otherwise, this
    function can return:

    * **BCL_ERROR_FATAL_ALLOC_ERR**

    There *must* be a valid context to do any arithmetic.

**void bcl_popContext(***void***)**

:   Pops the current context off of the stack, if one exists.

**BclContext bcl_context(***void***)**

:   Returns the current context, or **NULL** if no context exists.

**void bcl_ctxt_freeNums(BclContext** *ctxt***)**

:   Frees all numbers in use that are associated with *ctxt*. It is undefined
    behavior to attempt to use a number associated with *ctxt* after calling
    this procedure unless such numbers have been created with
    **bcl_num_create(***void***)** after calling this procedure.

**size_t bcl_ctxt_scale(BclContext** *ctxt***)**

:   Returns the **scale** for given context.

**void bcl_ctxt_setScale(BclContext** *ctxt***, size_t** *scale***)**

:   Sets the **scale** for the given context to the argument *scale*.

**size_t bcl_ctxt_ibase(BclContext** *ctxt***)**

:   Returns the **ibase** for the given context.

**void bcl_ctxt_setIbase(BclContext** *ctxt***, size_t** *ibase***)**

:   Sets the **ibase** for the given context to the argument *ibase*. If the
    argument *ibase* is invalid, it clamped, so an *ibase* of **0** or **1** is
    clamped to **2**, and any values above **36** are clamped to **36**.

**size_t bcl_ctxt_obase(BclContext** *ctxt***)**

:   Returns the **obase** for the given context.

**void bcl_ctxt_setObase(BclContext** *ctxt***, size_t** *obase***)**

:   Sets the **obase** for the given context to the argument *obase*.

## Errors

**BclError**

:   An **enum** of possible error codes. See the **ERRORS** section for a
    complete listing the codes.

**BclError bcl_err(BclNumber** *n***)**

:   Checks for errors in a **BclNumber**. All functions that can return a
    **BclNumber** can encode an error in the number, and this function will
    return the error, if any. If there was no error, it will return
    **BCL_ERROR_NONE**.

    There must be a valid current context.

## Numbers

All procedures in this section require a valid current context.

**BclNumber**

:   A handle to an arbitrary-precision number. The actual number type is not
    exposed; the **BclNumber** handle is the only way clients can refer to
    instances of arbitrary-precision numbers.

**BclNumber bcl_num_create(***void***)**

:   Creates and returns a **BclNumber**.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

	* **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**void bcl_num_free(BclNumber** *n***)**

:   Frees *n*. It is undefined behavior to use *n* after calling this function.

**bool bcl_num_neg(BclNumber** *n***)**

:   Returns **true** if *n* is negative, **false** otherwise.

**void bcl_num_setNeg(BclNumber** *n***, bool** *neg***)**

:   Sets *n*'s sign to *neg*, where **true** is negative, and **false** is
    positive.

**size_t bcl_num_scale(BclNumber** *n***)**

:   Returns the *scale* of *n*.

    The *scale* of a number is the number of decimal places it has after the
    radix (decimal point).

**BclError bcl_num_setScale(BclNumber** *n***, size_t** *scale***)**

:   Sets the *scale* of *n* to the argument *scale*. If the argument *scale* is
    greater than the *scale* of *n*, *n* is extended. If the argument *scale* is
    less than the *scale* of *n*, *n* is truncated.

    If there was no error, **BCL_ERROR_NONE** is returned. Otherwise, this
    function can return:

    * **BCL_ERROR_INVALID_NUM**
    * **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**size_t bcl_num_len(BclNumber** *n***)**

:   Returns the number of *significant decimal digits* in *n*.

## Conversion

All procedures in this section require a valid current context.

All procedures in this section consume the given **BclNumber** arguments that
are not given to pointer arguments. See the **Consumption and Propagation**
subsection below.

**BclNumber bcl_parse(const char \*restrict** *val***)**

:   Parses a number string according to the current context's **ibase** and
    returns the resulting number.

    *val* must be non-**NULL** and a valid string. See
    **BCL_ERROR_PARSE_INVALID_STR** in the **ERRORS** section for more
    information.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_PARSE_INVALID_STR**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**char\* bcl_string(BclNumber** *n***)**

:   Returns a string representation of *n* according the the current context's
    **ibase**. The string is dynamically allocated and must be freed by the
    caller.

    *n* is consumed; it cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

**BclError bcl_bigdig(BclNumber** *n***, BclBigDig \****result***)**

:   Converts *n* into a **BclBigDig** and returns the result in the space
    pointed to by *result*.

    *a* must be smaller than **BC_OVERFLOW_MAX**. See the **LIMITS** section.

    If there was no error, **BCL_ERROR_NONE** is returned. Otherwise, this
    function can return:

    * **BCL_ERROR_INVALID_NUM**
    * **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_MATH_OVERFLOW**

    *n* is consumed; it cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

**BclNumber bcl_bigdig2num(BclBigDig** *val***)**

:   Creates a **BclNumber** from *val*.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

	* **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

## Math

All procedures in this section require a valid current context.

All procedures in this section can return the following errors:

* **BCL_ERROR_INVALID_NUM**
* **BCL_ERROR_INVALID_CONTEXT**
* **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_add(BclNumber** *a***, BclNumber** *b***)**

:   Adds *a* and *b* and returns the result. The *scale* of the result is the
    max of the *scale*s of *a* and *b*.

    *a* and *b* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    *a* and *b* can be the same number.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_sub(BclNumber** *a***, BclNumber** *b***)**

:   Subtracts *b* from *a* and returns the result. The *scale* of the result is
    the max of the *scale*s of *a* and *b*.

    *a* and *b* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    *a* and *b* can be the same number.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_mul(BclNumber** *a***, BclNumber** *b***)**

:   Multiplies *a* and *b* and returns the result. If *ascale* is the *scale* of
    *a* and *bscale* is the *scale* of *b*, the *scale* of the result is equal
    to **min(ascale+bscale,max(scale,ascale,bscale))**, where **min()** and
    **max()** return the obvious values.

    *a* and *b* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    *a* and *b* can be the same number.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_div(BclNumber** *a***, BclNumber** *b***)**

:   Divides *a* by *b* and returns the result. The *scale* of the result is the
    *scale* of the current context.

    *b* cannot be **0**.

    *a* and *b* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    *a* and *b* can be the same number.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_DIVIDE_BY_ZERO**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_mod(BclNumber** *a***, BclNumber** *b***)**

:   Divides *a* by *b* to the *scale* of the current context, computes the
    modulus **a-(a/b)\*b**, and returns the modulus.

    *b* cannot be **0**.

    *a* and *b* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    *a* and *b* can be the same number.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_DIVIDE_BY_ZERO**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_pow(BclNumber** *a***, BclNumber** *b***)**

:   Calculates *a* to the power of *b* to the *scale* of the current context.
    *b* must be an integer, but can be negative. If it is negative, *a* must
    be non-zero.

    *b* must be an integer. If *b* is negative, *a* must not be **0**.

    *a* must be smaller than **BC_OVERFLOW_MAX**. See the **LIMITS** section.

    *a* and *b* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    *a* and *b* can be the same number.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_NON_INTEGER**
	* **BCL_ERROR_MATH_OVERFLOW**
	* **BCL_ERROR_MATH_DIVIDE_BY_ZERO**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_lshift(BclNumber** *a***, BclNumber** *b***)**

:   Shifts *a* left (moves the radix right) by *b* places and returns the
    result. This is done in decimal. *b* must be an integer.

    *b* must be an integer.

    *a* and *b* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    *a* and *b* can be the same number.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_NON_INTEGER**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_rshift(BclNumber** *a***, BclNumber** *b***)**

:   Shifts *a* right (moves the radix left) by *b* places and returns the
    result. This is done in decimal. *b* must be an integer.

    *b* must be an integer.

    *a* and *b* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    *a* and *b* can be the same number.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_NON_INTEGER**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_sqrt(BclNumber** *a***)**

:   Calculates the square root of *a* and returns the result. The *scale* of the
    result is equal to the **scale** of the current context.

    *a* cannot be negative.

    *a* is consumed; it cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_NEGATIVE**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclError bcl_divmod(BclNumber** *a***, BclNumber** *b***, BclNumber \****c***, BclNumber \****d***)**

:   Divides *a* by *b* and returns the quotient in a new number which is put
    into the space pointed to by *c*, and puts the modulus in a new number which
    is put into the space pointed to by *d*.

	*b* cannot be **0**.

    *a* and *b* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    *c* and *d* cannot point to the same place, nor can they point to the space
    occupied by *a* or *b*.

    If there was no error, **BCL_ERROR_NONE** is returned. Otherwise, this
    function can return:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_DIVIDE_BY_ZERO**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_modexp(BclNumber** *a***, BclNumber** *b***, BclNumber** *c***)**

:   Computes a modular exponentiation where *a* is the base, *b* is the
    exponent, and *c* is the modulus, and returns the result. The *scale* of the
    result is equal to the **scale** of the current context.

    *a*, *b*, and *c* must be integers. *c* must not be **0**. *b* must not be
    negative.

    *a*, *b*, and *c* are consumed; they cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_NEGATIVE**
	* **BCL_ERROR_MATH_NON_INTEGER**
	* **BCL_ERROR_MATH_DIVIDE_BY_ZERO**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

## Miscellaneous

**void bcl_zero(BclNumber** *n***)**

:   Sets *n* to **0**.

**void bcl_one(BclNumber** *n***)**

:   Sets *n* to **1**.

**ssize_t bcl_cmp(BclNumber** *a***, BclNumber** *b***)**

:   Compares *a* and *b* and returns **0** if *a* and *b* are equal, **<0** if
    *a* is less than *b*, and **>0** if *a* is greater than *b*.

**BclError bcl_copy(BclNumber** *d***, BclNumber** *s***)**

:   Copies *s* into *d*.

    If there was no error, **BCL_ERROR_NONE** is returned. Otherwise, this
    function can return:

    * **BCL_ERROR_INVALID_NUM**
    * **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_dup(BclNumber** *s***)**

:   Creates and returns a new **BclNumber** that is a copy of *s*.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

## Pseudo-Random Number Generator

The pseudo-random number generator in bcl(3) is a *seeded* PRNG. Given the same
seed twice, it will produce the same sequence of pseudo-random numbers twice.

By default, bcl(3) attempts to seed the PRNG with data from **/dev/urandom**. If
that fails, it seeds itself with by calling **libc**'s **srand(time(NULL))** and
then calling **rand()** for each byte, since **rand()** is only guaranteed to
return **15** bits.

This should provide fairly good seeding in the standard case while also
remaining fairly portable.

If necessary, the PRNG can be reseeded with one of the following functions:

* **bcl_rand_seedWithNum(BclNumber)**
* **bcl_rand_seed(unsigned char[BC_SEED_SIZE])**
* **bcl_rand_reseed(***void***)**

The following items allow clients to use the pseudo-random number generator. All
procedures require a valid current context.

**BCL_SEED_ULONGS**

:   The number of **unsigned long**'s in a seed for bcl(3)'s random number
    generator.

**BCL_SEED_SIZE**

:   The size, in **char**'s, of a seed for bcl(3)'s random number generator.

**BclBigDig**

:   bcl(3)'s overflow type (see the **PERFORMANCE** section).

**BclRandInt**

:   An unsigned integer type returned by bcl(3)'s random number generator.

**BclNumber bcl_irand(BclNumber** *a***)**

:   Returns a random number that is not larger than *a* in a new number. If *a*
    is **0** or **1**, the new number is equal to **0**. The bound is unlimited,
    so it is not bound to the size of **BclRandInt**. This is done by generating
    as many random numbers as necessary, multiplying them by certain exponents,
    and adding them all together.

    *a* must be an integer and non-negative.

    *a* is consumed; it cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    This procedure requires a valid current context.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_NEGATIVE**
	* **BCL_ERROR_MATH_NON_INTEGER**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_frand(size_t** *places***)**

:   Returns a random number between **0** (inclusive) and **1** (exclusive) that
    has *places* decimal digits after the radix (decimal point). There are no
    limits on *places*.

    This procedure requires a valid current context.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

	* **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclNumber bcl_ifrand(BclNumber** *a***, size_t** *places***)**

:   Returns a random number less than *a* with *places* decimal digits after the
    radix (decimal point). There are no limits on *a* or *places*.

    *a* must be an integer and non-negative.

    *a* is consumed; it cannot be used after the call. See the
    **Consumption and Propagation** subsection below.

    This procedure requires a valid current context.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**
	* **BCL_ERROR_MATH_NEGATIVE**
	* **BCL_ERROR_MATH_NON_INTEGER**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclError bcl_rand_seedWithNum(BclNumber** *n***)**

:   Seeds the PRNG with *n*.

    *n* is *not* consumed.

    This procedure requires a valid current context.

    If there was no error, **BCL_ERROR_NONE** is returned. Otherwise, this
    function can return:

    * **BCL_ERROR_INVALID_NUM**
	* **BCL_ERROR_INVALID_CONTEXT**

    Note that if **bcl_rand_seed2num(***void***)** or
    **bcl_rand_seed2num_err(BclNumber)** are called right after this function,
    they are not guaranteed to return a number equal to *n*.

**BclError bcl_rand_seed(unsigned char** *seed***[***BC_SEED_SIZE***])**

:   Seeds the PRNG with the bytes in *seed*.

    If there was no error, **BCL_ERROR_NONE** is returned. Otherwise, this
    function can return:

	* **BCL_ERROR_INVALID_CONTEXT**

**void bcl_rand_reseed(***void***)**

:   Reseeds the PRNG with the default reseeding behavior. First, it attempts to
    read data from **/dev/urandom** and falls back to **libc**'s **rand()**.

    This procedure cannot fail.

**BclNumber bcl_rand_seed2num(***void***)**

:   Returns the current seed of the PRNG as a **BclNumber**.

    This procedure requires a valid current context.

    bcl(3) will encode an error in the return value, if there was one. The error
    can be queried with **bcl_err(BclNumber)**. Possible errors include:

	* **BCL_ERROR_INVALID_CONTEXT**
    * **BCL_ERROR_FATAL_ALLOC_ERR**

**BclRandInt bcl_rand_int(***void***)**

:   Returns a random integer between **0** and **BC_RAND_MAX** (inclusive).

    This procedure cannot fail.

**BclRandInt bcl_rand_bounded(BclRandInt** *bound***)**

:   Returns a random integer between **0** and *bound* (exclusive). Bias is
    removed before returning the integer.

    This procedure cannot fail.

## Consumption and Propagation

Some functions are listed as consuming some or all of their arguments. This
means that the arguments are freed, regardless of if there were errors or not.

This is to enable compact code like the following:

    BclNumber n = bcl_num_add(bcl_num_mul(a, b), bcl_num_div(c, d));

If arguments to those functions were not consumed, memory would be leaked until
reclaimed with **bcl_ctxt_freeNums(BclContext)**.

When errors occur, they are propagated through. The result should always be
checked with **bcl_err(BclNumber)**, so the example above should properly
be:

    BclNumber n = bcl_num_add(bcl_num_mul(a, b), bcl_num_div(c, d));
    if (bc_num_err(n) != BCL_ERROR_NONE) {
        // Handle the error.
    }

# ERRORS

Most functions in bcl(3) return, directly or indirectly, any one of the error
codes defined in **BclError**. The complete list of codes is the following:

**BCL_ERROR_NONE**

:   Success; no error occurred.

**BCL_ERROR_INVALID_NUM**

:   An invalid **BclNumber** was given as a parameter.

**BCL_ERROR_INVALID_CONTEXT**

:   An invalid **BclContext** is being used.

**BCL_ERROR_SIGNAL**

:   A signal interrupted execution.

**BCL_ERROR_MATH_NEGATIVE**

:   A negative number was given as an argument to a parameter that cannot accept
    negative numbers, such as for square roots.

**BCL_ERROR_MATH_NON_INTEGER**

:   A non-integer was given as an argument to a parameter that cannot accept
    non-integer numbers, such as for the second parameter of **bcl_num_pow()**.

**BCL_ERROR_MATH_OVERFLOW**

:   A number that would overflow its result was given as an argument, such as
    for converting a **BclNumber** to a **BclBigDig**.

**BCL_ERROR_MATH_DIVIDE_BY_ZERO**

:   A divide by zero occurred.

**BCL_ERROR_PARSE_INVALID_STR**

:   An invalid number string was passed to a parsing function.

    A valid number string can only be one radix (period). In addition, any
    lowercase ASCII letters, symbols, or non-ASCII characters are invalid. It is
    allowed for the first character to be a dash. In that case, the number is
    considered to be negative.

    There is one exception to the above: one lowercase **e** is allowed in the
    number, after the radix, if it exists. If the letter **e** exists, the
    number is considered to be in scientific notation, where the part before the
    **e** is the number, and the part after, which must be an integer, is the
    exponent. There can be a dash right after the **e** to indicate a negative
    exponent.

    **WARNING**: Both the number and the exponent in scientific notation are
    interpreted according to the current **ibase**, but the number is still
    multiplied by **10\^exponent** regardless of the current **ibase**. For
    example, if **ibase** is **16** and bcl(3) is given the number string
    **FFeA**, the resulting decimal number will be **2550000000000**, and if
    bcl(3) is given the number string **10e-4**, the resulting decimal number
    will be **0.0016**.

**BCL_ERROR_FATAL_ALLOC_ERR**

:   bcl(3) failed to allocate memory.

    If clients call **bcl_setAbortOnFatalError()** with an **true** argument,
    this error will cause bcl(3) to throw a **SIGABRT**. This behavior can also
    be turned off later by calling that same function with a **false** argument.
    By default, this behavior is off.

    It is highly recommended that client libraries do *not* activate this
    behavior.

**BCL_ERROR_FATAL_UNKNOWN_ERR**

:   An unknown error occurred.

    If clients call **bcl_setAbortOnFatalError()** with an **true** argument,
    this error will cause bcl(3) to throw a **SIGABRT**. This behavior can also
    be turned off later by calling that same function with a **false** argument.
    By default, this behavior is off.

    It is highly recommended that client libraries do *not* activate this
    behavior.

# ATTRIBUTES

When **bcl_handleSignal(***void***)** is used properly, bcl(3) is
async-signal-safe.

bcl(3) is *MT-Unsafe*: it is unsafe to call any functions from more than one
thread.

# PERFORMANCE

Most bc(1) implementations use **char** types to calculate the value of **1**
decimal digit at a time, but that can be slow. bcl(3) does something
different.

It uses large integers to calculate more than **1** decimal digit at a time. If
built in a environment where **BC_LONG_BIT** (see the **LIMITS** section) is
**64**, then each integer has **9** decimal digits. If built in an environment
where **BC_LONG_BIT** is **32** then each integer has **4** decimal digits. This
value (the number of decimal digits per large integer) is called
**BC_BASE_DIGS**.

In addition, this bcl(3) uses an even larger integer for overflow checking. This
integer type depends on the value of **BC_LONG_BIT**, but is always at least
twice as large as the integer type used to store digits.

# LIMITS

The following are the limits on bcl(3):

**BC_LONG_BIT**

:   The number of bits in the **long** type in the environment where bcl(3) was
    built. This determines how many decimal digits can be stored in a single
    large integer (see the **PERFORMANCE** section).

**BC_BASE_DIGS**

:   The number of decimal digits per large integer (see the **PERFORMANCE**
    section). Depends on **BC_LONG_BIT**.

**BC_BASE_POW**

:   The max decimal number that each large integer can store (see
    **BC_BASE_DIGS**) plus **1**. Depends on **BC_BASE_DIGS**.

**BC_OVERFLOW_MAX**

:   The max number that the overflow type (see the **PERFORMANCE** section) can
    hold. Depends on **BC_LONG_BIT**.

**BC_BASE_MAX**

:   The maximum output base. Set at **BC_BASE_POW**.

**BC_SCALE_MAX**

:   The maximum **scale**. Set at **BC_OVERFLOW_MAX-1**.

**BC_NUM_MAX**

:   The maximum length of a number (in decimal digits), which includes digits
    after the decimal point. Set at **BC_OVERFLOW_MAX-1**.

**BC_RAND_MAX**

:   The maximum integer (inclusive) returned by the **bcl_rand_int()** function.
    Set at **2\^BC_LONG_BIT-1**.

Exponent

:   The maximum allowable exponent (positive or negative). Set at
    **BC_OVERFLOW_MAX**.

These limits are meant to be effectively non-existent; the limits are so large
(at least on 64-bit machines) that there should not be any point at which they
become a problem. In fact, memory should be exhausted before these limits should
be hit.

# SIGNAL HANDLING

If a signal handler calls **bcl_handleSignal(***void***)** from the same thread
that there are bcl(3) functions executing in, it will cause all execution to
stop as soon as possible, interrupting long-running calculations, if necessary
and cause the function that was executing to return. If possible, the error code
**BC_ERROR_SIGNAL** is returned.

If execution *is* interrupted, **bcl_handleSignal(***void***)** does *not*
return to its caller.

It is undefined behavior if **bcl_handleSignal(***void***)** is called from
a thread that is not executing bcl(3) functions, if bcl(3) functions are
executing.

# SEE ALSO

bc(1) and dc(1)

# STANDARDS

bcl(3) is compliant with the arithmetic defined in the
[IEEE Std 1003.1-2017 (“POSIX.1-2017”)][1] specification for bc(1).

Note that the specification explicitly says that bc(1) only accepts numbers that
use a period (**.**) as a radix point, regardless of the value of
**LC_NUMERIC**. This is also true of bcl(3).

# BUGS

None are known. Report bugs at https://git.yzena.com/gavin/bc.

# AUTHORS

Gavin D. Howard <gavin@yzena.com> and contributors.

[1]: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/bc.html
