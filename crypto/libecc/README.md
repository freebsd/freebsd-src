[![compilation](https://github.com/libecc/libecc/actions/workflows/libecc_compilation_tests.yml/badge.svg?branch=master)](https://github.com/libecc/libecc/actions/workflows/libecc_compilation_tests.yml)
[![runtime](https://github.com/libecc/libecc/actions/workflows/libecc_runtime_tests.yml/badge.svg?branch=master)](https://github.com/libecc/libecc/actions/workflows/libecc_runtime_tests.yml)
[![crossarch](https://github.com/libecc/libecc/actions/workflows/libecc_crossarch_tests.yml/badge.svg?branch=master)](https://github.com/libecc/libecc/actions/workflows/libecc_crossarch_tests.yml)
[![python](https://github.com/libecc/libecc/actions/workflows/libecc_python_tests.yml/badge.svg?branch=master)](https://github.com/libecc/libecc/actions/workflows/libecc_python_tests.yml)
[![examples](https://github.com/libecc/libecc/actions/workflows/libecc_examples.yml/badge.svg?branch=master)](https://github.com/libecc/libecc/actions/workflows/libecc_examples.yml)


# libecc project

## Copyright and license
Copyright (C) 2017-2023

This software is licensed under a dual BSD and GPL v2 license.
See [LICENSE](LICENSE) file at the root folder of the project.

## Authors

  * Ryad BENADJILA (<mailto:ryadbenadjila@gmail.com>)
  * Arnaud EBALARD (<mailto:arnaud.ebalard@ssi.gouv.fr>)
  * Jean-Pierre FLORI (<mailto:jpflori@gmail.com>)

## Contributors
  * Nicolas VIVET (<mailto:nicolas.vivet@ssi.gouv.fr>)
  * Karim KHALFALLAH (<mailto:karim.khalfallah@ssi.gouv.fr>)
  * Niels SAMWEL (<mailto:nsamwel@google.com>)

## Description
This software implements a library for elliptic curves based
cryptography (ECC). The API supports signature algorithms specified
in the [ISO 14888-3:2018](https://www.iso.org/standard/76382.html)
standard and some other signature algorithms as well as ECDH primitives, with the following specific curves and hash functions:

  * **Signatures**:
    * Core ISO 14888-3:2018 algorithms: ECDSA, ECKCDSA, ECGDSA, ECRDSA, EC{,O}SDSA, ECFSDSA, SM2.
    * EdDSA (25519 and 448 as specified in [RFC 8032](https://datatracker.ietf.org/doc/html/rfc8032)).
    * BIGN (as standardized in [STB 34.101.45-2013](https://github.com/bcrypto/bign)). We allow a more lax usage of
    BIGN than in the standard as we allow any curve and any hash function.
    * BIP0340, also known as the "Schnorr" Bitcoin proposal, as specified in [bip-0340](https://github.com/bitcoin/bips/blob/master/bip-0340.mediawiki).
    We allow a more lax usage of BIP0340 than the standard as we allow any curve and any hash function (the standard mandates SECP256K1 with SHA-256).

  * **ECDH**:
    * ECC-CDH (Elliptic Curve Cryptography Cofactor Diffie-Hellman) as described in [section 5.7.1.2 of the NIST SP 800-56A Rev. 3](https://csrc.nist.gov/publications/detail/sp/800-56a/rev-3/final) standard.
    * X25519 and X448 as specified in [RFC7748](https://datatracker.ietf.org/doc/html/rfc7748) (with some specificities, see the details below).
  * **Curves**: SECP{192,224,256,384,521}R1, SECP{192,224,256}K1, BRAINPOOLP{192,224,256,320,384,512}{R1,T1},
  FRP256V1, GOST{256,512}, GOSTR3410-2001-CryptoPro{A,B,C,XchA,XchB,Test}-ParamSet, GOSTR3410-2012-{256,512}-ParamSet{A,B,C}, GOSTR3410-2012-256-ParamSetD, GOSTR3410-2012-512-ParamSetTest, SM2P256V1, SM2P{192,256}Test, WEI{25519,448}, BIGN{256,384,512}V1. The library can be easily expanded with
  user defined curves using a standalone helper script.
  * **Hash functions**: SHA-2 and SHA-3 hash functions (224, 256, 384, 512), SM3, RIPEMD-160,
GOST 34.11-2012 as described in [RFC 6986](https://datatracker.ietf.org/doc/html/rfc6986)
(also known as [Streebog](https://tc26.ru/en/events/research-projects-competition/streebog-competition.html)),
SHAKE256 in its restricted version with 114 bytes output (mainly for Ed448), BELT-HASH (as standardized in
[STB 34.101.31-2011](https://github.com/bcrypto/belt)), and BASH-{224,256,384,512} (as standardized in
[STB 34.101.77-2020](http://apmi.bsu.by/assets/files/std/bash-spec24.pdf)).
**HMAC** based on any of these hash functions is also included.

ECDSA comes in two variants: the classical non-deterministic one, and the **deterministic** ECDSA
as described in [RFC 6979](https://datatracker.ietf.org/doc/html/rfc6979). The deterministic version
generates nonces using a HMAC-DRBG process, and is suitable for situations where there is
no RNG or where entropy sources are considered weak (please note that any leak on these nonces
bits can lead to devastating attacks exploiting the [Hidden Number Problem](https://eprint.iacr.org/2020/615.pdf)).
On the downside, the deterministic version of ECDSA is susceptible to [fault attacks](https://eprint.iacr.org/2017/1014.pdf).
Hence, one will have to **carefully select** the suitable version to use depending on the usage and
attack context (i.e. which of side-channel attacks or fault attacks are easier to perform).
The same applies to BIGN that comes in two flavours as standardized in [STB 34.101.45-2013](https://github.com/bcrypto/bign):
non-deterministic and deterministic (following an iterative generation process using the BELT hash function and its underlying block cipher).

The library also supports EdDSA (Ed25519 and Ed448) as defined in [RFC 8032](https://datatracker.ietf.org/doc/html/rfc8032) with
all their variants (with context, pre-hashed).
Since the core of the library supports short Weierstrass curves, and as
EdDSA uses instead Twisted Edwards curves with dedicated formulas, we use
**isogenies** as described in the [lwig-curve-representations](https://datatracker.ietf.org/doc/html/draft-ietf-lwig-curve-representations)
draft. Isogenies are transformations (homomorphisms that are almost isomorphisms) between
curves models, allowing to implement operations on one model by operating with
formulas on another model. Concretely, in our case we perform computations on
the Weierstrass WEI25519 that is isogenic to Ed25519 (Twisted Edwards)
and Curve25519 (Montgomery) curves. This, of course, induces overheads in computations
while having the great benefit of keeping the library core mathematical foundations simple
and keep the defense-in-depth (regarding software security and side-channels) focused on
a rather limited part: see the discussions below on libecc efforts with regards to security.

Please note that as for deterministic ECDSA and BIGN, EdDSA signatures are trivially susceptible to
[fault attacks](https://eprint.iacr.org/2017/1014.pdf) without having a non-deterministic
variant. Hence, when using EdDSA one will have to either ensure that the usage context naturally prevents
such attacks, that the platform implements countermeasures (e.g. using secure MCUs, etc.) or that
other means allow to detect/mitigate such attacks (e.g. on the compilation toolchain side).

Please refer to [this CFRG thread](https://mailarchive.ietf.org/arch/browse/cfrg/?gbt=1&index=5l3XCLHLCVfOmnkcv4mo2-pEV94)
for more insight on why deterministic versus non-deterministic EC signature schemes is still an open debate
and how the usage context and **attack model** is **crucial** when choosing to use one or the other.


**Batch verification** is implemented for the signature algorithms that support it - the signature schemes
that preserve some "reversible" projective point coordinate information in the signature value.
This is the case for some "Schnorr" based schemes, namely ECFSDSA ("full Schnorr" from ISO14888-3), EdDSA and BIP0340.
Batch verification allows (thanks to the Bos-Coster algorithm) to bring speedups between 2 to 6.5 times
the regular verification for batches of at least 10 signatures, which is not negligible depending on the usage.
Beware that for the specific case of BIP0340, results might depend on the underlying prime of the curve, since
the batch verification makes heavy use of square root residues and the Tonelli-Shanks algorithm complexity
is sensitive to the prime "form" (e.g. is equal to 1 modulo 4, etc.). Finally, beware that the speedup of
batch verification comes at an increased memory cost: the Bos-Coster algorithm requires a scratchpad memory space
that increases linearly with the number of signatures to be checked.


Regarding the specific case of ECRDSA (the Russian standard), libecc implements by default the
[RFC 7091](https://datatracker.ietf.org/doc/html/rfc7091) and [draft-deremin-rfc4491-bis](https://datatracker.ietf.org/doc/html/draft-deremin-rfc4491-bis)
versions to comply with the standard test vectors (provided in the form of X.509 certificates).
This version of the algorithm **differs** from the ISO/IEC 14888-3 description and test vectors,
the main difference coming from the way the hash of the message to be signed/verified is processed:
in the RFCs, the little endian representation of the hash is taken as big number while in ISO/IEC the big endian
representation is used. This seems (to be confirmed) to be a discrepancy of ISO/IEC 14888-3 algorithm description
that must be fixed there. In order to allow users to still be able to reproduce the ISO/IEC behavior, we provide
a compilation toggle that will force this mode `USE_ISO14888_3_ECRDSA=1`:

<pre>
	$ USE_ISO14888_3_ECRDSA=1 make
</pre>

**ECDH (Elliptic Curve Diffie-Hellman)** variants are also implemented in the
library. Classical ECDH over Weierstrass curves is implemented in the form
of ECC-CDH (Elliptic Curve Cryptography Cofactor Diffie-Hellman) as described
in [section 5.7.1.2 of the NIST SP 800-56A Rev. 3](https://csrc.nist.gov/publications/detail/sp/800-56a/rev-3/final) standard. Montgomery curves
based algorithms (Curve25519 and Curve448) are included as specified in [RFC7748](https://datatracker.ietf.org/doc/html/rfc7748),
although the implementation somehow diverges from the canonical ones as u coordinates on the curve
quadratic twist are rejected (this is due to the underlying usage of isogenies to
handle Montgomery curves). This divergence does not impact the ECDH use case though.


Advanced usages of this library also include the possible implementation
of elliptic curve based protocols as well as any algorithm
on top of prime fields based elliptic curves (or prime fields, or rings
of integers). Many examples are present in the [src/examples](src/examples)
folder, notable ones being:
  * Pollard-Rho, Miller-Rabin and square residues over finite fields.
  * The RSA cryptosystem as defined in the PKCS#1 [RFC8017](https://datatracker.ietf.org/doc/html/rfc8017)
standard. This implementation also comes with the integration of deprecated hash
functions such as MD2, MD4, MD5, SHA-0, SHA-1, MDC-2, GOSTR34-11-94 and so on in order to be compliant with existing
signatures (e.g. in X.509). These primitives are **not** included in the core
library on purpose: they are **dangerous and broken** and must only be used for
tests purposes.
  * The DSA cryptosystem as defined in [FIPS 186-4](https://csrc.nist.gov/publications/detail/fips/186/4/final).
  * The SDSA (Schnorr DSA) as defined in ISO14888-3
  * The KCDSA (Korean DSA) as defined in ISO14888-3
  * The GOSTR34-10-94 function as defined in [RFC4491](https://www.rfc-editor.org/rfc/rfc4491)
  * The SSS (Shamir Secret Sharing) algorithm over a prime field of 256 bits.


**NOTE**: for all the primitives (specifically relevant for signature primitives), a maximum
allowed size for big numbers is **4096 bits** with word size **64 bits** (this will be less
for word sizes 16 and 32 bits). This is due to an internal limitation of libecc
on big numbers allocation documented [here](include/libecc/nn/nn_config.h). We can live with
this limitation as the library is primarily intended to focus on ECC based algorithms.
However, one should be aware that for example RSA with modulus > 4096 will fail (as well
and DSA and other El-Gamal based algorithms): these primitives are only included as
examples and should be used with care.

**NOTE**: handling 4096 bits NN numbers must be explicitly configured at compilation
time using the `-DUSER_NN_BIT_LEN=4096` toggle in the `CFLAGS` or `EXTRA_CFLAGS` as explained
in [the dedicated section](https://github.com/ANSSI-FR/libecc#modifying-the-big-numbers-size).


Compared to other cryptographic libraries providing such
features, the differentiating points are:

  * A focus on code readability and auditability. The code is pure C99,
	with no dynamic allocation and includes pre/post-asserts in the code.
	Hence, this library is a good candidate for embedded targets (it should be
	easily portable accross various platforms).
  * A clean layer separation for all needed mathematical abstractions and
	operations. Strong typing (as "strong" as C99 allows, of course) of
	mathematical objects has been used in each layer.
  * The library has NOT been designed to break performance records, though
	it does a decent job (see the [performance section discussion](#performance)). Similarly,
	the library memory footprint (in terms of ROM and RAM usage) is not the
	smallest achievable one (though some efforts have been made to limit it
	and fit "common" platforms, see the [dedicated section](#constrained-devices)).
  * libecc library core has **no external dependency** (not even the standard
	libc library) to make it portable. See the
	[section about portability](#compatibility-and-portability) for more information.

## Building

### Building the static libraries and the signature self tests

The main [Makefile](Makefile) is in the root directory, and compiling is as simple as
executing:

<pre>
	$ make
</pre>

By default, compilation is quiet. **Verbose compilation** (i.e. showing all the compilation
executed commands) can be achieved using the `VERBOSE=1` toggle:

<pre>
	$ VERBOSE=1 make
</pre>

This will compile different elements in the [build](build/) directory:

  * Three **archive** static libraries, each one containing (based on) the previous ones:
	* **libarith.a**: this library contains the Natural Numbers (NN) and Finite field over primes
        (Fp) arithmetic layers.
	* **libec.a**: this library is based on libarith.a and contains the EC curves implementation
	(points abstraction, point addition/doubling formulas and scalar multiplication).
	* **libsign.a**: this library is based on libec.a and contains all our ISO 14888-3 signature
	algorithms over some statically defined curves and hash functions.
  * Two binaries based on the libsign.a static library:
	* **ec\_self\_tests**: the self tests for signature/verification algorithm of ISO 14888-3
	with known and random test vectors, as well as performance tests. Launching the self tests without
	an argument will execute the three tests (known and fixed test vectors, random sign/verify
	checks, and performance measurements). One can also launch each test separately.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; For known test vectors:
<pre>
	$ ./build/ec_self_tests vectors
	======= Known test vectors test ===================
	[+] ECDSA-SHA224/secp224r1 selftests: known test vectors sig/verif ok
	[+] ECDSA-SHA256/secp256r1 selftests: known test vectors sig/verif ok
	[+] ECDSA-SHA512/secp256r1 selftests: known test vectors sig/verif ok
	...
</pre>

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; For sign/verify checks (with random key pairs and random data):

<pre>
	$ ./build/ec_self_tests rand
	======= Random sig/verif test ===================
	[+]  ECDSA-SHA224/FRP256V1 randtests: random import/export with sig(0)/verif(0) ok
	[+] ECDSA-SHA224/SECP224R1 randtests: random import/export with sig(0)/verif(0) ok
	...
</pre>

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; For performance measurements:

<pre>
	$ ./build/ec_self_tests perf
	======= Performance test =====================
	[+]  ECDSA-SHA224/FRP256V1 perf: 462 sign/s and 243 verif/s
	[+] ECDSA-SHA224/SECP224R1 perf: 533 sign/s and 276 verif/s
	...
</pre>

**NOTE**: it is possible to parallelize self tests (known and random) using the
[OpenMP](https://en.wikipedia.org/wiki/OpenMP) framework (usually packaged with
most distros) by using the `OPENMP_SELF_TESTS=1` compilation toggle. This requires
the `WITH_STDLIB` option (as it obviously uses the standard library). Performance
tests are not parallelized due to possible shared ressources exhaustion between CPUs and cores
(e.g. caches, Branch Prediction Units, etc.).

- **ec\_utils**: a tool for signing and verifying user defined files, with a user
provided signature algorithm/curve/hash function triplet. The tool can also be
used to generate signature keys.

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; Generate keys for ECKCDSA over the BRAINPOOLP512R1 curve, with
the 'mykeypair' prefix:
<pre>
	$ ./build/ec_utils gen_keys BRAINPOOLP512R1 ECKCDSA mykeypair
</pre>

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; This will create four files. Two
binary '.bin' files corresponding to the private key (mykeypair\_private\_key.bin)
and the public key (mykeypair\_public\_key.bin). Two header '.h' files are also
created, corresponding to a C style header version of the keys so that these can
be included and used in a C program using libecc. Note that both kind of keys
(public and private) include leading metadata (type, algorithm, curve, etc) for
possible sanity checks when they are used (e.g. to detect passing of an ECDSA
private key to an ECKCDSA signature call, etc).

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; Once the key pair has been created,
one can sign a raw binary file named 'myfile' and store the signature in
'sig.bin'. In the example below, we use SHA3\_512 as the hash function for
the signature. BRAINPOOLP512R1 and ECKCDSA are explicitly given (matching the
type of key we generated during previous step). Note that the call would yield
an error if invalid parameters were given (thanks to the metadata elements
described above).
<pre>
	$ ./build/ec_utils sign BRAINPOOLP512R1 ECKCDSA SHA3_512 myfile mykeypair_private_key.bin sig.bin
</pre>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; After this, a raw signature is created, mainly consisting of the ECKCDSA (r, s) big
numbers concatenated (the length of this file should be 1024 bits = 2 x 512 bits). The signature can now be verified with
the 'verify' command and the public key, the result being either **OK** or **failed**:
<pre>
	$ ./build/ec_utils verify BRAINPOOLP512R1 ECKCDSA SHA3_512 myfile mykeypair_public_key.bin sig.bin
	  Signature check of myfile OK
</pre>

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; The ec\_utils tool can also be used to produce/verify **structured binaries**
containing a header, raw binary and their signature (see the 'struct\_sign' and 'struct\_verify' commands for a help on
this mode). The rationale behind these commands is to ease the production/verification of self-contained signed images
(which can be useful when dealing with embedded firmware updates for instance).

### Building the user examples

Since it is possible to use libecc as a NN (**positive** Natural Numbers), Fp (Finite field over primes) or EC curve layer library,
we provide some examples in the [src/examples](src/examples) folder. Compiling these examples is as simple as:
<pre>
	$ cd src/examples
	$ make
</pre>

* NN layer examples:
  * [src/examples/basic/nn&lowbar;miller&lowbar;rabin.c](src/examples/basic/nn_miller_rabin.c): this example implements the
    [Miller-Rabin](https://en.wikipedia.org/wiki/Miller%E2%80%93Rabin_primality_test) composition (or probabilistic primality) test as
    described in the [Handbook of Applied Cryptography (4.29)](http://cacr.uwaterloo.ca/hac/about/chap4.pdf).
  * [src/examples/basic/nn&lowbar;pollard&lowbar;rho.c](src/examples/nn_pollard_rho.c): this example is a straightforward
    implementation of the [Pollard's Rho](https://en.wikipedia.org/wiki/Pollard%27s_rho_algorithm) integer factorization
    algorithm as specified in the [Handbook of Applied Cryptography (3.9)](http://cacr.uwaterloo.ca/hac/about/chap3.pdf).

* Fp layer examples:
  * [src/examples/basic/fp&lowbar;square&lowbar;residue.c](src/examples/basic/fp_square_residue.c): this is an implementation of
  the [Tonelli-Shanks](https://en.wikipedia.org/wiki/Tonelli%E2%80%93Shanks_algorithm) algorithm for finding quadratic residues
  over a prime field Fp. Given a prime field element x, the algorithm finds y so that y<sup>2</sup> = x (or informs that there
  is no solution if this is the case).

* Curves layer examples:
  * [src/examples/basic/curve&lowbar;basic&lowbar;examples.c](src/examples/basic/curve_basic_examples.c): this example shows basic
  features of libec for playing with elliptic curves group arithmetic, namely loading defined named curves, generating random points on
  these curves, checking point addition and doubling formulas as well as scalar multiplication (both Montgomery and non Montgomery based).
  
  * [src/examples/basic/curve&lowbar;ecdh.c](src/examples/basic/curve_ecdh.c): the purpose of this code is to provide a toy example of
  how to implement an [Elliptic Curve Diffie-Hellman](https://en.wikipedia.org/wiki/Elliptic_curve_Diffie%E2%80%93Hellman) protocol between two
  entities 'Alice' and 'Bob' in order to produce a shared secret over a public channel.

**WARNING**: these examples are **toy implementations** not to be used in a production environment (for instance, the code
has neither been designed to be efficient nor robust against side channel attacks). Their purpose is only to show basic usage of the
libarith and libec libraries.

The **public headers** containing the functions to be used by higher level code are [include/libecc/libarith.h](include/libecc/libarith.h),
[include/libecc/libec.h](include/libecc/libec.h) and [include/libecc/libsig.h](include/libecc/libsig.h): they are respectively used for the NN and Fp arithmetic layers,
the Elliptic Curves layer, and the signature layer.

More advanced examples are present in the examples folder:

* Obsolete hash algorithms as an expansion to libecc core algorithms, in [src/examples/hash](src/examples/hash) (MD2, MD4, MD5, MDC2, SHA-0,
SHA-1, and TDES for supporting MDC2). Please **be careful** when using them, it is advised to use them as toy primitives in **non-production code**
(e.g. for checking old protocols and cipher suites).

* Pre-ECC Signature schemes (based on Fp finite fields discrete logarithm) in [src/examples/sig](src/examples/sig) (RSA, DSA, SDSA, KCDSA,
GOSTR34-10-94). Beware that for these signatures, you will have to expand the NN size to bigger values than the default (e.g. supporting RSA 4096
will need a size of at least 4096 bits for NN, see how to expand the size in the documentation [here](include/libecc/nn/nn_config.h)). Although some
efforts have been made when developing these signature algorithms, using them in production code should be decided with care (e.g. regarding
side-channel attack and so on).

* SSS (Shamir Secret Sharing) in [src/examples/sss](src/examples/sss).


### Building the NN and Fp arithmetic tests

libecc is provided with arithmetic random tests for the low level NN and Fp routines (addition, subtraction, logical
operations, multiplication and Montgomery multiplication, ...).

These tests are located inside the [src/arithmetic&lowbar;tests/](src/arithmetic_tests/) folder. More specifically, the tests
are split in two files:

* [src/arithmetic&lowbar;tests/arithmetic&lowbar;tests.c](src/arithmetic_tests/arithmetic_tests.c): a '.c' file to be compiled and linked with libecc
static library and performing a set of tests given on the standard input or in a file. The tests have a specific ASCII
format with expected input/output as big numbers, and crafted **opcodes** defining the operation type (addition over
NN, over Fp, ...).
* [src/arithmetic&lowbar;tests/arithmetic&lowbar;tests&lowbar;generator.py](src/arithmetic_tests/arithmetic_tests_generator.py): a python
script that generates a set of arithmetic tests.

### Building with the meson build system

In parallel to the `Makefile` build system, a migration to the newer and more user friendly `meson` build is a
**work in progress**. Compiling with `meson` can be simply achieved with:

<pre>
	$ meson setup builddir && cd builddir && meson dist
</pre>

Please note that you will need `meson`, `ninja` and `dunamai` (that can be installed from the Python `pip` installer).

Most of libecc compilation options have been migrated, please check the output of the `meson configure` command to get
a complete list of these (in the 'Project options' category). For instance, compiling libecc with a word size of 32 and
a debug mode can be triggered with:

<pre>
	$ meson setup -Dwith_wordsize=32 -Dwith_debug=true builddir && cd builddir && meson dist
</pre>

## Configuring the libecc library

### Basic configuration

libecc can be statically configured at compilation time: the user can tune what curves, hash functions and signature
algorithms are embedded in 'libsign.a' and all the binaries using it.

The main entry point to configure/tune the library is [include/libecc/lib&lowbar;ecc&lowbar;config.h](include/libecc/lib_ecc_config.h). By default libecc
embeds everything. In order to remove something, one has to **comment** the element to remove (i.e. comment the
`WITH_XXX` macro). For instance, removing
FRP256V1 is simply done by commenting the line:
<pre>
	/* Supported curves */
	/* #define WITH_CURVE_FRP256V1 */ /* REMOVING FRP256V1 */
	#define WITH_CURVE_SECP192R1
	#define WITH_CURVE_SECP224R1
	#define WITH_CURVE_SECP256R1
	#define WITH_CURVE_SECP384R1
	#define WITH_CURVE_SECP521R1
	#define WITH_CURVE_BRAINPOOLP224R1
	#define WITH_CURVE_BRAINPOOLP256R1
	#define WITH_CURVE_BRAINPOOLP384R1
	#define WITH_CURVE_BRAINPOOLP512R1
	#define WITH_CURVE_GOST256
	#define WITH_CURVE_GOST512
	...
</pre>

As another example, if one wants to build a custom project supporting only
ECFSDA using SHA3-256 on BrainpoolP256R1, this can be done by keeping only the
following elements in
[include/libecc/lib&lowbar;ecc&lowbar;config.h](include/libecc/lib_ecc_config.h):

<pre>
	#define WITH_SIG_ECFSDSA
	#define WITH_HASH_SHA3_256
	#define WITH_CURVE_BRAINPOOLP256R1
</pre>

### Advanced configuration

#### Modifying the word size

libecc supports 16, 32 and 64 bits word sizes. Though this word size is usually inferred during compilation
and adapted depending on the detected platform (to fit the best performance), the user can force it in three ways:

* Overloading the `WORDSIZE` macro in [include/libecc/words/words.h](include/libecc/words/words.h).
* Overloading the `WORDSIZE` macro in the Makefile `CFLAGS`.
* Use specific Makefile targets.

Please refer to the [portability guide](#libecc-portability-guide) for details on this.

#### Modifying the big numbers size

libecc infers the Natural Numbers maximum length from the **curves** parameters that have been statically
defined in [include/libecc/lib&lowbar;ecc&lowbar;config.h](include/libecc/lib_ecc_config.h). Though this behaviour is perfectly fine and transparent
for the user when dealing with the elliptic curves and signature layers, this can become a limitation when building
code around the NN and Fp arithmetic layers. The user will be stuck with a hard coded maximum size of numbers depending
on the curve that is used by libecc, which can be a nonsense if he is only interested in the big number basic
algorithmic side (when the default curves are used, this maximum size is 521 bits, corresponding to SECP521
parameters).

libecc provides a way to **overload the NN maximum size**, with a strong limit depending on the word size (around
5300 bits for 64-bit words, around 2650 bits for 32-bit words, and around 1300 bits for 16-bit words). See
the comments in [include/libecc/nn/nn&lowbar;config.h](include/libecc/nn/nn_config.h) for more details about this. In order to manually increase
the NN size, the user will have to define the macro `USER_NN_BIT_LEN`, either directly in
[include/libecc/nn/nn&lowbar;config.h](include/libecc/nn/nn_config.h), or more appropriately through overloading the Makefile `CFLAGS`
with `-DUSER_NN_BIT_LEN=` (see [the dedicated section](#overloading-makefile-variables) for more on how to do this).

**NOTE**: objects and binaries compiled with different word sizes and/or user defined NN maximum bit lengths **are not compatible**,
and could produce executables with dangerous runtime behaviour. In order to prevent possible honest mistakes, there is
a safety net function catching such situations **at compilation time** in [include/libecc/nn/nn&lowbar;config.h](include/libecc/nn/nn_config.h): the
`nn_check_libconsistency` routine will throw an error. For instance, if 'libarith.a' has been compiled with
`WORDSIZE=64`, and one tries to compile the arithmetic tests with `WORDSIZE=32`, here is the error the compiler
should produce:

<pre>
...
arithmetic_tests.c:(.text+0x3af21) : undefined reference to « nn_consistency_check_maxbitlen521wordsize32 »
...
</pre>

#### Small RAM footprint devices (small stack usage)

In order to squeeze the stack usage on very constrained devices, a `SMALLSTACK` toggle can be activated. Beware that this toggle
removes some countermeasures (Itoh et al. masking) in order to fit in 8KB of RAM stack usage. Also beware that this is incompatible
with EdDSA and X25519 as these specific functions need more than this amount of stack because of the isogenies usage (you should get
a compilation error when trying to activate them with `SMALLSTACK=1`).

<pre>
$ SMALLSTACK=1 make
</pre>


## Expanding the libecc library

Though libecc has been designed to be compiled with a static embedding of all its features (i.e. no dynamic modules
loading), its **static code extensibility** has been a matter of attention. The library can be:
* Easily expanded by **adding new curves**, with **zero coding effort**. Note that **only curves over prime fields are
supported**.
* Expanded with new hash functions and new signature algorithms with some coding effort, but clean and well defined
APIs should ease this task.

### Adding user defined curves

A companion python script [scripts/expand&lowbar;libecc.py](scripts/expand_libecc.py) will transparently add (and remove) new
user defined curves in the source tree of the project. The '.h' headers defining the new curves
will be created in a dedicated folder: [include/libecc/curves/user&lowbar;defined/](include/libecc/curves/user_defined/).

The python script should have a self explanatory and complete help:
<pre>
	$ python scripts/expand_libecc.py -h
	This script is intented to *statically* expand the ECC library with user defined curves.
	...
</pre>

In order to add a curve, one can give explicit parameters (prime, order, ...) on the command line or
provide a [RFC3279](https://www.ietf.org/rfc/rfc3279.txt) formatted ASN.1 file (DER or PEM) with the
parameters. Sanity checks are performed by the script. The script is also able to generate
**test vectors** for the new curve with the `--add-test-vectors` toggle.

Let's show how we can add the BRAINPOOLP320R1 supported by OpenSSL. We use the `ecparam` option of
the `openssl` command line:

<pre>
	$ openssl ecparam -param_enc explicit -outform DER -name brainpoolP320r1 -out brainpoolP320r1.der
</pre>

This creates a DER file 'brainpoolP320r1.der' embedding the parameters (beware of the `-param_enc explicit`
option that is important here). Now, in order to add this new curve to libecc, we will execute:
<pre>
	$ python scripts/expand_libecc.py --name="mynewcurve" --ECfile=brainpoolP320r1.der --add-test-vectors=1
	Test vectors generation asked: this can take some time! Please wait ...
	1/56
</pre>

This will create a new header file 'ec&lowbar;params&lowbar;user&lowbar;defined&lowbar;mynewcurve.h' in the [include/libecc/curves/user&lowbar;defined/](include/libecc/curves/user_defined/)
folder, and it will modify some libecc core files to transparently add this curve for the next compilation (modified files 
are [include/libecc/curves/curves&lowbar;list.h](include/libecc/curves/curves_list.h), [src/tests/ec&lowbar;self&lowbar;tests&lowbar;core.h](src/tests/ec_self_tests_core.h),
[include/libecc/lib&lowbar;ecc&lowbar;config.h](include/libecc/lib_ecc_config.h) and [include/libecc/lib&lowbar;ecc&lowbar;types.h](include/libecc/lib_ecc_types.h)).

The test vectors generation can take some time since all the possible triplets (curve, hash function, signature algorithm) are
processed with the new curve.

After compiling the library, the new curve should show up in the self tests:
<pre>
	$ ./build/ec_self_tests
	======= Known test vectors test =================
	...
	[+] ECDSA_SHA224_USER_DEFINED_MYNEWCURVE_0 selftests: known test vectors sig/verif ok
	...
	======= Random sig/verif test ===================
	...
	[+] ECDSA-SHA224/USER_DEFINED_MYNEWCURVE randtests: random import/export with sig/verif ok
	...
	======= Performance test ========================
	...
	[+] ECDSA-SHA224/USER_DEFINED_MYNEWCURVE perf: 269 sign/s and 141 verif/s
	...
</pre>

It should also appear in the `ec_utils` help:
<pre>
	$ ./build/ec_utils sign
	Bad args number for ./build/ec_utils sign:
	arg1 = curve name: FRP256V1 USER_DEFINED_MYNEWCURVE ...
	arg2 = signature algorithm type: ECDSA ...
	arg3 = hash algorithm type: SHA224 ...
	arg4 = input file to sign
	arg5 = input file containing the private key (in raw binary format)
	arg6 = output file containing the signature
        <arg7 (optional) = ancillary data to be used>
</pre>

It is possible to remove a user defined curve by using the python script and its name:
<pre>
	$ python scripts/expand_libecc.py --remove --name mynewcurve
	You asked to remove everything related to user defined mynewcurve curve. Enter y to confirm, n to cancel [y/n]. y
	Removing user defined curve mynewcurve ...
</pre>

It is also possible to remove **all** the user defined curves at once:
<pre>
	$ python scripts/expand_libecc.py --remove-all
</pre>

Finally, two companion shell scripts are provided along with the expanding python script in order to show its basic usage:

* [scripts/gen&lowbar;curves&lowbar;tests.sh](scripts/gen_curves_tests.sh): this script generates the default libecc curves
with explicit parameters given on the command line. Of course, since these curves are already embedded in
libecc, there is no real use of generating them - the script is only here to serve as a showcase for expanding
the library with explicit parameters.
* [scripts/gen&lowbar;openssl&lowbar;curves&lowbar;tests.sh](scripts/gen_openssl_curves_tests.sh): this script enumerates all OpenSSL
named curves, generates a DER file with their parameters, and adds them to libecc.

### Adding new hash and signature algorithms

Obviously, adding new algorithms (hash or signature) will require adding new code.

#### Adding new hash functions
We detail hereafter the necessary steps to add a new hash function. The main file listing all the hash functions is [include/libecc/hash/hash&lowbar;algs.h](include/libecc/hash/hash_algs.h). The new hash
algorithm should be added here in compliance with the API described in the `hash_mapping struct`. This API includes:

  * The digest and block sizes and a pretty print name for the algorithm.
  * `hfunc_init`: the hash function initialization routine.
  * `hfunc_update`: the hash function update routine.
  * `hfunc_finalize`: the hash function finalization routine.
  * `hfunc_scattered`: this function applies the hash function (i.e. compute the digest) on multiple messages
  (it takes as input an array of pointers to message chunks, and an array of sizes).

These libecc API functions are in fact redirections to the core routines of the hash algorithm, and
the user is expected to add the specific implementation in '.c' and '.h' files inside the [src/hash/](src/hash/)
folder. See [src/hash/sha224.c](src/hash/sha224.c) and [include/libecc/hash/sha224.h](include/libecc/hash/sha224.h) for a practical
example of how to do this with SHA-224.

Finally, the user is expected to update the libecc main configuration file [include/libecc/lib&lowbar;ecc&lowbar;config.h](include/libecc/lib_ecc_config.h)
with the `WITH_MY_NEW_HASH` toggle ('my&lowbar;new&lowbar;hash' being the new hash function).

#### Adding new signature algorithms
In order to add a new elliptic curve based signature algorithm, here is the needed work:
* The main file listing all the signature algorithms is [include/libecc/sig/sig&lowbar;algs&lowbar;internal.h](include/libecc/sig/sig_algs_internal.h).
The signature algorithm should be added in compliance with the API described in the `ec_sig_mapping struct`. This
API includes:
  * The signature type and a pretty print name.
  * `siglen`: a function giving the length of the produced signature.
  * `init_pub_key`: a routine producing a public key when given a corresponding private key.
  * `sign_init`, `sign_update` and `sign_finalize`: the usual functions initializing a signature, updating it with
   input buffers, and finalizing it to produce an output signature.
  * `verify_init`, `verify_update` and `verify_finalize`: the usual functions initializing a signature verification, updating
   it with input buffers, and finalizing it to produce a check status (i.e. signature OK or not OK).

These libecc APIs have to be plugged to the core signature functions, and the user is expected to handle this implementation
with adding the specific '.c' files inside the [src/sig](src/sig) folder and the specific '.h' files inside the [include/libecc/sig](include/libecc/sig) folder. See [src/sig/ecdsa.c](src/sig/ecdsa.c) and [include/libecc/sig/ecdsa.h](include/libecc/sig/ecdsa.h) for a practical example of how to do this with ECDSA.

Finally, the user is expected to update the libecc main configuration file [include/libecc/lib&lowbar;ecc&lowbar;config.h](include/libecc/lib_ecc_config.h)
with the `WITH_MY_NEW_SIGN_ALG` toggle ('my&lowbar;new&lowbar;sign&lowbar;alg' being the new signature algorithm).

## <a name="performance"></a> Performance

As already stated, libecc has not been designed with performance in mind, but
with **simplicity** and **portability** as guiding principles; this implies
several things when it comes to performance:

* libecc does not intend to compete with libraries developed with platform specific accelerations, such as the use of **assembly**
routines or the adaptation to CPUs quirks at execution time (e.g. a CPU with very slow shift instructions). [OpenSSL](https://www.openssl.org/)
is an example of such libraries with good and homogeneous performance in mind on most heterogeneous platforms (with the lack of
portability on very small embedded platforms though).
* Some algorithmic tricks on specific prime curves are not implemented: the same algorithms are used for all the curves.
This means for instance that curves using pseudo-Mersenne primes (such as NIST's SECP curves) won't be faster than
curves using generic random primes (such as Brainpool curves), though pseudo-Mersenne primes can benefit from a
dedicated reduction algorithm, yielding **orders of magnitude faster field arithmetic** (around five to ten times faster). See
[here](https://tls.mbed.org/kb/cryptography/elliptic-curve-performance-nist-vs-brainpool) for further discussions on this.
Consequently, we will only focus on performance comparison with other libraries using the Brainpool curves.
* We use a very straightforward elliptic curve arithmetic implementation, without using literature generic algorithmic optimizations
such as [windowing](https://en.wikipedia.org/wiki/Elliptic_curve_point_multiplication#Windowed_method) or
[fixed-base comb](https://link.springer.com/chapter/10.1007/3-540-45537-X_21) precomputations.

Nonetheless and despite all these elements, **libecc is on par with some other general purpose and portable cryptographic
libraries** such as [mbedTLS](https://tls.mbed.org) (see the performance figures given below).

We present hereafter the ECDSA performance comparison of libecc with mbedTLS and OpenSSL on various platforms representing
different CPU flavours. Here are some information about the tested version when not stated otherwise:

* mbedTLS: stable version 2.4.2, the figures have been gathered with the builtin benchmark.
* OpenSSL: debian packaged version 1.1.0e. Since OpenSSL builtin ECDSA benchmark does not handle Brainpool curves,
a basic C code using "named curves" have been compiled against the installed dynamic library.

### Performance oriented platforms

* **Core i7-5500U** (Broadwell family) is a typical x86 mid-range current laptop CPU.
* **Xeon E3-1535M** (Skylake family) is a typical x86 high-end CPU.
* **Power-7** is a typical server CPU of the previous generation (2010) with
a PowerPC architecture.

For all the platforms in this subsection, the CPUs have been tested in 64-bit mode.


| **libecc**      | Core i7-5500U @ 2.40GHz     | Xeon E3-1535M v5 @ 2.90GHz    | Power-7                   |
|-----------------|:----------------------------|:------------------------------|:--------------------------|
| BP256R1         | 583 sign/s - 300 verif/s    | 700 sign/s - 355 verif/s      | 213 sign/s - 110 verif/s  |
| BP384R1         | 231 sign/s - 118 verif/s    | 283 sign/s - 150 verif/s      | 98 sign/s  - 50 verif/s   |
| BP512R1         | 111 sign/s - 56 verif/s     | 133 sign/s - 68 verif/s       | 51 sign/s  - 26 verif/s   |

| **mbedTLS**     | Core i7-5500U @ 2.40GHz     | Xeon E3-1535M v5 @ 2.90GHz    | Power-7                   |
|-----------------|:----------------------------|:------------------------------|:--------------------------|
| BP256R1         | 426 sign/s - 106 verif/s    | 552 sign/s - 141 verif/s      | 178 sign/s - 45 verif/s   |
| BP384R1         | 239 sign/s - 56 verif/s     | 322 sign/s - 77 verif/s       | 44 sign/s  - 23 verif/s   |
| BP512R1         | 101 sign/s - 26 verif/s     | 155 sign/s - 34 verif/s       | 38 sign/s  - 12 verif/s   |

| **OpenSSL**     | Core i7-5500U @ 2.40GHz     | Xeon E3-1535M v5 @ 2.90GHz    | Power-7                   |
|-----------------|:----------------------------|:------------------------------|:--------------------------|
| BP256R1         | 2463 sign/s - 1757 verif/s    | 2873 sign/s - 2551 verif/s  | 1879 sign/s - 1655 verif/s|
| BP384R1         | 1091 sign/s - 966 verif/s     | 1481 sign/s - 1265 verif/s  | 792 sign/s  -  704 verif/s|
| BP512R1         | 727 sign/s - 643 verif/s      | 1029 sign/s - 892 verif/s   | 574 sign/s  -  520 verif/s|

### Embedded platforms with moderate constraints
* **Marvel Armada A388** is a good representative of moderately constrained embedded devices, such as
IAD (Internet Access Devices), NAS (Network Attached Storage), STB (Set Top Boxes) and smartphones.
This SoC is built around a Cortex-A9 ARMv7-A 32-bit architecture.
* **BCM2837** is a Broadcom SoC built around the recent 64-bit ARMv8-A architecture, with a
Cortex-A53 core. This SoC can be found in the Raspberry Pi 3, and also represents what can
be found in recent Smartphones.
* **Atom D2700** is a small x86 CPU typically embedded in NAS devices. Though its "embedded"
coloration, it uses a 64-bit mode that we have tested here.

| **libecc**      | Marvell A388 @ 1.6GHz | BCM2837 (aarch64) @ 1.2GHz | Atom D2700 @ 2.13GHz   |
|-----------------|:----------------------|----------------------------|:-----------------------|
| BP256R1         | 64 sign/s - 33 verif/s| 43 sign/s - 22 verif/s     | 68 sign/s - 35 verif/s |
| BP384R1         | 24 sign/s - 12 verif/s| 17 sign/s - 9 verif/s      | 25 sign/s - 13 verif/s |
| BP512R1         | 11 sign/s - 5 verif/s | 8 sign/s - 4 verif/s       | 12 sign/s - 6 verif/s  |

| **mbedTLS**     | Marvell A388 @ 1.6GHz | BCM2837 (aarch64) @ 1.2GHz | Atom D2700 @ 2.13GHz   -|
|-----------------|:----------------------|----------------------------|:------------------------|
| BP256R1         | 33 sign/s - 8 verif/s   | 14 sign/s - 4 verif/s      | 87 sign/s - 22 verif/s|
| BP384R1         | 20 sign/s - 4 verif/s   | 8 sign/s - 2 verif/s       | 50 sign/s - 11 verif/s|
| BP512R1         | 10 sign/s - 2 verif/s   | 4 sign/s - 1 verif/s       | 23 sign/s - 5 verif/s |

| **OpenSSL**     | Marvell A388 @ 1.6GHz   | BCM2837 (aarch64) @ 1.2GHz |  Atom D2700 @ 2.13GHz   |
|-----------------|:------------------------|----------------------------|:------------------------|
| BP256R1         | 369 sign/s - 332 verif/s| 124 sign/s - 112 verif/s   | 372 sign/s - 334 verif/s|
| BP384R1         | 102 sign/s - 94 verif/s | 54 sign/s - 49 verif/s     | 163 sign/s - 149 verif/s|
| BP512R1         | 87 sign/s - 81 verif/s  | 31 sign/s - 29 verif/s     |  92 sign/s - 83 verif/s |


### <a name="constrained-devices"></a> Very constrained embedded devices
The library, when configured for a 256-bit curve (SECP256R1, FRP256), SHA-256 and ECDSA signature fits in around
**30 Kilo Bytes of flash/EEPROM**, and uses around **8 Kilo Bytes of RAM** (stack) with variations depending on the
chosen WORDSIZE (16, 32, 64), the compilation options (optimization for space `-Os` or speed `-O3`) and the
target (depending on the instructions encoding, produced binary code can be more or less compact).
A 521-bit curve with SHA-256 hash function and ECDSA signature should fit in 38 Kilo Bytes of flash and around
16 Kilo Bytes of RAM (stack), with the same variations depending on the WORDSIZE and the compilation options.

**Note**: libecc does not use any heap allocation, and the only global variables used are the **constant ones**. The
constant data should end up in the flash/EEPROM section with a read only access to them: no RAM memory should
be consumed by these. The libecc read/write data are only made of local variables on the stack. Hence, RAM
consumption (essentially made of arrays representing internal objects such as numbers, point on curves ...)
should be reasonably constant across platforms. **However**, some platforms using the
**[Harvard architecture](https://en.wikipedia.org/wiki/Harvard_architecture)** (as opposed to Von Neumann's one)
can have big limitations when accessing so called "program memory" as data. The 8-bit
[Atmel AVR](http://www.atmel.com/products/microcontrollers/avr/) MCU
is such an example. Compilers and toolchains for such architectures usually copy read only data in RAM at run time,
and/or provide [non-standard ways](http://www.atmel.com/webdoc/avrlibcreferencemanual/pgmspace_1pgmspace_strings.html)
to access read only data in flash/EEPROM program memory (through specific macros, pragmas, functions). The
first case means that the RAM consumption will increase for libecc compared to the stack only usage (because of
the runtime copy). The second case means that libecc code will have to be **adapted** to the platform if the user
want to keep RAM usage at its lowest. In any case, tracking where `const` qualified data reside will be important
when the amount of RAM is a critical matter.

A full software stack containing a known test vector scenario has been compiled and tested on a **Cortex-M0**
([STM32F030R8T6](http://www.st.com/en/microcontrollers/stm32f030r8.html) @ 48MHz with 64KB of flash and 8KB of RAM).
It has also been compiled and tested on a **Cortex-M3** ([STM32F103C8T6](http://www.st.com/en/microcontrollers/stm32f103c8.html)
@ 72MHz with 64KB of flash and 20KB of RAM). The results of the flash/RAM occupancy are given in the table below,
as well as the timings of the ECDSA signature and verification operations.

**Note**: The Cortex-M0 case is a bit special in the ARM family. Since this MCU lacks a 32-bit x 32-bit to 64-bit
multiplication instruction, the multiplication is implemented using a builtin software function. This yields
in poor performance with WORDSIZE=64 compared to WORDSIZE=32 (this might be explained by the calling cost to
the builtin function).


| **libecc**      | STM32F103C8T6 (Cortex-M3 @ 72MHz) | STM32F030R8T6 (Cortex-M0 @ 48MHz) |
|-----------------|:----------------------------------|:----------------------------------|
| Flash size      |           32KB                    |       30KB                        |
| RAM size        |           8KB                     |       8KB                         |
| Sign time       |         950ms                     |    2146ms                         |
| Verif time      |        1850ms                     |    4182ms                         |

In order to compare the libecc performance on these embedded platforms, we give figures for mbedTLS on
Cortex-M3 taken from a [recent study by ARM](http://csrc.nist.gov/groups/ST/lwc-workshop2015/presentations/session7-vincent.pdf).
As we have previously discussed, only the figures without NIST curves specific optimizations are of interest
for a fair comparison:

| **mbedTLS**     | LPC1768 (Cortex-M3 @ 92MHz)<sup>1</sup>  |
|-----------------|:------------------------------|
| Flash size      |         ??                    |
| RAM size        |         3KB<sup>2</sup>|
| Sign time       |    1893ms                     |
| Verif time      |    3788ms                     |

<sup>1</sup> Beware of the MCU frequency difference when comparing with libecc test case.

<sup>2</sup> This figure only includes heap usage (stack usage is unknown so this is only a
rough lower limit for RAM usage).


## <a name="compatibility-and-portability"></a> Compatibility and Portability

### libecc compatibility

When dealing with the **portability** of a program across various platforms, many issues are
in fact hidden behind this property. This is due to the very complex nature of what a
**platform** is, namely:

* A **core CPU** architecture (x86, ARM, MIPS, PowerPC, ...).
* A target **OS** (Linux, Windows, Mac OS, ...) or more low level firmware (including a **bare-metal**
programming model or exotic real-time OS for microcontrollers for instance).
* A proper compilation **(cross-)toolchain** producing binaries for the platform. This toolchain will usually
include a compiler and a linker, both with possibly specific flags and limitations.

Regarding libecc, here are the main elements to be aware of when dealing with a "new" platform:

* libecc is in pure C-99 (no assembly), so it should compile on **any platform** with a decent C-99
compatible compiler. The code is **endian neutral**, meaning that libecc should work on little endian
and big endian platforms.
* The Makefile has been tested with clang and gcc under Linux, as well as gcc cross-compilation variants
such as **mingw** for Windows or gcc **Mac OS** version. In order to adapt the makefile behaviour when the
compiler is not gcc/clang compatible, the user can modify the CFLAGS as well as the LDFLAGS by exporting them.
* The library supports 16-bit/32-bit/64-bit word sizes, which should ensure compatibility with most of the platforms
for 8-bit MCUs to 64-bit CPUs. If the toolchain does not have a [`stdint.h`](http://pubs.opengroup.org/onlinepubs/009695399/basedefs/stdint.h.html)
header, it is still possible to compile libecc by exporting LIBECC_NOSTDLIB=1: in this case, the code will try to
guess and fit to native C types or throw an error so that the user can adapt [src/words/types.h](src/words/types.h) to its specific case.
* The library core is platform independent. However, when the platform is not recognized (i.e. everything aside UNIX/Windows/Mac OS),
an error is thrown at compilation time asking the user to provide implementations for **external dependencies**
in [src/external&lowbar;deps/](src/external_deps), namely:
  * The printing helper in [src/external&lowbar;deps/print.c](src/external_deps/print.c). This helper serves output debugging purposes.
  * The timing helper in [src/external&lowbar;deps/time.c](src/external_deps/time.c). This helper is used to measure performances of the
  library in the performance self tests.
  * The random helper in [src/external&lowbar;deps/rand.c](src/external_deps/rand.c). This helper is used in the core library for the signature
  schemes. One should notice that a **good random source** is **crucial** for the security of Elliptic Curve based signature schemes,
  so great care must be taken when implementing this.

Some other external dependencies could arise depending on the compilation chain and/or the platform. Such an example is the
implementation of the gcc and clang stack protection option, usually expecting the user to provide stack canaries generation
(with random values) and failover behavior.


### <a name="compiling-libecc-for-arm-cortex-m-with-GNU-gcc-arm"></a> Compiling libecc for ARM Cortex-M with GNU gcc-arm

Compiling for Cortex-M targets should be straightforward using the arm-gcc none-eabi (for bare metal) cross-compiler as
well as the specific Cortex-M target platform SDK. In order to compile the core libsign.a static library, the only thing to do is to execute
the makefile command by overloading `CROSS_COMPILE`, `CC` and the `CFLAGS`:
<pre>
	$ CROSS_COMPILE=arm-none-eabi- CC=gcc CFLAGS="$(TARGET_OPTS) -W -Wextra -Wall -Wunreachable-code \
	-pedantic -fno-builtin -std=c99 -Os \
	-ffreestanding -fno-builtin -nostdlib -DWORDSIZE=64" \
	make build/libsign.a
</pre>

where `$(TARGET_OPTS)` are the flags specific to the considered target: `-mcpu=cortex-m3 -mthumb` for Cortex-M3 for example. The word size
flag should be adapted to `-DWORDSIZE=32` for the specific case of Cortex-M0/M0+ as discussed in the [performance section](#performance)
(because of the lacking of 32-bit to 64-bit native multiplication instruction). The library can then be used to be linked against a file
containing the `main` calling function, and the linking part will depend on the
target platform (in addition to the target CPU): one will use the **linker scripts** provided by the platform/board manufacturer to produce
a firmware suitable for the target (ST for STM32, NXP for LPC, Atmel for SAM, ...).

If the external dependencies have been implemented by the user, it is also possible to build a self-tests binary by adding the
GNU ld linker script specific to the target platform (`linker_script.ld` in the example below):
<pre>
	$ CROSS_COMPILE=arm-none-eabi- CFLAGS="$(TARGET_OPTS) -W -Wextra -Wall -Wunreachable-code \
	-pedantic -fno-builtin -std=c99 -Os \
	-ffreestanding -fno-builtin -nostdlib -DWORDSIZE=64" \
	LDFLAGS="-T linker_script.ld" \
	make build/libsign.a
</pre>

**NOTE1**: By default, the linker scripts share the RAM between heap and stack. Since libecc only uses stack, it is convenient
(sometimes necessary, specifically on devices with very constrained RAM, such as Cortex-M0 with 8KB) to adapt the **stack base address**
so that no stack overflow errors occur. These errors can be tricky to detect since they generally produce hard faults silently at
run time. Also, a `SMALLSTACK=1` compilation toggle allows to limit stack consumption further: you can use it carefully if your device
does not have enough stack for the regular compilation options (use with care as some side channels countermeasures are deactivated, and this
mode is not compatible with the EdDSA signature and X25519 ECDH algorithm).

**NOTE2**: It is up to the user to link against the libc (if standard functions are necessary) or not, but this will obviously influence the
program size in flash. As already stated, the libc footprint is not included in the figured that have been given in
the [performance section](#performance).

**NOTE3**: libecc has also been successfully tested with other non-GNU compilation SDK and toolchains such as [Keil MDK](http://www.keil.com/)
configured to use the [ARM compiler](http://www2.keil.com/mdk5/compiler/5/).

### <a name="libecc-portability-guide"></a> libecc portability guide

This section is dedicated to giving some more details on how to compile libecc when non-GNU compilers
are used (i.e. C compilers that do not support gcc syntax), and/or when compiling with environments that
do not provide a **GNU make** compilation style (this is generally the case for all-in-one IDEs such as
Visual Studio or other BSP and SDK provided by proprietary integrated circuits founders and board manufacturers).

#### 1 - Compilers and C99 standard compliance

As we have already stated, libecc requires a C99 compiler. More specifically, libecc makes use of
only four feature of the C99 standard (over the older C89/C90 standard), namely:
* The `long long int` type.
* Designated initializers for structures.
* The usage of the `inline` keyword.
* The usage of variadic macros.

Hence, when compiling with a given compiler, one will have to check that the compiler is
**either fully C99 compliant**, or that these four features are at least **implemented as extensions**.
Such details are generally provided by the compiler documentation.

**NOTE**: if one wants to adapt libecc for compilers where some of the necessary C99 features are missing, here is a
big picture of the necessary work:
* The `long long int` and structures initializers are used all over libecc code, so they are
**strong requirements**, and would imply deep code modifications.
* The `inline` keyword can be removed in most cases, except in the context of header files where it is used
to define `static inline` functions. These functions will have to be moved to '.c' files, and one will have to
deal with minor adaptations.
* The usage of variadic macros is marginal and can be removed with minimal efforts: these are only used
to deal with debug helpers in [src/utils](src/utils).

#### 2 - Compiling with environments without GNU make

libecc uses a GNU style [Makefile](Makefile) to automate the compilation process. One can however use other
compilation environments that are not GNU make compatible by implementing the following guidelines:
* Make the compilation toolchain compile into **'.o' objects** all the necessary '.c' files in [src/nn](src/nn),
[src/fp](src/fp), [src/curve](src/curve), [src/sig](src/sig), [src/utils](src/utils) and [src/hash](src/hash).
* Make the compilation toolchain **link** the necessary object files to generate the static libraries:
<pre>
  libarith.a:
  °°°°°°°°°°°
  src/fp/fp_rand.o src/fp/fp_mul.o src/fp/fp_montgomery.o src/fp/fp_mul_redc1.o src/fp/fp_add.o src/fp/fp.o
  src/fp/fp_pow.o src/nn/nn_mul.o src/nn/nn_mul_redc1.o src/nn/nn_logical.o src/nn/nn.o src/nn/nn_modinv.o
  src/nn/nn_add.o src/nn/nn_rand.o src/nn/nn_div.o src/utils/print_nn.o src/utils/print_fp.o src/utils/print_keys.o
  src/utils/print_curves.o src/utils/utils.o
</pre>
<pre>
  libec.a:
  °°°°°°°°
  src/fp/fp_rand.o src/fp/fp_mul.o src/fp/fp_montgomery.o src/fp/fp_mul_redc1.o src/fp/fp_add.o src/fp/fp.o src/fp/fp_pow.o
  src/nn/nn_mul.o src/nn/nn_mul_redc1.o src/nn/nn_logical.o src/nn/nn.o src/nn/nn_modinv.o src/nn/nn_add.o src/nn/nn_rand.o
  src/nn/nn_div.o src/utils/print_nn.o src/utils/print_fp.o src/utils/print_keys.o src/utils/print_curves.o src/utils/utils.o
  src/curves/prj_pt.o src/curves/curves.o src/curves/aff_pt.o src/curves/prj_pt_monty.o src/curves/ec_shortw.o src/curves/ec_params.o
</pre>
<pre>
  libsign.a:
  °°°°°°°°°°
  src/fp/fp_rand.o src/fp/fp_mul.o src/fp/fp_montgomery.o src/fp/fp_mul_redc1.o src/fp/fp_add.o src/fp/fp.o src/fp/fp_pow.o
  src/nn/nn_mul.o src/nn/nn_mul_redc1.o src/nn/nn_logical.o src/nn/nn.o src/nn/nn_modinv.o src/nn/nn_add.o src/nn/nn_rand.o
  src/nn/nn_div.o src/utils/print_nn.o src/utils/print_fp.o src/utils/print_keys.o src/utils/print_curves.o src/utils/utils.o
  src/curves/prj_pt.o src/curves/curves.o src/curves/aff_pt.o src/curves/prj_pt_monty.o src/curves/ec_shortw.o src/curves/ec_params.o
  src/hash/sha384.o src/hash/sha3-512.o src/hash/sha512.o src/hash/sha3-256.o src/hash/sha3-224.o src/hash/sha3.o src/hash/sha256.o
  src/hash/sha3-384.o src/hash/sha224.o src/hash/hash_algs.o src/sig/ecsdsa.o src/sig/ecdsa.o src/sig/ecrdsa.o src/sig/ecosdsa.o
  src/sig/ecfsdsa.o src/sig/eckcdsa.o src/sig/ecgdsa.o src/sig/ecsdsa_common.o src/sig/sig_algs.o src/sig/ec_key.o
</pre>

Compiling binaries (such as `ec_self_tests` and `ec_utils`) is nothing more than compiling concerned '.c' files under [src/tests](src/tests)
and linking them with `libsign.a`.

#### 3 - Dealing with the standard library and stdint

Some important **preprocessor flags** are expected to be defined when compiling libecc:
* `WORDSIZE=`: this is a preprocessor flag defining libecc internal words size (16, 32, or 64). By default libecc will
detect the best size depending on the platform, but if the platform is not recognized the user is expected to
provide this flag.
* `WITH_STDLIB`: this flag is used for standard library usage inside libecc. Exporting the environment variable
`LIBECC_NOSTDLIB=1` will trigger the **non usage** of standard includes and libraries. Standard C library headers and files
are used for two things in the project:
  * Defining standard types through the `stdint.h` header.
Though using this header helps libecc to properly define basic types in [include/libecc/words/types.h](include/libecc/words/types.h), it is not
required to use it and some heuristics can be used to define these types without standard headers (see explanations on that
in [include/libecc/words/types.h](include/libecc/words/types.h)) comments.
  * Defining standard library functions used by external dependencies as well as `ec_utils`. Compiling without `WITH_STDLIB`
flag means that one has to provide these.

In any case, if the user forgot to provide important preprocessing flags whenever they are necessary, **errors will be thrown** during
the compilation process. As explained in [include/libecc/words/types.h](include/libecc/words/types.h), when `stdint.h` is not used (i.e. `WITH_STDLIB`
not defined), heuristics are used to guess primitive types sizes. These heuristics can fail and the user will have to adapt the types
definitions accordingly depending on the platform.

#### <a name="overloading-makefile-variables"></a> 4 - Overloading Makefile variables

When compiling using compilers that are not compatible with the gcc syntax, but still using a GNU make
compilation environment, it is possible to **adpat the Makefile behavior**. In addition to the `LIBECC_NOSTDLIB=1`
environment variable previously described, here is the list of the variables that tune the compilation process:

* `CC`: as usual, this overloads the compiler to be used.
* `CFLAGS` and `LDFLAGS`: these flags can be overloaded by user defined ones. The user defined flags will completely
shadow the default flags for both the static libraries (libarith.a, libec.a, libsign.a) and the produced binaries.
* `LIB_CFLAGS`, `BIN_CFLAGS`, `BIN_LDFLAGS`: when one wants to specifically tune compilation and linking flags for
the static libraries and the binaries, these flags can be used and they will shadow the `CFLAGS` and `LDFLAGS`.
* `AR` and `RANLIB`: these flags override the ar and ranlib tools used to generate the static library archives.

As a simple example of when and how to use this environment variables overloading system, let's take the following case:
one wants to compile libecc with an old version of gcc that does not support the `-fstack-protector-strong` option
(this is the case for [gcc < 4.9](https://lwn.net/Articles/584225/)). Since this is the flag used by default in
libecc Makefile, an error will be triggered. It is possible to overcome this issue by overloading the `CFLAGS` with
the following:
<pre>
	$ CFLAGS="-W -Werror -Wextra -Wall -Wunreachable-code -pedantic -fno-builtin -std=c99 -D_FORTIFY_SOURCE=2 \
	-fstack-protector-all -O3 -DWITH_STDLIB -fPIC" make
</pre>

As we can see, we keep the other `CFLAGS` from default compilation while replacing `-fstack-protector-strong` with
the **less efficient but more compatible** `-fstack-protector-all`.

In addition to compilation flags, it is also possible to overload the library **word sizes** as well as **debug**
modes through Makefile targets:
* `make debug` will compile a debug version of the library and binaries, with debugging symbols.
Setting the environment variable `VERBOSE_INNER_VALUES=1` will print out more values.
* `make 16`, `make 32` and `make 64` will respectively compile the library with 16, 32 and 64 bits word sizes. `make debug16`,
`make debug32` and `make debug64` will compile the debug versions of these.
* `make force_arch32` and `make force_arch64` will force 32-bit and 64-bit architectures compilation (`-m32` and `-m64`
flags under gcc). These targets allow cross-compilation for a 32-bit (respectively 64-bit) target under a 64-bit (respectively
32-bit) host: a typical example is compiling for i386 under x86\_64.

**NOTE**: the targets that we have described here can be used in conjunction with overloading the `CFLAGS` and `LDFLAGS`. Hence,
a: `CFLAGS="-fstack-protector-all" make debug16`
will indeed compile all the binaries for debug, with a word size of 16 bits and a `-fstack-protector-all` stack protection option.


#### 5 - A concrete example with SDCC 

As an example to show how to adapt the compilation process to compilers that are not compatible with the
GNU compilers syntax, we will detail how to proceed by exploring the [SDCC](http://sdcc.sourceforge.net/)
(Small Device C Compiler) toolchain. Porting libecc to this compiler is interesting for many reasons:

* The SDCC compiler uses some specific syntax, though it shares some similarities with
other compilers (`-c` flag to generate object files, `-o` flag to define output file).
* This compiler is "almost" C99 compliant: depending on the target, it has some C99 features
[partially implemented](http://sdcc.sourceforge.net/mediawiki/index.php/Standard_compliance).
* The compiler has "exotic" targets such as the Zilog Z80 MCU.

We suppose that the user has also provided the **external dependencies** for print, random and time
functions (otherwise explicit errors will be thrown by #error directives).

We will show how overloading the Makefile flags can be of use in this case. Say that we want
to compile libecc in order to embed it in a Game Boy ROM. The Game Boy console uses a proprietary
version of the Z80 MCU supported by SDCC under the target name `gbz80`.

Hence, a first attempt at compilation would be to:
* Overload `CC=sdcc` to change the default compiler.
* Overload `AR=sdar` and `RANLIB=sdranlib` to overload the archives handling binaries (they are specific
to SDCC).
* Overload `CFLAGS="-mbgz80 --std-sdcc99"` to specify the target, and ask for the C99 compatibility mode.
* Overload `LDFLAGS=" "` with nothing since we do not want default gcc linking flags to break compilation.

This first attempt will trigger an error:
<pre>
	$ CC=sdcc AR=sdar RANLIB=sdranlib CFLAGS="-mgbz80 --std-sdcc99" LDFLAGS=" " make
	...
	src/external_deps/../words/words.h:62:2: error: #error "Unrecognized platform. \
	Please specify the word size of your target (with make 16, make 32, make 64)"
</pre>

As we have explained, when the platform is not recognized one has to specify the word size. We will
do it by overloading `WORDSIZE=16`: the Z80 is an 8-bit CPU, so it seems reasonable to fit the word
size to 16-bit (8-bit half words). The second attempt will go further but will fail at some point when
trying to compile the final binaries:
<pre>
	$ CC=sdcc AR=sdar RANLIB=sdranlib CFLAGS="-mgbz80 --std-sdcc99 -DWORDSIZE=16" LDFLAGS=" " make
	...
	at 1: error 119: don't know what to do with file 'src/tests/ec_self_tests_core.o'. file extension unsupported
</pre>

However, one can notice that the static libraries and some object files have been compiled, which is a
first step! Compiling a full binary is a bit technical due to the fact that SDCC does not know how
to deal with '.o' object files and '.a' archives. However, we can find our way out of this by renaming
the 'libsign.a' to 'libsign.lib', and adding missing objects in the library. Compiling the `ec_self_tests`
binary needs external dependencies ([src/external&lowbar;deps/print.c](src/external&lowbar;deps/print.c),
[src/external&lowbar;deps/rand.c](src/external_deps/rand.c) and [src/external&lowbar;deps/time.c](src/external_deps/time.c))
as well as the two '.c' files [src/tests/ec&lowbar;self&lowbar;tests&lowbar;core.c](src/tests/ec_self_tests_core.c) and
[src/tests/ec&lowbar;self&lowbar;tests.c](src/tests/ec_self_tests.c), the latter being the one containing the `main`
function. So we will first add the necessary objects files in the existing library with `sdar`:
<pre>
	$ cp build/libsign.a build/libsign.lib
	$ sdar q build/libsign.lib src/external_deps/print.o src/external_deps/rand.o src/external_deps/time.o src/tests/ec_self_tests_core.o
</pre>

Then, we compile and link [src/tests/ec&lowbar;self&lowbar;tests.c](src/tests/ec_self_tests.c) with the library:
<pre>
	$ sdcc -mgbz80 -DWORDSIZE=16 --std-sdcc99 src/tests/ec_self_tests.c build/libsign.lib
</pre>

This should create a `ec_self_tests.ihx`, which has an [Intel HEX](https://fr.wikipedia.org/wiki/HEX_(Intel))
file format for firmware programming. From this file, it is usually straightforward to create a Game Boy ROM file
that can be interpreted by an [emulator](http://m.peponas.free.fr/gngb/) (there are however some quirks related
to the Game Boy platform hardware architecture, see the note below).

**NOTE**: the purpose of the section was to show how to adapt the compilation process to compilers non
compatible with the GNU C one. Consequently, fully porting libecc to the Game Boy platform is left as
a complementary work, and this is not a "so easy" task. Among other things, one will have to deal with
the ROM size limitation of 32KB that can be solved using [bank switching](http://gbdev.gg8.se/wiki/articles/Memory_Bank_Controllers),
which will involve some code and compilation tuning. Another issue would be the RAM size of 8KB and
properly handling the stack pointer base as described in the [previous sections](#compiling-libecc-for-arm-cortex-m-with-GNU-gcc-arm).

## libecc, side channel attacks and constant time

### Constant time

Though **some efforts** have been made to have (most of) the core algorithms
constant time, turning libecc into a library shielded against side channel attacks
is still a **work in progress**.

Beyond pure algorithmic considerations, many aspects of a program can turn
secret leakage resistance into a very complex problem, especially when writing
portable C code. Among other things, we can list the following:

* Low level issues can arise when dealing with heterogeneous platforms (some
instructions might not be constant time) and compilers optimizations
(a C code that seems constant time is in fact compiled to a non constant time assembly).
* Any shared hardware resource can become a leakage source (the caches, the
branch prediction unit, ...). When dealing with a portable source code
meant to run on most platforms, it is not an easy task to think of all these
leakage sources.

For a thorough discussion about cryptography and constant time challenges,
one can check [this page](https://bearssl.org/constanttime.html).

### Signature algorithm blinding

In order to avoid a range of attacks on the signature algorithm exploiting various
side channel attacks and leading to the recovery of the secret key
(see [here](https://www.nccgroup.trust/globalassets/our-research/us/whitepapers/2018/rohnp-return-of-the-hidden-number-problem.pdf)
for more details), **blinding** operations can be used.

Since such security countermeasures have a **significant performance hit** on the signature algorithm, we have
decided to leave the activation of such countermeasures as a **voluntary decision** to the end user.
The performance impact might be acceptable or not depending on the context where the signature is performed, and whether attackers exploiting side channels
are indeed considered in the threat model of the specific use case.
Of course, for **security critical use cases we recommend the blinding usage
despite its performance cost**.

Compiling the library with blinding is as simple as using the ``BLINDIG=1`` environment variable (or the ``-DUSE_SIG_BLINDING`` C flag):

<pre>
	$ BLINDING=1 make
</pre>

**NOTE**: if you are **unsure** about your current security context, use the ``BLINDING=1`` by default!


### Overview of SCA (Side Channel Attacks) countermeasures

All in all, libecc has now the following approaches to limit SCA:

* SPA (Simple Power Analysis) is thwarted using Montgomery Ladder (or Double and Add Always optionally using the ``ADALWAYS=1`` switch), plus complete formulas
(see [here](https://joostrenes.nl/publications/complete.pdf)) to avoid leaking point at infinity (by avoiding exceptions). Constant time
operations are (tentatively) used to limit leakage of different operations,
even though this task is very complex to achieve (especially in pure C). See
the discussion above.
* DDPA (Data DPA) is thwarted using blinding of the point (projective
coordinates) and of the scalar (with adding a random multiple of the
curve order with maximum entropy). Because of its major impact on
performance, blinding must be specifically turned on by the used using the
``BLINDING=1`` switch, see the discussion above.
* ADPA (Address-bit DPA) is limited using Itoh et al. Double and Add Always
masked variant. See the article "A Practical Countermeasure against
Address-Bit Differential Power Analysis" by Itoh, Izu and Takenaka for more information.

All these countermeasures must, of course, be validated on the specific target
where the library runs with leakage assessments. Because of the very nature of
C code and CPU microarchitectural details, it is very complex without such a leakage
assessment (that again depends on the target) to be sure that SCA protection
is indeed efficient.

### libecc against FIA (Fault Injection Attacks)

Efforts made to render libecc robust against FIA are a **work in progress**, and
will require **substantial additions**. As for SCA robustness, many elements
might depend on the low-level compilation process and are difficult to handle
at high-level in pure C.

For now, we check if points are on the curve when entering and leaving the
scalar multiplication algorithm, as well as when importing external public points.
Efforts are also made to sanity check the signature and verification contexts whenever possible,
as well as all the intermediate contexts (natural numbers, fields, hash functions, etc.).

Currently, no specific effort has been made to render conditional operations robust
(e.g. using double if and limiting compilation optimization).


## Software architecture

The public header of the libecc API are in the [include/libecc/](include/libecc/)
folder. Then, the source code is composed of eight main parts that consist of the
**core source code**:

  * [1] Machine code: in [src/words](src/words/)

    >Abstraction layer to handle word size depending
    >on the target machine (the word size can also be forced during
    >compilation). Some useful low level macros and functions are
    >handled there.

  * [2] Natural Numbers layer: in [src/nn](src/nn/)

    >This part implements all the functions
    >related to positive integers arithmetic (including modular
    >arithmetic).

  * [3] Fp layer: in [src/fp](src/fp/)

    >Finite field of prime order (binary fields are
    >intentionally not supported).

  * [4] Elliptic curves core: in [src/curves](src/curves/)

    >This layer implements all the primitives
    >handling elliptic curves over prime fields, including point
    >addition and doubling, affine and projective coordinates, ...

  * [5] Curves definitions: in [include/libecc/curves/known](include/libecc/curves/known) and
    [include/libecc/curves/user&lowbar;defined](include/libecc/curves/user_defined)

    >These are the definitions of some standard curves (SECP, Brainpool,
    >FRP, ...).

  * [6] EC\*DSA signature algorithms: in [src/sig](src/sig/)

    >This layer implements the main
    >elliptic curves based signature algorithms (ECSDSA, ECKCDSA,
    >ECFSDSA, ECGDSA, ECRDSA, ECOSDSA). It exposes a sign and
    >verify API with the standard Init/Update/Final logic.

  * [7] Hash functions: in [src/hash](src/hash/)

   >Hash functions (SHA-2 and SHA-3 based algorithms
   >for now).

  * [8] Utils: in [src/utils](src/utils/)

   >Various useful libc functions (memcpy, memset, ...) as well as
   >well as pretty printing functions for our NN, Fp and curves layers.

In addition to the core source code of the library, various resources
are also present in the source tree. We describe them hereafter.

Some self tests are provided for the signature algorithms over all the curves
and using all the hash functions [9], as well as tests targeting arithmetic
operations over NN and Fp more specifically [10]:

  * [9] Sig self tests: in [src/tests](src/tests/)

    >Functions to test that the compiled library is
    >properly working with regard to the signature algorithms over
    >the curves statically defined in the library.
    >These tests consiste in known test vectors, random test
    >vectors (i.e. random data sign/verify) as well as performance measurements.

  * [10] Arithmetic self tests: in [src/arithmetic](src/arithmetic_tests/)

    >Functions to test that the compiled arithmetic library is
    >properly working in its basic operations (addition, subtraction,
    >multiplication, ...).

Some examples to help the user interact with the NN, Fp and cruves layers
are also provided:

  * [11] User examples: in [src/examples](src/examples/)

    >User examples for each of the NN, Fp and curves layers. These
    >examples show what are headers to use, and how to interact with
    >the abstract mathematical objects of each layer. Other examples beyond
    >the basic ones (such as RSA signatures, SSS, etc.) are also present there.

The configuration of the library [13] as well as an external dependencies
abstraction layer are also provided:

  * [12] External dependencies: in [src/external&lowbar;deps](src/external_deps/)

    >These files contain the functions that
    >are considered as external dependencies, meaning that their
    >implementation is platform dependent (this concerns debug
    >output on a console or file, random generation, time measurement).
    >If no C standard library is provided, the user must implement
    >those functions.

  * [13] Configuration files: in [include/libecc/lib&lowbar;ecc&lowbar;config.h](include/libecc/lib_ecc_config.h)

    >These are top C headers that are used for
    >libecc configuration, i.e. activate given hash/curve/signature
    >algorithms at compilation time through ifdefs.

Finally, various useful scripts are provided:

  * [14] Scripts: in [scripts](scripts/)

   >Tools to expand the libecc with new user defined curves.

Here is a big picture of the library architecture summarizing the links
between the modules previously described:

<pre>
    +-------------------------+
    |EC*DSA signature         |
    |algorithms               | <------------------+
    |(ISO 14888-3)      [6]   |                    |
    +-----------+-------------+                    |
                ^                                  |
                |                                  |
    +-----------+-------------+         +----------+------------+
    |Curves (SECP, Brainpool, |         |         Hash          |
    |FRP, ...)                |         |       functions       |
    |                   [5]   |         |                   [7] |
    +-----------+-------------+         +-----------------------+
                ^                    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@
                |                    @ {Useful auxiliary modules}@
    +-----------+-------------+      @ +------------------------+@
    |  Elliptic curves  [4]   |      @ |         Utils      [8] |@
    |  core (scalar mul, ...) |      @ +------------------------+@
    +-----------+-------------+      @ |     Sig Self tests [9] |@
                ^                    @ |  Arith Self tests [10] |@
                |                    @ |     User Examples [11] |@
                |                    @ +------------------------+@
                |                    @ |    External deps  [12] |@
    +-----------+-------------+      @ +------------------------+@
    | Fp finite fields  [3]   |      @ | LibECC conf files [13] |@
    | arithmetic              |      @ +------------------------+@
    +-----------+-------------+      @ |        Scripts    [14] |@
                ^                    @ +------------------------+@
                |                    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    +-----------+-------------+        +------------------------+
    | NN natural        [2]   | <------+   Machine related      |
    | numbers arithmetic      |        |   (words, ...)     [1] |
    +-------------------------+        +------------------------+

</pre>

## Integration with hardware acceleration: IPECC

On the IPECC branch of the libecc repository, there is an integration
with the [IPECC](https://github.com/ANSSI-FR/IPECC) hardware accelerator
project. This project provides hardware acceleration for ECC points operations
(addition, doubling, scalar multiplication, etc.) with advanced SCA countermeasures.

In order to use this accelerator, please follow these steps. First checkout the
IPECC branch:

<pre>
	$ git clone https://github.com/libecc/libecc
	$ git checkout -b IPECC
</pre>

Then fetch the dedicated driver on the [IPECC repository](https://github.com/ANSSI-FR/IPECC)
(this will clone the current repository and place the IPECC drivers in the `src/curve` folder):

<pre>
	$ make install_hw_driver
</pre>

Then, you can compile the library with hardware acceleration by selecting the underlying platform:

<pre>
	$ make clean && CC=arm-linux-gnueabihf-gcc EXTRA_CFLAGS="-Wall -Wextra -O3 -g3 -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -static" VERBOSE=1 USE_EC_HW=1 USE_EC_HW_DEVMEM=1 USE_EC_HW_LOCKING=1 BLINDING=1 make
</pre>

Please note that `USE_EC_HW=1` selects the hardware accelerator (this is mandatory to activate the hardware acceleration backend in libecc),
and `USE_EC_HW_DEVMEM=1` selects the DEVMEM backend (you can use `USE_EC_HW_STANDALONE=1` for the standalone mode, `USE_EC_HW_UIO=1` for UIO,
and `USE_EC_HW_SOCKET_EMUL=1` for the socket emulation using the Python server).
We also override the `CC` compiler to `arm-linux-gnueabihf-gcc` for the Zynq platform (adapt at your will depending on your
target), and add some necessary extra CFLAGS for the platform (as well as a `-static` binary compilation to avoid library dependency issues).
Finally, `USE_EC_HW_LOCKING=1` is used here for thread safety during hardware access: this flag is necessary for multi-threading.

libecc has been successfully tested on a [Zynq Arty Z7](https://digilent.com/reference/programmable-logic/arty-z7/start) board with
a **factor 6** performance improvement compared to pure software on the same platform (with SCA countermeasures):

<pre>
az7-ecc-axi:/home/petalinux# ./ec_self_tests_sw perf
======= Performance test ========================
[+]          ECDSA-SHA224/FRP256V1 perf: 6 sign/s and 6 verif/s
[+]         ECDSA-SHA224/SECP192R1 perf: 9 sign/s and 9 verif/s
[+]         ECDSA-SHA224/SECP224R1 perf: 7 sign/s and 7 verif/s
[+]         ECDSA-SHA224/SECP256R1 perf: 6 sign/s and 6 verif/s
...

az7-ecc-axi:/home/petalinux# ./ec_self_tests_hw perf
======= Performance test ========================
[+]          ECDSA-SHA224/FRP256V1 perf: 34 sign/s and 32 verif/s
[+]         ECDSA-SHA224/SECP192R1 perf: 57 sign/s and 52 verif/s
[+]         ECDSA-SHA224/SECP224R1 perf: 44 sign/s and 39 verif/s
[+]         ECDSA-SHA224/SECP256R1 perf: 34 sign/s and 32 verif/s
[+]         ECDSA-SHA224/SECP384R1 perf: 16 sign/s and 15 verif/s
[+]         ECDSA-SHA224/SECP521R1 perf: 8 sign/s and 8 verif/s
[+]   ECDSA-SHA224/BRAINPOOLP192R1 perf: 57 sign/s and 52 verif/s
[+]   ECDSA-SHA224/BRAINPOOLP224R1 perf: 44 sign/s and 40 verif/s
</pre>

