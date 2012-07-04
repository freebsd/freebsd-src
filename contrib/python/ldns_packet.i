/******************************************************************************
 * ldns_packet.i: LDNS packet class
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
%typemap(in,numinputs=0,noblock=1) (ldns_pkt **)
{
 ldns_pkt *$1_pkt;
 $1 = &$1_pkt;
}
          
/* result generation */
%typemap(argout,noblock=1) (ldns_pkt **)
{
 $result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr($1_pkt), SWIGTYPE_p_ldns_struct_pkt, SWIG_POINTER_OWN |  0 ));
}

%newobject ldns_pkt_new;
%newobject ldns_pkt_clone;
%newobject ldns_pkt_rr_list_by_type;
%newobject ldns_pkt_rr_list_by_name_and_type;
%newobject ldns_pkt_rr_list_by_name;
%newobject ldns_update_pkt_new;


%nodefaultctor ldns_struct_pkt; //no default constructor & destructor
%nodefaultdtor ldns_struct_pkt;

%rename(ldns_pkt) ldns_struct_pkt;
#ifdef LDNS_DEBUG
%rename(__ldns_pkt_free) ldns_pkt_free;
%inline %{
void _ldns_pkt_free (ldns_pkt* p) {
   printf("******** LDNS_PKT free 0x%lX ************\n", (long unsigned int)p);
   ldns_pkt_free(p);
}
%}
#else
%rename(_ldns_pkt_free) ldns_pkt_free;
#endif

%newobject ldns_pkt2str;
%newobject ldns_pkt_opcode2str;
%newobject ldns_pkt_rcode2str;
%newobject ldns_pkt_algorithm2str;
%newobject ldns_pkt_cert_algorithm2str;


/* cloning of packet_lists to make them independent of the original packet */

%newobject _ldns_pkt_additional;
%newobject _ldns_pkt_answer;
%newobject _ldns_pkt_authority;
%newobject _ldns_pkt_question;

%rename(__ldns_pkt_additional) ldns_pkt_additional;
%inline %{
ldns_rr_list* _ldns_pkt_additional(ldns_pkt* p) {
   return ldns_rr_list_clone(ldns_pkt_additional(p));
}
%}

%rename(__ldns_pkt_answer) ldns_pkt_answer;
%inline %{
ldns_rr_list* _ldns_pkt_answer(ldns_pkt* p) {
   return ldns_rr_list_clone(ldns_pkt_answer(p));
}
%}

%rename(__ldns_pkt_authority) ldns_pkt_authority;
%inline %{
ldns_rr_list* _ldns_pkt_authority(ldns_pkt* p) {
   return ldns_rr_list_clone(ldns_pkt_authority(p));
}
%}

%rename(__ldns_pkt_question) ldns_pkt_question;
%inline %{
ldns_rr_list* _ldns_pkt_question(ldns_pkt* p) {
   return ldns_rr_list_clone(ldns_pkt_question(p));
}
%}

/* clone data when pushed in */

%rename(__ldns_pkt_push_rr) ldns_pkt_push_rr;
%inline %{
bool _ldns_pkt_push_rr(ldns_pkt* p, ldns_pkt_section sec, ldns_rr *rr) {
   return ldns_pkt_push_rr(p, sec, ldns_rr_clone(rr));
}
%}

%rename(__ldns_pkt_push_rr_list) ldns_pkt_push_rr_list;
%inline %{
bool _ldns_pkt_push_rr_list(ldns_pkt* p, ldns_pkt_section sec, ldns_rr_list *rrl) {
   return ldns_pkt_push_rr_list(p, sec, ldns_rr_list_clone(rrl));
}
%}

%feature("docstring") ldns_struct_pkt "LDNS packet object. 

The ldns_pkt object contains DNS packed (either a query or an answer). It is the complete representation of what you actually send to a nameserver, and what you get back (see :class:`ldns.ldns_resolver`).

**Usage**

