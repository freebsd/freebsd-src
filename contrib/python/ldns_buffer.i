/******************************************************************************
 * ldns_buffer.i: LDNS buffer class
 *
 * Copyright (c) 2009, Zdenek Vasicek (vasicek AT fit.vutbr.cz)
 *                     Karel Slany    (slany AT fit.vutbr.cz)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the organization nor the names of its
 *       contributors may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

%typemap(in,numinputs=0,noblock=1) (ldns_buffer **)
{
 ldns_buffer *$1_buf;
 $1 = &$1_buf;
}
          
/* result generation */
%typemap(argout,noblock=1) (ldns_buffer **)
{
  $result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr($1_buf), SWIGTYPE_p_ldns_struct_buffer, SWIG_POINTER_OWN |  0 ));
}

%nodefaultctor ldns_struct_buffer; //no default constructor & destructor
%nodefaultdtor ldns_struct_buffer;

%delobject ldns_buffer_free;
%newobject ldns_buffer_new;
%newobject ldns_dname_new;
%newobject ldns_dname_new_frm_data;
%newobject ldns_dname_label;

# limit the number of arguments to 2 and
# deal with variable number of arguments the Python way
%varargs(2, char *arg = NULL) ldns_buffer_printf;

%rename(ldns_buffer) ldns_struct_buffer;

#ifdef LDNS_DEBUG
%rename(__ldns_buffer_free) ldns_buffer_free;
%inline %{
void _ldns_buffer_free (ldns_buffer* b) {
   printf("******** LDNS_BUFFER free 0x%lX ************\n", (long unsigned int)b);
   ldns_buffer_free(b);
}
%}
#else
%rename(_ldns_buffer_free) ldns_buffer_free;
#endif

%ignore ldns_struct_buffer::_position;
%ignore ldns_struct_buffer::_limit;
%ignore ldns_struct_buffer::_capacity;
%ignore ldns_struct_buffer::_data;
%ignore ldns_struct_buffer::_fixed;
%ignore ldns_struct_buffer::_status;

