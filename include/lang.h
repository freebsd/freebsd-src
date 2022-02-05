/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Definitions for program data.
 *
 */

#ifndef BC_LANG_H
#define BC_LANG_H

#include <stdbool.h>
#if BC_C11
#include <assert.h>
#endif // BC_C11

#include <status.h>
#include <vector.h>
#include <num.h>

/// The instructions for bytecode.
typedef enum BcInst {

#if BC_ENABLED

	/// Postfix increment and decrement. Prefix are translated into
	/// BC_INST_ONE with either BC_INST_ASSIGN_PLUS or BC_INST_ASSIGN_MINUS.
	BC_INST_INC = 0,
	BC_INST_DEC,
#endif // BC_ENABLED

	/// Unary negation.
	BC_INST_NEG,

	/// Boolean not.
	BC_INST_BOOL_NOT,
#if BC_ENABLE_EXTRA_MATH
	/// Truncation operator.
	BC_INST_TRUNC,
#endif // BC_ENABLE_EXTRA_MATH

	/// These should be self-explanatory.
	BC_INST_POWER,
	BC_INST_MULTIPLY,
	BC_INST_DIVIDE,
	BC_INST_MODULUS,
	BC_INST_PLUS,
	BC_INST_MINUS,

#if BC_ENABLE_EXTRA_MATH

	/// Places operator.
	BC_INST_PLACES,

	/// Shift operators.
	BC_INST_LSHIFT,
	BC_INST_RSHIFT,
#endif // BC_ENABLE_EXTRA_MATH

	/// Comparison operators.
	BC_INST_REL_EQ,
	BC_INST_REL_LE,
	BC_INST_REL_GE,
	BC_INST_REL_NE,
	BC_INST_REL_LT,
	BC_INST_REL_GT,

	/// Boolean or and and.
	BC_INST_BOOL_OR,
	BC_INST_BOOL_AND,

#if BC_ENABLED
	/// Same as the normal operators, but assigment. So ^=, *=, /=, etc.
	BC_INST_ASSIGN_POWER,
	BC_INST_ASSIGN_MULTIPLY,
	BC_INST_ASSIGN_DIVIDE,
	BC_INST_ASSIGN_MODULUS,
	BC_INST_ASSIGN_PLUS,
	BC_INST_ASSIGN_MINUS,
#if BC_ENABLE_EXTRA_MATH
	/// Places and shift assignment operators.
	BC_INST_ASSIGN_PLACES,
	BC_INST_ASSIGN_LSHIFT,
	BC_INST_ASSIGN_RSHIFT,
#endif // BC_ENABLE_EXTRA_MATH

	/// Normal assignment.
	BC_INST_ASSIGN,

	/// bc and dc detect when the value from an assignment is not necessary.
	/// For example, a plain assignment statement means the value is never used.
	/// In those cases, we can get lots of performance back by not even creating
	/// a copy at all. In fact, it saves a copy, a push onto the results stack,
	/// a pop from the results stack, and a free. Definitely worth it to detect.
	BC_INST_ASSIGN_POWER_NO_VAL,
	BC_INST_ASSIGN_MULTIPLY_NO_VAL,
	BC_INST_ASSIGN_DIVIDE_NO_VAL,
	BC_INST_ASSIGN_MODULUS_NO_VAL,
	BC_INST_ASSIGN_PLUS_NO_VAL,
	BC_INST_ASSIGN_MINUS_NO_VAL,
#if BC_ENABLE_EXTRA_MATH
	/// Same as above.
	BC_INST_ASSIGN_PLACES_NO_VAL,
	BC_INST_ASSIGN_LSHIFT_NO_VAL,
	BC_INST_ASSIGN_RSHIFT_NO_VAL,
#endif // BC_ENABLE_EXTRA_MATH
#endif // BC_ENABLED

	/// Normal assignment that pushes no value on the stack.
	BC_INST_ASSIGN_NO_VAL,

	/// Push a constant onto the results stack.
	BC_INST_NUM,

	/// Push a variable onto the results stack.
	BC_INST_VAR,

	/// Push an array element onto the results stack.
	BC_INST_ARRAY_ELEM,

	/// Push an array onto the results stack. This is different from pushing an
	/// array *element* onto the results stack; it pushes a reference to the
	/// whole array. This is needed in bc for function arguments that are
	/// arrays. It is also needed for returning the length of an array.
	BC_INST_ARRAY,

	/// Push a zero or a one onto the stack. These are special cased because it
	/// does help performance, particularly for one since inc/dec operators
	/// use it.
	BC_INST_ZERO,
	BC_INST_ONE,

#if BC_ENABLED
	/// Push the last printed value onto the stack.
	BC_INST_LAST,
#endif // BC_ENABLED

	/// Push the value of any of the globals onto the stack.
	BC_INST_IBASE,
	BC_INST_OBASE,
	BC_INST_SCALE,

#if BC_ENABLE_EXTRA_MATH
	/// Push the value of the seed global onto the stack.
	BC_INST_SEED,
#endif // BC_ENABLE_EXTRA_MATH

	/// These are builtin functions.
	BC_INST_LENGTH,
	BC_INST_SCALE_FUNC,
	BC_INST_SQRT,
	BC_INST_ABS,

#if BC_ENABLE_EXTRA_MATH
	/// Another builtin function.
	BC_INST_IRAND,
#endif // BC_ENABLE_EXTRA_MATH

	/// Asciify.
	BC_INST_ASCIIFY,

	/// Another builtin function.
	BC_INST_READ,

#if BC_ENABLE_EXTRA_MATH
	/// Another builtin function.
	BC_INST_RAND,
#endif // BC_ENABLE_EXTRA_MATH

	/// Return the max for the various globals.
	BC_INST_MAXIBASE,
	BC_INST_MAXOBASE,
	BC_INST_MAXSCALE,
#if BC_ENABLE_EXTRA_MATH
	/// Return the max value returned by rand().
	BC_INST_MAXRAND,
#endif // BC_ENABLE_EXTRA_MATH

	/// bc line_length() builtin function.
	BC_INST_LINE_LENGTH,

#if BC_ENABLED

	/// bc global_stacks() builtin function.
	BC_INST_GLOBAL_STACKS,

#endif // BC_ENABLED

	/// bc leading_zero() builtin function.
	BC_INST_LEADING_ZERO,

	/// This is slightly misnamed versus BC_INST_PRINT_POP. Well, it is in bc.
	/// dc uses this instruction to print, but not pop. That's valid in dc.
	/// However, in bc, it is *never* valid to print without popping. In bc,
	/// BC_INST_PRINT_POP is used to indicate when a string should be printed
	/// because of a print statement or whether it should be printed raw. The
	/// reason for this is because a print statement handles escaped characters.
	/// So BC_INST_PRINT_POP is for printing a string from a print statement,
	/// BC_INST_PRINT_STR is for printing a string by itself.
	///
	/// In dc, BC_INST_PRINT_POP prints and pops, and BC_INST_PRINT just prints.
	///
	/// Oh, and BC_INST_STR pushes a string onto the results stack.
	BC_INST_PRINT,
	BC_INST_PRINT_POP,
	BC_INST_STR,
#if BC_ENABLED
	BC_INST_PRINT_STR,

	/// Jumps unconditionally.
	BC_INST_JUMP,

	/// Jumps if the top of the results stack is zero (condition failed). It
	/// turns out that we only want to jump when conditions fail to "skip" code.
	BC_INST_JUMP_ZERO,

	/// Call a function.
	BC_INST_CALL,

	/// Return the top of the stack to the caller.
	BC_INST_RET,

	/// Return 0 to the caller.
	BC_INST_RET0,

	/// Special return instruction for void functions.
	BC_INST_RET_VOID,

	/// Special halt instruction.
	BC_INST_HALT,
#endif // BC_ENABLED

	/// Pop an item off of the results stack.
	BC_INST_POP,

	/// Swaps the top two items on the results stack.
	BC_INST_SWAP,

	/// Modular exponentiation.
	BC_INST_MODEXP,

	/// Do divide and modulus at the same time.
	BC_INST_DIVMOD,

	/// Turns a number into a string and prints it.
	BC_INST_PRINT_STREAM,

#if DC_ENABLED

	/// dc's return; it pops an executing string off of the stack.
	BC_INST_POP_EXEC,

	/// Unconditionally execute a string.
	BC_INST_EXECUTE,

	/// Conditionally execute a string.
	BC_INST_EXEC_COND,

	/// Prints each item on the results stack, separated by newlines.
	BC_INST_PRINT_STACK,

	/// Pops everything off of the results stack.
	BC_INST_CLEAR_STACK,

	/// Pushes the current length of a register stack onto the results stack.
	BC_INST_REG_STACK_LEN,

	/// Pushes the current length of the results stack onto the results stack.
	BC_INST_STACK_LEN,

	/// Pushes a copy of the item on the top of the results stack onto the
	/// results stack.
	BC_INST_DUPLICATE,

	/// Copies the value in a register and pushes the copy onto the results
	/// stack.
	BC_INST_LOAD,

	/// Pops an item off of a register stack and pushes it onto the results
	/// stack.
	BC_INST_PUSH_VAR,

	/// Pops an item off of the results stack and pushes it onto a register's
	/// stack.
	BC_INST_PUSH_TO_VAR,

	/// Quit.
	BC_INST_QUIT,

	/// Quit executing some number of strings.
	BC_INST_NQUIT,

	/// Push the depth of the execution stack onto the stack.
	BC_INST_EXEC_STACK_LEN,

#endif // DC_ENABLED

	/// Invalid instruction.
	BC_INST_INVALID,

} BcInst;