>>> import ldns
>>> resolver = ldns.ldns_resolver.new_frm_file(\"/etc/resolv.conf\")
>>> pkt = resolver.query(\"nic.cz\", ldns.LDNS_RR_TYPE_NS,ldns.LDNS_RR_CLASS_IN)
>>> print pkt
;; ->>HEADER<<- opcode: QUERY, rcode: NOERROR, id: 63004
;; flags: qr rd ra ; QUERY: 1, ANSWER: 3, AUTHORITY: 0, ADDITIONAL: 0 
;; QUESTION SECTION:
;; nic.cz.	IN	NS
;; ANSWER SECTION:
nic.cz.	758	IN	NS	a.ns.nic.cz.
nic.cz.	758	IN	NS	c.ns.nic.cz.
nic.cz.	758	IN	NS	e.ns.nic.cz.
;; AUTHORITY SECTION:
;; ADDITIONAL SECTION:
;; Query time: 8 msec
;; SERVER: 82.100.38.2
;; WHEN: Thu Jan 11 12:54:33 2009
;; MSG SIZE  rcvd: 75

This simple example instances a resolver in order to resolve NS for nic.cz. 
"

%extend ldns_struct_pkt {
 
 %pythoncode %{
        def __init__(self):
            raise Exception("This class can't be created directly. Please use: ldns_pkt_new(), ldns_pkt_query_new() or ldns_pkt_query_new_frm_str()")

        __swig_destroy__ = _ldns._ldns_pkt_free

        #LDNS_PKT_CONSTRUCTORS_#
        @staticmethod
        def new_query(rr_name, rr_type, rr_class, flags):
            """Creates a packet with a query in it for the given name, type and class.
               
               :param rr_name: the name to query for
               :param rr_type: the type to query for
               :param rr_class: the class to query for
               :param flags: packet flags
               :returns: new ldns_pkt object
            """
            return _ldns.ldns_pkt_query_new(rr_name, rr_type, rr_class, flags)

        @staticmethod
        def new_query_frm_str(rr_name, rr_type, rr_class, flags, raiseException = True):
            """Creates a query packet for the given name, type, class.
               
               :param rr_name: the name to query for
               :param rr_type: the type to query for
               :param rr_class: the class to query for
               :param flags: packet flags
               :param raiseException: if True, an exception occurs in case a resolver object can't be created
               :returns: query packet object or None. If the object can't be created and raiseException is True, an exception occurs.


               **Usage**

               >>> pkt = ldns.ldns_pkt.new_query_frm_str("test.nic.cz",ldns.LDNS_RR_TYPE_ANY, ldns.LDNS_RR_CLASS_IN, ldns.LDNS_QR | ldns.LDNS_AA)
               >>> rra = ldns.ldns_rr.new_frm_str("test.nic.cz. IN A 192.168.1.1",300)
               >>> list = ldns.ldns_rr_list()
               >>> if (rra): list.push_rr(rra)
               >>> pkt.push_rr_list(ldns.LDNS_SECTION_ANSWER, list)
               >>> print pkt
               ;; ->>HEADER<<- opcode: QUERY, rcode: NOERROR, id: 0
               ;; flags: qr aa ; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0 
               ;; QUESTION SECTION:
               ;; test.nic.cz.	IN	ANY
               ;; ANSWER SECTION:
               test.nic.cz.	300	IN	A	192.168.1.1
               ;; AUTHORITY SECTION:
               ;; ADDITIONAL SECTION:
               ;; Query time: 0 msec
               ;; WHEN: Thu Jan  1 01:00:00 1970
               ;; MSG SIZE  rcvd: 0
            """
            status, pkt = _ldns.ldns_pkt_query_new_frm_str(rr_name, rr_type, rr_class, flags)
            if status != LDNS_STATUS_OK:
                if (raiseException): raise Exception("Can't create query packet, error: %d" % status)
                return None
            return pkt
        #_LDNS_PKT_CONSTRUCTORS#
     
        def __str__(self):
            """Converts the data in the DNS packet to presentation format"""
            return _ldns.ldns_pkt2str(self)

        def opcode2str(self):
            """Converts a packet opcode to its mnemonic and returns that as an allocated null-terminated string."""
            return _ldns.ldns_pkt_opcode2str(sefl.get_opcode())

        def rcode2str(self):
            """Converts a packet rcode to its mnemonic and returns that as an allocated null-terminated string."""
            return _ldns.ldns_pkt_rcode2str(self.get_rcode())

        def print_to_file(self,output):
            """Prints the data in the DNS packet to the given file stream (in presentation format)."""
            _ldns.ldns_pkt_print(output,self)
            #parameters: FILE *,const ldns_pkt *,

        def write_to_buffer(self, buffer):
            """Copies the packet data to the buffer in wire format.
               
               :param buffer: buffer to append the result to
               :returns: (ldns_status) ldns_status
            """
            return _ldns.ldns_pkt2buffer_wire(buffer, self)
            #parameters: ldns_buffer *,const ldns_pkt *,
            #retvals: ldns_status

        @staticmethod
        def algorithm2str(alg):
            """Converts a signing algorithms to its mnemonic and returns that as an allocated null-terminated string."""
            return _ldns.ldns_pkt_algorithm2str(alg)
            #parameters: ldns_algorithm,

        @staticmethod
        def cert_algorithm2str(alg):
            """Converts a cert algorithm to its mnemonic and returns that as an allocated null-terminated string."""
            return _ldns.ldns_pkt_cert_algorithm2str(alg)
            #parameters: ldns_algorithm,

         #LDNS_PKT_METHODS_#
        def aa(self):
            """Read the packet's aa bit.
               
               :returns: (bool) value of the bit
            """
            return _ldns.ldns_pkt_aa(self)
            #parameters: const ldns_pkt *,
            #retvals: bool

        def ad(self):
            """Read the packet's ad bit.
               
               :returns: (bool) value of the bit
            """
            return _ldns.ldns_pkt_ad(self)
            #parameters: const ldns_pkt *,
            #retvals: bool

        def additional(self):
            """Return the packet's additional section.
               
               :returns: (ldns_rr_list \*) the section
            """
            return _ldns._ldns_pkt_additional(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_rr_list *

        def all(self):
            return _ldns.ldns_pkt_all(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_rr_list *

        def all_noquestion(self):
            return _ldns.ldns_pkt_all_noquestion(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_rr_list *

        def ancount(self):
            """Return the packet's an count.
               
               :returns: (uint16_t) the an count
            """
            return _ldns.ldns_pkt_ancount(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

        def answer(self):
            """Return the packet's answer section.
               
               :returns: (ldns_rr_list \*) the section
            """
            return _ldns._ldns_pkt_answer(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_rr_list *

        def answerfrom(self):
            """Return the packet's answerfrom.
               
               :returns: (ldns_rdf \*) the name of the server
            """
            return _ldns.ldns_pkt_answerfrom(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_rdf *

        def arcount(self):
            """Return the packet's ar count.
               
               :returns: (uint16_t) the ar count
            """
            return _ldns.ldns_pkt_arcount(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

        def authority(self):
            """Return the packet's authority section.
               
               :returns: (ldns_rr_list \*) the section
            """
            return _ldns._ldns_pkt_authority(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_rr_list *

        def cd(self):
            """Read the packet's cd bit.
               
               :returns: (bool) value of the bit
            """
            return _ldns.ldns_pkt_cd(self)
            #parameters: const ldns_pkt *,
            #retvals: bool

        def clone(self):
            """clones the given packet, creating a fully allocated copy
               
               :returns: (ldns_pkt \*) ldns_pkt* pointer to the new packet
            """
            return _ldns.ldns_pkt_clone(self)
            #parameters: ldns_pkt *,
            #retvals: ldns_pkt *

        def edns(self):
            """returns true if this packet needs and EDNS rr to be sent.
               
               At the moment the only reason is an expected packet size larger than 512 bytes, but for instance dnssec would be a good reason too.
               
               :returns: (bool) true if packet needs edns rr
            """
            return _ldns.ldns_pkt_edns(self)
            #parameters: const ldns_pkt *,
            #retvals: bool

        def edns_data(self):
            """return the packet's edns data
               
               :returns: (ldns_rdf \*) the data
            """
            return _ldns.ldns_pkt_edns_data(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_rdf *

        def edns_do(self):
            """return the packet's edns do bit
               
               :returns: (bool) the bit's value
            """
            return _ldns.ldns_pkt_edns_do(self)
            #parameters: const ldns_pkt *,
            #retvals: bool

        def edns_extended_rcode(self):
            """return the packet's edns extended rcode
               
               :returns: (uint8_t) the rcode
            """
            return _ldns.ldns_pkt_edns_extended_rcode(self)
            #parameters: const ldns_pkt *,
            #retvals: uint8_t

        def edns_udp_size(self):
            """return the packet's edns udp size
               
               :returns: (uint16_t) the size
            """
            return _ldns.ldns_pkt_edns_udp_size(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

        def edns_version(self):
            """return the packet's edns version
               
               :returns: (uint8_t) the version
            """
            return _ldns.ldns_pkt_edns_version(self)
            #parameters: const ldns_pkt *,
            #retvals: uint8_t

        def edns_z(self):
            """return the packet's edns z value
               
               :returns: (uint16_t) the z value
            """
            return _ldns.ldns_pkt_edns_z(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

        def empty(self):
            """check if a packet is empty
               
               :returns: (bool) true: empty, false: empty
            """
            return _ldns.ldns_pkt_empty(self)
            #parameters: ldns_pkt *,
            #retvals: bool

        def get_opcode(self):
            """Read the packet's code.
               
               :returns: (ldns_pkt_opcode) the opcode
            """
            return _ldns.ldns_pkt_get_opcode(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_pkt_opcode

        def get_rcode(self):
            """Return the packet's respons code.
               
               :returns: (ldns_pkt_rcode) the respons code
            """
            return _ldns.ldns_pkt_get_rcode(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_pkt_rcode

        def get_section_clone(self,s):
            """return all the rr_list's in the packet.
               
               Clone the lists, instead of returning pointers.
               
               :param s:
                   what section(s) to return
               :returns: (ldns_rr_list \*) ldns_rr_list with the rr's or NULL if none were found
            """
            return _ldns.ldns_pkt_get_section_clone(self,s)
            #parameters: const ldns_pkt *,ldns_pkt_section,
            #retvals: ldns_rr_list *

        def id(self):
            """Read the packet id.
               
               :returns: (uint16_t) the packet id
            """
            return _ldns.ldns_pkt_id(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

        def nscount(self):
            """Return the packet's ns count.
               
               :returns: (uint16_t) the ns count
            """
            return _ldns.ldns_pkt_nscount(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

        def push_rr(self,section,rr):
            """push an rr on a packet
               
               :param section:
                   where to put it
               :param rr:
                   rr to push
               :returns: (bool) a boolean which is true when the rr was added
            """
            return _ldns._ldns_pkt_push_rr(self,section,rr)
            #parameters: ldns_pkt *,ldns_pkt_section,ldns_rr *,
            #retvals: bool

        def push_rr_list(self,section,list):
            """push a rr_list on a packet
               
               :param section:
                   where to put it
               :param list:
                   the rr_list to push
               :returns: (bool) a boolean which is true when the rr was added
            """
            return _ldns._ldns_pkt_push_rr_list(self,section,list)
            #parameters: ldns_pkt *,ldns_pkt_section,ldns_rr_list *,
            #retvals: bool

        def qdcount(self):
            """Return the packet's qd count.
               
               :returns: (uint16_t) the qd count
            """
            return _ldns.ldns_pkt_qdcount(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

        def qr(self):
            """Read the packet's qr bit.
               
               :returns: (bool) value of the bit
            """
            return _ldns.ldns_pkt_qr(self)
            #parameters: const ldns_pkt *,
            #retvals: bool

        def querytime(self):
            """Return the packet's querytime.
               
               :returns: (uint32_t) the querytime
            """
            return _ldns.ldns_pkt_querytime(self)
            #parameters: const ldns_pkt *,
            #retvals: uint32_t

        def question(self):
            """Return the packet's question section.
               
               :returns: (ldns_rr_list \*) the section
            """
            return _ldns._ldns_pkt_question(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_rr_list *

        def ra(self):
            """Read the packet's ra bit.
               
               :returns: (bool) value of the bit
            """
            return _ldns.ldns_pkt_ra(self)
            #parameters: const ldns_pkt *,
            #retvals: bool

        def rd(self):
            """Read the packet's rd bit.
               
               :returns: (bool) value of the bit
            """
            return _ldns.ldns_pkt_rd(self)
            #parameters: const ldns_pkt *,
            #retvals: bool

        def reply_type(self):
            """looks inside the packet to determine what kind of packet it is, AUTH, NXDOMAIN, REFERRAL, etc.
               
               :returns: (ldns_pkt_type) the type of packet
            """
            return _ldns.ldns_pkt_reply_type(self)
            #parameters: ldns_pkt *,
            #retvals: ldns_pkt_type

        def rr(self,sec,rr):
            """check to see if an rr exist in the packet
               
               :param sec:
                   in which section to look
               :param rr:
                   the rr to look for
               :returns: (bool) 
            """
            return _ldns.ldns_pkt_rr(self,sec,rr)
            #parameters: ldns_pkt *,ldns_pkt_section,ldns_rr *,
            #retvals: bool

        def rr_list_by_name(self,r,s):
            """return all the rr with a specific name from a packet.
               
               Optionally specify from which section in the packet
               
               :param r:
                   the name
               :param s:
                   the packet's section
               :returns: (ldns_rr_list \*) a list with the rr's or NULL if none were found
            """
            return _ldns.ldns_pkt_rr_list_by_name(self,r,s)
            #parameters: ldns_pkt *,ldns_rdf *,ldns_pkt_section,
            #retvals: ldns_rr_list *

        def rr_list_by_name_and_type(self,ownername,atype,sec):
            """return all the rr with a specific type and type from a packet.
               
               Optionally specify from which section in the packet
               
               :param ownername:
                   the name
               :param atype:
               :param sec:
                   the packet's section
               :returns: (ldns_rr_list \*) a list with the rr's or NULL if none were found
            """
            return _ldns.ldns_pkt_rr_list_by_name_and_type(self,ownername,atype,sec)
            #parameters: const ldns_pkt *,const ldns_rdf *,ldns_rr_type,ldns_pkt_section,
            #retvals: ldns_rr_list *

        def rr_list_by_type(self,t,s):
            """return all the rr with a specific type from a packet.
               
               Optionally specify from which section in the packet
               
               :param t:
                   the type
               :param s:
                   the packet's section
               :returns: (ldns_rr_list \*) a list with the rr's or NULL if none were found
            """
            return _ldns.ldns_pkt_rr_list_by_type(self,t,s)
            #parameters: const ldns_pkt *,ldns_rr_type,ldns_pkt_section,
            #retvals: ldns_rr_list *

        def safe_push_rr(self,sec,rr):
            """push an rr on a packet, provided the RR is not there.
               
               :param sec:
                   where to put it
               :param rr:
                   rr to push
               :returns: (bool) a boolean which is true when the rr was added
            """
            return _ldns.ldns_pkt_safe_push_rr(self,sec,rr)
            #parameters: ldns_pkt *,ldns_pkt_section,ldns_rr *,
            #retvals: bool

        def safe_push_rr_list(self,sec,list):
            """push an rr_list to a packet, provided the RRs are not already there.
               
               :param sec:
                   where to put it
               :param list:
                   the rr_list to push
               :returns: (bool) a boolean which is true when the rr was added
            """
            return _ldns.ldns_pkt_safe_push_rr_list(self,sec,list)
            #parameters: ldns_pkt *,ldns_pkt_section,ldns_rr_list *,
            #retvals: bool

        def section_count(self,s):
            return _ldns.ldns_pkt_section_count(self,s)
            #parameters: const ldns_pkt *,ldns_pkt_section,
            #retvals: uint16_t

        def set_aa(self,b):
            """Set the packet's aa bit.
               
               :param b:
                   the value to set (boolean)
            """
            _ldns.ldns_pkt_set_aa(self,b)
            #parameters: ldns_pkt *,bool,
            #retvals: 

        def set_ad(self,b):
            """Set the packet's ad bit.
               
               :param b:
                   the value to set (boolean)
            """
            _ldns.ldns_pkt_set_ad(self,b)
            #parameters: ldns_pkt *,bool,
            #retvals: 

        def set_additional(self,rr):
            """directly set the additional section
               
               :param rr:
                   rrlist to set
            """
            _ldns.ldns_pkt_set_additional(self,rr)
            #parameters: ldns_pkt *,ldns_rr_list *,
            #retvals: 

        def set_ancount(self,c):
            """Set the packet's an count.
               
               :param c:
                   the count
            """
            _ldns.ldns_pkt_set_ancount(self,c)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def set_answer(self,rr):
            """directly set the answer section
               
               :param rr:
                   rrlist to set
            """
            _ldns.ldns_pkt_set_answer(self,rr)
            #parameters: ldns_pkt *,ldns_rr_list *,
            #retvals: 

        def set_answerfrom(self,r):
            """Set the packet's answering server.
               
               :param r:
                   the address
            """
            _ldns.ldns_pkt_set_answerfrom(self,r)
            #parameters: ldns_pkt *,ldns_rdf *,
            #retvals: 

        def set_arcount(self,c):
            """Set the packet's arcount.
               
               :param c:
                   the count
            """
            _ldns.ldns_pkt_set_arcount(self,c)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def set_authority(self,rr):
            """directly set the auhority section
               
               :param rr:
                   rrlist to set
            """
            _ldns.ldns_pkt_set_authority(self,rr)
            #parameters: ldns_pkt *,ldns_rr_list *,
            #retvals: 

        def set_cd(self,b):
            """Set the packet's cd bit.
               
               :param b:
                   the value to set (boolean)
            """
            _ldns.ldns_pkt_set_cd(self,b)
            #parameters: ldns_pkt *,bool,
            #retvals: 

        def set_edns_data(self,data):
            """Set the packet's edns data.
               
               :param data:
                   the data
            """
            _ldns.ldns_pkt_set_edns_data(self,data)
            #parameters: ldns_pkt *,ldns_rdf *,
            #retvals: 

        def set_edns_do(self,value):
            """Set the packet's edns do bit.
               
               :param value:
                   the bit's new value
            """
            _ldns.ldns_pkt_set_edns_do(self,value)
            #parameters: ldns_pkt *,bool,
            #retvals: 

        def set_edns_extended_rcode(self,c):
            """Set the packet's edns extended rcode.
               
               :param c:
                   the code
            """
            _ldns.ldns_pkt_set_edns_extended_rcode(self,c)
            #parameters: ldns_pkt *,uint8_t,
            #retvals: 

        def set_edns_udp_size(self,s):
            """Set the packet's edns udp size.
               
               :param s:
                   the size
            """
            _ldns.ldns_pkt_set_edns_udp_size(self,s)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def set_edns_version(self,v):
            """Set the packet's edns version.
               
               :param v:
                   the version
            """
            _ldns.ldns_pkt_set_edns_version(self,v)
            #parameters: ldns_pkt *,uint8_t,
            #retvals: 

        def set_edns_z(self,z):
            """Set the packet's edns z value.
               
               :param z:
                   the value
            """
            _ldns.ldns_pkt_set_edns_z(self,z)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def set_flags(self,flags):
            """sets the flags in a packet.
               
               :param flags:
                   ORed values: LDNS_QR| LDNS_AR for instance
               :returns: (bool) true on success otherwise false
            """
            return _ldns.ldns_pkt_set_flags(self,flags)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: bool

        def set_id(self,id):
            """Set the packet's id.
               
               :param id:
                   the id to set
            """
            _ldns.ldns_pkt_set_id(self,id)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def set_nscount(self,c):
            """Set the packet's ns count.
               
               :param c:
                   the count
            """
            _ldns.ldns_pkt_set_nscount(self,c)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def set_opcode(self,c):
            """Set the packet's opcode.
               
               :param c:
                   the opcode
            """
            _ldns.ldns_pkt_set_opcode(self,c)
            #parameters: ldns_pkt *,ldns_pkt_opcode,
            #retvals: 

        def set_qdcount(self,c):
            """Set the packet's qd count.
               
               :param c:
                   the count
            """
            _ldns.ldns_pkt_set_qdcount(self,c)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def set_qr(self,b):
            """Set the packet's qr bit.
               
               :param b:
                   the value to set (boolean)
            """
            _ldns.ldns_pkt_set_qr(self,b)
            #parameters: ldns_pkt *,bool,
            #retvals: 

        def set_querytime(self,t):
            """Set the packet's query time.
               
               :param t:
                   the querytime in msec
            """
            _ldns.ldns_pkt_set_querytime(self,t)
            #parameters: ldns_pkt *,uint32_t,
            #retvals: 

        def set_question(self,rr):
            """directly set the question section
               
               :param rr:
                   rrlist to set
            """
            _ldns.ldns_pkt_set_question(self,rr)
            #parameters: ldns_pkt *,ldns_rr_list *,
            #retvals: 

        def set_ra(self,b):
            """Set the packet's ra bit.
               
               :param b:
                   the value to set (boolean)
            """
            _ldns.ldns_pkt_set_ra(self,b)
            #parameters: ldns_pkt *,bool,
            #retvals: 

        def set_random_id(self):
            """Set the packet's id to a random value.
            """
            _ldns.ldns_pkt_set_random_id(self)
            #parameters: ldns_pkt *,
            #retvals: 

        def set_rcode(self,c):
            """Set the packet's respons code.
               
               :param c:
                   the rcode
            """
            _ldns.ldns_pkt_set_rcode(self,c)
            #parameters: ldns_pkt *,uint8_t,
            #retvals: 

        def set_rd(self,b):
            """Set the packet's rd bit.
               
               :param b:
                   the value to set (boolean)
            """
            _ldns.ldns_pkt_set_rd(self,b)
            #parameters: ldns_pkt *,bool,
            #retvals: 

        def set_section_count(self,s,x):
            """Set a packet's section count to x.
               
               :param s:
                   the section
               :param x:
                   the section count
            """
            _ldns.ldns_pkt_set_section_count(self,s,x)
            #parameters: ldns_pkt *,ldns_pkt_section,uint16_t,
            #retvals: 

        def set_size(self,s):
            """Set the packet's size.
               
               :param s:
                   the size
            """
            _ldns.ldns_pkt_set_size(self,s)
            #parameters: ldns_pkt *,size_t,
            #retvals: 

        def set_tc(self,b):
            """Set the packet's tc bit.
               
               :param b:
                   the value to set (boolean)
            """
            _ldns.ldns_pkt_set_tc(self,b)
            #parameters: ldns_pkt *,bool,
            #retvals: 

        def set_timestamp(self,timeval):
            _ldns.ldns_pkt_set_timestamp(self,timeval)
            #parameters: ldns_pkt *,struct timeval,
            #retvals: 

        def set_tsig(self,t):
            """Set the packet's tsig rr.
               
               :param t:
                   the tsig rr
            """
            _ldns.ldns_pkt_set_tsig(self,t)
            #parameters: ldns_pkt *,ldns_rr *,
            #retvals: 

        def size(self):
            """Return the packet's size in bytes.
               
               :returns: (size_t) the size
            """
            return _ldns.ldns_pkt_size(self)
            #parameters: const ldns_pkt *,
            #retvals: size_t

        def tc(self):
            """Read the packet's tc bit.
               
               :returns: (bool) value of the bit
            """
            return _ldns.ldns_pkt_tc(self)
            #parameters: const ldns_pkt *,
            #retvals: bool

        def timestamp(self):
            """Return the packet's timestamp.
               
               :returns: (struct timeval) the timestamp
            """
            return _ldns.ldns_pkt_timestamp(self)
            #parameters: const ldns_pkt *,
            #retvals: struct timeval

        def tsig(self):
            """Return the packet's tsig pseudo rr's.
               
               :returns: (ldns_rr \*) the tsig rr
            """
            return _ldns.ldns_pkt_tsig(self)
            #parameters: const ldns_pkt *,
            #retvals: ldns_rr *

            #_LDNS_PKT_METHODS#

            #LDNS update methods
            #LDNS_METHODS_#
        def update_pkt_tsig_add(self,r):
            """add tsig credentials to a packet from a resolver
               
               :param r:
                   resolver to copy from
               :returns: (ldns_status) status wether successfull or not
            """
            return _ldns.ldns_update_pkt_tsig_add(self,r)
            #parameters: ldns_pkt *,ldns_resolver *,
            #retvals: ldns_status

        def update_prcount(self):
            """Get the zo count.
               
               :returns: (uint16_t) the pr count
            """
            return _ldns.ldns_update_prcount(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

        def update_set_adcount(self,c):
            """Set the ad count.
               
               :param c:
                   the ad count to set
            """
            _ldns.ldns_update_set_adcount(self,c)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def update_set_prcount(self,c):
            """Set the pr count.
               
               :param c:
                   the pr count to set
            """
            _ldns.ldns_update_set_prcount(self,c)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def update_set_upcount(self,c):
            """Set the up count.
               
               :param c:
                   the up count to set
            """
            _ldns.ldns_update_set_upcount(self,c)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def update_set_zo(self,v):
            _ldns.ldns_update_set_zo(self,v)
            #parameters: ldns_pkt *,uint16_t,
            #retvals: 

        def update_upcount(self):
            """Get the zo count.
               
               :returns: (uint16_t) the up count
            """
            return _ldns.ldns_update_upcount(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

        def update_zocount(self):
            """Get the zo count.
               
               :returns: (uint16_t) the zo count
            """
            return _ldns.ldns_update_zocount(self)
            #parameters: const ldns_pkt *,
            #retvals: uint16_t

            #_LDNS_METHODS#
 %}
} 
