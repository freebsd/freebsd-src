# NB. libssh and libopenbsd-compat should be built with the same sanitizer opts.
CXX=clang++-6.0
FUZZ_FLAGS=-fsanitize=address,undefined -fsanitize-coverage=edge,trace-pc
FUZZ_LIBS=-lFuzzer

CXXFLAGS=-O2 -g -Wall -Wextra -Wno-unused-parameter -I ../../.. $(FUZZ_FLAGS)
LDFLAGS=-L ../../.. -L ../../../openbsd-compat -g $(FUZZ_FLAGS)
LIBS=-lssh -lopenbsd-compat -lcrypto -lfido2 -lcbor $(FUZZ_LIBS)
COMMON_OBJS=ssh-sk-null.o

TARGETS=pubkey_fuzz sig_fuzz authopt_fuzz sshsig_fuzz \
	sshsigopt_fuzz privkey_fuzz

all: $(TARGETS)

.cc.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

pubkey_fuzz: pubkey_fuzz.o $(COMMON_OBJS)
	$(CXX) -o $@ pubkey_fuzz.o $(COMMON_OBJS) $(LDFLAGS) $(LIBS)

sig_fuzz: sig_fuzz.o $(COMMON_OBJS)
	$(CXX) -o $@ sig_fuzz.o $(COMMON_OBJS) $(LDFLAGS) $(LIBS)

authopt_fuzz: authopt_fuzz.o $(COMMON_OBJS)
	$(CXX) -o $@ authopt_fuzz.o $(COMMON_OBJS) ../../../auth-options.o $(LDFLAGS) $(LIBS)

sshsig_fuzz: sshsig_fuzz.o $(COMMON_OBJS)
	$(CXX) -o $@ sshsig_fuzz.o $(COMMON_OBJS) ../../../sshsig.o $(LDFLAGS) $(LIBS)

sshsigopt_fuzz: sshsigopt_fuzz.o $(COMMON_OBJS)
	$(CXX) -o $@ sshsigopt_fuzz.o $(COMMON_OBJS) ../../../sshsig.o $(LDFLAGS) $(LIBS)

privkey_fuzz: privkey_fuzz.o $(COMMON_OBJS)
	$(CXX) -o $@ privkey_fuzz.o $(COMMON_OBJS) $(LDFLAGS) $(LIBS)

clean:
	-rm -f *.o $(TARGETS)
