/*-
 * Copyright (c) 2013 David Chisnall
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "input_buffer.hh"
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <functional>
#ifndef NDEBUG
#include <iostream>
#endif


#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>

#ifndef MAP_PREFAULT_READ
#define MAP_PREFAULT_READ 0
#endif

namespace dtc
{
void
input_buffer::skip_spaces()
{
	if (cursor >= size) { return; }
	if (cursor < 0) { return; }
	char c = buffer[cursor];
	while ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\f')
	       || (c == '\v') || (c == '\r'))
	{
		cursor++;
		if (cursor > size)
		{
			c = '\0';
		}
		else
		{
			c = buffer[cursor];
		}
	}
}

input_buffer
input_buffer::buffer_from_offset(int offset, int s)
{
	if (s == 0)
	{
		s = size - offset;
	}
	if (offset > size)
	{
		return input_buffer();
	}
	if (s > (size-offset))
	{
		return input_buffer();
	}
	return input_buffer(&buffer[offset], s);
}

bool
input_buffer::consume(const char *str)
{
	int len = strlen(str);
	if (len > size - cursor)
	{
		return false;
	}
	else
	{
		for (int i=0 ; i<len ; ++i)
		{
			if (str[i] != buffer[cursor + i])
			{
				return false;
			}
		}
		cursor += len;
		return true;
	}
	return false;
}

bool
input_buffer::consume_integer(unsigned long long &outInt)
{
	// The first character must be a digit.  Hex and octal strings
	// are prefixed by 0 and 0x, respectively.
	if (!isdigit((*this)[0]))
	{
		return false;
	}
	char *end=0;
	outInt = strtoull(&buffer[cursor], &end, 0);
	if (end == &buffer[cursor])
	{
		return false;
	}
	cursor = end - buffer;
	return true;
}

namespace {

/**
 * Convenience typedef for the type that we use for all values.
 */
typedef unsigned long long valty;

/**
 * Expression tree currently being parsed.
 */
struct expression
{
	/**
	 * Evaluate this node, taking into account operator precedence.
	 */
	virtual valty operator()() = 0;
	/**
	 * Returns the precedence of this node.  Lower values indicate higher
	 * precedence.
	 */
	virtual int precedence() = 0;
	virtual ~expression() {}
#ifndef NDEBUG
	/**
	 * Dumps this expression to `std::cerr`, appending a newline if `nl` is
	 * `true`.
	 */
	void dump(bool nl=false)
	{
		if (this == nullptr)
		{
			std::cerr << "{nullptr}\n";
			return;
		}
		dump_impl();
		if (nl)
		{
			std::cerr << '\n';
		}
	}
	private:
	/**
	 * Method that sublcasses override to implement the behaviour of `dump()`.
	 */
	virtual void dump_impl() = 0;
#endif
};

/**
 * Expression wrapping a single integer.  Leaf nodes in the expression tree.
 */
class terminal_expr : public expression
{
	/**
	 * The value that this wraps.
	 */
	valty val;
	/**
	 * Evaluate.  Trivially returns the value that this class wraps.
	 */
	valty operator()() override
	{
		return val;
	}
	int precedence() override
	{
		return 0;
	}
	public:
	/**
	 * Constructor.
	 */
	terminal_expr(valty v) : val(v) {}
#ifndef NDEBUG
	void dump_impl() override { std::cerr << val; }
#endif
};

/**
 * Parenthetical expression.  Exists to make the contents opaque.
 */
struct paren_expression : public expression
{
	/**
	 * The expression within the parentheses.
	 */
	expression_ptr subexpr;
	/**
	 * Constructor.  Takes the child expression as the only argument.
	 */
	paren_expression(expression_ptr p) : subexpr(std::move(p)) {}
	int precedence() override
	{
		return 0;
	}
	/**
	 * Evaluate - just forwards to the underlying expression.
	 */
	valty operator()() override
	{
		return (*subexpr)();
	}
#ifndef NDEBUG
	void dump_impl() override
	{
		std::cerr << " (";
		subexpr->dump();
		std::cerr << ") ";
	}
#endif
};

/**
 * Template class for unary operators.  The `OpChar` template parameter is
 * solely for debugging and makes it easy to print the expression.  The `Op`
 * template parameter is a function object that implements the operator that
 * this class provides.  Most of these are provided by the `<functional>`
 * header.
 */
