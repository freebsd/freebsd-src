# Makefile: compilation of sources and documentation, test environment
#
# Copyright (c) 2009, Zdenek Vasicek (vasicek AT fit.vutbr.cz)
#                     Karel Slany    (slany AT fit.vutbr.cz)
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the organization nor the names of its
#       contributors may be used to endorse or promote products derived from this
#       software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

help:
	@echo "Please use \`make <target>' where <target> is one of"
	@echo "  testenv   to make test environment and run bash "
	@echo "            usefull in case you don't want to install ldns but want to test examples"
	@echo "  doc       to make documentation"
	@echo "  clean     clean all"

../../Makefile: ../../configure
	cd ../.. && ./configure --with-python
	
_ldns.so:	../../Makefile
	$(MAKE) -C ../..

../../.libs/ldns.so.1:	../../Makefile
	$(MAKE) -C ../..

clean:
	rm -rdf examples/ldns
	rm -f _ldns.so ldns_wrapper.o
	$(MAKE) -C ../.. clean

testenv: ../../.libs/libldns.so.1 _ldns.so
	rm -rdf examples/ldns
	cd examples && mkdir ldns && ln -s ../../ldns.py ldns/__init__.py && ln -s ../../_ldns.so ldns/_ldns.so && ln -s ../../../../.libs/libldns.so.1 ldns/libldns.so.1 && ls -la
	@echo "Run a script by typing ./script_name.py"
	cd examples && LD_LIBRARY_PATH=ldns bash
	rm -rdf examples/ldns

doc: ../../.libs/ldns.so.1 _ldns.so
	$(MAKE) -C docs html

#for development only
swig: ldns.i
	swig -python -py3 -o ldns_wrapper.c -I../.. ldns.i
	gcc -c ldns_wrapper.c -O9 -fPIC -I../.. -I../../ldns -I/usr/include/python3.1 -I. -o ldns_wrapper.o
	ld -shared ldns_wrapper.o -L../../.libs -lldns -o _ldns.so 

