# Contributing

Bug reports, testing on other hardware and code contributions are welcome.

## The one hard rule: no Nintendo files

Nothing owned by Nintendo may be committed or attached to an issue or pull request:

- no copies of the game in any form (`.xci`, `.nsp`, `.nca`, `.nso`, RomFS or ExeFS contents)
- no keys or firmware (`prod.keys`, `title.keys`, ...)
- no generated code from the game (`private/wonder/gen`), no translated shaders (`private/wonder/spirv`), no
  APK or data zip built from a dump (they contain the game's code and data)
- no game assets: textures, models, music, text, fonts

Everything derived from the game stays in `private/` (gitignored). Pull requests that include such files cannot be
merged. Screenshots and short clips in bug reports are fine.

## Branches

- `main` is the stable branch.
- `dev` is where work lands first. Open pull requests against `dev`; `dev` is merged into `main` when it is tested.

## Reporting bugs

Please include:

- platform (macOS / Windows / Linux / Android), device, CPU and GPU, driver version
- what you did, what happened, what you expected
- the log (`NX_LOG=<file>`), and a screenshot or short clip for visual bugs
- the debug switches you had set (see [docs/nvn/SWITCHES.md](docs/nvn/SWITCHES.md))

## Testing your change

Build on your platform and look at the result in the game: a fix is not verified until you have seen it work.
If you touch shared rendering code (`runtime/src/nvn/`), say which platforms you tested. The regression suite is
described in [docs/REGRESSION.md](docs/REGRESSION.md).
