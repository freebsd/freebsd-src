#!/bin/make
#
# Create the WebIDL for core DOM and HTML spec directly from
#  downloaded specifications. The resulting IDL is pretty close but
#  carries the example text etc. and should be manually cleaned up and
#  tagged with the retrival date and source URL
#
# needs debin packages: curl w3m tidy html-xml-utils
#
# Copyright 2012 Vincent Sanders 
# MIT licenced

.PHONY:all clean

all: dom.idl html.idl

.INTERMEDIATE:dom-spec.html dom-spec.xml dom-idl.html html-spec.html html-spec.xml html-idl.html


dom-spec.html:
	curl -s http://dom.spec.whatwg.org/ -o $@

html-spec.html:
	curl -s http://www.whatwg.org/specs/web-apps/current-work/ -o $@

%-spec.xml: %-spec.html
	-tidy -q -f $@.errors --new-blocklevel-tags header,hgroup,figure -o $@ -asxml $< 

%-idl.html: %-spec.xml
	hxselect pre.idl < $< > $@

%.idl: %-idl.html
	cat $< | w3m -dump -T text/html >$@



clean:
	${RM} dom.idl html.idl dom-spec.html dom-spec.xml dom-idl.html html-spec.html html-spec.xml html-idl.html