#if BC_C11
static_assert(BC_INST_INVALID <= UCHAR_MAX,
              "Too many instructions to fit into an unsigned char");
#endif // BC_C11

/// Used by maps to identify where items are in the array.
typedef struct BcId {

	/// The name of the item.
	char *name;

	/// The index into the array where the item is.
	size_t idx;

} BcId;

/// The location of a var, array, or array element.
typedef struct BcLoc {

	/// The index of the var or array.
	size_t loc;

	/// The index of the array element. Only used for array elements.
	size_t idx;

} BcLoc;

/// An entry for a constant.
typedef struct BcConst {

	/// The original string as parsed from the source code.
	char *val;

	/// The last base that the constant was parsed in.
	BcBigDig base;

	/// The parsed constant.
	BcNum num;

} BcConst;

/// A function. This is also used in dc, not just bc. The reason is that strings
/// are executed in dc, and they are converted to functions in order to be
/// executed.
typedef struct BcFunc {

	/// The bytecode instructions.
	BcVec code;

#if BC_ENABLED

	/// The labels. This is a vector of indices. The index is the index into
	/// the bytecode vector where the label is.
	BcVec labels;

	/// The autos for the function. The first items are the parameters, and the
	/// arguments to the parameters must match the types in this vector.
	BcVec autos;

	/// The number of parameters the function takes.
	size_t nparams;

#endif // BC_ENABLED

	/// The strings encountered in the function.
	BcVec strs;

	/// The constants encountered in the function.
	BcVec consts;

	/// The function's name.
	const char *name;

#if BC_ENABLED
	/// True if the function is a void function.
	bool voidfn;
#endif // BC_ENABLED

} BcFunc;

