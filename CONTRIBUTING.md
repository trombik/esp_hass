# How to contribute

Thank you for reading this. We appreciate your contributions.

## Reporting bugs

Please follow the issue template. You need to fill answers the template asks,
and to check appropriate check boxes.

## Contributing code

TL;DR

* Read [README.md](README.md)
* Use prefixes, such as `bugfix`, in the PR branch name.
* Use the Conventional Commits
* Make sure the branch is based on the latest `main`

Please read [README.md](README.md). Your Pull Request should follow the styles,
the branch name convention, etc.

[Fork the repository](../../fork) and clone _your_ fork on local machine.

```console
git clone https://github.com/${OWNER}/${PROJECT}
cd ${OWNER}/${PROJECT}
```

Create a branch. The branch name should follow the branch name convention in
the README. In short, create `bugfix-foo` branch for a bug fix, `feat-foo` for
a feature. `foo` should be something short, and descriptive.

```console
git checkout -b bugfix-issue-1
```

Hack and commit your changes. Follow
[the Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/).

Types of commit include:

* `feat` for a feature or an enhancement
* `bugfix` for fixing bugs
* `doc` for documentation
* `ci` for a CI-related fix or enhancement

Include related GitHub issue number in the description if any. If the type is
`bugfix`, open a GitHub Issue first, describe the bug.

In short, the commit log should have:

* a line of description of the commit, prefixed by a type, possibly with
  related issue numbers
* an empty line
* longer explanation of the commit if necessary. if the PR fixes issues, use
  keywords that close the issue. See
  [Linking a pull request to an issue using a keyword](https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue#linking-a-pull-request-to-an-issue-using-a-keyword)
  for the keywords.

An example:

```text
bugfix: fix issue #1

fixes #1.
```

Push the branch to your fork.

```console
git push --set-upstream origin bugfix-issue-1
```

See if the branch passes the tests. If not, fix the branch. Use `git rebase -i
main` and `git push -f` to merge multiple commits into one. A single commit in
a PR is preferred, but multiple commits are accepted if necessary.

When the branch passes the tests, create a Pull Request. After this point, you
should not use `git rebase`. Follow instuctions in the PR template.
