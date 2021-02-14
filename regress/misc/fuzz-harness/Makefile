# NB. libssh and libopenbsd-compat should be built with the same sanitizer opts.
CXX=clang++-6.0
FUZZ_FLAGS=-fsanitize=address,undefined -fsanitize-coverage=edge,trace-pc
FUZZ_LIBS=-lFuzzer

CXXFLAGS=-O2 -g -Wall -Wextra -I ../../.. $(FUZZ_FLAGS)
LDFLAGS=-L ../../.. -L ../../../openbsd-compat -g $(FUZZ_FLAGS)
LIBS=-lssh -lopenbsd-compat -lcrypto $(FUZZ_LIBS)

TARGETS=pubkey_fuzz sig_fuzz authopt_fuzz sshsig_fuzz sshsigopt_fuzz

all: $(TARGETS)

.cc.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

pubkey_fuzz: pubkey_fuzz.o
	$(CXX) -o $@ pubkey_fuzz.o $(LDFLAGS) $(LIBS)

sig_fuzz: sig_fuzz.o
	$(CXX) -o $@ sig_fuzz.o $(LDFLAGS) $(LIBS)

authopt_fuzz: authopt_fuzz.o
	$(CXX) -o $@ authopt_fuzz.o ../../../auth-options.o $(LDFLAGS) $(LIBS)

sshsig_fuzz: sshsig_fuzz.o
	$(CXX) -o $@ sshsig_fuzz.o ../../../sshsig.o $(LDFLAGS) $(LIBS)

sshsigopt_fuzz: sshsigopt_fuzz.o
	$(CXX) -o $@ sshsigopt_fuzz.o ../../../sshsig.o $(LDFLAGS) $(LIBS)

clean:
	-rm -f *.o $(TARGETS)
