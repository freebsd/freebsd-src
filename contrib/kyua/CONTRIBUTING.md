Contributing code to Kyua
=========================

Want to contribute?  Great!  But first, please take a few minutes to read this
document in full.  Doing so upfront will minimize the turnaround time required
to get your changes incorporated.


Legal notes
-----------

* Before we can use your code, you must sign the
  [Google Individual Contributor License
  Agreement](https://developers.google.com/open-source/cla/individual),
  also known as the CLA, which you can easily do online.  The CLA is necessary
  mainly because you own the copyright to your changes, even after your
  contribution becomes part of our codebase, so we need your permission to use
  and distribute your code.  We also need to be sure of various other
  things--for instance that you will tell us if you know that your code
  infringes on other people's patents.  You do not have to sign the CLA until
  after you have submitted your code for review and a member has approved it,
  but you must do it before we can put your code into our codebase.

* Contributions made by corporations are covered by a different agreement than
  the one above: the
  [Google Software Grant and Corporate Contributor License
  Agreement](https://developers.google.com/open-source/cla/corporate).
  Please get your company to sign this agreement instead if your contribution is
  on their behalf.

* Unless you have a strong reason not to, please assign copyright of your
  changes to Google Inc. and use the 3-clause BSD license text included
  throughout the codebase (see [LICENSE](LICENSE)).  Keeping the whole project
  owned by a single entity is important, particularly to avoid the problem of
  having to replicate potentially hundreds of different copyright notes in
  documentation materials, etc.


Communication
-------------

* Before you start working on a larger contribution, you should get in touch
  with us first through the
  [kyua-discuss mailing
  list](https://groups.google.com/forum/#!forum/kyua-discuss)
  with your idea so that we can help out and possibly guide you.  Coordinating
  upfront makes it much easier to avoid frustration later on.

* Subscribe to the
  [kyua-log mailing list](https://groups.google.com/forum/#!forum/kyua-log) to
  get notifications on new commits, Travis CI results, or changes to bugs.


Git workflow
------------

* Always work on a non-master branch.

* Make sure the history of your branch is clean.  (Ab)use `git rebase -i master`
  to ensure the sequence of commits you want pulled is easy to follow and that
  every commit does one (and only one) thing.  In particular, commits of the
  form `Fix previous` or `Fix build` should never ever exist; merge those fixes
  into the relevant commits so that the history is clean at pull time.

* Always trigger Travis CI builds for your changes (hence why working on a
  branch is important).  Push your branch to GitHub so that Travis CI picks it
  up and performs a build.  If you have forked the repository, you may need to
  enable Travis CI builds on your end.  Wait for a green result.

* It is OK and expected for you to `git push --force` on **non-master**
  branches.  This is required if you need to go through the commit/test cycle
  more than once for any given branch after you have "fixed-up" commits to
  correct problems spotted in earlier builds.

* Do not send pull requests that subsume other/older pull requests.  Each major
  change being submitted belongs in a different pull request, which is trivial
  to achieve if you use one branch per change as requested in this workflow.


Code reviews
------------

* All changes will be subject to code reviews pre-merge time.  In other words:
  all pull requests will be carefully inspected before being accepted and they
  will be returned to you with comments if there are issues to be fixed.

* Be careful of stylistic errors in your code (see below for style guidelines).
  Style violations hinder the review process and distract from the actual code.
  By keeping your code clean of style issues upfront, you will speed up the
  review process and avoid frustration along the way.

* Whenever you are ready to submit a pull request, review the *combined diff*
  you are requesting to be pulled and look for issues.  This is the diff that
  will be subject to review, not necessarily the individual commits.  You can
  view this diff in GitHub at the bottom of the `Open a pull request` form that
  appears when you click the button to file a pull request, or you can see the
  diff by typing `git diff <your-branch> master`.


Commit messages
---------------

* Follow standard Git commit message guidelines.  The first line has a maximum
  length of 50 characters, does not terminate in a period, and has to summarize
  the whole commit.  Then a blank line comes, and then multiple plain-text
  paragraphs provide details on the commit if necessary with a maximum length of
  72-75 characters per line.  Vim has syntax highlighting for Git commit
  messages and will let you know when you go above the maximum line lengths.

* Use the imperative tense.  Say `Add foo-bar` or `Fix baz` instead of `Adding
  blah`, `Adds bleh`, or `Added bloh`.


Handling bug tracker issues
---------------------------

* All changes pushed to `master` should cross-reference one or more issues in
  the bug tracker.  This is particularly important for bug fixes, but also
  applies to major feature improvements.

* Unless you have a good reason to do otherwise, name your branch `issue-N`
  where `N` is the number of the issue being fixed.

* If the fix to the issue can be done *in a single commit*, terminate the commit
  message with `Fixes #N.` where `N` is the number of the issue being fixed and
  include a note in `NEWS` about the issue in the same commit.  Such fixes can
  be merged onto master using fast-forward (the default behavior of `git
  merge`).

* If the fix to the issue requires *more than one commit*, do **not** include
  `Fixes #N.` in any of the individual commit messages of the branch nor include
  any changes to the `NEWS` file in those commits.  These "announcement" changes
  belong in the merge commit onto `master`, which is done by `git merge --no-ff
  --no-commit your-branch`, followed by an edit of `NEWS`, and terminated with a
  `git commit -a` with the proper note on the bug being fixed.


Style guide
-----------

These notes are generic and certainly *non-exhaustive*:

* Respect formatting of existing files.  Note where braces are placed, number of
  blank lines between code chunks, how continuation lines are indented, how
  docstrings are typed, etc.

* Indentation is *always* done using spaces, not tabs.  The only exception is in
  `Makefile`s, where any continuation line within a target must be prefixed by a
  *single tab*.

* [Be mindful of spelling and
  grammar.](http://julipedia.meroh.net/2013/06/readability-mind-your-typos-and-grammar.html)
  Mistakes of this kind are enough of a reason to return a pull request.

* Use proper punctuation for all sentences.  Always start with a capital letter
  and terminate with a period.

* Respect lexicographical sorting wherever possible.

* Lines must not be over 80 characters.

* No trailing whitespace.

* Two spaces after end-of-sentence periods.

* Two blank lines between functions.  If there are two blank lines among code
  blocks, they usually exist for a reason: keep them.

* In C++ code, prefix all C identifiers (those coming from `extern "C"`
  includes) with `::`.

* Getter functions/methods only need to be documented via `\return`. A
  redundant summary is not necessary.
