/*-
* Copyright (c) 2026 Tobias Stoeckmann
* All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef ARCHIVE_XML_PRIVATE_H_INCLUDED
#define ARCHIVE_XML_PRIVATE_H_INCLUDED

#ifndef __LIBARCHIVE_BUILD
#error This header is only to be used internally to libarchive.
#endif
#ifndef __LIBARCHIVE_CONFIG_H_INCLUDED
#error "Should have include config.h first!"
#endif

/*
 * Choose XML support from third party libraries in this order of preference:
 *
 * 1. libxml2
 * 2. XmlLite
 * 3. expat
 * 4. bsdxml
 */

#if HAVE_LIBXML_XMLREADER_H && HAVE_LIBXML2
#include <libxml/xmlreader.h>
#define ARCHIVE_XML_READ 1
#define ARCHIVE_XML_READER_XML2 1
#define ARCHIVE_XML_USE_XML2 1
#elif HAVE_XMLLITE_H && HAVE_LIBXMLLITE
#include <objidl.h>
#include <initguid.h>
#include <xmllite.h>
#define ARCHIVE_XML_READ 1
#define ARCHIVE_XML_READER_XMLLITE 1
#define ARCHIVE_XML_USE_XMLLITE 1
#elif HAVE_EXPAT_H && HAVE_LIBEXPAT
#include <expat.h>
#define ARCHIVE_XML_READ 1
#define ARCHIVE_XML_READER_EXPAT 1
#define ARCHIVE_XML_USE_EXPAT 1
#elif HAVE_BSDXML_H
#include <bsdxml.h>
#define ARCHIVE_XML_READ 1
#define ARCHIVE_XML_READER_BSDXML 1
#define ARCHIVE_XML_USE_BSDXML 1
#endif

#if HAVE_LIBXML_XMLWRITER_H && HAVE_LIBXML2
#include <libxml/xmlwriter.h>
#define ARCHIVE_XML_WRITE 1
#define ARCHIVE_XML_WRITER_XML2 1
#define ARCHIVE_XML_USE_XML2 1
#elif HAVE_XMLLITE_H && HAVE_LIBXMLLITE
#include <objidl.h>
#include <initguid.h>
#include <xmllite.h>
#define ARCHIVE_XML_WRITE 1
#define ARCHIVE_XML_WRITER_XMLLITE 1
#define ARCHIVE_XML_USE_XMLLITE 1
#endif

#endif
