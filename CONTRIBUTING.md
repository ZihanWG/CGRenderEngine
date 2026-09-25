# Contributing

This is a single-maintainer project, so nothing here is process for its own
sake. These are the conventions worth writing down so they survive a gap
between sessions, and so the ones worth checking can be checked.

## Branching

`main` is the only long-lived branch. Everything else is short-lived: branch
off `main`, do one thing, merge back, delete.

Branch names are `type/short-description` in kebab-case:

| Type | For |
| --- | --- |
| `feat` | A new capability |
| `fix` | A defect in existing behaviour |
| `perf` | A change whose point is measured cost |
| `refactor` | Restructuring that neither fixes nor adds |
| `docs` | Documentation only |
| `test` | Tests only |
| `chore` | Build, tooling, dependencies |
| `style` | Formatting only, no semantic change |

For example: `feat/cascaded-shadows`, `fix/uniform-ring-fence`,
`perf/material-bind-hoist`, `refactor/texture-move-semantics`.

A merged pull request's branch name cannot be renamed and the pull request
cannot be deleted, so pick the name carefully the first time.

### Why not Git Flow

Long-lived `develop`, `test` and `release` branches solve problems this project
does not have: several teams landing work at once, a QA group that needs a
stable branch, and a staged release train. With one contributor and no
deployment environments, `develop` would be a shadow of `main` that costs an
extra merge per change. If the project ever ships versioned binaries, the
increment to add is a `release/x.y` branch cut from `main` for backporting
fixes — not the rest of the model.

## Commits

One logical change per commit. "Logical" is about the change, not the file
count: an implementation and the tests that cover it are one commit, while
three unrelated fixes are three commits even though they are small.

Write the subject in the imperative mood, describing the change itself:

```
Reject unsupported glTF features
Fix uniform ring buffer reuse and pass cleanup
Add batching metrics and CPU-only meshes
```

Subjects carry **no `feat:` / `fix:` type prefix**. The branch name already
carries the type, and nothing in this repository consumes Conventional Commits
— there is no generated changelog and no semantic-version release.

Use the body for why, not what — the diff already says what. Worth including:
the reasoning a future reader would otherwise have to reconstruct, the numbers
behind a `perf` change, and what you actually verified.

### Fixing a commit you have not pushed

```sh
git commit --amend            # reword or extend the last commit
git reset --soft HEAD~1       # undo the last commit, keep the changes staged
git reset --hard HEAD~1       # undo the last commit AND discard its changes
```

`git reset --hard HEAD` is **not** on that list. It does not undo anything:
`HEAD` is the commit you just made, so the commit stays where it is, and
`--hard` throws away every uncommitted change in the working tree.

Only rewrite history that has not been pushed, or that lives on a branch nobody
else has.

## Formatting

`.clang-format` describes the style the tree is already written in: Allman
braces, 4-space indentation, indented namespace bodies, and a 120-column limit.
`.editorconfig` covers encoding, line endings, and indentation for editors that
do not run clang-format.

The tree has **not** been bulk-reformatted against the config, and CI does not
check formatting. Hand-wrapped argument lists and operator chains are common
and are left alone. So:

- format the lines you change (your editor's "reformat selection" or
  `clang-format-diff`), not whole files;
- never mix a reformat into a behavioural commit.

If a whole-tree reformat ever lands, make it a `style/` commit of its own, list
its hash in `.git-blame-ignore-revs`, and turn the formatting check on in CI in
the same change so the tree cannot drift again. Pin one clang-format major
version for it; the output changes between majors.

`SortIncludes` is off: include order is left to the author.

GLSL under `Shaders/` is not formatted by any tool. Match the surrounding style
by hand.

## Static analysis

`.clang-tidy` holds a deliberately narrow check set (`bugprone-*`,
`performance-*`, a few `modernize-*`) so that what it prints is worth reading.
It is report-only — `WarningsAsErrors` is empty and CI does not run it yet.
It needs a `compile_commands.json`, which a Ninja build tree produces with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`.

## Verification

`Tools/dev/verify_renderer.sh` is the single entrypoint. It finds CMake, Ninja
and MinGW in a CLion install when they are not on `PATH`.

| Mode | What it runs |
| --- | --- |
| `shaders` | `glslangValidator` on every shader in `Shaders/` |
| `tests` | configure if needed, build, run the CPU-only tests |
| `fast` | `shaders`, build every target including the editor, run the tests |
| `full` | `fast`, then a Release build and test run in `out/build-release` |

`fast` is the normal gate before pushing. None of the modes runs the renderer:
GPU output and frame timings need a run of `Sandbox`, and are separate evidence.

## What CI checks

| Check | Workflow | Blocking |
| --- | --- | --- |
| Windows (MSVC) Release build + CPU-only tests | `ci.yml` | Yes |
| Linux (GCC, X11) Release build + CPU-only tests | `ci.yml` | Yes |
| GLSL validation with `glslangValidator` | `ci.yml` | Yes |

CI does not check formatting or run clang-tidy, and never opens a window: GPU
output still needs a local run of `Sandbox`.

## Before pushing

- The change is one logical unit, with a subject that says what it does and a
  body that says why.
- `Tools/dev/verify_renderer.sh fast` passes.
- A behavioural change that can be checked without a window has a CPU-only
  test, and that test fails without the change.
- `README.md` and `docs/ARCHITECTURE.md` still describe the controls, targets
  and frame flow accurately.
- Anything a reader would have to take on trust — a measurement, a claim about
  behaviour — is either verified in the commit body or not claimed.
