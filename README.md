<p align="center"><img src=".github/icon.png" width="96" height="96" alt="jsHexen2 HexenC Compiler"></p>

# jsHexen2 HexenC Compiler

![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)
![HCC](https://img.shields.io/badge/hcc-1.0.1-orange.svg)
![Base](https://img.shields.io/badge/base-Raven_HCC-8B0000.svg)

> [!TIP]
> **Works with any Hexen II engine.** It reads the same HexenC and writes the same standard
> `progs.dat`, so it also works with Raven's original Hexen II. It's a drop-in replacement for
> Raven's HCC with the same command line, and your code compiles as it is.
>
> - **Fits about four times more code:** `-ot` cuts the globals a progs needs by about 80%.
> - **Fixes Raven compiler bugs** that silently broke parts of Hexen II's own game code, and
>   warns you where your code had them.
> - **Never writes a progs too big for the format:** the build stops with an error instead.

HCC is Raven Software's HexenC compiler, the tool that turns Hexen II game code into `progs.dat`.
This is Raven's HCC from the Portal of Praevus tools source, maintained alongside the
[jsHexen2](https://github.com/KoMiKoZa) engine and used to build
[jsHexen2 Progs](https://github.com/KoMiKoZa/jsHexen2-progs). Your mod doesn't need jsHexen2 to
use it.

## Usage

Run it in the folder that holds your `progs.src`. The first line of `progs.src` names the output
file, and the lines after it list the `.hc` files in compile order, as with Raven's HCC.

```
cd portals
hcc -ot -name progs.src
```

It compiles HexenC as Raven's HCC defines it. Extensions that FTEQCC adds, such as the
preprocessor (`#define`, `#ifdef`, `#include`), `for` loops, `?:` and `%`, are not supported.

| Option | Effect |
|---|---|
| `-name <file>` | The `.src` file to compile (default `progs.src`). |
| `-src <folder>` | Read the `.src` and `.hc` files from this folder. |
| `-ot` | Share globals between intermediate results (see below). |
| `-noretfix` | In an expression with two calls, let the later call overwrite the earlier result, as Raven's HCC did. |
| `-noifvecfix` | Test only x in `if (vector)` and the other vector conditions, as Raven's HCC did. |
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

### A progs too big for the format stops the build

A progs that needs more than 65,536 globals cannot be addressed by any engine. Raven's HCC wrote
it anyway and reported success; this one stops with an error:

```
too many globals: 65537, the progs format allows 65536
```

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

Code without such an expression compiles to the same instructions as with Raven's HCC. The fix is
on by default, with or without `-ot`. The kept result is an ordinary intermediate result, so if a
function calls itself between the two calls, as in `return fib(n - 1) + fib(n - 2);`, the inner
call can still overwrite it, as it can any other intermediate result of that expression; keep such
a result in a local variable.

If you need Raven's code, for example to match an older build of your mod, `-noretfix`
compiles these expressions the way Raven's HCC did.

[jsHexen2 Progs](https://github.com/KoMiKoZa/jsHexen2-progs) fixes all ten in the source; its
changelog tags them **[RAVEN HCC BUG]**.

### `if (vector)` tests only x (a Raven HCC bug)

A condition tests a single float, and Raven's HCC gave it the vector's first component, so
`if (self.velocity)` was false for anything moving only along y or z. The same went for `if not`,
`while`, `until`, `do ... while` and `do ... until` with a vector. `!vector` was always right.

Hexen II's own game code has 34 such tests: 25 in the Praevus boss, the others in Death's horse,
the hydra, the tornado, the mezzoman's reflection, pushable objects, the Pentacles and ordered
counters.

This compiler tests all three components, as `!vector` does, and prints a warning at each place:

```
object.hc(352) : warning : this vector condition tests x, y and z: Raven's HCC tested only x
```

Code without a vector condition compiles to the same instructions as with Raven's HCC.
`-noifvecfix` tests only x, as Raven's HCC did. To get the same result under any compiler, write
the test as `if (self.velocity != '0 0 0')`.

### Parameters survive a nested call (a Raven HCC bug)

On each call the engine saves a function's parameters and locals (its saved range), and restores
them when the call returns, so a function can be called again while it is running. Raven's HCC
ended the saved range at the last local declaration, so in a function with no locals the
parameters weren't saved: after a nested call back into the same function, they held the inner
call's values. This compiler always saves them. jsHexen2 Progs has 105 such functions,
`AddMultDamg` among them.

Only the size of the saved range changes; the instructions are the same, and there is no switch
for it.

### Locals declared with a value

A local declared with a value, such as `float n = 4;`, gets a warning. HCC stores the value in the
progs, and no code assigns it. A call normally starts with that value anyway, because the engine
restores a function's locals when it returns, but a nested call into the same function starts
with the outer call's current value. Other compilers assign it on every call. Assign it in the
function body instead:

```
golem.hc(1291) : warning : local 'numSolid' gets its value when the progs loads, not from code: a nested call into this function starts with the outer call's value; assign it in the function body
```

### Smaller changes

- `\\` in a string gives a backslash, which Raven's HCC rejected. Message text can then use
  the `\1` to `\4` lettering escapes of Shanjaq's uHexen2 and jsHexen2, written `"\\1"`.
- A function parameter of a field type, such as `.float fld`, holds the field passed to it.
  Raven's HCC also gave it a field of its own: an unused slot on every entity, and a field name in
  the progs that other compilers don't write. Engines that compare field names when loading a
  save, jsHexen2 among them, could then refuse saves after the progs was rebuilt with another
  compiler. The compiled code is unchanged.
- `-quiet` also silences the line `-ot` prints about intermediate results.

## Credits

- **Raven Software** wrote HCC and released it, with the Hexen II source, under the GNU GPL v2.
- **FTEQCC** (Spike): comparing this compiler's output with FTEQCC's found two of the Raven bugs
  fixed here.

## License

GPL v2, the license of Raven's Hexen II source release. See `LICENSE`.
