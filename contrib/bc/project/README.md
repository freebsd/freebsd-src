# `bc` Project Management History

This directory has the project management history of `bc`. This `README` exists
to explain what the files are.

* `gitea.db`

  Because I (Gavin Howard, the main author) do not trust big companies, I moved
  `bc` to a self-hosted Gitea instance. This is what's left of the database from
  that instance. Obviously, I wiped of personal identifying information as much
  as possible, but it still has the comments on issues and pull requests, as
  well as the issues and pull requests themselves (whatever matters anyway).

* `github_issues.json`

  This is information about issues reported on GitHub. I used the GitHub CLI to
  export *all* of the available information, since it's public.

* `github_prs.json`

  This is information about pull requests opened on GitHub. I used the GitHub
  CLI to export *all* of the available information, since it's public.

* `issue10.md`

  When I first started self-hosting Gitea, I was not a good sysadmin. On top of
  that, I was learning how to use OpenZFS, and Gitea was stored in a ZFS
  dataset.

  This is the best I could do to reproduce an actual issue that was reported and
  got erased when I rolled back to a ZFS snapshot. It was originally `#10` on
  Gitea, hence the file name.
