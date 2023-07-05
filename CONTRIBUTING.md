# Contribution Guidelines for GitHub

## General Contributions to FreeBSD

Please read the guidelines in [Contributing to FreeBSD](https://docs.freebsd.org/en/articles/contributing/)
for all the ways you can contribute to the project, how the project is organized,
how to build different parts of the project, etc. The
[developer's handbook](https://docs.freebsd.org/en/books/developers-handbook/)
is another useful resource.

FreeBSD accepts source code contributions using one of several methods:
- A GitHub [pull request](https://github.com/freebsd/freebsd-src/pulls)
- A code review in [Phabricator](https://reviews.freebsd.org/differential)
- An attachment on a [Bugzilla ticket](https://bugs.freebsd.org)
- Direct access to the [Git repository](https://cgit.freebsd.org/src/)

The preferred method depends on a few factors including the size or scope of
the change.  GitHub pull requests are preferred for relatively straightforward
changes where the contributor already has a GitHub account.

## GitHub Pull Requests

Presently, GitHub 'freebsd-src' repository is one of the publish-only services
for the FreeBSD 'src' repository the project uses to promote collaboration and
contribution.  Pull requests that need little developer time, are generally
small, and have limited scope should be submitted. Do not submit pull requests
that are security-related, problem reports, works in progress, changes that are controversial
and need discussion, or changes that require specialized review.

A pull request will be considered if:

* It is ready or nearly ready to be committed. A committer should be able to land the pull request with less than 10 minutes of additional work.
* It passes all the GitHub CI jobs.
* You can respond to feedback quickly.
* It touches fewer than about 10 files and the changes are less than about 200 lines. Changes larger than this may be OK, or you may be asked to submit multiple pull requests of a more manageable size.
* Each logical change is a separate commit within the pull request. Commit messages for each change should follow the [commit log message guide](https://docs.freebsd.org/en/articles/committers-guide/#commit-log-message).
* All commits have, as the author, your name and valid email address as you would like to see them in the FreeBSD repository. Fake github.com addresses cannot be used.
* The scope of the pull request should not change during review. If the review suggests changes that expand the scope, please create an independent pull request.
* Fixup commits should be squashed with the commit they are fixing. Each commit in your branch should be suitable for FreeBSD's repository.
* Commits should include one or more `Signed-off-by:` lines with full name and email address certifying [Developer Certificate of Origin](https://developercertificate.org/).
* The commits follow FreeBSD's style guide. See [Style](#Style).
* Run tools/build/checkstyle9.pl on your Git branch and eliminate all errors
* The commits do not introduce trailing white space.
* If the commmit fixes a bug, please add 'PR: \<bugnumber>\' to the commit message.
* If there's a code review in Phabricator, please include a link as a 'Differential Revision: ' line.

When updating your pull request, please rebase with a forced push rather than a
merge commit.

More complex changes may be submitted as pull requests, but they may be closed
if they are too large, too unwieldy, become inactive, need further discussion in
the community, or need extensive revision.  Please avoid creating large,
wide-ranging cleanup patches: they are too large and lack the focus needed for a
good review.  Misdirected patches may be redirected to a more appropriate forum
for the patch to be resolved.

Please make sure that your submissions compile and work before submitting. If
you need feedback, a pull request might not be the right place (it may just be
summarily closed if there are too many obvious mistakes).

If you want to cleanup style or older coding conventions in preparation for some
other commit, please go ahead and prepare those patches. However, please avoid just
cleaning up to make it cleaner, unless there's a clear advantage to doing
so. While the project strives to have a uniform coding style, our style offers a
range of choices making some 'cleanups' ambiguous at best. Also, some files have
their own consistent style that deviates from style(9). Style changes take
volunteer time to process, but that time can be quite limited, so please be
respectful.

The current theory for pull requests on GitHub is to facilitate inclusion in the
project. The guidelines are streamlined for quick decisions about each pull
request. Unless explicitly stated, rejection here as "not ready" or "too large"
does not mean the project is uninterested in the work, it just means the
submission does not meet the limited scope for pull requests accepted
here. Sometimes it is easier to review a GihHub pull request than to do the
review in Phabricator, so that's also allowed.

## Style

Avoid adding trailing newlines and whitespace. These slow down the integration
process and are a distraction. `git diff` will highlight them in red, as will
the Files Changed tab in the pull request.

For C programs, see [style(9)](https://man.freebsd.org/cgi/man.cgi?query=style&sektion=9)
for details. You can use [Clang format](https://clang.llvm.org/docs/ClangFormat.html)
with the top level .clang-format file if you are unsure. The
[git clang-format](https://github.com/llvm-mirror/clang/blob/master/tools/clang-format/git-clang-format)
command can help minimize churn by only formatting the areas nearby the changes. While
not perfect, using these tools will maximize your chances of not having style
comments on your pull requests.

For Makefiles changes, see
[style.Makefile(5)](https://man.freebsd.org/cgi/man.cgi?query=style.Makefile&sektion=5)
for details. FreeBSD's base system uses the in-tree make, not GNU Make, so 
[make(1)](https://man.freebsd.org/cgi/man.cgi?query=make&sektion=1) is another useful
resource.

The project uses mdoc for all its man pages. Changes should pass `mandoc -Tlint` and igor (install the latter with `pkg install igor`).
Please be sure to observe the one-sentence-per-line rule so manual pages properly render. Any semantic changes to the manual pages should bump the date.
[style.mdoc(5)](https://man.freebsd.org/cgi/man.cgi?query=style.mdoc&sektion=5) has the all details. 

For [Lua](https://www.lua.org), please see
[style.lua(9)](https://man.freebsd.org/cgi/man.cgi?query=style.lua&sektion=9)
for details. Lua is used for the boot loader and a few scripts in the base system.

For shell scripts, avoid using bash. The system shell (/bin/sh) is preferred.
Shell scripts in the base system cannot use bash or bash extensions
not present in FreeBSD's [shell](https://man.freebsd.org/cgi/man.cgi?query=sh&sektion=1).

## Signed-off-by

Other projects use Signed-off-by to create a paper trail for contributions they
receive. The Developer Certificate of Origin is an attestation that the person
making the contribution can do it under the current license of the file. Other
projects that have 'delegated' hierarchies also use it when maintainers
integrate these patches and submit them upstream.

Right now, pull requests on GitHub are an experimental feature. We strongly
suggest that people add this line. It creates a paper trail for infrequent
contributors. Also, developers that are landing a pull request will use a
Signed-off-by line to set the author for the commit.

These lines are easy to add with `git commit -s`.
