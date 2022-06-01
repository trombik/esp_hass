# `esp_hass`

## Branches

`main` is the latest development branch. All PRs should target this branch.

Use prefixes defined in [.github/pr-labeler.yml](.github/pr-labeler.yml) in
branch name, such as `bugfix-issue-2`, `feat-new-feature`, or `doc-README`.
These prefixes are used by
[pr-labeler-action](https://github.com/TimonVS/pr-labeler-action), which
automatically lables PRs.  If a PR does not follow the branch naming
convention, the PR will not be labeled, and will not be included in release
notes. In that case, the PR must be labeled manually.

## Labels for Pull Requests

Labels are applied to PRs automatically by matching branch name.

* `bugfix`, a PR that fixes bugs
* `documentation`, a PR that includes documentation
* `enhancement`, a PR that implements, or adds a feature

These labels are used by
[release-drafter](https://github.com/release-drafter/release-drafter#change-template-variables),
which automatically generates a draft release note.
