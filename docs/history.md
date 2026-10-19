<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Commit history in this restricted workspace

The initial environment allowed source-file writes but explicitly prohibited
writes to this directory's `.git`. Development commits were therefore made with
a separate temporary Git directory, and exported to `samsa-history.bundle` in
the workspace. Each commit has Joshua Jun's signoff and an AI co-author trailer.
No remote was configured, and nothing was pushed or published.

After permission escalation became available, the existing bundle was imported
into this workspace's `.git`. This is now an ordinary checkout; use `git log`
and `git status` here. Subsequent validation commits are recorded there.

`samsa-history.bundle` remains an ignored, portable backup of the signed history.
To obtain a separate checkout from it:

```sh
git clone samsa-history.bundle ../samsa-checkout
```

The bundle contains all tracked sources, fixtures, vectors, tests, documentation,
and CI configuration. Nothing was pushed or published.
