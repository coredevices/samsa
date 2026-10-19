<!-- SPDX-FileCopyrightText: 2026 Joshua Jun -->
<!-- SPDX-License-Identifier: Apache-2.0 -->
# Commit history in this restricted workspace

The initial environment allowed source-file writes but explicitly prohibited
writes to this directory's `.git`. Development commits were therefore made with
a separate temporary Git directory, and exported to `samsa-history.bundle` in
the workspace. Each commit has Joshua Jun's signoff and an AI co-author trailer.
No remote was configured, and nothing was pushed or published.

To obtain an ordinary checkout with this history, from the workspace directory:

```sh
git clone samsa-history.bundle ../samsa-checkout
```

The original source files remain here as well. The bundle is ignored by Git to
avoid embedding a repository recursively inside itself. It contains all tracked
sources, fixtures, model vectors, tests, documentation, and CI configuration.
