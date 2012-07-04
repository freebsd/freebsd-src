/******************************************************************************
 * ldns_dname.i: LDNS domain name class
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
%pythoncode %{
    class ldns_dname(ldns_rdf):
        """Domain name

           This class contains methods to read and manipulate domain names.
           Domain names are stored in ldns_rdf structures, with the type LDNS_RDF_TYPE_DNAME

           **Usage** 

           >>> import ldns
           >>> resolver = ldns.ldns_resolver.new_frm_file("/etc/resolv.conf")
           >>> dn1 = ldns.ldns_dname("test.nic.cz")
           >>> print dn1
           test.nic.cz.
           >>> dn2 = ldns.ldns_dname("nic.cz")
           >>> if dn2.is_subdomain(dn1): print dn2,"is subdomain of",dn1
           >>> if dn1.is_subdomain(dn2): print dn1,"is subdomain of",dn2
           test.nic.cz. is subdomain of nic.cz.
        """
        def __init__(self, str):
            """Creates a new dname rdf from a string.
            
               :parameter str: str string to use
            """
            self.this = _ldns.ldns_dname_new_frm_str(str)
       
        @staticmethod
        def new_frm_str(str):
            """Creates a new dname rdf instance from a string.
            
               This static method is equivalent to using of default class constructor.
 
               :parameter str: str string to use
            """
            return ldns_dname(str)

        def absolute(self):
            """Checks whether the given dname string is absolute (i.e. ends with a '.')

               :returns: (bool) True or False
            """
            return self.endswith(".")


        def make_canonical(self):
            """Put a dname into canonical fmt - ie. lowercase it
            """
            _ldns.ldns_dname2canonical(self)

        def __cmp__(self,other):
            """Compares the two dname rdf's according to the algorithm for ordering in RFC4034 Section 6.
               
               :param other:
                   the second dname rdf to compare
               :returns: (int) -1 if dname comes before other, 1 if dname comes after other, and 0 if they are equal.
            """
            return _ldns.ldns_dname_compare(self,other)

        def write_to_buffer(self,buffer):
            """Copies the dname data to the buffer in wire format.
               
               :param buffer: buffer to append the result to
               :returns: (ldns_status) ldns_status
            """
            return _ldns.ldns_dname2buffer_wire(buffer,self)
            #parameters: ldns_buffer *,const ldns_rdf *,
            #retvals: ldns_status

            #LDNS_DNAME_METHODS_#

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

            #_LDNS_DNAME_METHODS#
%}

