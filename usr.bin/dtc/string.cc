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

#include "string.hh"
#include <ctype.h>
#include <stdio.h>

namespace
{
/**
 * The source files are ASCII, so we provide a non-locale-aware version of
 * isalpha.  This is a class so that it can be used with a template function
 * for parsing strings.
 */
struct is_alpha 
{
	static inline bool check(const char c)
	{
		return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') &&
			(c <= 'Z'));
	}
};
/**
 * Check whether a character is in the set allowed for node names.  This is a
 * class so that it can be used with a template function for parsing strings.
 */
struct is_node_name_character
{
	static inline bool check(const char c)
	{
		switch(c)
		{
			default:
				return false;
			case 'a'...'z': case 'A'...'Z': case '0'...'9':
			case ',': case '.': case '+': case '-':
			case '_':
				return true;
		}
	}
};
/**
 * Check whether a character is in the set allowed for property names.  This is
 * a class so that it can be used with a template function for parsing strings.
 */
struct is_property_name_character
{
	static inline bool check(const char c)
	{
		switch(c)
		{
			default:
				return false;
			case 'a'...'z': case 'A'...'Z': case '0'...'9':
			case ',': case '.': case '+': case '-':
			case '_': case '#':
				return true;
		}
	}
};

}

namespace dtc
{

template<class T> string
string::parse(input_buffer &s)
{
	const char *start = s;
	int l=0;
	while (T::check(*s)) { l++; ++s; }
	return string(start, l);
}

string::string(input_buffer &s) : start((const char*)s), length(0)
{
	while(s[length] != '\0')
	{
		length++;
	}
}

string
string::parse_node_name(input_buffer &s)
{
	return parse<is_node_name_character>(s);
}

string
string::parse_property_name(input_buffer &s)
{
	return parse<is_property_name_character>(s);
}
string
string::parse_node_or_property_name(input_buffer &s, bool &is_property)
{
	if (is_property)
	{
		return parse_property_name(s);
	}
	const char *start = s;
	int l=0;
	while (is_node_name_character::check(*s))
	{
		l++;
		++s;
	}
	while (is_property_name_character::check(*s))
	{
		l++;
		++s;
		is_property = true;
	}
	return string(start, l);
}

bool
string::operator==(const string& other) const
{
	return (length == other.length) &&
	       (memcmp(start, other.start, length) == 0);
}

bool
string::operator==(const char *other) const
{
	return strncmp(other, start, length) == 0;
}

bool
string::operator<(const string& other) const
{
	if (length < other.length) { return true; }
	if (length > other.length) { return false; }
	return memcmp(start, other.start, length) < 0;
}

void
string::push_to_buffer(byte_buffer &buffer, bool escapes)
{
	for (int i=0 ; i<length ; ++i)
	{
		uint8_t c = start[i];
		if (escapes && c == '\\' && i+1 < length)
		{
			c = start[++i];
			switch (c)
			{
				// For now, we just ignore invalid escape sequences.
				default:
				case '"':
				case '\'':
				case '\\':
					break;
				case 'a':
					c = '\a';
					break;
				case 'b':
					c = '\b';
					break;
				case 't':
					c = '\t';
					break;
				case 'n':
					c = '\n';
					break;
				case 'v':
					c = '\v';
					break;
				case 'f':
					c = '\f';
					break;
				case 'r':
					c = '\r';
					break;
				case '0'...'7':
				{
					int v = digittoint(c);
					if (i+1 < length && start[i+1] <= '7' && start[i+1] >= '0')
					{
						v <<= 3;
						v |= digittoint(start[i+1]);
						i++;
						if (i+1 < length && start[i+1] <= '7' && start[i+1] >= '0')
						{
							v <<= 3;
							v |= digittoint(start[i+1]);
						}
					}
					c = (uint8_t)v;
					break;
				}
				case 'x':
				{
					++i;
					if (i >= length)
					{
						break;
					}
					int v = digittoint(start[i]);
					if (i+1 < length && ishexdigit(start[i+1]))
					{
						v <<= 4;
						v |= digittoint(start[++i]);
					}
					c = (uint8_t)v;
					break;
				}
			}
		}
		buffer.push_back(c);
	}
}

void
string::print(FILE *file)
{
	fwrite(start, length, 1, file);
}

void
string::dump()
{
	print(stderr);
}

} // namespace dtc

