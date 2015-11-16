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

#ifndef _FDT_HH_
#define _FDT_HH_
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>

#include "util.hh"
#include "string.hh"

namespace dtc
{

namespace dtb 
{
struct output_writer;
class string_table;
}

namespace fdt
{
class property;
class node;
/**
 * Type for (owned) pointers to properties.
 */
typedef std::shared_ptr<property> property_ptr;
/**
 * Owning pointer to a node.
 */
typedef std::unique_ptr<node> node_ptr;
/**
 * Map from macros to property pointers.
 */
typedef std::unordered_map<string, property_ptr> define_map;
/**
 * Properties may contain a number of different value, each with a different
 * label.  This class encapsulates a single value.
 */
struct property_value
{
	/**
	 * The label for this data.  This is usually empty.
	 */
	string label;
	/**
	 * If this value is a string, or something resolved from a string (a
	 * reference) then this contains the source string.
	 */
	string string_data;
	/**
	 * The data that should be written to the final output.
	 */
	byte_buffer byte_data;
	/**
	 * Enumeration describing the possible types of a value.  Note that
	 * property-coded arrays will appear simply as binary (or possibly
	 * string, if they happen to be nul-terminated and printable), and must
	 * be checked separately.
	 */
	enum value_type
	{
		/**
		 * This is a list of strings.  When read from source, string
		 * lists become one property value for each string, however
		 * when read from binary we have a single property value
		 * incorporating the entire text, with nul bytes separating the
		 * strings.
		 */
		STRING_LIST,
		/**
		 * This property contains a single string.
		 */
		STRING,
		/**
		 * This is a binary value.  Check the size of byte_data to
		 * determine how many bytes this contains.
		 */
		BINARY,
		/** This contains a short-form address that should be replaced
		 * by a fully-qualified version.  This will only appear when
		 * the input is a device tree source.  When parsed from a
		 * device tree blob, the cross reference will have already been
		 * resolved and the property value will be a string containing
		 * the full path of the target node.  */
		CROSS_REFERENCE,
		/**
		 * This is a phandle reference.  When parsed from source, the
		 * string_data will contain the node label for the target and,
		 * after cross references have been resolved, the binary data
		 * will contain a 32-bit integer that should match the phandle
		 * property of the target node.
		 */
		PHANDLE,
		/**
		 * An empty property value.  This will never appear on a real
		 * property value, it is used by checkers to indicate that no
		 * property values should exist for a property.
		 */
		EMPTY,
		/**
		 * The type of this property has not yet been determined.
		 */
		UNKNOWN
	};
	/**
	 * The type of this property.
	 */
	value_type type;
	/**
	 * Returns true if this value is a cross reference, false otherwise.
	 */
	inline bool is_cross_reference()
	{
		return is_type(CROSS_REFERENCE);
	}
	/**
	 * Returns true if this value is a phandle reference, false otherwise.
	 */
	inline bool is_phandle()
	{
		return is_type(PHANDLE);
	}
	/**
	 * Returns true if this value is a string, false otherwise.
	 */
	inline bool is_string()
	{
		return is_type(STRING);
	}
	/**
	 * Returns true if this value is a string list (a nul-separated
	 * sequence of strings), false otherwise.
	 */
	inline bool is_string_list()
	{
		return is_type(STRING_LIST);
	}
	/**
	 * Returns true if this value is binary, false otherwise.
	 */
	inline bool is_binary()
	{
		return is_type(BINARY);
	}
	/**
	 * Returns this property value as a 32-bit integer.  Returns 0 if this
	 * property value is not 32 bits long.  The bytes in the property value
	 * are assumed to be in big-endian format, but the return value is in
	 * the host native endian.
	 */
	uint32_t get_as_uint32();
	/**
	 * Default constructor, specifying the label of the value.
	 */
	property_value(string l=string()) : label(l), type(UNKNOWN) {}
	/**
	 * Writes the data for this value into an output buffer.
	 */
	void push_to_buffer(byte_buffer &buffer);