/// Types of results that can be pushed onto the results stack.
typedef enum BcResultType {

	/// Result is a variable.
	BC_RESULT_VAR,

	/// Result is an array element.
	BC_RESULT_ARRAY_ELEM,

	/// Result is an array. This is only allowed for function arguments or
	/// returning the length of the array.
	BC_RESULT_ARRAY,

	/// Result is a string.
	BC_RESULT_STR,

	/// Result is a temporary. This is used for the result of almost all
	/// expressions.
	BC_RESULT_TEMP,

	/// Special casing the two below gave performance improvements.

	/// Result is a 0.
	BC_RESULT_ZERO,

	/// Result is a 1. Useful for inc/dec operators.
	BC_RESULT_ONE,

#if BC_ENABLED

	/// Result is the special "last" variable.
	BC_RESULT_LAST,

	/// Result is the return value of a void function.
	BC_RESULT_VOID,
#endif // BC_ENABLED

	/// Result is the value of ibase.
	BC_RESULT_IBASE,

	/// Result is the value of obase.
	BC_RESULT_OBASE,

	/// Result is the value of scale.
	BC_RESULT_SCALE,

#if BC_ENABLE_EXTRA_MATH

	/// Result is the value of seed.
	BC_RESULT_SEED,

#endif // BC_ENABLE_EXTRA_MATH

} BcResultType;

