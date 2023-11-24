all: capsicum-test smoketest mini-me mini-me.noexec mini-me.setuid $(EXTRA_PROGS)
OBJECTS=capsicum-test-main.o capsicum-test.o capability-fd.o copy_file_range.o fexecve.o procdesc.o capmode.o fcntl.o ioctl.o openat.o sysctl.o select.o mqueue.o socket.o sctp.o capability-fd-pair.o linux.o overhead.o rename.o

GTEST_DIR=gtest-1.10.0
GTEST_INCS=-I$(GTEST_DIR)/include -I$(GTEST_DIR)
GTEST_FLAGS=-DGTEST_USE_OWN_TR1_TUPLE=1 -DGTEST_HAS_TR1_TUPLE=1
CXXFLAGS+=$(ARCHFLAG) -Wall -g $(GTEST_INCS) $(GTEST_FLAGS) --std=c++11
CFLAGS+=$(ARCHFLAG) -Wall -g

capsicum-test: $(OBJECTS) libgtest.a $(LOCAL_LIBS)
	$(CXX) $(CXXFLAGS) -g -o $@ $(OBJECTS) libgtest.a -lpthread -lrt $(LIBSCTP) $(LIBCAPRIGHTS) $(EXTRA_LIBS)

# Small statically-linked program for fexecve tests
# (needs to be statically linked so that execve()ing it
# doesn't involve ld.so traversing the filesystem).
mini-me: mini-me.c
	$(CC) $(CFLAGS) -static -o $@ $<
mini-me.noexec: mini-me
	cp mini-me $@ && chmod -x $@
mini-me.setuid: mini-me
	rm -f $@ && cp mini-me $@&& sudo chown root $@ && sudo chmod u+s $@

# Simple C test of Capsicum syscalls
SMOKETEST_OBJECTS=smoketest.o
smoketest: $(SMOKETEST_OBJECTS) $(LOCAL_LIBS)
	$(CC) $(CFLAGS) -o $@ $(SMOKETEST_OBJECTS) $(LIBCAPRIGHTS)

test: capsicum-test mini-me mini-me.noexec mini-me.setuid $(EXTRA_PROGS)
	./capsicum-test
gtest-all.o:
	$(CXX) $(CXXFLAGS) $(ARCHFLAG) -I$(GTEST_DIR)/include -I$(GTEST_DIR) $(GTEST_FLAGS) -c ${GTEST_DIR}/src/gtest-all.cc
libgtest.a: gtest-all.o
	$(AR) -rv libgtest.a gtest-all.o

clean:
	rm -rf gtest-all.o libgtest.a capsicum-test mini-me mini-me.noexec smoketest $(SMOKETEST_OBJECTS) $(OBJECTS) $(LOCAL_CLEAN) $(EXTRA_PROGS)