template<char OpChar, class Op>
class unary_operator : public expression
{
	/**
	 * The subexpression for this unary operator.
	 */
	expression_ptr subexpr;
	valty operator()() override
	{
		Op op;
		return op((*subexpr)());
	}
	/**
	 * All unary operators have the same precedence.  They are all evaluated
	 * before binary expressions, but after parentheses.
	 */
	int precedence() override
	{
		return 3;
	}
	public:
	unary_operator(expression_ptr p) : subexpr(std::move(p)) {}
#ifndef NDEBUG
	void dump_impl() override
	{
		std::cerr << OpChar;
		subexpr->dump();
	}
#endif
};

/**
 * Abstract base class for binary operators.  Allows the tree to be modified
 * without knowing what the operations actually are.
 */
struct binary_operator_base : public expression
{
	/**
	 * The left side of the expression.
	 */
	expression_ptr lhs;
	/**
	 * The right side of the expression.
	 */
	expression_ptr rhs;
	/**
	 * Insert a node somewhere down the path of left children, until it would
	 * be preempting something that should execute first.
	 */
	void insert_left(binary_operator_base *new_left)
	{
		if (lhs->precedence() < new_left->precedence())
		{
			new_left->rhs = std::move(lhs);
			lhs.reset(new_left);
		}
		else
		{
			static_cast<binary_operator_base*>(lhs.get())->insert_left(new_left);
		}
	}
};

/**
 * Template class for binary operators.  The precedence and the operation are
 * provided as template parameters.
 */
template<int Precedence, class Op>
struct binary_operator : public binary_operator_base
{
	valty operator()() override
	{
		Op op;
		return op((*lhs)(), (*rhs)());
	}
	int precedence() override
	{
		return Precedence;
	}
#ifdef NDEBUG
	/**
	 * Constructor.  Takes the name of the operator as an argument, for
	 * debugging.  Only stores it in debug mode.
	 */
	binary_operator(const char *) {}
#else
	const char *opName;
	binary_operator(const char *o) : opName(o) {}
	void dump_impl() override
	{
		lhs->dump();
		std::cerr << opName;
		rhs->dump();
	}
#endif
};

/**
 * Ternary conditional operators (`cond ? true : false`) are a special case -
 * there are no other ternary operators.
 */
class ternary_conditional_operator : public expression
{
	/**
	 * The condition for the clause.
	 */
	expression_ptr cond;
	/**
	 * The expression that this evaluates to if the condition is true.
	 */
	expression_ptr lhs;
	/**
	 * The expression that this evaluates to if the condition is false.
	 */
	expression_ptr rhs;
	valty operator()() override
	{
		return (*cond)() ? (*lhs)() : (*rhs)();
	}
	int precedence() override
	{
		// The actual precedence of a ternary conditional operator is 15, but
		// its associativity is the opposite way around to the other operators,
		// so we fudge it slightly.
		return 3;
	}
#ifndef NDEBUG
	void dump_impl() override
	{
		cond->dump();
		std::cerr << " ? ";
		lhs->dump();
		std::cerr << " : ";
		rhs->dump();
	}
#endif
	public:
	ternary_conditional_operator(expression_ptr c,
	                             expression_ptr l,
	                             expression_ptr r) :
		cond(std::move(c)), lhs(std::move(l)), rhs(std::move(r)) {}
};

template<typename T>
struct lshift
{
	constexpr T operator()(const T &lhs, const T &rhs) const
	{
		return lhs << rhs;
	}
};
template<typename T>
struct rshift
{
	constexpr T operator()(const T &lhs, const T &rhs) const
	{
		return lhs >> rhs;
	}
};
template<typename T>
struct unary_plus
{
	constexpr T operator()(const T &val) const
	{
		return +val;
	}
};
// TODO: Replace with std::bit_not once we can guarantee C++14 as a baseline.
template<typename T>
struct bit_not
{
	constexpr T operator()(const T &val) const
	{
		return ~val;
	}
};

} // anonymous namespace


