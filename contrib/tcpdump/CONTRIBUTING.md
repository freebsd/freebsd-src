# Some Information for Contributors
Thank you for considering to make a contribution to tcpdump! Please use the
guidelines below to achieve the best results and experience for everyone.

## How to report bugs and other problems
**To report a security issue (segfault, buffer overflow, infinite loop, arbitrary
code execution etc) please send an e-mail to security@tcpdump.org, do not use
the bug tracker!**

To report a non-security problem (failure to compile, incorrect output in the
protocol printout, missing support for a particular protocol etc) please check
first that it reproduces with the latest stable release of tcpdump and the latest
stable release of libpcap. If it does, please check that the problem reproduces
with the current git master branch of tcpdump and the current git master branch of
libpcap. If it does (and it is not a security-related problem, otherwise see
above), please navigate to the
[bug tracker](https://github.com/the-tcpdump-group/tcpdump/issues)
and check if the problem has already been reported. If it has not, please open
a new issue and provide the following details:

* tcpdump and libpcap version (`tcpdump --version`)
* operating system name and version and any other details that may be relevant
  (`uname -a`, compiler name and version, CPU type etc.)
* custom `configure`/`cmake` flags, if any
* statement of the problem
* steps to reproduce

Please note that if you know exactly how to solve the problem and the solution
would not be too intrusive, it would be best to contribute some development time
and to open a pull request instead as discussed below.

Still not sure how to do? Feel free to
[subscribe to the mailing list](https://www.tcpdump.org/#mailing-lists)
and ask!


## How to add new code and to update existing code

1) Check that there isn't a pull request already opened for the changes you
   intend to make.

2) [Fork](https://help.github.com/articles/fork-a-repo/) the Tcpdump
   [repository](https://github.com/the-tcpdump-group/tcpdump).

3) The easiest way to test your changes on multiple operating systems and
   architectures is to let the upstream CI test your pull request (more on
   this below).

4) Setup your git working copy
   ```
   git clone https://github.com/<username>/tcpdump.git
   cd tcpdump
   git remote add upstream https://github.com/the-tcpdump-group/tcpdump
   git fetch upstream
   ```

5) Do a `touch .devel` in your working directory.
   Currently, the effect is
   * add (via `configure`, in `Makefile`) some warnings options (`-Wall`,
     `-Wmissing-prototypes`, `-Wstrict-prototypes`, ...) to the compiler if it
     supports these options,
   * have the `Makefile` support `make depend` and the `configure` script run it.

6) Configure and build
   ```
   ./configure && make -s && make check
   ```

7) Add/update tests
   The `tests` directory contains regression tests of the dissection of captured
   packets.  Those captured packets were saved running tcpdump with option
   `-w sample.pcap`.  Additional options, such as `-n`, are used to create relevant
   and reproducible output; `-#` is used to indicate which particular packets
   have output that differs.  The tests are run with the `TZ` environment
   variable set to `GMT0`, so that UTC, rather than the local time where the
   tests are being run, is used when "local time" values are printed.  The
   actual test compares the current text output with the expected result
   (`sample.out`) saved from a previous version.

   Any new/updated fields in a dissector must be present in a `sample.pcap` file
   and the corresponding output file.

   Configuration is set in `tests/TESTLIST`.
   Each line in this file has the following format:
   ```
   test-name   sample.pcap   sample.out   tcpdump-options
   ```

   The `sample.out` file can be produced as follows:
   ```
   (cd tests && TZ=GMT0 ../tcpdump -# -n -r sample.pcap tcpdump-options > sample.out)
   ```

   Or, for convenience, use `./update-test.sh test-name`

   It is often useful to have test outputs with different verbosity levels
   (none, `-v`, `-vv`, `-vvv`, etc.) depending on the code.

8) Test using `make check` (current build options) and `./build_matrix.sh`
   (a multitude of build options, build systems and compilers). If you can,
   test on more than one operating system. Don't send a pull request until
   all tests pass.

