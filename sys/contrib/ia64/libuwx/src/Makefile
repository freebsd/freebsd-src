# Makefile for IPF unwind express library, libuwx.
#
# To build a cross-unwind library (i.e., one hosted on a
# non-IPF, non-HP-UX system), omit the "self" callbacks
# and the dependency on libuca by setting SELFOBJS and
# SELFLIBS to empty strings.

AR = ar
RANLIB = :

OTHERCFLAGS =
# OTHERCFLAGS = -DUWX_TRACE_ENABLE	# Enables trace output
# OTHERCFLAGS = +DD64			# Builds 64-bit library

CFLAGS = -O $(OTHERCFLAGS)

OBJS =		uwx_bstream.o uwx_context.o uwx_env.o uwx_scoreboard.o \
		uwx_step.o uwx_str.o uwx_swap.o uwx_trace.o uwx_uinfo.o \
		uwx_utable.o

# SELFOBJS =				# For cross-unwind library
SELFOBJS = 	uwx_self.o uwx_self_context.o uwx_ttrace.o

# SELFLIBS =				# For cross-unwind library
SELFLIBS =	-luca

libuwx.a:	$(OBJS) $(SELFOBJS)
	$(AR) rv libuwx.a $?
	$(RANLIB) libuwx.a

libuwx.so:	$(OBJS) $(SELFOBJS)
	ld -b -o libuwx.so $(OBJS) $(SELFOBJS) $(SELFLIBS)

libuwx.sl:	$(OBJS) $(SELFOBJS)
	ld -b -o libuwx.sl $(OBJS) $(SELFOBJS) $(SELFLIBS)

uwx_bstream.o:	uwx.h uwx_env.h uwx_bstream.h

uwx_context.o:	uwx.h uwx_env.h uwx_scoreboard.h uwx_step.h uwx_trace.h

uwx_env.o:	uwx.h uwx_env.h uwx_scoreboard.h uwx_str.h uwx_trace.h

uwx_scoreboard.o: uwx.h uwx_env.h uwx_scoreboard.h uwx_trace.h

uwx_step.o:	uwx.h uwx_env.h uwx_context.h uwx_utable.h \
		uwx_uinfo.h uwx_scoreboard.h uwx_str.h uwx_trace.h

uwx_str.o:	uwx.h uwx_env.h uwx_str.h

uwx_swap.o:	uwx.h uwx_env.h uwx_swap.h

uwx_trace.o:	uwx.h uwx_env.h uwx_uinfo.h uwx_scoreboard.h uwx_trace.h

uwx_uinfo.o:	uwx.h uwx_env.h uwx_uinfo.h uwx_utable.h \
		uwx_scoreboard.h uwx_bstream.h uwx_trace.h

uwx_utable.o:	uwx.h uwx_env.h uwx_utable.h uwx_swap.h uwx_trace.h

uwx_self.o:	uwx.h uwx_env.h uwx_context.h uwx_trace.h uwx_self.h

uwx_self_context.o:	uwx_self_context.s
	$(CC) -c $(CFLAGS) -o uwx_self_context.o uwx_self_context.s

uwx_ttrace.o:	uwx.h uwx_env.h uwx_context.h uwx_trace.h uwx_ttrace.h
