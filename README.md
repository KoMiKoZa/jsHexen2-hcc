<p align="center"><img src=".github/icon.png" width="96" height="96" alt="jsHexen2 HexenC Compiler"></p>

# jsHexen2 HexenC Compiler

![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)
![HCC](https://img.shields.io/badge/hcc-1.0.0-orange.svg)
![Base](https://img.shields.io/badge/base-Raven_HCC-8B0000.svg)

> [!TIP]
> **Works with any Hexen II engine.** Same HexenC in, same standard `progs.dat` out, Raven's
> original Hexen II included. It's a drop-in replacement for Raven's HCC with the same command
> line, and your code needs no changes.
>
> - **Fits about four times more code:** `-ot` cuts the globals a progs needs by about 80%.
> - **Fixes a Raven compiler bug** that silently broke parts of Hexen II's own game code, and
>   warns you where your code had it.
> - **Never writes a broken progs:** too big for the format, and the build stops with an error.

HCC is Raven Software's HexenC compiler, the tool that turns Hexen II game code into `progs.dat`.
This is Raven's HCC from the Portal of Praevus tools source, maintained alongside the
[jsHexen2](https://github.com/KoMiKoZa) engine and used to build
[jsHexen2 Progs](https://github.com/KoMiKoZa/jsHexen2-progs).

It reads the same HexenC and writes the same standard `progs.dat` format (version 6) that every
Hexen II engine runs. Your mod does not need jsHexen2 to use it.

## What's different from Raven's HCC

### `-ot`: room for much more code

**`-ot` cuts the globals a progs needs by about 80%.** jsHexen2's progs went from 99.8% of the
65,536 limit to 20%, leaving room for roughly four times as much new code. The file also comes out
about 17% smaller.

HCC gives every intermediate result of every expression a global of its own, and never reuses it.
A `progs.dat` can address at most 65,536 globals, so a large progs runs out long before it runs out
of anything else: jsHexen2's progs uses 65,392 of them, and Raven's own Portal of Praevus progs
uses 64,454.

With `-ot`, intermediate results share globals whenever no other code can run between the moment
they are computed and their last use. The same jsHexen2 progs compiles to 12,997 globals.

- The instructions are the same; only the globals that hold intermediate results change. A progs
  built with `-ot` behaves exactly like one built without it.
- Saves store globals, fields and functions by name, and none of those names change, so saves
  load in either build.
- Without `-ot`, intermediate results get their globals the way Raven's HCC gives them.

### The overwritten return value (a Raven HCC bug)

Every call and every `random()` or `randomv()` leaves its result in one shared return slot, and
Raven's HCC reads it from there. When a second call in the same expression runs before the first
result is used, the second result is read in place of the first:

```c
if (visible(self.enemy) && infront(self.enemy))   // Raven's HCC runs this as infront() && infront()
	...
```

This is not a corner case. Raven's own Hexen II game code has ten such expressions, and they
change how the game plays:

- Golems fire their eye beam without line of sight.
- Hurt imps and sheep stagger toward a fixed direction instead of away from what hurt them.
- Mezzomen block shots that come from behind them.
- Mezzomen and yakmen roar over a kill through walls.
- In co-op, a player pushes another from any side.
- The Pentacles climbs down even when the step down is blocked.
- Some particle sprays come out narrower than written.

This compiler keeps the first result before the second call runs, so the line does what it says.
It prints a warning with the file and line at each place it fixes:

```
golem.hc(1026) : warning : kept the result of visible() in a temp: a later call in this expression would overwrite it
```

Code without such an expression compiles byte for byte as before. The fix is on by default, with or
without `-ot`. The kept result is an ordinary HCC temp, so if a function calls itself between the
two calls, as in `return fib(n - 1) + fib(n - 2);`, the inner call can still overwrite it, as it
can any temp of that expression; keep such a result in a local variable.

If you need Raven's exact output, for example to match an older build of your mod, `-noretfix`
compiles these expressions the way Raven's HCC did.

[jsHexen2 Progs](https://github.com/KoMiKoZa/jsHexen2-progs) fixes all ten in the source; its
changelog tags them **[RAVEN HCC BUG]**.

### A progs too big for the format stops the build

A progs that needs more than 65,536 globals cannot be addressed by any engine. Raven's HCC wrote
it anyway and reported success; this one stops with an error:

```
too many globals: 65537, the progs format allows 65536
```

### Smaller changes

- `\\` in a string gives a backslash, which Raven's HCC rejected. Message text can then use
  the `\1` to `\4` lettering escapes of Shanjaq's uHexen2 and jsHexen2, written `"\\1"`.
- Builds with Visual Studio 2022 (`build_vs2022.bat`).
- `-quiet` also silences the line `-ot` prints about its temps.

## Usage

Run it in the folder that holds your `progs.src`. The first line of `progs.src` names the output
file, and the lines after it list the `.hc` files in compile order, as with Raven's HCC.

```
cd portals
hcc -ot -name progs.src
```

| Option | Effect |
|---|---|
| `-name <file>` | The `.src` file to compile (default `progs.src`). |
| `-src <folder>` | Read the `.src` and `.hc` files from this folder. |
| `-ot` | Share globals between intermediate results (see above). |
| `-noretfix` | Let a later call overwrite an unused result, as Raven's HCC did. |
| `-oi` | Share immediates (constants) with named constants of the same value. |
| `-on` | Write a smaller name table: locals and unsaved globals get no names. |
| `-quiet` | Print less. |
| `-fileinfo` | Print the size of each compiled file. |
| `-nowarnings` | Print no warnings. |
| `-urfunc` | List functions that are never referenced. |
| `-asm <function ...>` | Print the compiled statements of these functions. |

Besides `progs.dat`, HCC writes `progdefs.h` (the engine-facing globals and fields) and `files.dat`
(the precache lists).

`-on` drops the names of locals and of globals that saves don't store, so engine features or tools
that look up definitions by name may not find them; leave it off unless you need the space.

## Building

A ready-built `hcc.exe` for Windows is on the
[Releases](https://github.com/KoMiKoZa/jsHexen2-hcc/releases) page. To build it yourself, on
Windows with Visual Studio 2022 (the "Desktop development with C++" workload), run:

```
build_vs2022.bat
```

The compiler is written to `release\hcc.exe`. The source is plain C with no dependencies besides
the C library; other compilers are untested.

## Credits

- **Raven Software** wrote HCC and released it, with the Hexen II source, under the GNU GPL v2.
- **NHCC** (Jacques Krige) and the **hcc of Hammer of Thyrion** (uHexen2) are earlier builds of
  Raven's HCC. This compiler comes from Raven's source and shares no code with them.

## License

GPL v2, the license of Raven's Hexen II source release. See `LICENSE`.