9) Try to rebase your commits to keep the history simple.
   ```
   git fetch upstream
   git rebase upstream/master
   ```
   (If the rebase fails and you cannot resolve, issue `git rebase --abort`
   and ask for help in the pull request comment.)

10) Once 100% happy, put your work into your forked repository using `git push`.

11) [Initiate and send](https://help.github.com/articles/using-pull-requests/)
    a pull request.
    This will trigger the upstream repository CI tests.


## Code style and generic remarks
1) A thorough reading of some other printers code is useful.

2) To help learn how tcpdump works or to help debugging:
   You can configure and build tcpdump with the instrumentation of functions:
   ```
   $ ./configure --enable-instrument-functions
   $ make -s clean all
   ```

   This generates instrumentation calls for entry and exit to functions.
   Just after function entry and just before function exit, these
   profiling functions are called and print the function names with
   indentation and call level.

   If entering in a function, it prints also the calling function name with
   file name and line number. There may be a small shift in the line number.

   In some cases, with Clang 11, the file number is unknown (printed '??')
   or the line number is unknown (printed '?'). In this case, use GCC.

   If the environment variable INSTRUMENT is
   - unset or set to an empty string, print nothing, like with no
     instrumentation
   - set to "all" or "a", print all the functions names
   - set to "global" or "g", print only the global functions names

   This allows to run:
   ```
   $ INSTRUMENT=a ./tcpdump ...
   $ INSTRUMENT=g ./tcpdump ...
   $ INSTRUMENT= ./tcpdump ...
   ```
   or
   ```
   $ export INSTRUMENT=global
   $ ./tcpdump ...
   ```

   The library libbfd is used, therefore the binutils-dev package is required.

3) Put the normative reference if any as comments (RFC, etc.).

4) Put the format of packets/headers/options as comments if there is no
   published normative reference.

5) The printer may receive incomplete packet in the buffer, truncated at any
   random position, for example by capturing with `-s size` option.
   This means that an attempt to fetch packet data based on the expected
   format of the packet may run the risk of overrunning the buffer.

   Furthermore, if the packet is complete, but is not correctly formed,
   that can also cause a printer to overrun the buffer, as it will be
   fetching packet data based on the expected format of the packet.

   Therefore, integral, IPv4 address, and octet sequence values should
   be fetched using the `GET_*()` macros, which are defined in
   `extract.h`.

   If your code reads and decodes every byte of the protocol packet, then to
   ensure proper and complete bounds checks it would be sufficient to read all
   packet data using the `GET_*()` macros.

   If your code uses the macros above only on some packet data, then the gaps
   would have to be bounds-checked using the `ND_TCHECK_*()` macros:
   ```
   ND_TCHECK_n(p), n in { 1, 2, 3, 4, 5, 6, 7, 8, 16 }
   ND_TCHECK_SIZE(p)
   ND_TCHECK_LEN(p, l)
   ```

   where *p* points to the data not being decoded.  For `ND_CHECK_n()`,
   *n* is the length of the gap, in bytes.  For `ND_CHECK_SIZE()`, the
   length of the gap, in bytes, is the size of an item of the data type
   to which *p* points.  For `ND_CHECK_LEN()`, *l* is the length of the
   gap, in bytes.

   For the `GET_*()` and `ND_TCHECK_*` macros (if not already done):
   * Assign: `ndo->ndo_protocol = "protocol";`
   * Define: `ND_LONGJMP_FROM_TCHECK` before including `netdissect.h`
   * Make sure that the intersection of `GET_*()` and `ND_TCHECK_*()` is minimal,
     but at the same time their union covers all packet data in all cases.

   You can test the code via:
   ```
   sudo ./tcpdump -s snaplen [-v][v][...] -i lo # in a terminal
   sudo tcpreplay -i lo sample.pcap             # in another terminal
   ```
   You should try several values for snaplen to do various truncation.