%extend ldns_struct_buffer {
 
 %pythoncode %{
        def __init__(self, capacity):
            """Creates a new buffer with the specified capacity.

              :param capacity: the size (in bytes) to allocate for the buffer
            """
            self.this = _ldns.ldns_buffer_new(capacity)
       
        __swig_destroy__ = _ldns._ldns_buffer_free

        def __str__(self):
            """Returns the data in the buffer as a string. Buffer data must be char * type."""
            return _ldns.ldns_buffer2str(self)

        def getc(self):
            """returns the next character from a buffer.
               
               Advances the position pointer with 1. When end of buffer is reached returns EOF. This is the buffer's equivalent for getc().
               
               :returns: (int) EOF on failure otherwise return the character
            """
            return _ldns.ldns_bgetc(self)

        #LDNS_BUFFER_METHODS_#
        def at(self,at):
            """returns a pointer to the data at the indicated position.
               
               :param at:
                   position
               :returns: (uint8_t \*) the pointer to the data
            """
            return _ldns.ldns_buffer_at(self,at)
            #parameters: const ldns_buffer *,size_t,
            #retvals: uint8_t *

        def available(self,count):
            """checks if the buffer has count bytes available at the current position
               
               :param count:
                   how much is available
               :returns: (int) true or false
            """
            return _ldns.ldns_buffer_available(self,count)
            #parameters: ldns_buffer *,size_t,
            #retvals: int

        def available_at(self,at,count):
            """checks if the buffer has at least COUNT more bytes available.
               
               Before reading or writing the caller needs to ensure enough space is available!
               
               :param at:
                   indicated position
               :param count:
                   how much is available
               :returns: (int) true or false
            """
            return _ldns.ldns_buffer_available_at(self,at,count)
            #parameters: ldns_buffer *,size_t,size_t,
            #retvals: int

        def begin(self):
            """returns a pointer to the beginning of the buffer (the data at position 0).
               
               :returns: (uint8_t \*) the pointer
            """
            return _ldns.ldns_buffer_begin(self)
            #parameters: const ldns_buffer *,
            #retvals: uint8_t *

        def capacity(self):
            """returns the number of bytes the buffer can hold.
               
               :returns: (size_t) the number of bytes
            """
            return _ldns.ldns_buffer_capacity(self)
            #parameters: ldns_buffer *,
            #retvals: size_t

        def clear(self):
            """clears the buffer and make it ready for writing.
               
               The buffer's limit is set to the capacity and the position is set to 0.
            """
            _ldns.ldns_buffer_clear(self)
            #parameters: ldns_buffer *,
            #retvals: 

        def copy(self,bfrom):
            """Copy contents of the other buffer to this buffer.
               
               Silently truncated if this buffer is too small.
               
               :param bfrom: other buffer
            """
            _ldns.ldns_buffer_copy(self,bfrom)
            #parameters: ldns_buffer *,ldns_buffer *,
            #retvals: 

        def current(self):
            """returns a pointer to the data at the buffer's current position.
               
               :returns: (uint8_t \*) the pointer
            """
            return _ldns.ldns_buffer_current(self)
            #parameters: ldns_buffer *,
            #retvals: uint8_t *

        def end(self):
            """returns a pointer to the end of the buffer (the data at the buffer's limit).
               
               :returns: (uint8_t \*) the pointer
            """
            return _ldns.ldns_buffer_end(self)
            #parameters: ldns_buffer *,
            #retvals: uint8_t *

        def export(self):
            """Makes the buffer fixed and returns a pointer to the data.
               
               The caller is responsible for free'ing the result.
               
               :returns: (void \*) void
            """
            return _ldns.ldns_buffer_export(self)
            #parameters: ldns_buffer *,
            #retvals: void *

        def flip(self):
            """makes the buffer ready for reading the data that has been written to the buffer.
               
               The buffer's limit is set to the current position and the position is set to 0.
            """
            _ldns.ldns_buffer_flip(self)
            #parameters: ldns_buffer *,

        def invariant(self):
            _ldns.ldns_buffer_invariant(self)
            #parameters: ldns_buffer *,

        def limit(self):
            """returns the maximum size of the buffer
               
               :returns: (size_t) the size
            """
            return _ldns.ldns_buffer_limit(self)
            #parameters: ldns_buffer *,
            #retvals: size_t

        def position(self):
            """returns the current position in the buffer (as a number of bytes)
               
               :returns: (size_t) the current position
            """
            return _ldns.ldns_buffer_position(self)
            #parameters: ldns_buffer *,
            #retvals: size_t

        def printf(self, str, *args):
            """Prints to the buffer, increasing the capacity if required using buffer_reserve().
               
               The buffer's position is set to the terminating '\0'. Returns the number of characters written (not including the terminating '\0') or -1 on failure.
               :param str: a string
               :returns: (int) 
            """
            data = str % args
            return _ldns.ldns_buffer_printf(self,data)
            #parameters: ldns_buffer *,const char *,...
            #retvals: int

        def read(self,data,count):
            """copies count bytes of data at the current position to the given data-array
               
               :param data:
                   buffer to copy to
               :param count:
                   the length of the data to copy
            """
            _ldns.ldns_buffer_read(self,data,count)
            #parameters: ldns_buffer *,void *,size_t,
            #retvals: 

        def read_at(self,at,data,count):
            """copies count bytes of data at the given position to the given data-array
               
               :param at:
                   the position in the buffer to start
               :param data:
                   buffer to copy to
               :param count:
                   the length of the data to copy
            """
            _ldns.ldns_buffer_read_at(self,at,data,count)
            #parameters: ldns_buffer *,size_t,void *,size_t,
            #retvals: 

        def read_u16(self):
            """returns the 2-byte integer value at the current position in the buffer
               
               :returns: (uint16_t) 2 byte integer
            """
            return _ldns.ldns_buffer_read_u16(self)
            #parameters: ldns_buffer *,
            #retvals: uint16_t

        def read_u16_at(self,at):
            """returns the 2-byte integer value at the given position in the buffer
               
               :param at:
                   position in the buffer
               :returns: (uint16_t) 2 byte integer
            """
            return _ldns.ldns_buffer_read_u16_at(self,at)
            #parameters: ldns_buffer *,size_t,
            #retvals: uint16_t

        def read_u32(self):
            """returns the 4-byte integer value at the current position in the buffer
               
               :returns: (uint32_t) 4 byte integer
            """
            return _ldns.ldns_buffer_read_u32(self)
            #parameters: ldns_buffer *,
            #retvals: uint32_t

        def read_u32_at(self,at):
            """returns the 4-byte integer value at the given position in the buffer
               
               :param at:
                   position in the buffer
               :returns: (uint32_t) 4 byte integer
            """
            return _ldns.ldns_buffer_read_u32_at(self,at)
            #parameters: ldns_buffer *,size_t,
            #retvals: uint32_t

        def read_u8(self):
            """returns the byte value at the current position in the buffer
               
               :returns: (uint8_t) 1 byte integer
            """
            return _ldns.ldns_buffer_read_u8(self)
            #parameters: ldns_buffer *,
            #retvals: uint8_t

        def read_u8_at(self,at):
            """returns the byte value at the given position in the buffer
               
               :param at:
                   the position in the buffer
               :returns: (uint8_t) 1 byte integer
            """
            return _ldns.ldns_buffer_read_u8_at(self,at)
            #parameters: ldns_buffer *,size_t,
            #retvals: uint8_t

        def remaining(self):
            """returns the number of bytes remaining between the buffer's position and limit.
               
               :returns: (size_t) the number of bytes
            """
            return _ldns.ldns_buffer_remaining(self)
            #parameters: ldns_buffer *,
            #retvals: size_t

        def remaining_at(self,at):
            """returns the number of bytes remaining between the indicated position and the limit.
               
               :param at:
                   indicated position
               :returns: (size_t) number of bytes
            """
            return _ldns.ldns_buffer_remaining_at(self,at)
            #parameters: ldns_buffer *,size_t,
            #retvals: size_t

        def reserve(self,amount):
            """ensures BUFFER can contain at least AMOUNT more bytes.
               
               The buffer's capacity is increased if necessary using buffer_set_capacity().
               
               The buffer's limit is always set to the (possibly increased) capacity.
               
               :param amount:
                   amount to use
               :returns: (bool) whether this failed or succeeded
            """
            return _ldns.ldns_buffer_reserve(self,amount)
            #parameters: ldns_buffer *,size_t,
            #retvals: bool

        def rewind(self):
            """make the buffer ready for re-reading the data.
               
               The buffer's position is reset to 0.
            """
            _ldns.ldns_buffer_rewind(self)
            #parameters: ldns_buffer *,
            #retvals: 

        def set_capacity(self,capacity):
            """changes the buffer's capacity.
               
               The data is reallocated so any pointers to the data may become invalid. The buffer's limit is set to the buffer's new capacity.
               
               :param capacity:
                   the capacity to use
               :returns: (bool) whether this failed or succeeded
            """
            return _ldns.ldns_buffer_set_capacity(self,capacity)
            #parameters: ldns_buffer *,size_t,
            #retvals: bool

        def set_limit(self,limit):
            """changes the buffer's limit.
               
               If the buffer's position is greater than the new limit the position is set to the limit.
               
               :param limit:
                   the new limit
            """
            _ldns.ldns_buffer_set_limit(self,limit)
            #parameters: ldns_buffer *,size_t,
            #retvals: 

        def set_position(self,mark):
            """sets the buffer's position to MARK.
               
               The position must be less than or equal to the buffer's limit.
               
               :param mark:
                   the mark to use
            """
            _ldns.ldns_buffer_set_position(self,mark)
            #parameters: ldns_buffer *,size_t,
            #retvals: 

        def skip(self,count):
            """changes the buffer's position by COUNT bytes.
               
               The position must not be moved behind the buffer's limit or before the beginning of the buffer.
               
               :param count:
                   the count to use
            """
            _ldns.ldns_buffer_skip(self,count)
            #parameters: ldns_buffer *,ssize_t,
            #retvals: 

        def status(self):
            """returns the status of the buffer
               
               :returns: (ldns_status) the status
            """
            return _ldns.ldns_buffer_status(self)
            #parameters: ldns_buffer *,
            #retvals: ldns_status

        def status_ok(self):
            """returns true if the status of the buffer is LDNS_STATUS_OK, false otherwise
               
               :returns: (bool) true or false
            """
            return _ldns.ldns_buffer_status_ok(self)
            #parameters: ldns_buffer *,
            #retvals: bool

        def write(self,data,count):
            """writes count bytes of data to the current position of the buffer
               
               :param data:
                   the data to write
               :param count:
                   the lenght of the data to write
            """
            _ldns.ldns_buffer_write(self,data,count)
            #parameters: ldns_buffer *,const void *,size_t,
            #retvals: 

        def write_at(self,at,data,count):
            """writes the given data to the buffer at the specified position
               
               :param at:
                   the position (in number of bytes) to write the data at
               :param data:
                   pointer to the data to write to the buffer
               :param count:
                   the number of bytes of data to write
            """
            _ldns.ldns_buffer_write_at(self,at,data,count)
            #parameters: ldns_buffer *,size_t,const void *,size_t,
            #retvals: 

        def write_string(self,str):
            """copies the given (null-delimited) string to the current position at the buffer
               
               :param str:
                   the string to write
            """
            _ldns.ldns_buffer_write_string(self,str)
            #parameters: ldns_buffer *,const char *,
            #retvals: 

        def write_string_at(self,at,str):
            """copies the given (null-delimited) string to the specified position at the buffer
               
               :param at:
                   the position in the buffer
               :param str:
                   the string to write
            """
            _ldns.ldns_buffer_write_string_at(self,at,str)
            #parameters: ldns_buffer *,size_t,const char *,
            #retvals: 

        def write_u16(self,data):
            """writes the given 2 byte integer at the current position in the buffer
               
               :param data:
                   the 16 bits to write
            """
            _ldns.ldns_buffer_write_u16(self,data)
            #parameters: ldns_buffer *,uint16_t,
            #retvals: 

        def write_u16_at(self,at,data):
            """writes the given 2 byte integer at the given position in the buffer
               
               :param at:
                   the position in the buffer
               :param data:
                   the 16 bits to write
            """
            _ldns.ldns_buffer_write_u16_at(self,at,data)
            #parameters: ldns_buffer *,size_t,uint16_t,
            #retvals: 

        def write_u32(self,data):
            """writes the given 4 byte integer at the current position in the buffer
               
               :param data:
                   the 32 bits to write
            """
            _ldns.ldns_buffer_write_u32(self,data)
            #parameters: ldns_buffer *,uint32_t,
            #retvals: 

        def write_u32_at(self,at,data):
            """writes the given 4 byte integer at the given position in the buffer
               
               :param at:
                   the position in the buffer
               :param data:
                   the 32 bits to write
            """
            _ldns.ldns_buffer_write_u32_at(self,at,data)
            #parameters: ldns_buffer *,size_t,uint32_t,
            #retvals: 

        def write_u8(self,data):
            """writes the given byte of data at the current position in the buffer
               
               :param data:
                   the 8 bits to write
            """
            _ldns.ldns_buffer_write_u8(self,data)
            #parameters: ldns_buffer *,uint8_t,
            #retvals: 

        def write_u8_at(self,at,data):
            """writes the given byte of data at the given position in the buffer
               
               :param at:
                   the position in the buffer
               :param data:
                   the 8 bits to write
            """
            _ldns.ldns_buffer_write_u8_at(self,at,data)
            #parameters: ldns_buffer *,size_t,uint8_t,
            #retvals: 

        #_LDNS_BUFFER_METHODS#
 %}
}

