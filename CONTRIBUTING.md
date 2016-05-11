Thank you for helping us improve libarchive.
The following guidelines will help ensure your contribution gets prompt attention.

# Bugs and other Issues

If you encounter any problems with libarchive,
[please file an issue on our issue tracker](https://github.com/libarchive/libarchive/issues).

All bug reports should include the following information.  You can copy the text below directly into the issue tracker to get started:

```
Basic Information
  Version of libarchive:
  How you obtained it:  (build from source, pre-packaged binary, etc)
  Operating system and version:
  What compiler and/or IDE you are using (include version):

If you are using a pre-packaged binary
  Exact package name and version:
  Repository you obtained it from:

Description of the problem you are seeing:
  What did you do?
  What did you expect to happen?
  What actually happened?
  What log files or error messages were produced?

How the libarchive developers can reproduce your problem:
  What other software was involved?
  What other files were involved?
  How can we obtain any of the above?
```

Depending on the specific type of issue, other information will be helpful:

## Test Failures

If you see any test failures, please include the information above and also:

* Names of the tests that failed.

* Look for the .log files in the /tmp/libarchive_test_*date-and-time* directories.  (On Mac OS, look in $TMPDIR which is different than /tmp.)

Please paste the .log files you will find there directly into your report.


## Problems using libarchive in a program

If you are trying to write a program using libarchive, please include the information above and also:

* It will help us if we can actually run the program.  This is easiest if you can provide source to a short program that illustrates your problem.

* If you have a sufficiently short program that shows the problem, you can either paste it into the report or [put it into a gist](https://gist.github.com).


## Libarchive produced incorrect output

Please tell us what program you ran, any command-line arguments you provided, and details of the input files (`ls -l` output is helpful here).  If the problem involved a command-line program, please copy the full terminal text into the report, including the command line and any error messages.

Please try to make the output file available to us.  Unless it is very large, you can upload it into a fresh github repository and provide a link in your issue report.


## Libarchive could not read a particular input file

Note: If you can provide a **very small** input file that reproduces the problem, we can add that to our test suite.  This will ensure that the bug does not reappear in the future.

A link to the relevant file is usually sufficient.

If you cannot provide the input file or a link to the file, please let us know if there is some other way to obtain it.


## Documentation improvements

We are always interested in improving the libarchive documentation.  Please tell us about any errors you find, including:

* Typos or errors in the manpages provided with libarchive source.

* Mistakes in the [libarchive Wiki](https://github.com/libarchive/libarchive/wiki)

* Problems with the PDF or Wiki files that are automatically generated from the manpages.


# Code Submissions

We welcome all code submissions.  But of course, some code submissions are easier for us to respond to than others. The best code submissions:

* Address a single issue.  There have been many cases where a simple fix to an obvious problem did not get handled for months because the patch that was provided also included an unrelated change affecting an especially complex area of the code.

* Follow existing libarchive code style and conventions.  Libarchive generally follows [BSD KNF](https://www.freebsd.org/cgi/man.cgi?query=style&sektion=9) for formatting code.

* Do not make unnecessary changes to existing whitespace, capitalization, or spelling.

* Include detailed instructions for reproducing the problem you're fixing.  We do try to verify that a submission actually fixes a real problem.  If we can't reproduce the problem, it will take us longer to evaluate the fix.  For this reason, we encourage you to file an issue report first with details on reproducing the problem, then refer to that issue in your pull request.

* Includes a test case.  The libarchive Wiki has [detailed documentation for adding new test cases](https://github.com/libarchive/libarchive/wiki/LibarchiveAddingTest).

* Are provided via Github pull requests.  We welcome patches in almost any format, but github's pull request management makes it significantly easier for us to evaluate and test changes.

