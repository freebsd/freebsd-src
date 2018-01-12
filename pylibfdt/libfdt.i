/*
 * pylibfdt - Flat Device Tree manipulation in Python
 * Copyright (C) 2017 Google, Inc.
 * Written by Simon Glass <sjg@chromium.org>
 *
 * libfdt is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 *
 *  a) This library is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of the
 *     License, or (at your option) any later version.
 *
 *     This library is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this library; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 *     MA 02110-1301 USA
 *
 * Alternatively,
 *
 *  b) Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

%module libfdt

%include <stdint.i>

%{
#define SWIG_FILE_WITH_INIT
#include "libfdt.h"
%}

%pythoncode %{

import struct

# Error codes, corresponding to FDT_ERR_... in libfdt.h
(NOTFOUND,
        EXISTS,
        NOSPACE,
        BADOFFSET,
        BADPATH,
        BADPHANDLE,
        BADSTATE,
        TRUNCATED,
        BADMAGIC,
        BADVERSION,
        BADSTRUCTURE,
        BADLAYOUT,
        INTERNAL,
        BADNCELLS,
        BADVALUE,
        BADOVERLAY,
        NOPHANDLES) = QUIET_ALL = range(1, 18)
# QUIET_ALL can be passed as the 'quiet' parameter to avoid exceptions
# altogether. All # functions passed this value will return an error instead
# of raising an exception.

# Pass this as the 'quiet' parameter to return -ENOTFOUND on NOTFOUND errors,
# instead of raising an exception.
QUIET_NOTFOUND = (NOTFOUND,)


class FdtException(Exception):
    """An exception caused by an error such as one of the codes above"""
    def __init__(self, err):
        self.err = err

    def __str__(self):
        return 'pylibfdt error %d: %s' % (self.err, fdt_strerror(self.err))

def strerror(fdt_err):
    """Get the string for an error number

    Args:
        fdt_err: Error number (-ve)

    Returns:
        String containing the associated error
    """
    return fdt_strerror(fdt_err)

def check_err(val, quiet=()):
    """Raise an error if the return value is -ve

    This is used to check for errors returned by libfdt C functions.

    Args:
        val: Return value from a libfdt function
        quiet: Errors to ignore (empty to raise on all errors)

    Returns:
        val if val >= 0

    Raises
        FdtException if val < 0
    """
    if val < 0:
        if -val not in quiet:
            raise FdtException(val)
    return val

def check_err_null(val, quiet=()):
    """Raise an error if the return value is NULL

    This is used to check for a NULL return value from certain libfdt C
    functions

    Args:
        val: Return value from a libfdt function
        quiet: Errors to ignore (empty to raise on all errors)

    Returns:
        val if val is a list, None if not

    Raises
        FdtException if val indicates an error was reported and the error
        is not in @quiet.
    """
    # Normally a list is returned which contains the data and its length.
    # If we get just an integer error code, it means the function failed.
    if not isinstance(val, list):
        if -val not in quiet:
            raise FdtException(val)
    return val

class Fdt:
    """Device tree class, supporting all operations

    The Fdt object is created is created from a device tree binary file,
    e.g. with something like:

       fdt = Fdt(open("filename.dtb").read())

    Operations can then be performed using the methods in this class. Each
    method xxx(args...) corresponds to a libfdt function fdt_xxx(fdt, args...).

    All methods raise an FdtException if an error occurs. To avoid this
    behaviour a 'quiet' parameter is provided for some functions. This
    defaults to empty, but you can pass a list of errors that you expect.
    If one of these errors occurs, the function will return an error number
    (e.g. -NOTFOUND).
    """
    def __init__(self, data):
        self._fdt = bytearray(data)
        check_err(fdt_check_header(self._fdt));

    def subnode_offset(self, parentoffset, name, quiet=()):
        """Get the offset of a named subnode

        Args:
            parentoffset: Offset of the parent node to check
            name: Name of the required subnode, e.g. 'subnode@1'
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            The node offset of the found node, if any

        Raises
            FdtException if there is no node with that name, or other error
        """
        return check_err(fdt_subnode_offset(self._fdt, parentoffset, name),
                         quiet)

    def path_offset(self, path, quiet=()):
        """Get the offset for a given path

        Args:
            path: Path to the required node, e.g. '/node@3/subnode@1'
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            Node offset

        Raises
            FdtException if the path is not valid or not found
        """
        return check_err(fdt_path_offset(self._fdt, path), quiet)

    def first_property_offset(self, nodeoffset, quiet=()):
        """Get the offset of the first property in a node offset

        Args:
            nodeoffset: Offset to the node to check
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            Offset of the first property

        Raises
            FdtException if the associated node has no properties, or some
                other error occurred
        """
        return check_err(fdt_first_property_offset(self._fdt, nodeoffset),
                         quiet)

    def next_property_offset(self, prop_offset, quiet=()):
        """Get the next property in a node

        Args:
            prop_offset: Offset of the previous property
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            Offset of the next property

        Raises:
            FdtException if the associated node has no more properties, or
                some other error occurred
        """
        return check_err(fdt_next_property_offset(self._fdt, prop_offset),
                         quiet)

    def get_name(self, nodeoffset):
        """Get the name of a node

        Args:
            nodeoffset: Offset of node to check

        Returns:
            Node name

        Raises:
            FdtException on error (e.g. nodeoffset is invalid)
        """
        return check_err_null(fdt_get_name(self._fdt, nodeoffset))[0]

    def get_property_by_offset(self, prop_offset, quiet=()):
        """Obtains a property that can be examined

        Args:
            prop_offset: Offset of property (e.g. from first_property_offset())
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            Property object, or None if not found

        Raises:
            FdtException on error (e.g. invalid prop_offset or device
            tree format)
        """
        pdata = check_err_null(
                fdt_get_property_by_offset(self._fdt, prop_offset), quiet)
        if isinstance(pdata, (int)):
            return pdata
        return Property(pdata[0], pdata[1])

    def first_subnode(self, nodeoffset, quiet=()):
        """Find the first subnode of a parent node

        Args:
            nodeoffset: Node offset of parent node
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            The offset of the first subnode, if any

        Raises:
            FdtException if no subnode found or other error occurs
        """
        return check_err(fdt_first_subnode(self._fdt, nodeoffset), quiet)

    def next_subnode(self, nodeoffset, quiet=()):
        """Find the next subnode

        Args:
            nodeoffset: Node offset of previous subnode
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            The offset of the next subnode, if any

        Raises:
            FdtException if no more subnode found or other error occurs
        """
        return check_err(fdt_next_subnode(self._fdt, nodeoffset), quiet)

    def totalsize(self):
        """Return the total size of the device tree

        Returns:
            Total tree size in bytes
        """
        return check_err(fdt_totalsize(self._fdt))

    def off_dt_struct(self):
        """Return the start of the device tree struct area

        Returns:
            Start offset of struct area
        """
        return check_err(fdt_off_dt_struct(self._fdt))

    def pack(self, quiet=()):
        """Pack the device tree to remove unused space

        This adjusts the tree in place.

        Args:
            quiet: Errors to ignore (empty to raise on all errors)

        Raises:
            FdtException if any error occurs
        """
        return check_err(fdt_pack(self._fdt), quiet)

    def delprop(self, nodeoffset, prop_name):
        """Delete a property from a node

        Args:
            nodeoffset: Node offset containing property to delete
            prop_name: Name of property to delete

        Raises:
            FdtError if the property does not exist, or another error occurs
        """
        return check_err(fdt_delprop(self._fdt, nodeoffset, prop_name))

    def getprop(self, nodeoffset, prop_name, quiet=()):
        """Get a property from a node

        Args:
            nodeoffset: Node offset containing property to get
            prop_name: Name of property to get
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            Value of property as a bytearray, or -ve error number

        Raises:
            FdtError if any error occurs (e.g. the property is not found)
        """
        pdata = check_err_null(fdt_getprop(self._fdt, nodeoffset, prop_name),
                               quiet)
        if isinstance(pdata, (int)):
            return pdata
        return bytearray(pdata[0])

    def get_phandle(self, nodeoffset):
        """Get the phandle of a node

        Args:
            nodeoffset: Node offset to check

        Returns:
            phandle of node, or 0 if the node has no phandle or another error
            occurs
        """
        return fdt_get_phandle(self._fdt, nodeoffset)

    def parent_offset(self, nodeoffset, quiet=()):
        """Get the offset of a node's parent

        Args:
            nodeoffset: Node offset to check
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            The offset of the parent node, if any

        Raises:
            FdtException if no parent found or other error occurs
        """
        return check_err(fdt_parent_offset(self._fdt, nodeoffset), quiet)

    def node_offset_by_phandle(self, phandle, quiet=()):
        """Get the offset of a node with the given phandle

        Args:
            phandle: Phandle to search for
            quiet: Errors to ignore (empty to raise on all errors)

        Returns:
            The offset of node with that phandle, if any

        Raises:
            FdtException if no node found or other error occurs
        """
        return check_err(fdt_node_offset_by_phandle(self._fdt, phandle), quiet)

class Property:
    """Holds a device tree property name and value.

    This holds a copy of a property taken from the device tree. It does not
    reference the device tree, so if anything changes in the device tree,
    a Property object will remain valid.

    Properties:
        name: Property name
        value: Proper value as a bytearray
    """
    def __init__(self, name, value):
        self.name = name
        self.value = value
%}

%rename(fdt_property) fdt_property_func;

typedef int fdt32_t;

%include "libfdt/fdt.h"

%include "typemaps.i"

/* Most functions don't change the device tree, so use a const void * */
%typemap(in) (const void *)(const void *fdt) {
	if (!PyByteArray_Check($input)) {
		SWIG_exception_fail(SWIG_TypeError, "in method '" "$symname"
			"', argument " "$argnum"" of type '" "$type""'");
	}
	$1 = (void *)PyByteArray_AsString($input);
        fdt = $1;
        fdt = fdt; /* avoid unused variable warning */
}

