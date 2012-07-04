/******************************************************************************
 * ldns_rdata.i: LDNS record data
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

//automatic conversion of const ldns_rdf* parameter from string 
%typemap(in,noblock=1) const ldns_rdf * (void* argp, $1_ltype tmp = 0, int res) {
   if (Python_str_Check($input)) {
#ifdef SWIG_Python_str_AsChar
      tmp = ldns_dname_new_frm_str(SWIG_Python_str_AsChar($input));
#else
      tmp = ldns_dname_new_frm_str(PyString_AsString($input));
#endif
      if (tmp == NULL) {
         %argument_fail(SWIG_TypeError, "char *", $symname, $argnum);
      }
      $1 = ($1_ltype) tmp;
   } else {
      res = SWIG_ConvertPtr($input, &argp, SWIGTYPE_p_ldns_struct_rdf, 0 |  0 );
      if (!SWIG_IsOK(res)) {
         %argument_fail(res, "ldns_rdf const *", $symname, $argnum);
      }
      $1 = ($1_ltype) argp;
   }
}

%typemap(in,numinputs=0,noblock=1) (ldns_rdf **)
{
 ldns_rdf *$1_rdf;
 $1 = &$1_rdf;
}
          
// result generation 
%typemap(argout,noblock=1) (ldns_rdf **)
{
  $result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr($1_rdf), SWIGTYPE_p_ldns_struct_rdf, SWIG_POINTER_OWN |  0 ));
}

%nodefaultctor ldns_struct_rdf; //no default constructor & destructor
%nodefaultdtor ldns_struct_rdf;

%newobject ldns_dname_new;
%newobject ldns_dname_new_frm_str;
%newobject ldns_dname_new_frm_data;

%newobject ldns_rdf_new;
%newobject ldns_rdf_new_frm_str;
%newobject ldns_rdf_new_frm_data;

%delobject ldns_rdf_deep_free;
%delobject ldns_rdf_free;

%rename(ldns_rdf) ldns_struct_rdf;

%inline %{

const char *ldns_rdf_type2str(const ldns_rdf *rdf)
{
	if (rdf) {
		switch(ldns_rdf_get_type(rdf)) {
		case LDNS_RDF_TYPE_NONE:     return 0;
		case LDNS_RDF_TYPE_DNAME:    return "DNAME";
		case LDNS_RDF_TYPE_INT8:     return "INT8";
		case LDNS_RDF_TYPE_INT16:    return "INT16";
		case LDNS_RDF_TYPE_INT32:    return "INT32";
		case LDNS_RDF_TYPE_PERIOD:   return "PERIOD";
		case LDNS_RDF_TYPE_TSIGTIME: return "TSIGTIME";
		case LDNS_RDF_TYPE_A:        return "A";
		case LDNS_RDF_TYPE_AAAA:     return "AAAA";
		case LDNS_RDF_TYPE_STR:      return "STR";
		case LDNS_RDF_TYPE_APL:      return "APL";
		case LDNS_RDF_TYPE_B32_EXT:  return "B32_EXT";
		case LDNS_RDF_TYPE_B64:      return "B64";
		case LDNS_RDF_TYPE_HEX:      return "HEX";
		case LDNS_RDF_TYPE_NSEC:     return "NSEC";
		case LDNS_RDF_TYPE_NSEC3_SALT: return "NSEC3_SALT";
		case LDNS_RDF_TYPE_TYPE:     return "TYPE";
		case LDNS_RDF_TYPE_CLASS:    return "CLASS";
		case LDNS_RDF_TYPE_CERT_ALG: return "CER_ALG";
		case LDNS_RDF_TYPE_ALG:      return "ALG";
		case LDNS_RDF_TYPE_UNKNOWN:  return "UNKNOWN";
		case LDNS_RDF_TYPE_TIME:     return "TIME";
		case LDNS_RDF_TYPE_LOC:      return "LOC";
		case LDNS_RDF_TYPE_WKS:      return "WKS";
		case LDNS_RDF_TYPE_SERVICE:  return "SERVICE";
		case LDNS_RDF_TYPE_NSAP:     return "NSAP";
		case LDNS_RDF_TYPE_ATMA:     return "ATMA";
		case LDNS_RDF_TYPE_IPSECKEY: return "IPSECKEY";
		case LDNS_RDF_TYPE_TSIG:     return "TSIG";
		case LDNS_RDF_TYPE_INT16_DATA: return "INT16_DATA";
		case LDNS_RDF_TYPE_NSEC3_NEXT_OWNER: return "NSEC3_NEXT_OWNER";
		}
	}
	return 0;
}
%}

#ifdef LDNS_DEBUG
%rename(__ldns_rdf_deep_free) ldns_rdf_deep_free;
%rename(__ldns_rdf_free) ldns_rdf_free;
%inline %{
void _ldns_rdf_free (ldns_rdf* r) {
   printf("******** LDNS_RDF free 0x%lX ************\n", (long unsigned int)r);
   ldns_rdf_free(r);
}
%}
#else
%rename(_ldns_rdf_deep_free) ldns_rdf_deep_free;
%rename(_ldns_rdf_free) ldns_rdf_free;
#endif

%newobject ldns_rdf2str;


%feature("docstring") ldns_struct_rdf "Resource record data field.

The data is a network ordered array of bytes, which size is specified by the (16-bit) size field. To correctly parse it, use the type specified in the (16-bit) type field with a value from ldns_rdf_type."

%extend ldns_struct_rdf {
 
 %pythoncode %{
        def __init__(self):
            raise Exception("This class can't be created directly. Please use: ldns_rdf_new, ldns_rdf_new_frm_data, ldns_rdf_new_frm_str, ldns_rdf_new_frm_fp, ldns_rdf_new_frm_fp_l")
       
        __swig_destroy__ = _ldns._ldns_rdf_deep_free

        #LDNS_RDF_CONSTRUCTORS_#
        @staticmethod
        def new_frm_str(str, rr_type, raiseException = True):
            """Creates a new rdf from a string of a given type.
               
               :param str: string to use
               :param rr_type: the type of RDF. See predefined `RDF_TYPE_` constants
               :param raiseException: if True, an exception occurs in case a RDF object can't be created
               :returns: RDF object or None. If the object can't be created and raiseException is True, an exception occurs.

               **Usage**
                  >>> rdf = ldns.ldns_rdf.new_frm_str("74.125.43.99",ldns.LDNS_RDF_TYPE_A)
                  >>> print rdf, rdf.get_type_str()
                  A 74.125.43.99
                  >>> name = ldns.ldns_resolver.new_frm_file().get_name_by_addr(rdf)
                  >>> if (name): print name
                  99.43.125.74.in-addr.arpa.	85277	IN	PTR	bw-in-f99.google.com.
            """
            rr = _ldns.ldns_rdf_new_frm_str(rr_type, str)
            if not rr:
                if (raiseException): raise Exception("Can't create query packet, error: %d" % status)
            return rr
        #_LDNS_RDF_CONSTRUCTORS#

        def __str__(self):
            """Converts the rdata field to presentation format"""
            return _ldns.ldns_rdf2str(self)

        def __cmp__(self,other):
            """compares two rdf's on their wire formats.
               
               (To order dnames according to rfc4034, use ldns_dname_compare)
               
               :param other:
                   the second one RDF
               :returns: (int) 0 if equal -1 if self comes before other +1 if other comes before self
            """
            return _ldns.ldns_rdf_compare(self,other)
            
        def print_to_file(self,output):
            """Prints the data in the rdata field to the given file stream (in presentation format)."""
            _ldns.ldns_rdf_print(output,self)

        def get_type_str(self):
            """Converts type to string"""
            return ldns_rdf_type2str(self)

        def write_to_buffer(self, buffer):
            """Copies the rdata data to the buffer in wire format.
               
               :param buffer: buffer to append the result to
               :returns: (ldns_status) ldns_status
            """
            return _ldns.ldns_rdf2buffer_wire(buffer, self)
            #parameters: ldns_buffer *,const ldns_rdf *,
            #retvals: ldns_status

        def write_to_buffer_canonical(self, buffer):
            """Copies the rdata data to the buffer in wire format If the rdata is a dname, the letters will be lowercased during the conversion.
               
               :param buffer: LDNS buffer
               :returns: (ldns_status) ldns_status
            """
            return _ldns.ldns_rdf2buffer_wire_canonical(buffer, self)
            #parameters: ldns_buffer *,const ldns_rdf *,
            #retvals: ldns_status

            #LDNS_RDF_METHODS_#
        def address_reverse(self):
            """reverses an rdf, only actually useful for AAAA and A records.
               
               The returned rdf has the type LDNS_RDF_TYPE_DNAME!
               
               :returns: (ldns_rdf \*) the reversed rdf (a newly created rdf)
            """
            return _ldns.ldns_rdf_address_reverse(self)
            #parameters: ldns_rdf *,
            #retvals: ldns_rdf *

        def clone(self):
            """clones a rdf structure.
               
               The data is copied.
               
               :returns: (ldns_rdf \*) a new rdf structure
            """
            return _ldns.ldns_rdf_clone(self)
            #parameters: const ldns_rdf *,
            #retvals: ldns_rdf *

        def data(self):
            """returns the data of the rdf.
               
               :returns: (uint8_t \*) uint8_t* pointer to the rdf's data
            """
            return _ldns.ldns_rdf_data(self)
            #parameters: const ldns_rdf *,
            #retvals: uint8_t *

        def get_type(self):
            """returns the type of the rdf.
               
               We need to insert _get_ here to prevent conflict the the rdf_type TYPE.
               
               :returns: (ldns_rdf_type) ldns_rdf_type with the type
            """
            return _ldns.ldns_rdf_get_type(self)
            #parameters: const ldns_rdf *,
            #retvals: ldns_rdf_type

        def set_data(self,data):
            """sets the size of the rdf.
               
               :param data:
            """
            _ldns.ldns_rdf_set_data(self,data)
            #parameters: ldns_rdf *,void *,
            #retvals: 

        def set_size(self,size):
            """sets the size of the rdf.
               
               :param size:
                   the new size
            """
            _ldns.ldns_rdf_set_size(self,size)
            #parameters: ldns_rdf *,size_t,
            #retvals: 

        def set_type(self,atype):
            """sets the size of the rdf.
               
               :param atype:
            """
            _ldns.ldns_rdf_set_type(self,atype)
            #parameters: ldns_rdf *,ldns_rdf_type,
            #retvals: 

        def size(self):
            """returns the size of the rdf.
               
               :returns: (size_t) uint16_t with the size
            """
            return _ldns.ldns_rdf_size(self)
            #parameters: const ldns_rdf *,
            #retvals: size_t

        @staticmethod
        def dname_new_frm_str(str):
            """Creates a new dname rdf instance from a string.
            
               This static method is equivalent to using of default class constructor.
 
               :parameter str: str string to use
            """
            return _ldns.ldns_dname_new_frm_str(str)

        def absolute(self):
            """Checks whether the given dname string is absolute (i.e. ends with a '.')

               :returns: (bool) True or False
            """
            return self.endswith(".")

        def make_canonical(self):
            """Put a dname into canonical fmt - ie. lowercase it
            """
            _ldns.ldns_dname2canonical(self)

        def dname_compare(self,other):
            """Compares the two dname rdf's according to the algorithm for ordering in RFC4034 Section 6.
               
               :param other:
                   the second dname rdf to compare
               :returns: (int) -1 if dname comes before other, 1 if dname comes after other, and 0 if they are equal.
            """
            return _ldns.ldns_dname_compare(self,other)

        def cat(self,rd2):
            """concatenates rd2 after this dname (rd2 is copied, this dname is modified)
               
               :param rd2:
                   the rightside
               :returns: (ldns_status) LDNS_STATUS_OK on success
            """
            return _ldns.ldns_dname_cat(self,rd2)
            #parameters: ldns_rdf *,ldns_rdf *,
            #retvals: ldns_status

        def cat_clone(self,rd2):
            """concatenates two dnames together
               
               :param rd2:
                   the rightside
               :returns: (ldns_rdf \*) a new rdf with leftside/rightside
            """
            return _ldns.ldns_dname_cat_clone(self,rd2)
            #parameters: const ldns_rdf *,const ldns_rdf *,
            #retvals: ldns_rdf *

        def interval(self,middle,next):
            """check if middle lays in the interval defined by prev and next prev <= middle < next.
               
               This is usefull for nsec checking
               
               :param middle:
                   the dname to check
               :param next:
                   the next dname return 0 on error or unknown, -1 when middle is in the interval, +1 when not
               :returns: (int) 
            """
            return _ldns.ldns_dname_interval(self,middle,next)
            #parameters: const ldns_rdf *,const ldns_rdf *,const ldns_rdf *,
            #retvals: int

        def is_subdomain(self,parent):
            """Tests wether the name sub falls under parent (i.e. is a subdomain of parent). 

               This function will return false if the given dnames are equal.
               
               :param parent:
                   (ldns_rdf) the parent's name
               :returns: (bool) true if sub falls under parent, otherwise false
            """
            return _ldns.ldns_dname_is_subdomain(self,parent)
            #parameters: const ldns_rdf *,const ldns_rdf *,
            #retvals: bool

        def label(self,labelpos):
            """look inside the rdf and if it is an LDNS_RDF_TYPE_DNAME try and retrieve a specific label.
               
               The labels are numbered starting from 0 (left most).
               
               :param labelpos:
                   return the label with this number
               :returns: (ldns_rdf \*) a ldns_rdf* with the label as name or NULL on error
            """
            return _ldns.ldns_dname_label(self,labelpos)
            #parameters: const ldns_rdf *,uint8_t,
            #retvals: ldns_rdf *

        def label_count(self):
            """count the number of labels inside a LDNS_RDF_DNAME type rdf.
               
               :returns: (uint8_t) the number of labels
            """
            return _ldns.ldns_dname_label_count(self)
            #parameters: const ldns_rdf *,
            #retvals: uint8_t

        def left_chop(self):
            """chop one label off the left side of a dname.
               
               so wwww.nlnetlabs.nl, becomes nlnetlabs.nl
               
               :returns: (ldns_rdf \*) the remaining dname
            """
            return _ldns.ldns_dname_left_chop(self)
            #parameters: const ldns_rdf *,
            #retvals: ldns_rdf *

        def reverse(self):
            """Returns a clone of the given dname with the labels reversed.
               
               :returns: (ldns_rdf \*) clone of the dname with the labels reversed.
            """
            return _ldns.ldns_dname_reverse(self)
            #parameters: const ldns_rdf *,
            #retvals: ldns_rdf *

            #_LDNS_RDF_METHODS#
 %}
} 
