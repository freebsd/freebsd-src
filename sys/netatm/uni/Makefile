#
#
# ===================================
# HARP  |  Host ATM Research Platform
# ===================================
#
#
# This Host ATM Research Platform ("HARP") file (the "Software") is
# made available by Network Computing Services, Inc. ("NetworkCS")
# "AS IS".  NetworkCS does not provide maintenance, improvements or
# support of any kind.
#
# NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
# INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
# SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
# In no event shall NetworkCS be responsible for any damages, including
# but not limited to consequential damages, arising from or relating to
# any use of the Software or related support.
#
# Copyright 1994-1998 Network Computing Services, Inc.
#
# Copies of this Software may be made, however, the above copyright
# notice must be reproduced on all copies.
#
#	@(#) $Id: Makefile,v 1.6 1998/08/26 23:29:17 mks Exp $
#
#

#
# ATM Forum UNI Support
# ---------------------
#
# Source directory Makefile
#
#

DEFS=

UNI_HDRS=	uni.h
UNI_SRCS=	uni_load.c
UNI_OBJS=	uni_load.o

SIG_HDRS=	unisig.h unisig_decode.h unisig_mbuf.h		\
		unisig_msg.h unisig_print.h unisig_var.h
SIG_SRCS=	unisig_decode.c unisig_encode.c unisig_if.c	\
		unisig_mbuf.c unisig_msg.c			\
		unisig_print.c unisig_proto.c			\
		unisig_sigmgr_state.c unisig_subr.c		\
		unisig_util.c unisig_vc_state.c
SIG_OBJS=	unisig_decode.o unisig_encode.o unisig_if.o	\
		unisig_mbuf.o unisig_msg.o			\
		unisig_print.o unisig_proto.o			\
		unisig_sigmgr_state.o unisig_subr.o		\
		unisig_util.o unisig_vc_state.o

SAAL_HDRS=	sscop.h sscop_misc.h sscop_pdu.h sscop_var.h	\
		sscf_uni.h sscf_uni_var.h
SAAL_SRCS=	sscop.c sscop_lower.c sscop_pdu.c sscop_sigaa.c	\
		sscop_sigcpcs.c sscop_subr.c sscop_timer.c sscop_upper.c \
		qsaal1_sigaa.c qsaal1_sigcpcs.c qsaal1_subr.c	\
		q2110_sigaa.c q2110_sigcpcs.c q2110_subr.c	\
		sscf_uni.c sscf_uni_lower.c sscf_uni_upper.c
SAAL_OBJS=	sscop.o sscop_lower.o sscop_pdu.o sscop_sigaa.o	\
		sscop_sigcpcs.o sscop_subr.o sscop_timer.o sscop_upper.o \
		qsaal1_sigaa.o qsaal1_sigcpcs.o qsaal1_subr.o	\
		q2110_sigaa.o q2110_sigcpcs.o q2110_subr.o	\
		sscf_uni.o sscf_uni_lower.o sscf_uni_upper.o

IP_HDRS=	uniip_var.h
IP_SRCS=	uniip.c uniarp.c uniarp_cache.c uniarp_input.c	\
		uniarp_output.c uniarp_timer.c uniarp_vcm.c
IP_OBJS=	uniip.o uniarp.o uniarp_cache.o uniarp_input.o	\
		uniarp_output.o uniarp_timer.o uniarp_vcm.o

HDRS=		$(UNI_HDRS) $(SIG_HDRS) $(SAAL_HDRS) $(IP_HDRS)
SRCS=		$(UNI_SRCS) $(SIG_SRCS) $(SAAL_SRCS) $(IP_SRCS)
OBJS=		$(UNI_OBJS) $(SIG_OBJS) $(SAAL_OBJS) $(IP_OBJS)
MOD=		uni_mod.o

OBJDIR=		../../`../../config/mkobjname -d`/uni

all $(OBJS) $(MOD) config install clean depend lint load unload:
	@if [ -d $(OBJDIR) ]; then \
		echo "cd $(OBJDIR); $(MAKE) $@"; \
		cd $(OBJDIR); \
		$(MAKE) $(MFLAGS) DEFS='$(DEFS)' HDRS='$(HDRS)' SRCS='$(SRCS)' OBJS='$(OBJS)' $@; \
		exit $$?; \
	else \
		echo "Object directory \"$(OBJDIR)\" does not exist."; \
		exit 1; \
	fi