/* Some functions do change the device tree, so use void * */
%typemap(in) (void *)(const void *fdt) {
	if (!PyByteArray_Check($input)) {
		SWIG_exception_fail(SWIG_TypeError, "in method '" "$symname"
			"', argument " "$argnum"" of type '" "$type""'");
	}
	$1 = PyByteArray_AsString($input);
        fdt = $1;
        fdt = fdt; /* avoid unused variable warning */
}

%typemap(out) (struct fdt_property *) {
	PyObject *buff;

	if ($1) {
		resultobj = PyString_FromString(
			fdt_string(fdt1, fdt32_to_cpu($1->nameoff)));
		buff = PyByteArray_FromStringAndSize(
			(const char *)($1 + 1), fdt32_to_cpu($1->len));
		resultobj = SWIG_Python_AppendOutput(resultobj, buff);
	}
}

%apply int *OUTPUT { int *lenp };

/* typemap used for fdt_getprop() */
%typemap(out) (const void *) {
	if (!$1)
		$result = Py_None;
	else
		$result = Py_BuildValue("s#", $1, *arg4);
}

/* We have both struct fdt_property and a function fdt_property() */
%warnfilter(302) fdt_property;

/* These are macros in the header so have to be redefined here */
int fdt_magic(const void *fdt);
int fdt_totalsize(const void *fdt);
int fdt_off_dt_struct(const void *fdt);
int fdt_off_dt_strings(const void *fdt);
int fdt_off_mem_rsvmap(const void *fdt);
int fdt_version(const void *fdt);
int fdt_last_comp_version(const void *fdt);
int fdt_boot_cpuid_phys(const void *fdt);
int fdt_size_dt_strings(const void *fdt);
int fdt_size_dt_struct(const void *fdt);

%include <../libfdt/libfdt.h>
