# Windows: RomFS files with a 260+ character path are unreadable (world 2 boss freeze)

**Platform:** Windows x86-64 (build 2026-10-19)
**Game:** Super Mario Bros. Wonder, base 1.0.0
**Severity:** game-breaking (soft-lock), but trivially avoidable
**Contains no game files** (no assets, code, keys or firmware - see CONTRIBUTING.md)

## Environment

- Windows 11 x86-64, Intel i5-12500H (16 logical cores), 32 GB RAM, RTX 4060 Laptop 8 GB, 144 Hz
- Vulkan 1.3 capable NVIDIA driver, current Game Ready
- runtime build 2026-10-19, game version 1.0.0
- reproduced 6/6: with the default launcher, with no `NX_*` variable at all, with `NX_MUTE=1`,
  and with the process restricted to 2 and to 4 cores

## Reproduction

1. Extract the game to a deep folder, e.g.
   `C:\Users\<user>\Downloads\Super Mario Bros - Wonder (NS) (WinPort)\Super Mario Bros - Wonder (NS) (WinPort)`
   (the shipped archive nests the folder name twice, so the data root alone is 126 characters)
2. Enter the World 2 boss (Fluff-Puff Peaks, Bowser Jr. intro).
3. The last frame stays on screen, music/ambience keep playing, ~60 fps presented, ~0.9 of one core
   used. The save is untouched and the process has to be killed.

## Symptom

The game soft-locks when the World 2 boss (Fluff-Puff Peaks, Bowser Jr. intro) is
supposed to start. The last rendered frame stays on screen, the music/ambience keeps
playing, the process stays at ~0.9 of one core and presents ~60 fps. Restarting the
game, reloading the save, changing affinity, NX_MUTE, or running with no `NX_*`
variables at all does not change anything.

## Cause

`runtime/src/platform_win.cpp` opens host files with `_wfopen` (and checks them with
`std::filesystem`), neither of which is long-path aware in this build; the process
also carries no `longPathAware` manifest. Windows therefore rejects any path of 260+
characters, and `hle_fs.cpp` reports the file as "not found".

With the game extracted to a deep folder (the shipped archive nests the folder name
twice, e.g. `...\Downloads\Super Mario Bros - Wonder (NS) (WinPort)\Super Mario Bros -
Wonder (NS) (WinPort)`), the data root alone is 126 characters, so the longest RomFS
entries cross the limit.

Measured on a real install (19,012 files in `extract/romfs`):

| full path length | result |
|---|---|
| <= 259 chars | every file opens |
| >= 260 chars | every file fails ("not found") |

Exactly 6 files are above 259, and they are exactly the 6 files that fail:

```
264  Event\...\ExceptionalActorSetting\LastBossKoopaHeadChangeStep...bgyml
263  Event\...\ExceptionalActorSetting\BossKoopaJrWonderTransform...bgyml   <- world 2 boss
262  Event\...\ExceptionalActorSetting\CourseDemo_GoroGoroTunnel...bgyml
262  Event\...\ExceptionalActorSetting\CourseDemo_GabonDokan_M12...bgyml
261  Event\...\ExceptionalActorSetting\CourseDemo_ZundokoValley...bgyml
260  Event\...\ExceptionalActorSetting\CourseDemo_PullingNight...bgyml
```

`BossKoopaJrWonderTransform` is the Bowser Jr. transform event setting: it cannot be
loaded, the event never runs, and the game waits for it forever. The same bug is
waiting at the final boss (`LastBossKoopaHeadChangeStep`) and in 4 course cutscenes.

Note: `NX_DATA_ROOT=dados` (a *relative* root) does **not** help - it is apparently
absolutised before the file check, and the same 6 files still fail. A genuinely short
absolute root does help.

## Evidence

`NX_TRACE_FS=1` with the deep install:

```
fs: mounted content: -> C:\Users\...\Super Mario Bros - Wonder (NS) (WinPort)\Super Mario Bros - Wonder (NS) (WinPort)\dados\extract\romfs
fs: OpenFile(content:/Event/EventSetting/EventSettingComponent/ExceptionalActorSetting/BossKoopaJrWonderTransform.engine__event__ExceptionalActorSetting.bgyml) not found
```

with the same install reached through a short junction (`C:\SMW` -> game folder) and
`NX_DATA_ROOT=C:\SMW\dados`:

```
fs: mounted content: -> C:\SMW\dados\extract\romfs
fs: OpenFile(.../BossKoopaJrWonderTransform...bgyml, 1) -> 168
fs: OpenFile(.../BossKoopaJrWonderTransform.bfevfl.zs, 1) -> 1638
fs: OpenFile(.../LastBossKoopaHeadChangeStep...bgyml, 1) -> ...
```

All 17 `ExceptionalActorSetting` files now load, the boss event flow loads, and the
boss fight starts. Pipelines cache is still used (0 new pipelines), so nothing else
regresses. 26 thread dumps taken while frozen show the main thread waiting on a
mutex held by the event thread - that is a symptom of the stuck event, not the cause.

## Suggested fix (runtime side)

Make host file access long-path aware, e.g. in `platform_win.cpp`:

* prefix absolute paths with `\\?\` (after converting `/` to `\` and normalising
  `.`/`..`) in `fopen_utf8`, and use the same helper for the `std::filesystem`
  existence checks in `hle_fs.cpp`; or
* add a `longPathAware` manifest and require the `LongPathsEnabled` policy.

A cheap defensive option is to detect at startup that
`fs::weakly_canonical(data_root() / "extract" / "romfs")` plus the longest known
RomFS relative path exceeds `MAX_PATH`, and print a clear warning that the user must
move the game to a short folder.

## Workaround (no code change)

`docs/WINDOWS-PLAN.md` already recommends extracting to a short path
(`C:\WonderPC`); that alone fixes it. For users who do not want to move the folder, a
junction (`mklink /J C:\SMW <game folder>`) plus `NX_DATA_ROOT=C:\SMW\dados` works.

Attached helper (for other players): `check-path.bat` reports whether an install is
affected, and `FIX-launcher.bat` applies the junction workaround.