expression_ptr input_buffer::parse_binary_expression(expression_ptr lhs)
{
	next_token();
	binary_operator_base *expr = nullptr;
	char op = ((*this)[0]);
	switch (op)
	{
		default:
			return lhs;
		case '+':
			expr = new binary_operator<6, std::plus<valty>>("+");
			break;
		case '-':
			expr = new binary_operator<6, std::minus<valty>>("-");
			break;
		case '%':
			expr = new binary_operator<5, std::modulus<valty>>("%");
			break;
		case '*':
			expr = new binary_operator<5, std::multiplies<valty>>("*");
			break;
		case '/':
			expr = new binary_operator<5, std::divides<valty>>("/");
			break;
		case '<':
			cursor++;
			switch ((*this)[0])
			{
				default:
					parse_error("Invalid operator");
					return nullptr;
				case ' ':
				case '(':
				case '0'...'9':
					cursor--;
					expr = new binary_operator<8, std::less<valty>>("<");
					break;
				case '=':
					expr = new binary_operator<8, std::less_equal<valty>>("<=");
					break;
				case '<':
					expr = new binary_operator<7, lshift<valty>>("<<");
					break;
			}
			break;
		case '>':
			cursor++;
			switch ((*this)[0])
			{
				default:
					parse_error("Invalid operator");
					return nullptr;
				case '(':
				case ' ':
				case '0'...'9':
					cursor--;
					expr = new binary_operator<8, std::greater<valty>>(">");
					break;
				case '=':
					expr = new binary_operator<8, std::greater_equal<valty>>(">=");
					break;
				case '>':
					expr = new binary_operator<7, rshift<valty>>(">>");
					break;
					return lhs;
			}
			break;
		case '=':
			if ((*this)[1] != '=')
			{
				parse_error("Invalid operator");
				return nullptr;
			}
			consume('=');
			expr = new binary_operator<9, std::equal_to<valty>>("==");
			break;
		case '!':
			if ((*this)[1] != '=')
			{
				parse_error("Invalid operator");
				return nullptr;
			}
			cursor++;
			expr = new binary_operator<9, std::not_equal_to<valty>>("!=");
			break;
		case '&':
			if ((*this)[1] == '&')
			{
				expr = new binary_operator<13, std::logical_and<valty>>("&&");
			}
			else
			{
				expr = new binary_operator<10, std::bit_and<valty>>("&");
			}
			break;
		case '|':
			if ((*this)[1] == '|')
			{
				expr = new binary_operator<12, std::logical_or<valty>>("||");
			}
			else
			{
				expr = new binary_operator<14, std::bit_or<valty>>("|");
			}
			break;
		case '?':
		{
			consume('?');
			expression_ptr true_case = parse_expression();
			next_token();
			if (!true_case || !consume(':'))
			{
				parse_error("Expected : in ternary conditional operator");
				return nullptr;
			}
			expression_ptr false_case = parse_expression();
			if (!false_case)
			{
				parse_error("Expected false condition for ternary operator");
				return nullptr;
			}
			return expression_ptr(new ternary_conditional_operator(std::move(lhs),
						std::move(true_case), std::move(false_case)));
		}
	}
	cursor++;
	next_token();
	expression_ptr e(expr);
	expression_ptr rhs(parse_expression());
	if (!rhs)
	{
		return nullptr;
	}
	expr->lhs = std::move(lhs);
	if (rhs->precedence() < expr->precedence())
	{
		expr->rhs = std::move(rhs);
	}
	else
	{
		// If we're a normal left-to-right expression, then we need to insert
		// this as the far-left child node of the rhs expression
		binary_operator_base *rhs_op =
			static_cast<binary_operator_base*>(rhs.get());
		rhs_op->insert_left(expr);
		e.release();
		return rhs;
	}
	return e;
}

expression_ptr input_buffer::parse_expression(bool stopAtParen)
{
	next_token();
	unsigned long long leftVal;
	expression_ptr lhs;
	switch ((*this)[0])
	{
		case '0'...'9':
			if (!consume_integer(leftVal))
			{
				return nullptr;
			}
			lhs.reset(new terminal_expr(leftVal));
			break;
		case '(':
		{
			consume('(');
			expression_ptr &&subexpr = parse_expression();
			if (!subexpr)
			{
				return nullptr;
			}
			lhs.reset(new paren_expression(std::move(subexpr)));
			if (!consume(')'))
			{
				return nullptr;
			}
			if (stopAtParen)
			{
				return lhs;
			}
			break;
		}
		case '+':
		{
			consume('+');
			expression_ptr &&subexpr = parse_expression();
			if (!subexpr)
			{
				return nullptr;
			}
			lhs.reset(new unary_operator<'+', unary_plus<valty>>(std::move(subexpr)));
			break;
		}
		case '-':
		{
			consume('-');
			expression_ptr &&subexpr = parse_expression();
			if (!subexpr)
			{
				return nullptr;
			}
			lhs.reset(new unary_operator<'-', std::negate<valty>>(std::move(subexpr)));
			break;
		}
		case '!':
		{
			consume('!');
			expression_ptr &&subexpr = parse_expression();
			if (!subexpr)
			{
				return nullptr;
			}
			lhs.reset(new unary_operator<'!', std::logical_not<valty>>(std::move(subexpr)));
			break;
		}
		case '~':
		{
			consume('~');
			expression_ptr &&subexpr = parse_expression();
			if (!subexpr)
			{
				return nullptr;
			}
			lhs.reset(new unary_operator<'~', bit_not<valty>>(std::move(subexpr)));
			break;
		}
	}
	if (!lhs)
	{
		return nullptr;
	}
	return parse_binary_expression(std::move(lhs));
}

