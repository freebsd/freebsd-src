.SH
Functions of RCS (Revision Control System)
.PP
RCS manages software libraries. It greatly increases programmer productivity
by providing the following functions.
.IP 1.
RCS stores and retrieves multiple revisions of program and other text.
Thus, one can maintain one or more releases while developing the next
release, with a minimum of space overhead. Changes no longer destroy the
original -- previous revisions remain accessible.
.RS
.IP a.
Maintains each module as a tree of revisions.
.IP b.
Project libraries can
be organized centrally, decentralized, or any way you like.
.IP c.
RCS works for any type of text: programs, documentation, memos, papers,
graphics, VLSI layouts, form letters, etc.
.RE
.IP 2.
RCS maintains a complete history of changes.
Thus, one can find out what happened to a module easily
and quickly, without having to compare source listings or
having to track down colleagues.
.RS
.IP a.
RCS performs automatic record keeping.
.IP b.
RCS logs all changes automatically.
.IP c.
RCS guarantees project continuity.
.RE
.IP 3.
RCS manages multiple lines of development.
.IP 4.
RCS can merge multiple lines of development.
Thus, when several parallel lines of development must be consolidated
into one line, the merging of changes is automatic.
.IP 5.
RCS flags coding conflicts.
If two or more lines of development modify the same section of code,
RCS can alert programmers about overlapping changes.
.IP 6.
RCS resolves access conflicts.
When two or more programmers wish to modify the same revision,
RCS alerts the programmers and makes sure that one modification won't wipe
out the other one.
.IP 7.
RCS provides high-level retrieval functions.
Revisions can be retrieved according to ranges of revision numbers,
symbolic names, dates, authors, and states.
.IP 8.
RCS provides release and configuration control.
Revisions can be marked as released, stable, experimental, etc.
Configurations of modules can be described simply and directly.
.IP 9.
RCS performs automatic identification of modules with name, revision
number, creation time, author, etc.
Thus, it is always possible to determine which revisions of which
modules make up a given configuration.
.IP 10.
Provides high-level management visibility.
Thus, it is easy to track the status of a software project.
.RS
.IP a.
RCS provides a complete change history.
.IP b.
RCS records who did what when to which revision of which module.
.RE
.IP 11.
RCS is fully compatible with existing software development tools.
RCS is unobtrusive -- its interface to the file system is such that
all your existing software tools can be used as before.
.IP 12.
RCS' basic user interface is extremely simple. The novice need to learn
only two commands. Its more sophisticated features have been
tuned towards advanced software development environments and the
experienced software professional.
.IP 13.
RCS simplifies software distribution if customers
maintain sources with RCS also. This technique assures proper
identification of versions and configurations, and tracking of customer
modifications. Customer modifications can be merged into distributed
versions locally or by the development group.
.IP 14.
RCS needs little extra space for the revisions (only the differences).
If intermediate revisions are deleted, the corresponding
differences are compressed into the shortest possible form.
.IP 15.
RCS is implemented with reverse deltas. This means that
the latest revision, which is the one that is accessed most often,
is stored intact. All others are regenerated from the latest one
by applying reverse deltas (backward differences). This
results in fast access time for the revision needed most often.