*  The `GET_*()` macros that fetch integral values are:
   ```
   GET_U_1(p)
   GET_S_1(p)
   GET_BE_U_n(p), n in { 2, 3, 4, 5, 6, 7, 8 }
   GET_BE_S_n(p), n in { 2, 3, 4, 5, 6, 7, 8 }
   GET_LE_U_n(p), n in { 2, 3, 4, 5, 6, 7, 8 }
   GET_LE_S_n(p), n in { 2, 3, 4, 5, 6, 7, 8 }
   ```

   where *p* points to the integral value in the packet buffer. The
   macro returns the integral value at that location.

   `U` indicates that an unsigned value is fetched; `S` indicates that a
   signed value is fetched.  For multi-byte values, `BE` indicates that
   a big-endian value ("network byte order") is fetched, and `LE`
   indicates that a little-endian value is fetched. *n* is the length,
   in bytes, of the multi-byte integral value to be fetched.

   In addition to the bounds checking the `GET_*()` macros perform,
   using those macros has other advantages:

   * tcpdump runs on both big-endian and little-endian systems, so
     fetches of multi-byte integral values must be done in a fashion
     that works regardless of the byte order of the machine running
     tcpdump.  The `GET_BE_*()` macros will fetch a big-endian value and
     return a host-byte-order value on both big-endian and little-endian
     machines, and the `GET_LE_*()` macros will fetch a little-endian
     value and return a host-byte-order value on both big-endian and
     little-endian machines.

   * tcpdump runs on machines that do not support unaligned access to
     multi-byte values, and packet values are not guaranteed to be
     aligned on the proper boundary.  The `GET_BE_*()` and `GET_LE_*()`
     macros will fetch values even if they are not aligned on the proper
     boundary.

*  The `GET_*()` macros that fetch IPv4 address values are:
   ```
   GET_IPV4_TO_HOST_ORDER(p)
   GET_IPV4_TO_NETWORK_ORDER(p)
   ```

   where *p* points to the address in the packet buffer.
  `GET_IPV4_TO_HOST_ORDER()` returns the address in the byte order of
   the host that is running tcpdump; `GET_IPV4_TO_NETWORK_ORDER()`
   returns it in network byte order.

   Like the integral `GET_*()` macros, these macros work correctly on
   both big-endian and little-endian machines and will fetch values even
   if they are not aligned on the proper boundary.

*  The `GET_*()` macro that fetches an arbitrary sequences of bytes is:
   ```
   GET_CPY_BYTES(dst, p, len)
   ```

   where *dst* is the destination to which the sequence of bytes should
   be copied, *p* points to the first byte of the sequence of bytes, and
   *len* is the number of bytes to be copied.  The bytes are copied in
   the order in which they appear in the packet.

