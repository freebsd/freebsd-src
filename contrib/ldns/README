
Contents: 
	REQUIREMENTS
	INSTALLATION
		libdns
		examples
		drill
	INFORMATION FOR SPECIFIC OPERATING SYSTEMS
		Mac OS X
		Solaris
	KNOWN ISSUES
		pyldns
        Your Support

Project page:
http://www.nlnetlabs.nl/ldns/
On that page you can also subscribe to the ldns mailing list.

* Development 
ldns is mainly developed on Linux and FreeBSD. It is regularly tested to
compile on other systems like Solaris and Mac OS X.

REQUIREMENTS
- OpenSSL (Optional, but needed for features like DNSSEC)
  - OpenSSL >= 0.9.7f for DANE support
  - OpenSSL >= 1.0.0  for ECDSA and GOST support
- libpcap (Optional, but needed for examples/ldns-dpa)
- (GNU) libtool (in OSX, that's glibtool, not libtool)
- GNU make

INSTALLATION
1. Unpack the tarball
2. cd ldns-<VERSION>
3. ./configure --with-examples --with-drill
   (optionally compile python bindings too with: --with-pyldns)
4. make
5. make install


* Building from repository

If you are building from the repository you will need to have (gnu)
autotools like libtool and autoreconf installed. A list of all the commands
needed to build everything can be found in README.git. Note that the actual
commands may be a little bit different on your machine. Most notably, you'll
need to run libtoolize (or glibtoolize). If you skip this step, you'll get
an error about missing config.sub.

* Developers
ldns is developed by the ldns team at NLnet Labs. This team currently
consists of:
  o Willem Toorop
  o Wouter Wijngaards

Former main developers:
  o Jelte Jansen
  o Miek Gieben
  o Matthijs Mekking

* Credits
We have received patches from the following people, thanks!
  o Bedrich Kosata
  o Erik Rozendaal
  o Håkan Olsson
  o Jakob Schlyter
  o Paul Wouters
  o Simon Vallet
  o Ondřej Surý
  o Karel Slany
  o Havard Eidnes
  o Leo Baltus
  o Dag-Erling Smørgrav
  o Felipe Gasper


INFORMATION FOR SPECIFIC OPERATING SYSTEMS

MAC OS X

For MACOSX 10.4 and later, it seems that you have to set the
MACOSX_DEPLOYMENT_TARGET environment variable to 10.4 before running
make. Apparently it defaults to 10.1.

This appears to be a known problem in 10.2 to 10.4, see:
http://developer.apple.com/qa/qa2001/qa1233.html
for more information.


SOLARIS

In Solaris multi-architecture systems (which have both 32-bit and
64-bit support), it can be a bit taxing to convince the system to
compile in 64-bit mode. Jakob Schlyter has kindly contributed a build
script that sets the right build and link options. You can find it in
contrib/build-solaris.sh

KNOWN ISSUES

A complete list of currently known open issues can be found here:
https://github.com/NLnetLabs/ldns/issues

* pyldns
Compiling pyldns produces many ``unused parameter'' warnings.  Those are
harmless and may safely be ignored.
Also, when building with SWIG older than 2.0.4, compiling
pyldns produces many ``missing initializer'' warnings. Those are harmless
too.