/// A union to store data for various result types.
typedef union BcResultData {

	/// A number. Strings are stored here too; they are numbers with
	/// cap == 0 && num == NULL. The string's index into the strings vector is
	/// stored in the scale field. But this is only used for strings stored in
	/// variables.
	BcNum n;

	/// A vector.
	BcVec v;

	/// A variable, array, or array element reference. This could also be a
	/// string if a string is not stored in a variable (dc only).
	BcLoc loc;

} BcResultData;

/// A tagged union for results.
typedef struct BcResult {

	/// The tag. The type of the result.
	BcResultType t;

	/// The data. The data for the result.
	BcResultData d;

} BcResult;

/// An instruction pointer. This is how bc knows where in the bytecode vector,
/// and which function, the current execution is.
typedef struct BcInstPtr {

	/// The index of the currently executing function in the fns vector.
	size_t func;

	/// The index into the bytecode vector of the *next* instruction.
	size_t idx;

	/// The length of the results vector when this function started executing.
	/// This is mostly used for bc where functions should not affect the results
	/// of their callers.
	size_t len;

} BcInstPtr;

/// Types of identifiers.
typedef enum BcType {

	/// Variable.
	BC_TYPE_VAR,

	/// Array.
	BC_TYPE_ARRAY,

#if BC_ENABLED

	/// Array reference.
	BC_TYPE_REF,

#endif // BC_ENABLED

} BcType;

#if BC_ENABLED
/// An auto variable in bc.
typedef struct BcAuto {

	/// The index of the variable in the vars or arrs vectors.
	size_t idx;

	/// The type of the variable.
	BcType type;

} BcAuto;
#endif // BC_ENABLED

/// Forward declaration.
struct BcProgram;

/**
 * Initializes a function.
 * @param f     The function to initialize.
 * @param name  The name of the function. The string is assumed to be owned by
 *              some other entity.
 */
void bc_func_init(BcFunc *f, const char* name);

/**
 * Inserts an auto into the function.
 * @param f     The function to insert into.
 * @param p     The program. This is to search for the variable or array name.
 * @param name  The name of the auto to insert.
 * @param type  The type of the auto.
 * @param line  The line in the source code where the insert happened. This is
 *              solely for error reporting.
 */
void bc_func_insert(BcFunc *f, struct BcProgram* p, char* name,
                    BcType type, size_t line);

/**
 * Resets a function in preparation for it to be reused. This can happen in bc
 * because it is a dynamic language and functions can be redefined.
 * @param f  The functio to reset.
 */
void bc_func_reset(BcFunc *f);

#ifndef NDEBUG
/**
 * Frees a function. This is a destructor. This is only used in debug builds
 * because all functions are freed at exit. We free them in debug builds to
 * check for memory leaks.
 * @param func  The function to free as a void pointer.
 */
void bc_func_free(void *func);
#endif // NDEBUG

/**
 * Initializes an array, which is the array type in bc and dc source code. Since
 * variables and arrays are both arrays (see the development manual,
 * manuals/development.md#execution, for more information), the @a nums
 * parameter tells bc whether to initialize an array of numbers or an array of
 * arrays of numbers. If the latter, it does a recursive call with nums set to
 * true.
 * @param a     The array to initialize.
 * @param nums  True if the array should be for numbers, false if it should be
 *              for vectors.
 */