*  To fetch a network address and convert it to a printable string, use
   the following `GET_*()` macros, defined in `addrtoname.h`, to
   perform bounds checks to make sure the entire address is within the
   buffer and to translate the address to a string to print:
   ```
   GET_IPADDR_STRING(p)
   GET_IP6ADDR_STRING(p)
   GET_MAC48_STRING(p)
   GET_EUI64_STRING(p)
   GET_EUI64LE_STRING(p)
   GET_LINKADDR_STRING(p, type, len)
   GET_ISONSAP_STRING(nsap, nsap_length)
   ```

   `GET_IPADDR_STRING()` fetches an IPv4 address pointed to by *p* and
   returns a string that is either a host name, if the `-n` flag wasn't
   specified and a host name could be found for the address, or the
   standard XXX.XXX.XXX.XXX-style representation of the address.

   `GET_IP6ADDR_STRING()` fetches an IPv6 address pointed to by *p* and
   returns a string that is either a host name, if the `-n` flag wasn't
   specified and a host name could be found for the address, or the
   standard XXXX::XXXX-style representation of the address.

   `GET_MAC48_STRING()` fetches a 48-bit MAC address (Ethernet, 802.11,
   etc.) pointed to by *p* and returns a string that is either a host
   name, if the `-n` flag wasn't specified and a host name could be
   found in the ethers file for the address, or the standard
   XX:XX:XX:XX:XX:XX-style representation of the address.

   `GET_EUI64_STRING()` fetches a 64-bit EUI pointed to by *p* and
   returns a string that is the standard XX:XX:XX:XX:XX:XX:XX:XX-style
   representation of the address.

   `GET_EUI64LE_STRING()` fetches a 64-bit EUI, in reverse byte order,
   pointed to by *p* and returns a string that is the standard
   XX:XX:XX:XX:XX:XX:XX:XX-style representation of the address.

   `GET_LINKADDR_STRING()` fetches an octet string, of length *length*
   and type *type*,  pointed to by *p* and returns a string whose format
   depends on the value of *type*:

   * `LINKADDR_MAC48` - if the length is 6, the string has the same
   value as `GET_MAC48_STRING()` would return for that address,
   otherwise, the string is a sequence of XX:XX:... values for the bytes
   of the address;

   * `LINKADDR_FRELAY` - the string is "DLCI XXX", where XXX is the
   DLCI, if the address is a valid Q.922 header, and an error indication
   otherwise;

   * `LINKADDR_EUI64`, `LINKADDR_ATM`, `LINKADDR_OTHER` -
   the string is a sequence of XX:XX:... values for the bytes
   of the address.

6) When defining a structure corresponding to a packet or part of a
   packet, so that a pointer to packet data can be cast to a pointer to
   that structure and that structure pointer used to refer to fields in
   the packet, use the `nd_*` types for the structure members.

   Those types all are aligned only on a 1-byte boundary, so a
   compiler will not assume that the structure is aligned on a boundary
   stricter than one byte; there is no guarantee that fields in packets
   are aligned on any particular boundary.

   This means that all padding in the structure must be explicitly
   declared as fields in the structure.

   The `nd_*` types for integral values are:

   * `nd_uintN_t`, for unsigned integral values, where *N* is the number
      of bytes in the value.
   * `nd_intN_t`, for signed integral values, where *N* is the number
      of bytes in the value.

   The `nd_*` types for IP addresses are:

   * `nd_ipv4`, for IPv4 addresses;
   * `nd_ipv6`, for IPv6 addresses.

   The `nd_*` types for link-layer addresses are:

   * `nd_mac48`, for MAC-48 (Ethernet, 802.11, etc.) addresses;
   * `nd_eui64`, for EUI-64 values.

   The `nd_*` type for a byte in a sequence of bytes is `nd_byte`; an
   *N*-byte sequence should be declared as `nd_byte[N]`.

7) Do invalid packet checks in code: Think that your code can receive in input
   not only a valid packet but any arbitrary random sequence of octets (packet
   * built malformed originally by the sender or by a fuzz tester,
   * became corrupted in transit or for some other reason).

   Print with: `nd_print_invalid(ndo);	/* to print " (invalid)" */`

8) Use `struct tok` for indexed strings and print them with
   `tok2str()` or `bittok2str()` (for flags).
   All `struct tok` must end with `{ 0, NULL }`.

9) Avoid empty lines in output of printers.

10) A commit message must have:
   ```
   First line: Capitalized short summary in the imperative (50 chars or less)

   If the commit concerns a protocol, the summary line must start with
   "protocol: ".

   Body: Detailed explanatory text, if necessary. Fold it to approximately
   72 characters. There must be an empty line separating the summary from
   the body.
   ```

11) Avoid non-ASCII characters in code and commit messages.

12) Use the style of the modified sources.

13) Don't mix declarations and code.

14) tcpdump requires a compiler that supports C99 or later, so C99
   features may be used in code, but C11 or later features should not be
   used.

15) Avoid trailing tabs/spaces
