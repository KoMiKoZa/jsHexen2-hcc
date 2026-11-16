<p align="center"><img src=".github/icon.png" width="96" height="96" alt="jsHexen2 HexenC Compiler"></p>

# jsHexen2 HexenC Compiler

![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)
![HCC](https://img.shields.io/badge/hcc-1.0.2-orange.svg)
![Base](https://img.shields.io/badge/base-Raven_HCC-8B0000.svg)

> [!TIP]
> **Works with any Hexen II engine.** It reads the same HexenC and writes the same standard
> `progs.dat`, so it also works with Raven's original Hexen II. It's a drop-in replacement for
> Raven's HCC with the same command line, and your code compiles as it is.
>
> - **Fits about four times more code:** a progs needs about 80% fewer globals than with Raven's
>   HCC.
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
hcc
```

It prints the full path of the progs it wrote, after `object file`, so you can check that it's the
file your game loads.

It compiles HexenC as Raven's HCC defines it. For a mod written for FTEQCC, add `-fte` (see
[Code written for FTEQCC](#code-written-for-fteqcc-fte)).

| Option | Effect |
|---|---|
| `-name <file>` | The `.src` file to compile (default `progs.src`). |
| `-src <folder>` | Read the `.src` and `.hc` files from this folder. |
| `-o <file>` | Write the progs to this file instead of the one named in `progs.src`. |
| `-fte` | Also accept code written for FTEQCC. |
| `-noot` | Give every intermediate result a global of its own, as Raven's HCC did (see [Room for much more code](#room-for-much-more-code)). |
| `-noretfix` | In an expression with two calls, let the later call overwrite the earlier result, as Raven's HCC did. |
| `-noifvecfix` | Test only x in `if (vector)` and the other vector conditions, as Raven's HCC did. |
| `-oi` | Share immediates (constants) with named constants of the same value. |
| `-on` | Write a smaller name table: locals and unsaved globals get no names. |
| `-quiet` | Print less. |
| `-fileinfo` | Print the size of each compiled file. |
| `-nowarnings` | Print no warnings. |
| `-urfunc` | List functions that are never referenced. |
| `-progdefs` | Also write `progdefs.h`, the engine-facing globals and fields, to the current folder. Only engine source code uses it. |
| `-asm <function ...>` | Print the compiled statements of these functions. |

Besides `progs.dat`, HCC writes `files.dat` (the precache lists) to the current folder, or to the
`-src` folder.

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

### Room for much more code

**A progs needs about 80% fewer globals.** jsHexen2's progs went from 99.97% of the 65,536 limit
to 20%, leaving room for roughly four times as much new code. The file also comes out about 17%
smaller.

Raven's HCC gives every intermediate result of every expression a global of its own, and never
reuses it. A `progs.dat` can address at most 65,536 globals, so a large progs runs out long before
it runs out of anything else: jsHexen2's progs needs 65,518 of them that way, and Raven's own
Portal of Praevus progs 64,454.

This compiler lets intermediate results share globals whenever no other code can run between the
moment they are computed and their last use. The same jsHexen2 progs compiles to 13,023 globals.

- The instructions are the same; only the globals that hold intermediate results change. A progs
  built either way behaves exactly the same.
- Saves store globals, fields and functions by name, and none of those names change, so saves
  load in either build.
- `-noot` gives every intermediate result a global of its own, as Raven's HCC did. Use it to
  compare a build with one from Raven's HCC, or to rule out the shared globals while you track
  down a problem.
- `-ot`, which turned the sharing on in earlier versions, is still accepted and changes nothing.

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
It prints a warning with the file and line at each place it fixes (not with `-fte`):

```
golem.hc(1026) : warning : kept the result of visible() in a temp: a later call in this expression would overwrite it
```

Code without such an expression compiles to the same instructions as with Raven's HCC. The fix is
on by default, with or without `-noot`. The kept result is an ordinary intermediate result, so if a
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

Hexen II's own game code has 34 such tests: 24 in the Praevus boss, the others in Death's horse,
the hydra, the tornado, the mezzoman's reflection, pushable objects, the Pentacles and ordered
counters.

This compiler tests all three components, as `!vector` does, and prints a warning at each place
(not with `-fte`):

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

### Global strings declared with a value (a Raven HCC bug)

`string GREETING = "Hello";` didn't give `GREETING` the text "Hello". Raven's HCC stored the last
string literal it had read before that line, or nothing. Raven's own game code never declares
strings this way, so Hexen II wasn't affected, but a mod that does showed the wrong text. Arrays of
strings declared with values, such as `string names[2] = {"a", "b"};`, had the same problem. This
compiler stores each string's own text.

### Code written for FTEQCC (`-fte`)

With `-fte`, HCC also accepts the FTEQCC syntax that Hexen II mods use, so many mods written for
FTEQCC compile without edits. The table below shows what it accepts.

```
hcc -fte -o progs.dat
```

`-o` writes the progs to the file you name, whatever path `progs.src` gives. `files.dat` is still
written to the current folder. Without `-fte`, HCC compiles HexenC as Raven's HCC defines it, with
only the string fix above and the changes under Smaller changes.

| FTEQCC code | Meaning |
|---|---|
| `if !(x)` | `if not (x)`; for a string, an empty string counts as false |
| `for (float i = 0; i < 3; i++)` | a loop; `continue` goes to the step |
| `i++`, `++i`, `i--`, `--i` | add or subtract 1; the postfix form gives the old value |
| `local float n = a - b;` | the value is assigned by code on every call |
| a local or parameter named like a global | a new variable that hides the global |
| `-x`, `+x`, `a-1`, `float k = -.5;` | unary minus and plus; `a-1` is a subtraction; `-K` uses the value a global `K` was declared with |
| `v / f`, `v /= f`, `ent.v *= f` | a vector divided or multiplied by a float |
| `v = 0` | a float assigned to a vector sets all three components |
| `[x, y, z]` | a vector made of any three float expressions |
| `a && b` with entities, strings or functions | each value tested as it is |
| `a (+) b` used in an expression | the new value of `a` (for an entity field such as `self.flags (+) b`, the value of `b`) |
| `(a, b)` and `a = 1, b = 2;` | the comma operator |
| `void() f { ... }`, `void() f [frames] { ... }`, `void f(float x) = [frames] { ... }` | function bodies without `=` or with `=` before the frames |
| `f = { ... }` without `;`, a stray `;`, an empty block `{ }` | accepted |
| `nosave float x;`, `const float x = 1;` | `nosave` keeps a global out of saves; `const` changes nothing |
| `float x = 1;` twice with the same value, `{ 1, 2, 3, }` | accepted |
| a large number such as `10000000000` | keeps its value |
| a builtin called with extra arguments, a function with too few | allowed, with a warning |
| `[++ 27 .. 18]` | a cycle that counts down; HCC prints a warning, and you can write `[-- 27 .. 18]` instead |

With `-fte`, a call used as an argument can't overwrite the arguments already stored.

FTEQCC's preprocessor (`#define`, `#ifdef`, `#include`), `?:`, `%`, integer types, classes and
pointers aren't supported. Code that uses them stops the build with an error; it is never compiled
into something else.

Two things keep Raven's meaning under `-fte`: `x (-) BITS` on a global clears the bits, and
`$framesave` and `$framerestore` number frames as Raven's tools do.

With `-fte`, the warnings about the overwritten return value and about `if (vector)` are off. They
mark places where the code's meaning changes from Raven's HCC, and code written for FTEQCC has none.

`-fte` is new. It has been tested on Storm Over Thyrion, whose source compiles unchanged. Mathuzzz,
its author, built it with `-fte -ot` and played its Winter through to the courtyard on uHexen2. If
your mod compiles with `-fte` but plays differently than with FTEQCC, please
[open an issue](https://github.com/KoMiKoZa/jsHexen2-hcc/issues).

### Smaller changes

- A file that ends inside a `//` or `/* */` comment, with no newline after it, compiles. Raven's
  HCC failed on it.
- The compiler holds up to 524,288 statements and 65,536 definitions; Storm Over Thyrion needs
  about 135,000 statements. A progs that outgrows any of the compiler's tables stops with an error;
  Raven's HCC crashed or wrote a broken file.
- `\\` in a string gives a backslash, which Raven's HCC rejected. Message text can then use
  the `\1` to `\4` lettering escapes of Shanjaq's uHexen2 and jsHexen2, written `"\\1"`.
- A function parameter of a field type, such as `.float fld`, holds the field passed to it.
  Raven's HCC also gave it a field of its own: an unused slot on every entity, and a field name in
  the progs that other compilers don't write. Engines that compare field names when loading a
  save, jsHexen2 among them, could then refuse saves after the progs was rebuilt with another
  compiler. The compiled code is unchanged.
- `-quiet` also silences the line about intermediate results.
- Each progs names the compiler and the options that built it, in one string such as
  `jsHexen2-hcc 1.0.2 -fte -ot`, which you can find with a text or hex viewer. Nothing reads it,
  so every engine loads the file as before. It has no date, so the same source and options always
  give the same file.

## Credits

- **Raven Software** wrote HCC and released it, with the Hexen II source, under the GNU GPL v2.
- **FTEQCC** (Spike): comparing this compiler's output with FTEQCC's found two of the Raven bugs
  fixed here.

## License

GPL v2, the license of Raven's Hexen II source release. See `LICENSE`.