void bc_array_init(BcVec *a, bool nums);

/**
 * Copies an array to another array. This is used to do pass arrays to functions
 * that do not take references to arrays. The arrays are passed entirely by
 * value, which means that they need to be copied.
 * @param d  The destination array.
 * @param s  The source array.
 */
void bc_array_copy(BcVec *d, const BcVec *s);

/**
 * Frees a string stored in a function. This is a destructor.
 * @param string  The string to free as a void pointer.
 */
void bc_string_free(void *string);

/**
 * Frees a constant stored in a function. This is a destructor.
 * @param constant  The constant to free as a void pointer.
 */
void bc_const_free(void *constant);

/**
 * Clears a result. It sets the type to BC_RESULT_TEMP and clears the union by
 * clearing the BcNum in the union. This is to ensure that bc does not use
 * uninitialized data.
 * @param r  The result to clear.
 */
void bc_result_clear(BcResult *r);

/**
 * Copies a result into another. This is done for things like duplicating the
 * top of the results stack or copying the result of an assignment to put back
 * on the results stack.
 * @param d    The destination result.
 * @param src  The source result.
 */
void bc_result_copy(BcResult *d, BcResult *src);

/**
 * Frees a result. This is a destructor.
 * @param result  The result to free as a void pointer.
 */
void bc_result_free(void *result);

/**
 * Expands an array to @a len. This can happen because in bc, you do not have to
 * explicitly initialize elements of an array. If you access an element that is
 * not initialized, the array is expanded to fit it, and all missing elements
 * are initialized to 0 if they are numbers, or arrays with one element of 0.
 * This function does that expansion.
 * @param a    The array to expand.
 * @param len  The length to expand to.
 */
void bc_array_expand(BcVec *a, size_t len);

/**
 * Compare two BcId's and return the result. Since they are just comparing the
 * names in the BcId, I return the result from strcmp() exactly. This is used by
 * maps in their binary search.
 * @param e1  The first id.
 * @param e2  The second id.
 * @return    The result of strcmp() on the BcId's names.
 */
int bc_id_cmp(const BcId *e1, const BcId *e2);

#if BC_ENABLED

/**
 * Returns non-zero if the bytecode instruction i is an assignment instruction.
 * @param i  The instruction to test.
 * @return   Non-zero if i is an assignment instruction, zero otherwise.
 */
#define BC_INST_IS_ASSIGN(i) \
	((i) == BC_INST_ASSIGN || (i) == BC_INST_ASSIGN_NO_VAL)

/**
 * Returns true if the bytecode instruction @a i requires the value to be
 * returned for use.
 * @param i  The instruction to test.
 * @return   True if @a i requires the value to be returned for use, false
 *           otherwise.
 */
#define BC_INST_USE_VAL(i) ((i) <= BC_INST_ASSIGN)

#else // BC_ENABLED

/**
 * Returns non-zero if the bytecode instruction i is an assignment instruction.
 * @param i  The instruction to test.
 * @return   Non-zero if i is an assignment instruction, zero otherwise.
 */
#define BC_INST_IS_ASSIGN(i) ((i) == BC_INST_ASSIGN_NO_VAL)

/**
 * Returns true if the bytecode instruction @a i requires the value to be
 * returned for use.
 * @param i  The instruction to test.
 * @return   True if @a i requires the value to be returned for use, false
 *           otherwise.
 */
#define BC_INST_USE_VAL(i) (false)

#endif // BC_ENABLED

#if BC_DEBUG_CODE
/// Reference to string names for all of the instructions. For debugging.
extern const char* bc_inst_names[];
#endif // BC_DEBUG_CODE

/// References to the names of the main and read functions.
extern const char bc_func_main[];
extern const char bc_func_read[];

#endif // BC_LANG_H
