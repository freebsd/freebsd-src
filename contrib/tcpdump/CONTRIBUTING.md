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

0) Check that there isn't a pull request already opened for the changes you
   intend to make.

1) [Fork](https://help.github.com/articles/fork-a-repo/) the Tcpdump
   [repository](https://github.com/the-tcpdump-group/tcpdump).

2) The easiest way to test your changes on multiple operating systems and
   architectures is to let the upstream CI test your pull request (more on
   this below).

3) Setup your git working copy
   ```
   git clone https://github.com/<username>/tcpdump.git
   cd tcpdump
   git remote add upstream https://github.com/the-tcpdump-group/tcpdump
   git fetch upstream
   ```

4) Do a `touch .devel` in your working directory.
   Currently, the effect is
   * add (via `configure`, in `Makefile`) some warnings options (`-Wall`,
     `-Wmissing-prototypes`, `-Wstrict-prototypes`, ...) to the compiler if it
     supports these options,
   * have the `Makefile` support `make depend` and the `configure` script run it.

5) Configure and build
   ```
   ./configure && make -s && make check
   ```

6) Add/update tests
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

7) Test using `make check` (current build options) and `./build_matrix.sh`
   (a multitude of build options, build systems and compilers). If you can,
   test on more than one operating system. Don't send a pull request until
   all tests pass.

8) Try to rebase your commits to keep the history simple.
   ```
   git fetch upstream
   git rebase upstream/master
   ```
   (If the rebase fails and you cannot resolve, issue `git rebase --abort`
   and ask for help in the pull request comment.)

9) Once 100% happy, put your work into your forked repository using `git push`.

10) [Initiate and send](https://help.github.com/articles/using-pull-requests/)
    a pull request.
    This will trigger the upstream repository CI tests.


## Code style and generic remarks
*  A thorough reading of some other printers code is useful.

*  Put the normative reference if any as comments (RFC, etc.).

*  Put the format of packets/headers/options as comments if there is no
   published normative reference.

*  The printer may receive incomplete packet in the buffer, truncated at any
   random position, for example by capturing with `-s size` option.
   If your code reads and decodes every byte of the protocol packet, then to
   ensure proper and complete bounds checks it would be sufficient to read all
   packet data using the `GET_*()` macros, typically:
   ```
   GET_U_1(p)
   GET_S_1(p)
   GET_BE_U_n(p), n in { 2, 3, 4, 5, 6, 7, 8 }
   GET_BE_S_n(p), n in { 2, 3, 4, 5, 6, 7, 8 }
   ```
   If your code uses the macros above only on some packet data, then the gaps
   would have to be bounds-checked using the `ND_TCHECK_*()` macros:
   ```
   ND_TCHECK_n(p), n in { 1, 2, 3, 4, 5, 6, 7, 8, 16 }
   ND_TCHECK_SIZE(p)
   ND_TCHECK_LEN(p, l)
   ```
   For the `ND_TCHECK_*` macros (if not already done):
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

*  Do invalid packet checks in code: Think that your code can receive in input
   not only a valid packet but any arbitrary random sequence of octets (packet
   * built malformed originally by the sender or by a fuzz tester,
   * became corrupted in transit or for some other reason).

   Print with: `nd_print_invalid(ndo);	/* to print " (invalid)" */`

*  Use `struct tok` for indexed strings and print them with
   `tok2str()` or `bittok2str()` (for flags).

*  Avoid empty lines in output of printers.

*  A commit message must have:
   ```
   First line: Capitalized short summary in the imperative (50 chars or less)

   If the commit concerns a protocol, the summary line must start with
   "protocol: ".

   Body: Detailed explanatory text, if necessary. Fold it to approximately
   72 characters. There must be an empty line separating the summary from
   the body.
   ```

*  Avoid non-ASCII characters in code and commit messages.

*  Use the style of the modified sources.

*  Don't mix declarations and code.

*  Don't use `//` for comments.
   Not all C compilers accept C++/C99 comments by default.

*  Avoid trailing tabs/spaces
