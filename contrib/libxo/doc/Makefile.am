#
# $Id$
#
# Copyright 2014, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.

OXTRADOC_DIR = ${SLAX_OXTRADOCDIR}
OXTRADOC_PREFIX = ${OXTRADOC_DIR}
OXTRADOC = ${OXTRADOC_DIR}/oxtradoc
SLAXPROC_BINDIR = ${SLAX_BINDIR}

XML2RFC = ${OXTRADOC_DIR}/xml2rfc.tcl
XML2HTMLDIR = ${OXTRADOC_DIR}
XML2HTMLBIN = ${XML2HTMLDIR}/rfc2629-to-html.slax
SLAXPROC = ${SLAX_BINDIR}/slaxproc

SLAXPROC_ARGS = \
    -a oxtradoc-dir ${OXTRADOC_DIR} \
    -a oxtradoc-install-dir ${OXTRADOC_DIR} \
    -a anchor-prefix docs

SLAXPROC_ARGS_INLINE = \
    -a oxtradoc-inline yes

SLAXPROC_ARGS += ${SLAXPROC_ARGS_INLINE}

XML2HTML = \
    ${SLAXPROC} -g -e -I ${OXTRADOC_DIR} -I . \
    ${SLAXPROC_ARGS} \
    ${XML2HTMLBIN}

OX_ARGS = -P ${OXTRADOC_PREFIX} -L ${OXTRADOC_PREFIX}
OX_ARGS += -S ${SLAXPROC} -p doc
OX_CMD = ${PERL} ${PERLOPTS} ${OXTRADOC} ${OX_ARGS}
OXTRADOC_CMD = ${OX_CMD}


OUTPUT = libxo-manual
INPUT = libxo.txt

EXTRA_DIST = \
    ${INPUT} \
    ${OUTPUT}.html \
    ${OUTPUT}.txt

doc docs: ${OUTPUT}.txt ${OUTPUT}.html

${OUTPUT}.txt: ${INPUT} ${OXTRADOC} xolint.txt
	${OXTRADOC_CMD} -m text -o $@ $<

${OUTPUT}.html: ${INPUT} ${OXTRADOC} ${XML2HTMLBIN} xolint.txt
	${OXTRADOC_CMD} -m html -o $@ $<

xolint.txt: ${top_srcdir}/xolint/xolint.pl
	perl ${top_srcdir}/xolint/xolint.pl -D > xolint.txt

CLEANFILES = \
${OUTPUT}.xml \
${OUTPUT}.txt \
${OUTPUT}.fxml \
${OUTPUT}.html
