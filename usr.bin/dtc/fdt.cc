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

#define __STDC_LIMIT_MACROS 1

#include "fdt.hh"

#include <algorithm>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dtb.hh"

namespace dtc
{

namespace fdt
{

uint32_t
property_value::get_as_uint32()
{
	if (byte_data.size() != 4)
	{
		return 0;
	}
	uint32_t v = 0;
	v &= byte_data[0] << 24;
	v &= byte_data[1] << 16;
	v &= byte_data[2] << 8;
	v &= byte_data[3] << 0;
	return v;
}

void
property_value::push_to_buffer(byte_buffer &buffer)
{
	if (!byte_data.empty())
	{
		buffer.insert(buffer.end(), byte_data.begin(), byte_data.end());
	}
	else
	{
		string_data.push_to_buffer(buffer, true);
		// Trailing nul
		buffer.push_back(0);
	}
}

void
property_value::write_dts(FILE *file)
{
	resolve_type();
	switch (type)
	{
		default:
			assert(0 && "Invalid type");
		case STRING:
		case STRING_LIST:
		case CROSS_REFERENCE:
			write_as_string(file);
			break;
		case PHANDLE:
			write_as_cells(file);
			break;
		case BINARY:
			if (byte_data.size() % 4 == 0)
			{
				write_as_cells(file);
				break;
			}
			write_as_bytes(file);
			break;
	}
}

void
property_value::resolve_type()
{
	if (type != UNKNOWN)
	{
		return;
	}
	if (byte_data.empty())
	{
		type = STRING;
		return;
	}
	if (byte_data.back() == 0)
	{
		bool is_all_printable = true;
		int nuls = 0;
		int bytes = 0;
		for (byte_buffer::iterator i=byte_data.begin(), e=byte_data.end()-1; i<e ; i++)
		{
			bytes++;
			is_all_printable &= (*i == '\0') || isprint(*i);
			if (*i == '\0')
			{
				nuls++;
			}
			if (!is_all_printable)
			{
				break;
			}
		}
		if (is_all_printable && (bytes > nuls))
		{
			type = STRING;
			if (nuls > 0)
			{
				type = STRING_LIST;
			}
			return;
		}
	}
	type = BINARY;
}

void
property_value::write_as_string(FILE *file)
{
	putc('"', file);
	if (byte_data.empty())
	{
		string_data.print(file);
	}
	else
	{
		for (byte_buffer::iterator i=byte_data.begin(), e=byte_data.end()-1; i!=e ; ++i)
		{
			// FIXME Escape tabs, newlines, and so on.
			if (*i == '\0')
			{
				fputs("\", \"", file);
				continue;
			}
			putc(*i, file);
		}
	}
	putc('"', file);
}

void
property_value::write_as_cells(FILE *file)
{
	putc('<', file);
	assert((byte_data.size() % 4) == 0);
	for (byte_buffer::iterator i=byte_data.begin(), e=byte_data.end(); i!=e ; ++i)
	{
		uint32_t v = 0;
		v = (v << 8) | *i;
		++i;
		v = (v << 8) | *i;
		++i;
		v = (v << 8) | *i;
		++i;
		v = (v << 8) | *i;
		fprintf(file, "0x%" PRIx32, v);
		if (i+1 != e)
		{
			putc(' ', file);
		}
	}
	putc('>', file);
}

void
property_value::write_as_bytes(FILE *file)
{
	putc('[', file);
	for (byte_buffer::iterator i=byte_data.begin(), e=byte_data.end(); i!=e ; i++)
	{
		fprintf(file, "%hhx", *i);
		if (i+1 != e)
		{
			putc(' ', file);
		}
	}
	putc(']', file);
}

void
property::parse_string(input_buffer &input)
{
	property_value v;
	assert(input[0] == '"');
	++input;
	const char *start = (const char*)input;
	int length = 0;
	while (char c = input[0])
	{
		if (c == '"' && input[-1] != '\\')
		{
			input.consume('"');
			break;
		}
		++input;
		++length;
	}
	v.string_data = string(start, length);
	values.push_back(v);
}

void
property::parse_cells(input_buffer &input)
{
	assert(input[0] == '<');
	++input;
	property_value v;
	input.next_token();
	while (!input.consume('>'))
	{
		input.next_token();
		// If this is a phandle then we need to get the name of the
		// referenced node
		if (input.consume('&'))
		{
			input.next_token();
			// FIXME: We should support full paths here, but we
			// don't.
			string referenced = string::parse_node_name(input);
			if (referenced.empty())
			{
				input.parse_error("Expected node name");
				valid = false;
				return;
			}
			input.next_token();
			// If we already have some bytes, make the phandle a
			// separate component.
			if (!v.byte_data.empty())
			{
				values.push_back(v);
				v = property_value();
			}
			v.string_data = referenced;
			v.type = property_value::PHANDLE;
			values.push_back(v);
			v = property_value();
		}
		else
		{
			//FIXME: We should support labels in the middle
			//of these, but we don't.
			long long val;
			if (!input.consume_integer(val))
			{
				input.parse_error("Expected numbers in array of cells");
				valid = false;
				return;
			}
			if ((val < 0) || (val > UINT32_MAX))
			{
				input.parse_error("Value out of range");
				valid = false;
				return;
			}
			push_big_endian(v.byte_data, (uint32_t)val);
			input.next_token();
		}
	}
	// Don't store an empty string value here.
	if (v.byte_data.size() > 0)
	{
		values.push_back(v);
	}
}

void
property::parse_bytes(input_buffer &input)
{
	assert(input[0] == '[');
	++input;
	property_value v;
	input.next_token();
	while (!input.consume(']'))
	{
		{
			//FIXME: We should support
			//labels in the middle of
			//these, but we don't.
			uint8_t val;
			if (!input.consume_hex_byte(val))
			{
				input.parse_error("Expected hex bytes in array of bytes");
				valid = false;
				return;
			}
			v.byte_data.push_back(val);
			input.next_token();
		}
	}
	values.push_back(v);
}

void
property::parse_reference(input_buffer &input)
{
	assert(input[0] == '&');
	++input;
	input.next_token();
	property_value v;
	v.string_data = string::parse_node_name(input);
	if (v.string_data.empty())
	{
		input.parse_error("Expected node name");
		valid = false;
		return;
	}
	v.type = property_value::CROSS_REFERENCE;
	values.push_back(v);
}

property::property(input_buffer &structs, input_buffer &strings)
{
	uint32_t name_offset;
	uint32_t length;
	valid = structs.consume_binary(length) &&
		structs.consume_binary(name_offset);
	if (!valid)
	{
		fprintf(stderr, "Failed to read property\n");
		return;
	}
	// Find the name
	input_buffer name_buffer = strings.buffer_from_offset(name_offset);
	if (name_buffer.empty())
	{
		fprintf(stderr, "Property name offset %" PRIu32
			" is past the end of the strings table\n",
			name_offset);
		valid = false;
		return;
	}
	key = string(name_buffer);
	// Read the value
	uint8_t byte;
	property_value v;
	for (uint32_t i=0 ; i<length ; i++)
	{
		if (!(valid = structs.consume_binary(byte)))
		{
			fprintf(stderr, "Failed to read property value\n");
			return;
		}
		v.byte_data.push_back(byte);
	}
	values.push_back(v);
}

void property::parse_define(input_buffer &input, define_map *defines)
{
	input.consume('$');
	if (!defines)
	{
		input.parse_error("No predefined properties to match name\n");
		valid = false;
		return;
	}
	string name = string::parse_property_name(input);
	define_map::iterator found;
	if ((name == string()) ||
	    ((found = defines->find(name)) == defines->end()))
	{
		input.parse_error("Undefined property name\n");
		valid = false;
		return;
	}
	values.push_back((*found).second->values[0]);
}

property::property(input_buffer &input,
                   string k,
                   string l,
                   bool semicolonTerminated,
                   define_map *defines) : key(k), label(l), valid(true)
{
	do {
		input.next_token();
		switch (input[0])
		{
			case '$':
			{
				parse_define(input, defines);
				if (valid)
				{
					break;
				}
			}
			default:
				input.parse_error("Invalid property value.");
				valid = false;
				return;
			case '"':
				parse_string(input);
				break;
			case '<':
				parse_cells(input);
				break;
			case '[':
				parse_bytes(input);
				break;
			case '&':
				parse_reference(input);
				break;
			case ';':
			{
				break;
			}
		}
		input.next_token();
	} while (input.consume(','));
	if (semicolonTerminated && !input.consume(';'))
	{
		input.parse_error("Expected ; at end of property");
		valid = false;
	}
}

property*
property::parse_dtb(input_buffer &structs, input_buffer &strings)
{
	property *p = new property(structs, strings);
	if (!p->valid)
	{
		delete p;
		p = 0;
	}
	return p;
}

property*
property::parse(input_buffer &input, string key, string label,
                bool semicolonTerminated, define_map *defines)
{
	property *p = new property(input, key, label, semicolonTerminated, defines);
	if (!p->valid)
	{
		delete p;
		p = 0;
	}
	return p;
}

void
property::write(dtb::output_writer &writer, dtb::string_table &strings)
{
	writer.write_token(dtb::FDT_PROP);
	byte_buffer value_buffer;
	for (value_iterator i=begin(), e=end() ; i!=e ; ++i)
	{
		i->push_to_buffer(value_buffer);
	}
	writer.write_data((uint32_t)value_buffer.size());
	writer.write_comment(key);
	writer.write_data(strings.add_string(key));
	writer.write_data(value_buffer);
}

void
property::write_dts(FILE *file, int indent)
{
	for (int i=0 ; i<indent ; i++)
	{
		putc('\t', file);
	}
	if (label != string())
	{
		label.print(file);
		fputs(": ", file);
	}
	if (key != string())
	{
		key.print(file);
	}
	if (!values.empty())
	{
		fputs(" = ", file);
		for (value_iterator i=begin(), e=end() ; i!=e ; ++i)
		{
			i->write_dts(file);
			if (i+1 != e)
			{
				putc(',', file);
				putc(' ', file);
			}
		}
	}
	fputs(";\n", file);
}

string
node::parse_name(input_buffer &input, bool &is_property, const char *error)
{
	if (!valid)
	{
		return string();
	}
	input.next_token();
	if (is_property)
	{
		return string::parse_property_name(input);
	}
	string n = string::parse_node_or_property_name(input, is_property);
	if (n.empty())
	{
		if (n.empty())
		{
			input.parse_error(error);
			valid = false;
		}
	}
	return n;
}

node::node(input_buffer &structs, input_buffer &strings) : valid(true)
{
	const char *name_start = (const char*)structs;
	int name_length = 0;
	while (structs[0] != '\0' && structs[0] != '@')
	{
		name_length++;
		++structs;
	}
	name = string(name_start, name_length);
	if (structs[0] == '@')
	{
		++structs;
		name_start = (const char*)structs;
		name_length = 0;
		while (structs[0] != '\0')
		{
			name_length++;
			++structs;
		}
		unit_address = string(name_start, name_length);
	}
	++structs;
	uint32_t token;
	while (structs.consume_binary(token))
	{
		switch (token)
		{
			default:
				fprintf(stderr, "Unexpected token 0x%" PRIx32
					" while parsing node.\n", token);
				valid = false;
				return;
			// Child node, parse it.
			case dtb::FDT_BEGIN_NODE:
			{
				node *child = node::parse_dtb(structs, strings);
				if (child == 0)
				{
					valid = false;
					return;
				}
				children.push_back(child);
				break;
			}
			// End of this node, no errors.
			case dtb::FDT_END_NODE:
				return;
			// Property, parse it.
			case dtb::FDT_PROP:
			{
				property *prop = property::parse_dtb(structs, strings);
				if (prop == 0)
				{
					valid = false;
					return;
				}
				properties.push_back(prop);
				break;
			}
				break;
			// End of structs table.  Should appear after
			// the end of the last node.
			case dtb::FDT_END:
				fprintf(stderr, "Unexpected FDT_END token while parsing node.\n");
				valid = false;
				return;
			// NOPs are padding.  Ignore them.
			case dtb::FDT_NOP:
				break;
		}
	}
	fprintf(stderr, "Failed to read token from structs table while parsing node.\n");
	valid = false;
	return;
}

node::node(input_buffer &input, string n, string l, string a, define_map *defines) : 
	label(l), name(n), unit_address(a), valid(true)
{
	if (!input.consume('{'))
	{
		input.parse_error("Expected { to start new device tree node.\n");
	}
	input.next_token();
	while (valid && !input.consume('}'))
	{
		// flag set if we find any characters that are only in
		// the property name character set, not the node 
		bool is_property = false;
		string child_name, child_label, child_address;
		child_name = parse_name(input, is_property,
				"Expected property or node name");
		if (input.consume(':'))
		{
			// Node labels can contain any characters?  The
			// spec doesn't say, so we guess so...
			is_property = false;
			child_label = child_name;
			child_name = parse_name(input, is_property, "Expected property or node name");
		}
		if (input.consume('@'))
		{
			child_address = parse_name(input, is_property, "Expected unit address");
		}
		if (!valid)
		{
			return;
		}
		input.next_token();
		// If we're parsing a property, then we must actually do that.
		if (input.consume('='))
		{
			property *p= property::parse(input, child_name,
					child_label, true, defines);
			if (p == 0)
			{
				valid = false;
			}
			else
			{
				properties.push_back(p);
			}
		}
		else if (!is_property && input[0] == ('{'))
		{
			node *child = node::parse(input, child_name,
					child_label, child_address, defines);
			if (child)
			{
				children.push_back(child);
			}
			else
			{
				valid = false;
			}
		}
		else if (input.consume(';'))
		{
			properties.push_back(new property(child_name, child_label));
		}
		else
		{
			input.parse_error("Error parsing property.");
			valid = false;
		}
		input.next_token();
	}
	input.consume(';');
}

bool
node::cmp_properties(property *p1, property *p2)
{
	return p1->get_key() < p2->get_key();
}

bool
node::cmp_children(node *c1, node *c2)
{
	if (c1->name == c2->name)
	{
		return c1->unit_address < c2->unit_address;
	}
	return c1->name < c2->name;
}

void
node::sort()
{
	std::sort(property_begin(), property_end(), cmp_properties);
	std::sort(child_begin(), child_end(), cmp_children);
	for (child_iterator i=child_begin(), e=child_end() ; i!=e ; ++i)
	{
		(*i)->sort();
	}
}

node*
node::parse(input_buffer &input,
            string name,
            string label,
            string address,
            define_map *defines)
{
	node *n = new node(input, name, label, address, defines);
	if (!n->valid)
	{
		delete n;
		n = 0;
	}
	return n;
}

node*
node::parse_dtb(input_buffer &structs, input_buffer &strings)
{
	node *n = new node(structs, strings);
	if (!n->valid)
	{
		delete n;
		n = 0;
	}
	return n;
}

node::~node()
{
	while (!children.empty())
	{
		delete children.back();
		children.pop_back();
	}
	while (!properties.empty())
	{
		delete properties.back();
		properties.pop_back();
	}
}

property*
node::get_property(string key)
{
	for (property_iterator i=property_begin(), e=property_end() ; i!=e ; ++i)
	{
		if ((*i)->get_key() == key)
		{
			return *i;
		}
	}
	return 0;
}

void
node::merge_node(node *other)
{
	if (!other->label.empty())
	{
		label = other->label;
	}
	// Note: this is an O(n*m) operation.  It might be sensible to
	// optimise this if we find that there are nodes with very
	// large numbers of properties, but for typical usage the
	// entire vector will fit (easily) into cache, so iterating
	// over it repeatedly isn't that expensive.
	while (!other->properties.empty())
	{
		property *p = other->properties.front();
		for (property_iterator i=property_begin(), e=property_end() ; i!=e ; ++i)
		{
			if ((*i)->get_key() == p->get_key())
			{
				delete *i;
				properties.erase(i);
				break;
			}
		}
		add_property(p);
		other->properties.erase(other->properties.begin());
	}
	while (!other->children.empty())
	{
		node *c = other->children.front();
		bool found = false;
		for (child_iterator i=child_begin(), e=child_end() ; i!=e ; ++i)
		{
			if ((*i)->name == c->name && (*i)->unit_address == c->unit_address)
			{
				(*i)->merge_node(c);
				delete c;
				found = true;
				break;
			}
		}
		if (!found)
		{
			children.push_back(c);
		}
		other->children.erase(other->children.begin());
	}
}

void
node::write(dtb::output_writer &writer, dtb::string_table &strings)
{
	writer.write_token(dtb::FDT_BEGIN_NODE);
	byte_buffer name_buffer;
	name.push_to_buffer(name_buffer);
	if (unit_address != string())
	{
		name_buffer.push_back('@');
		unit_address.push_to_buffer(name_buffer);
	}
	writer.write_comment(name);
	writer.write_data(name_buffer);
	writer.write_data((uint8_t)0);
	for (property_iterator i=property_begin(), e=property_end() ; i!=e ; ++i)
	{
		(*i)->write(writer, strings);
	}
	for (child_iterator i=child_begin(), e=child_end() ; i!=e ; ++i)
	{
		(*i)->write(writer, strings);
	}
	writer.write_token(dtb::FDT_END_NODE);
}

void
node::write_dts(FILE *file, int indent)
{
	for (int i=0 ; i<indent ; i++)
	{
		putc('\t', file);
	}
	if (label != string())
	{
		label.print(file);
		fputs(": ", file);
	}
	if (name != string())
	{
		name.print(file);
	}
	if (unit_address != string())
	{
		putc('@', file);
		unit_address.print(file);
	}
	fputs(" {\n\n", file);
	for (property_iterator i=property_begin(), e=property_end() ; i!=e ; ++i)
	{
		(*i)->write_dts(file, indent+1);
	}
	for (child_iterator i=child_begin(), e=child_end() ; i!=e ; ++i)
	{
		(*i)->write_dts(file, indent+1);
	}
	for (int i=0 ; i<indent ; i++)
	{
		putc('\t', file);
	}
	fputs("};\n", file);
}

void
device_tree::collect_names_recursive(node* n, node_path &path)
{
	string name = n->label;
	path.push_back(std::make_pair(n->name, n->unit_address));
	if (name != string())
	{
		if (node_names.find(name) == node_names.end())
		{
			node_names.insert(std::make_pair(name, n));
			node_paths.insert(std::make_pair(name, path));
		}
		else
		{
			node_names[name] = (node*)-1;
			std::map<string, node_path>::iterator i = node_paths.find(name);
			if (i != node_paths.end())
			{
				node_paths.erase(name);
			}
			fprintf(stderr, "Label not unique: ");
			name.dump();
			fprintf(stderr, ".  References to this label will not be resolved.");
		}
	}
	for (node::child_iterator i=n->child_begin(), e=n->child_end() ; i!=e ; ++i)
	{
		collect_names_recursive(*i, path);
	}
	path.pop_back();
	// Now we collect the phandles and properties that reference
	// other nodes.
	for (node::property_iterator i=n->property_begin(), e=n->property_end() ; i!=e ; ++i)
	{
		for (property::value_iterator p=(*i)->begin(),pe=(*i)->end() ; p!=pe ; ++p)
		{
			if (p->is_phandle())
			{
				phandles.push_back(&*p);
			}
			if (p->is_cross_reference())
			{
				cross_references.push_back(&*p);
			}
		}
		if ((*i)->get_key() == string("phandle") ||
		    (*i)->get_key() == string("linux,phandle"))
		{
			if ((*i)->begin()->byte_data.size() != 4)
			{
				fprintf(stderr, "Invalid phandle value for node ");
				n->name.dump();
				fprintf(stderr, ".  Should be a 4-byte value.\n");
				valid = false;
			}
			else
			{
				uint32_t phandle = (*i)->begin()->get_as_uint32();
				used_phandles.insert(std::make_pair(phandle, n));
			}
		}
	}
}

void
device_tree::collect_names()
{
	node_path p;
	collect_names_recursive(root, p);
}

void
device_tree::resolve_cross_references()
{
	for (std::vector<property_value*>::iterator i=cross_references.begin(), e=cross_references.end() ; i!=e ; ++i)
	{
		property_value* pv = *i;
		node_path path = node_paths[pv->string_data];
		// Skip the first name in the path.  It's always "", and implicitly /
		for (node_path::iterator p=path.begin()+1, pe=path.end() ; p!=pe ; ++p)
		{
			pv->byte_data.push_back('/');
			p->first.push_to_buffer(pv->byte_data);
			if (!(p->second.empty()))
			{
				pv->byte_data.push_back('@');
				p->second.push_to_buffer(pv->byte_data);
			}
		}
		pv->byte_data.push_back(0);
	}
	uint32_t phandle = 1;
	for (std::vector<property_value*>::iterator i=phandles.begin(), e=phandles.end() ; i!=e ; ++i)
	{
		string target_name = (*i)->string_data;
		node *target = node_names[target_name];
		if (target == 0)
		{
			fprintf(stderr, "Failed to find node with label:");
			target_name.dump();
			fprintf(stderr, "\n");
			valid = 0;
			return;
		}
		// If there is an existing phandle, use it
		property *p = target->get_property("phandle");
		if (p == 0)
		{
			p = target->get_property("linux,phandle");
		}
		if (p == 0)
		{
			// Otherwise insert a new phandle node
			property_value v;
			while (used_phandles.find(phandle) != used_phandles.end())
			{
				// Note that we only don't need to
				// store this phandle in the set,
				// because we are monotonically
				// increasing the value of phandle and
				// so will only ever revisit this value
				// if we have used 2^32 phandles, at
				// which point our blob won't fit in
				// any 32-bit system and we've done
				// something badly wrong elsewhere
				// already.
				phandle++;
			}
			push_big_endian(v.byte_data, phandle++);
			if (phandle_node_name == BOTH || phandle_node_name == LINUX)
			{
				p = new property(string("linux,phandle"));
				p->add_value(v);
				target->add_property(p);
			}
			if (phandle_node_name == BOTH || phandle_node_name == EPAPR)
			{
				p = new property(string("phandle"));
				p->add_value(v);
				target->add_property(p);
			}
		}
		p->begin()->push_to_buffer((*i)->byte_data);
		assert((*i)->byte_data.size() == 4);
	}
}

void
device_tree::parse_roots(input_buffer &input, std::vector<node*> &roots)
{
	input.next_token();
	while (valid && input.consume('/'))
	{
		input.next_token();
		node *n = node::parse(input, string("", 1), string(), string(), &defines);
		if (n)
		{
			roots.push_back(n);
		}
		else
		{
			valid = false;
		}
	}
}

input_buffer*
device_tree::buffer_for_file(const char *path)
{
	if (string(path) == string("-"))
	{
		input_buffer *b = new stream_input_buffer();
		buffers.push_back(b);
		return b;
	}
	int source = open(path, O_RDONLY);
	if (source == -1)
	{
		fprintf(stderr, "Unable to open file %s\n", path);
		return 0;
	}
	input_buffer *b = new mmap_input_buffer(source);
	// Keep the buffer that owns the memory around for the lifetime
	// of this FDT.  Ones simply referring to it may have shorter
	// lifetimes.
	buffers.push_back(b);
	close(source);
	return b;
}

template<class writer> void
device_tree::write(int fd)
{
	dtb::string_table st;
	dtb::header head;
	writer head_writer;
	writer reservation_writer;
	writer struct_writer;
	writer strings_writer;

	// Build the reservation table
	reservation_writer.write_comment(string("Memory reservations"));
	reservation_writer.write_label(string("dt_reserve_map"));
	for (std::vector<reservation>::iterator i=reservations.begin(),
	     e=reservations.end() ; i!=e ; ++i)
	{
		reservation_writer.write_comment(string("Reservation start"));
		reservation_writer.write_data(i->first);
		reservation_writer.write_comment(string("Reservation length"));
		reservation_writer.write_data(i->first);
	}
	// Write n spare reserve map entries, plus the trailing 0.
	for (uint32_t i=0 ; i<=spare_reserve_map_entries ; i++)
	{
		reservation_writer.write_data((uint64_t)0);
		reservation_writer.write_data((uint64_t)0);
	}


	struct_writer.write_comment(string("Device tree"));
	struct_writer.write_label(string("dt_struct_start"));
	root->write(struct_writer, st);
	struct_writer.write_token(dtb::FDT_END);
	struct_writer.write_label(string("dt_struct_end"));

	st.write(strings_writer);
	// Find the strings size before we stick padding on the end.
	// Note: We should possibly use a new writer for the padding.
	head.size_dt_strings = strings_writer.size();

	// Stick the padding in the strings writer, but after the
	// marker indicating that it's the end.
	// Note: We probably should add a padding call to the writer so
	// that the asm back end can write padding directives instead
	// of a load of 0 bytes.
	for (uint32_t i=0 ; i<blob_padding ; i++)
	{
		strings_writer.write_data((uint8_t)0);
	}
	head.totalsize = sizeof(head) + strings_writer.size() +
		struct_writer.size() + reservation_writer.size();
	while (head.totalsize < minimum_blob_size)
	{
		head.totalsize++;
		strings_writer.write_data((uint8_t)0);
	}
	head.off_dt_struct = sizeof(head) + reservation_writer.size();;
	head.off_dt_strings = head.off_dt_struct + struct_writer.size();
	head.off_mem_rsvmap = sizeof(head);
	head.boot_cpuid_phys = boot_cpu;
	head.size_dt_struct = struct_writer.size();
	head.write(head_writer);

	head_writer.write_to_file(fd);
	reservation_writer.write_to_file(fd);
	struct_writer.write_to_file(fd);
	strings_writer.write_label(string("dt_blob_end"));
	strings_writer.write_to_file(fd);
}

node*
device_tree::referenced_node(property_value &v)
{
	if (v.is_phandle())
	{
		return node_names[v.string_data];
	}
	if (v.is_binary())
	{
		return used_phandles[v.get_as_uint32()];
	}
	return 0;
}

void
device_tree::write_binary(int fd)
{
	write<dtb::binary_writer>(fd);
}

void
device_tree::write_asm(int fd)
{
	write<dtb::asm_writer>(fd);
}

void
device_tree::write_dts(int fd)
{
	FILE *file = fdopen(fd, "w");
	fputs("/dtc-v1/;\n\n", file);

	if (!reservations.empty())
	{
		const char msg[] = "/memreserve/";
		fwrite(msg, sizeof(msg), 1, file);
		for (std::vector<reservation>::iterator i=reservations.begin(),
		     e=reservations.end() ; i!=e ; ++i)
		{
			fprintf(file, " %" PRIx64 " %" PRIx64, i->first, i->second);
		}
		fputs(";\n\n", file);
	}
	putc('/', file);
	putc(' ', file);
	root->write_dts(file, 0);
	fclose(file);
}

void
device_tree::parse_dtb(const char *fn, FILE *depfile)
{
	input_buffer *in = buffer_for_file(fn);
	if (in == 0)
	{
		valid = false;
		return;
	}
	input_buffer &input = *in;
	dtb::header h;
	valid = h.read_dtb(input);
	boot_cpu = h.boot_cpuid_phys;
	if (h.last_comp_version > 17)
	{
		fprintf(stderr, "Don't know how to read this version of the device tree blob");
		valid = false;
	}
	if (!valid)
	{
		return;
	}
	input_buffer reservation_map =
		input.buffer_from_offset(h.off_mem_rsvmap, 0);
	uint64_t start, length;
	do
	{
		if (!(reservation_map.consume_binary(start) &&
		      reservation_map.consume_binary(length)))
		{
			fprintf(stderr, "Failed to read memory reservation table\n");
			valid = false;
			return;
		}
	} while (!((start == 0) && (length == 0)));
	input_buffer struct_table =
		input.buffer_from_offset(h.off_dt_struct, h.size_dt_struct);
	input_buffer strings_table =
		input.buffer_from_offset(h.off_dt_strings, h.size_dt_strings);
	uint32_t token;
	if (!(struct_table.consume_binary(token) &&
		(token == dtb::FDT_BEGIN_NODE)))
	{
		fprintf(stderr, "Expected FDT_BEGIN_NODE token.\n");
		valid = false;
		return;
	}
	root = node::parse_dtb(struct_table, strings_table);
	if (!(struct_table.consume_binary(token) && (token == dtb::FDT_END)))
	{
		fprintf(stderr, "Expected FDT_END token after parsing root node.\n");
		valid = false;
		return;
	}
	valid = (root != 0);
}

void
device_tree::parse_dts(const char *fn, FILE *depfile)
{
	input_buffer *in = buffer_for_file(fn);
	if (in == 0)
	{
		valid = false;
		return;
	}
	std::vector<node*> roots;
	input_buffer &input = *in;
	input.next_token();
	bool read_header = false;
	// Read the header
	if (input.consume("/dts-v1/;"))
	{
		read_header = true;
	}
	input.next_token();
	while(input.consume("/include/"))
	{
		bool reallyInclude = true;
		if (input.consume("if "))
		{
			input.next_token();
			string name = string::parse_property_name(input);
			// XXX: Error handling
			if (defines.find(name) == defines.end())
			{
				reallyInclude = false;
			}
			input.consume('/');
		}
		input.next_token();
		if (!input.consume('"'))
		{
			input.parse_error("Expected quoted filename");
			valid = false;
			return;
		}
		int length = 0;
		while (input[length] != '"') length++;

		const char *file = (const char*)input;
		const char *dir = dirname(fn);
		int dir_length = strlen(dir);
		char *include_file = (char*)malloc(strlen(dir) + length + 2);
		memcpy(include_file, dir, dir_length);
		include_file[dir_length] = '/';
		memcpy(include_file+dir_length+1, file, length);
		include_file[dir_length+length+1] = 0;

		input.consume(include_file+dir_length+1);
		input.consume('"');
		if (!reallyInclude)
		{
			continue;
		}

		input_buffer *include_buffer = buffer_for_file(include_file);

		if (include_buffer == 0)
		{
			for (std::vector<const char*>::iterator i=include_paths.begin(), e=include_paths.end() ; e!=i ; ++i)
			{
				free(include_file);
				dir = *i;
				dir_length = strlen(dir);
				include_file = (char*)malloc(strlen(dir) +
						length + 2);
				memcpy(include_file, dir, dir_length);
				include_file[dir_length] = '/';
				memcpy(include_file+dir_length+1, file, length);
				include_file[dir_length+length+1] = 0;
				include_buffer = buffer_for_file(include_file);
				if (include_buffer != 0)
				{
					break;
				}
			}
		}
		if (depfile != 0)
		{
			putc(' ', depfile);
			fputs(include_file, depfile);
		}
		if (include_buffer == 0)
		{
			valid = false;
			return;
		}
		input_buffer &include = *include_buffer;
		free((void*)include_file);

		if (!read_header)
		{
			include.next_token();
			read_header = include.consume("/dts-v1/;");
		}
		parse_roots(include, roots);
	}
	input.next_token();
	if (!read_header)
	{
		input.parse_error("Expected /dts-v1/; version string");
	}
	// Read any memory reservations
	while(input.consume("/memreserve/"))
	{
		long long start, len;
		input.next_token();
		// Read the start and length.
		if (!(input.consume_integer(start) &&
		    (input.next_token(),
		    input.consume_integer(len))))
		{
			input.parse_error("Expected /dts-v1/; version string");
		}
		input.next_token();
		input.consume(';');
		reservations.push_back(reservation(start, len));
	}
	parse_roots(input, roots);
	switch (roots.size())
	{
		case 0:
			valid = false;
			input.parse_error("Failed to find root node /.");
			return;
		case 1:
			root = roots[0];
			break;
		default:
		{
			root = roots[0];
			for (std::vector<node*>::iterator i=roots.begin()+1,
			     e=roots.end() ; i!=e ; ++i)
			{
				root->merge_node(*i);
				delete *i;
			}
			roots.resize(1);
		}
	}
	collect_names();
	resolve_cross_references();
}

device_tree::~device_tree()
{
	if (root != 0)
	{
		delete root;
	}
	while (!buffers.empty())
	{
		delete buffers.back();
		buffers.pop_back();
	}
	for (define_map::iterator i=defines.begin(), e=defines.end() ;
	     i!=e ; ++i)
	{
		delete i->second;
	}
}

bool device_tree::parse_define(const char *def)
{
	char *val = strchr(def, '=');
	if (!val)
	{
		if (strlen(def) != 0)
		{
			string name(def);
			defines[name];
			return true;
		}
		return false;
	}
	string name(def, val-def);
	val++;
	input_buffer in = input_buffer(val, strlen(val));
	property *p = property::parse(in, name, string(), false);
	if (p)
		defines[name] = p;
	return p;
}

} // namespace fdt

} // namespace dtc

