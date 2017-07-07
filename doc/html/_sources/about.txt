Contributing to the MIT Kerberos Documentation
==============================================

We are looking for documentation writers and editors who could contribute
towards improving the MIT KC documentation content.  If you are an experienced
Kerberos developer and/or administrator, please consider sharing your knowledge
and experience with the Kerberos Community.  You can suggest your own topic or
write about any of the topics listed
`here <http://k5wiki.kerberos.org/wiki/Projects/Documentation_Tasks>`__.

If you have any questions, comments, or suggestions on the existing documents,
please send your feedback via email to krb5-bugs@mit.edu. The HTML version of
this documentation has a "FEEDBACK" link to the krb5-bugs@mit.edu email
address with a pre-constructed subject line.


Background
----------

Starting with release 1.11, the Kerberos documentation set is
unified in a central form.  Man pages, HTML documentation, and PDF
documents are compiled from reStructuredText sources, and the application
developer documentation incorporates Doxygen markup from the source
tree.  This project was undertaken along the outline described
`here <http://k5wiki.kerberos.org/wiki/Projects/Kerberos_Documentation>`__.

Previous versions of Kerberos 5 attempted to maintain separate documentation
in the texinfo format, with separate groff manual pages.  Having the API
documentation disjoint from the source code implementing that API
resulted in the documentation becoming stale, and over time the documentation
ceased to match reality.  With a fresh start and a source format that is
easier to use and maintain, reStructuredText-based documents should provide
an improved experience for the user.  Consolidating all the documentation
formats into a single source document makes the documentation set easier
to maintain.