	/**
	 * Writes the property value to the standard output.  This uses the
	 * following heuristics for deciding how to print the output:
	 *
	 * - If the value is nul-terminated and only contains printable
	 *   characters, it is written as a string.
	 * - If it is a multiple of 4 bytes long, then it is printed as cells.
	 * - Otherwise, it is printed as a byte buffer.
	 */
	void write_dts(FILE *file);
	/**
	 * Tries to merge adjacent property values, returns true if it succeeds and
	 * false otherwise.
	 */
	bool try_to_merge(property_value &other);
	private:
	/**
	 * Returns whether the value is of the specified type.  If the type of
	 * the value has not yet been determined, then this calculates it.
	 */
	inline bool is_type(value_type v)
	{
		if (type == UNKNOWN)
		{
			resolve_type();
		}
		return type == v;
	}
	/**
	 * Determines the type of the value based on its contents.
	 */
	void resolve_type();
	/**
	 * Writes the property value to the specified file as a quoted string.
	 * This is used when generating DTS.
	 */
	void write_as_string(FILE *file);
	/**
	 * Writes the property value to the specified file as a sequence of
	 * 32-bit big-endian cells.  This is used when generating DTS.
	 */
	void write_as_cells(FILE *file);
	/**
	 * Writes the property value to the specified file as a sequence of
	 * bytes.  This is used when generating DTS.
	 */
	void write_as_bytes(FILE *file);
};

/**
 * A value encapsulating a single property.  This contains a key, optionally a
 * label, and optionally one or more values.
 */
class property
{
	/**
	 * The name of this property.
	 */
	string key;
	/**
	 * An optional label.
	 */
	string label;
	/**
	 * The values in this property.
	 */
	std::vector<property_value> values;
	/**
	 * Value indicating that this is a valid property.  If a parse error
	 * occurs, then this value is false.
	 */
	bool valid;
	/**
	 * Parses a string property value, i.e. a value enclosed in double quotes.
	 */
	void parse_string(input_buffer &input);
	/**
	 * Parses one or more 32-bit values enclosed in angle brackets.
	 */
	void parse_cells(input_buffer &input, int cell_size);
	/**
	 * Parses an array of bytes, contained within square brackets.
	 */
	void parse_bytes(input_buffer &input);
	/**
	 * Parses a reference.  This is a node label preceded by an ampersand
	 * symbol, which should expand to the full path to that node.
	 *
	 * Note: The specification says that the target of such a reference is
	 * a node name, however dtc assumes that it is a label, and so we
	 * follow their interpretation for compatibility.
	 */
	void parse_reference(input_buffer &input);
	/**
	 * Parse a predefined macro definition for a property.
	 */
	void parse_define(input_buffer &input, define_map *defines);
	/**
	 * Constructs a new property from two input buffers, pointing to the
	 * struct and strings tables in the device tree blob, respectively.
	 * The structs input buffer is assumed to have just consumed the
	 * FDT_PROP token.
	 */
	property(input_buffer &structs, input_buffer &strings);
	/**
	 * Parses a new property from the input buffer.  
	 */
	property(input_buffer &input,
	         string k,
	         string l,
	         bool terminated,
	         define_map *defines);
	public:
	/**
	 * Creates an empty property.
	 */
	property(string k, string l=string()) : key(k), label(l), valid(true)
	{}
	/**
	 * Copy constructor.
	 */
	property(property &p) : key(p.key), label(p.label), values(p.values),
		valid(p.valid) {}
	/**
	 * Factory method for constructing a new property.  Attempts to parse a
	 * property from the input, and returns it on success.  On any parse
	 * error, this will return 0.
	 */
	static property_ptr parse_dtb(input_buffer &structs,
	                           input_buffer &strings);
	/**
	 * Factory method for constructing a new property.  Attempts to parse a
	 * property from the input, and returns it on success.  On any parse
	 * error, this will return 0.
	 */
	static property_ptr parse(input_buffer &input,
	                          string key,
	                          string label=string(),
	                          bool semicolonTerminated=true,
	                          define_map *defines=0);
	/**
	 * Iterator type used for accessing the values of a property.
	 */
	typedef std::vector<property_value>::iterator value_iterator;
	/**
	 * Returns an iterator referring to the first value in this property.
	 */
	inline value_iterator begin()
	{
		return values.begin();
	}
	/**
	 * Returns an iterator referring to the last value in this property.
	 */
	inline value_iterator end()
	{
		return values.end();
	}
	/**
	 * Adds a new value to an existing property.
	 */
	inline void add_value(property_value v)
	{
		values.push_back(v);
	}
	/**
	 * Returns the key for this property.
	 */
	inline string get_key()
	{
		return key;
	}
	/**
	 * Writes the property to the specified writer.  The property name is a
	 * reference into the strings table.
	 */
	void write(dtb::output_writer &writer, dtb::string_table &strings);
	/**
	 * Writes in DTS format to the specified file, at the given indent
	 * level.  This will begin the line with the number of tabs specified
	 * as the indent level and then write the property in the most
	 * applicable way that it can determine.
	 */
	void write_dts(FILE *file, int indent);
};

/**
 * Class encapsulating a device tree node.  Nodes may contain properties and
 * other nodes.
 */
class node
{
	public:
	/**
	 * The label for this node, if any.  Node labels are used as the
	 * targets for cross references.
	 */
	string label;
	/**
	 * The name of the node.
	 */
	string name;
	/**
	 * The unit address of the node, which is optionally written after the
	 * name followed by an at symbol.
	 */
	string unit_address;
	/**
	 * The type for the property vector.
	 */
	typedef std::vector<property_ptr> property_vector;
	private:
	/**
	 * The properties contained within this node.
	 */
	property_vector properties;
	/**
	 * The children of this node.
	 */
	std::vector<node_ptr> children;
	/**
	 * A flag indicating whether this node is valid.  This is set to false
	 * if an error occurs during parsing.
	 */
	bool valid;
	/**
	 * Parses a name inside a node, writing the string passed as the last
	 * argument as an error if it fails.  
	 */
	string parse_name(input_buffer &input,
	                  bool &is_property,
	                  const char *error);
	/**
	 * Constructs a new node from two input buffers, pointing to the struct
	 * and strings tables in the device tree blob, respectively.
	 */
	node(input_buffer &structs, input_buffer &strings);
	/**
	 * Parses a new node from the specified input buffer.  This is called
	 * when the input cursor is on the open brace for the start of the
	 * node.  The name, and optionally label and unit address, should have
	 * already been parsed.
	 */
	node(input_buffer &input, string n, string l, string a, define_map*);
	/**
	 * Comparison function for properties, used when sorting the properties
	 * vector.  Orders the properties based on their names.
	 */
	static inline bool cmp_properties(property_ptr &p1, property_ptr &p2);
		/*
	{
		return p1->get_key() < p2->get_key();
	}
	*/
	/**
	 * Comparison function for nodes, used when sorting the children
	 * vector.  Orders the nodes based on their names or, if the names are
	 * the same, by the unit addresses.
	 */
	static inline bool cmp_children(node_ptr &c1, node_ptr &c2);
	public:
	/**
	 * Sorts the node's properties and children into alphabetical order and
	 * recursively sorts the children.
	 */
	void sort();
	/**
	 * Iterator type for child nodes.
	 */
	typedef std::vector<node_ptr>::iterator child_iterator;
	/**
	 * Returns an iterator for the first child of this node.
	 */
	inline child_iterator child_begin()
	{
		return children.begin();
	}
	/**
	 * Returns an iterator after the last child of this node.
	 */
	inline child_iterator child_end()
	{
		return children.end();
	}
	/**
	 * Returns an iterator after the last property of this node.
	 */
	inline property_vector::iterator property_begin()
	{
		return properties.begin();
	}
	/**
	 * Returns an iterator for the first property of this node.
	 */
	inline property_vector::iterator property_end()
	{
		return properties.end();
	}
	/**
	 * Factory method for constructing a new node.  Attempts to parse a
	 * node in DTS format from the input, and returns it on success.  On
	 * any parse error, this will return 0.  This should be called with the
	 * cursor on the open brace of the property, after the name and so on
	 * have been parsed.
	 */
	static node_ptr parse(input_buffer &input,
	                      string name,
	                      string label=string(),
	                      string address=string(),
	                      define_map *defines=0);
	/**
	 * Factory method for constructing a new node.  Attempts to parse a
	 * node in DTB format from the input, and returns it on success.  On
	 * any parse error, this will return 0.  This should be called with the
	 * cursor on the open brace of the property, after the name and so on
	 * have been parsed.
	 */
	static node_ptr parse_dtb(input_buffer &structs, input_buffer &strings);
	/**
	 * Returns a property corresponding to the specified key, or 0 if this
	 * node does not contain a property of that name.
	 */
	property_ptr get_property(string key);
	/**
	 * Adds a new property to this node.
	 */
	inline void add_property(property_ptr &p)
	{
		properties.push_back(p);
	}
	/**
	 * Merges a node into this one.  Any properties present in both are
	 * overridden, any properties present in only one are preserved.
	 */
	void merge_node(node_ptr other);
	/**
	 * Write this node to the specified output.  Although nodes do not
	 * refer to a string table directly, their properties do.  The string
	 * table passed as the second argument is used for the names of
	 * properties within this node and its children.
	 */
	void write(dtb::output_writer &writer, dtb::string_table &strings);
	/**
	 * Writes the current node as DTS to the specified file.  The second
	 * parameter is the indent level.  This function will start every line
	 * with this number of tabs.  
	 */
	void write_dts(FILE *file, int indent);
};

/**
 * Class encapsulating the entire parsed FDT.  This is the top-level class,
 * which parses the entire DTS representation and write out the finished
 * version.
 */
class device_tree
{
	public:
	/**
	 * Type used for node paths.  A node path is sequence of names and unit
	 * addresses.
	 */
	typedef std::vector<std::pair<string,string> > node_path;
	/**
	 * Name that we should use for phandle nodes.
	 */
	enum phandle_format
	{
		/** linux,phandle */
		LINUX,
		/** phandle */
		EPAPR,
		/** Create both nodes. */
		BOTH
	};
	private:
	/**
	 * The format that we should use for writing phandles.
	 */
	phandle_format phandle_node_name;
	/**
	 * Flag indicating that this tree is valid.  This will be set to false
	 * on parse errors. 
	 */
	bool valid;
	/**
	 * Type used for memory reservations.  A reservation is two 64-bit
	 * values indicating a base address and length in memory that the
	 * kernel should not use.  The high 32 bits are ignored on 32-bit
	 * platforms.
	 */
	typedef std::pair<uint64_t, uint64_t> reservation;
	/**
	 * The memory reserves table.
	 */
	std::vector<reservation> reservations;
	/**
	 * Root node.  All other nodes are children of this node.
	 */
	node_ptr root;
	/**
	 * Mapping from names to nodes.  Only unambiguous names are recorded,
	 * duplicate names are stored as (node*)-1.
	 */
	std::unordered_map<string, node*> node_names;
	/**
	 * A map from labels to node paths.  When resolving cross references,
	 * we look up referenced nodes in this and replace the cross reference
	 * with the full path to its target.
	 */
	std::unordered_map<string, node_path> node_paths;
	/**
	 * A collection of property values that are references to other nodes.
	 * These should be expanded to the full path of their targets.
	 */
	std::vector<property_value*> cross_references;
	/**
	 * A collection of property values that refer to phandles.  These will
	 * be replaced by the value of the phandle property in their
	 * destination.
	 */
	std::vector<property_value*> phandles;
	/**
	 * The names of nodes that target phandles.
	 */
	std::unordered_set<string> phandle_targets;
	/**
	 * A collection of input buffers that we are using.  These input
	 * buffers are the ones that own their memory, and so we must preserve
	 * them for the lifetime of the device tree.  
	 */
	std::vector<std::unique_ptr<input_buffer>> buffers;
	/**
	 * A map of used phandle values to nodes.  All phandles must be unique,
	 * so we keep a set of ones that the user explicitly provides in the
	 * input to ensure that we don't reuse them.
	 *
	 * This is a map, rather than a set, because we also want to be able to
	 * find phandles that were provided by the user explicitly when we are
	 * doing checking.
	 */
	std::unordered_map<uint32_t, node*> used_phandles;
	/**
	 * Paths to search for include files.  This contains a set of
	 * nul-terminated strings, which are not owned by this class and so
	 * must be freed separately.
	 */
	std::vector<std::string> include_paths;
	/**
	 * Dictionary of predefined macros provided on the command line.
	 */
	define_map               defines;
	/**
	 * The default boot CPU, specified in the device tree header.
	 */
	uint32_t boot_cpu;
	/**
	 * The number of empty reserve map entries to generate in the blob.
	 */
	uint32_t spare_reserve_map_entries;
	/**
	 * The minimum size in bytes of the blob.
	 */
	uint32_t minimum_blob_size;
	/**
	 * The number of bytes of padding to add to the end of the blob.
	 */
	uint32_t blob_padding;
	/**
	 * Visit all of the nodes recursively, and if they have labels then add
	 * them to the node_paths and node_names vectors so that they can be
	 * used in resolving cross references.  Also collects phandle
	 * properties that have been explicitly added.  
	 */
	void collect_names_recursive(node_ptr &n, node_path &path);
	/**
	 * Assign phandle properties to all nodes that have been referenced and
	 * require one.  This method will recursively visit the tree starting at
	 * the node that it is passed.
	 */
	void assign_phandles(node_ptr &n, uint32_t &next);
	/**
	 * Calls the recursive version of this method on every root node.
	 */
	void collect_names();
	/**
	 * Resolves all cross references.  Any properties that refer to another
	 * node must have their values replaced by either the node path or
	 * phandle value.
	 */
	void resolve_cross_references();
	/**
	 * Parses a dts file in the given buffer and adds the roots to the parsed
	 * set.  The `read_header` argument indicates whether the header has
	 * already been read.  Some dts files place the header in an include,
	 * rather than in the top-level file.
	 */
	void parse_file(input_buffer &input,
	                const std::string &dir,
	                std::vector<node_ptr> &roots,
	                FILE *depfile,
	                bool &read_header);
	/**
	 * Allocates a new mmap()'d input buffer for use in parsing.  This
	 * object then keeps a reference to it, ensuring that it is not
	 * deallocated until the device tree is destroyed.
	 */
	input_buffer *buffer_for_file(const char *path);
	/**
	 * Template function that writes a dtb blob using the specified writer.
	 * The writer defines the output format (assembly, blob).
	 */
	template<class writer>
	void write(int fd);
	public:
	/**
	 * Returns the node referenced by the property.  If this is a tree that
	 * is in source form, then we have a string that we can use to index
	 * the cross_references array and so we can just look that up.  
	 */
	node *referenced_node(property_value &v);
	/**
	 * Writes this FDT as a DTB to the specified output.
	 */
	void write_binary(int fd);
	/**
	 * Writes this FDT as an assembly representation of the DTB to the
	 * specified output.  The result can then be assembled and linked into
	 * a program.
	 */
	void write_asm(int fd);
	/**
	 * Writes the tree in DTS (source) format.
	 */
	void write_dts(int fd);
	/**
	 * Default constructor.  Creates a valid, but empty FDT.
	 */
	device_tree() : phandle_node_name(EPAPR), valid(true),
		boot_cpu(0), spare_reserve_map_entries(0),
		minimum_blob_size(0), blob_padding(0) {}
	/**
	 * Constructs a device tree from the specified file name, referring to
	 * a file that contains a device tree blob.
	 */
	void parse_dtb(const char *fn, FILE *depfile);
	/**
	 * Constructs a device tree from the specified file name, referring to
	 * a file that contains device tree source.
	 */
	void parse_dts(const char *fn, FILE *depfile);
	/**
	 * Returns whether this tree is valid.
	 */
	inline bool is_valid()
	{
		return valid;
	}
	/**
	 * Sets the format for writing phandle properties.
	 */
	inline void set_phandle_format(phandle_format f)
	{
		phandle_node_name = f;
	}
	/**
	 * Returns a pointer to the root node of this tree.  No ownership
	 * transfer.
	 */
	inline const node_ptr &get_root() const
	{
		return root;
	}
	/**
	 * Sets the physical boot CPU.
	 */
	void set_boot_cpu(uint32_t cpu)
	{
		boot_cpu = cpu;
	}
	/**
	 * Sorts the tree.  Useful for debugging device trees.
	 */
	void sort()
	{
		root->sort();
	}
	/**
	 * Adds a path to search for include files.  The argument must be a
	 * nul-terminated string representing the path.  The device tree keeps
	 * a pointer to this string, but does not own it: the caller is
	 * responsible for freeing it if required.
	 */
	void add_include_path(const char *path)
	{
		std::string p(path);
		include_paths.push_back(std::move(p));
	}
	/**
	 * Sets the number of empty reserve map entries to add.
	 */
	void set_empty_reserve_map_entries(uint32_t e)
	{
		spare_reserve_map_entries = e;
	}
	/**
	 * Sets the minimum size, in bytes, of the blob.
	 */
	void set_blob_minimum_size(uint32_t s)
	{
		minimum_blob_size = s;
	}
	/**
	 * Sets the amount of padding to add to the blob.
	 */
	void set_blob_padding(uint32_t p)
	{
		blob_padding = p;
	}
	/**
	 * Parses a predefined macro value.
	 */
	bool parse_define(const char *def);
};

} // namespace fdt

} // namespace dtc

#endif // !_FDT_HH_