bool
input_buffer::consume_integer_expression(unsigned long long &outInt)
{
	switch ((*this)[0])
	{
		case '(':
		{
			expression_ptr e(parse_expression(true));
			if (!e)
			{
				return false;
			}
			outInt = (*e)();
			return true;
		}
		case '0'...'9':
			return consume_integer(outInt);
		default:
			return false;
	}
}

bool
input_buffer::consume_hex_byte(uint8_t &outByte)
{
	if (!ishexdigit((*this)[0]) && !ishexdigit((*this)[1]))
	{
		return false;
	}
	outByte = (digittoint((*this)[0]) << 4) | digittoint((*this)[1]);
	cursor += 2;
	return true;
}

input_buffer&
input_buffer::next_token()
{
	int start;
	do {
		start = cursor;
		skip_spaces();
		// Parse /* comments
		if ((*this)[0] == '/' && (*this)[1] == '*')
		{
			// eat the start of the comment
			++(*this);
			++(*this);
			do {
				// Find the ending * of */
				while ((**this != '\0') && (**this != '*'))
				{
					++(*this);
				}
				// Eat the *
				++(*this);
			} while ((**this != '\0') && (**this != '/'));
			// Eat the /
			++(*this);
		}
		// Parse // comments
		if (((*this)[0] == '/' && (*this)[1] == '/'))
		{
			// eat the start of the comment
			++(*this);
			++(*this);
			// Find the ending of the line
			while (**this != '\n')
			{
				++(*this);
			}
			// Eat the \n
			++(*this);
		}
	} while (start != cursor);
	return *this;
}

void
input_buffer::parse_error(const char *msg)
{
	int line_count = 1;
	int line_start = 0;
	int line_end = cursor;
	for (int i=cursor ; i>0 ; --i)
	{
		if (buffer[i] == '\n')
		{
			line_count++;
			if (line_start == 0)
			{
				line_start = i+1;
			}
		}
	}
	for (int i=cursor+1 ; i<size ; ++i)
	{
		if (buffer[i] == '\n')
		{
			line_end = i;
			break;
		}
	}
	fprintf(stderr, "Error on line %d: %s\n", line_count, msg);
	fwrite(&buffer[line_start], line_end-line_start, 1, stderr);
	putc('\n', stderr);
	for (int i=0 ; i<(cursor-line_start) ; ++i)
	{
		char c = (buffer[i+line_start] == '\t') ? '\t' : ' ';
		putc(c, stderr);
	}
	putc('^', stderr);
	putc('\n', stderr);
}
#ifndef NDEBUG
void
input_buffer::dump()
{
	fprintf(stderr, "Current cursor: %d\n", cursor);
	fwrite(&buffer[cursor], size-cursor, 1, stderr);
}
#endif

mmap_input_buffer::mmap_input_buffer(int fd) : input_buffer(0, 0)
{
	struct stat sb;
	if (fstat(fd, &sb))
	{
		perror("Failed to stat file");
	}
	size = sb.st_size;
	buffer = (const char*)mmap(0, size, PROT_READ, MAP_PRIVATE |
			MAP_PREFAULT_READ, fd, 0);
	if (buffer == MAP_FAILED)
	{
		perror("Failed to mmap file");
		exit(EXIT_FAILURE);
	}
}

mmap_input_buffer::~mmap_input_buffer()
{
	if (buffer != 0)
	{
		munmap((void*)buffer, size);
	}
}

stream_input_buffer::stream_input_buffer() : input_buffer(0, 0)
{
	int c;
	while ((c = fgetc(stdin)) != EOF)
	{
		b.push_back(c);
	}
	buffer = b.data();
	size = b.size();
}

} // namespace dtc

