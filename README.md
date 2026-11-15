# Omacalc

A calculator built with Qt Quick and C++ that automatically follows the Omarchy theme and system dark/light mode. It has standard, scientific, graphing, programmer, and date modes, plus unit and currency converters.

<img width="800" alt="Omacalc with the Tokyo Night theme" src="screenshots/omacalc.png" />


## Install

Install via the Omarchy Package Repository via the `omacalc` package.

## Usage

Open the menu at the top left (☰) to switch between the calculators and the
converters. The app reopens in the mode you used last.

### Standard

Percent, CE, C, backspace, 1/x, x², √x, and ± around the digits, with the four
operators in the usual precedence (`×` and `÷` before `+` and `−`). From the
keyboard, `^` raises to a power ahead of everything else, so `2 × 3 ^ 2` gives
`18`. The expression builds up above the display and stays visible with the
result after `=`. Numbers are grouped with thousands separators as you type.

Percent follows the familiar calculator convention: with a pending `+` or `−`
it takes that percentage of the running total (`200 + 10 % =` gives `220`),
while with `×`, `÷`, or `^` (or on its own) it simply divides by 100.

### Scientific

Everything above, plus parentheses, xʸ, 10ˣ, log, ln, n!, |x|, mod, π, e,
the `exp` key for scientific notation (`2 exp 5` is 2 × 10⁵), and F-E to show
results in scientific notation. 2nd swaps in x³, ∛x, ʸ√x, 2ˣ, logᵧx, and eˣ.
The Trig menu has sin, cos, tan, sec, csc, and cot, with 2nd for the inverses
and hyp for the hyperbolic functions. The Func menu has floor, ceiling, a random
number, and degree ↔ degree-minute-second conversion. Angles are in degrees,
radians, or gradians (the DEG button cycles them), and land exactly on
quarter turns, so `sin(180)` is 0.

Function keys act on the number just entered, so `30 sin` gives `sin(30)`.
With nothing entered, they open a call for you to fill in.

### Memory and history

Standard and Scientific share a memory and a history. MS stores the display in
a new memory slot. M+ and M− change the newest slot, MR recalls it, and MC
clears them all. M▾ lists every slot, each with its own MC, M+, and M−.
History lists every calculation since launch. Click an entry to bring its
result back. Neither is saved when the window closes.

### Graphing

Plot up to six equations in `x` at once, in the same syntax the Scientific
keypad types: `x^2`, `2x + 1`, `sin(x)`, `√x`, `|x|`, `1/x`, … (`y =` in front is
optional). Drag to pan, scroll to zoom around the pointer, and hover to trace
each curve's value. Trigonometry is in radians unless you switch the unit.

### Programmer

Whole numbers in two's complement at QWORD, DWORD, WORD, or BYTE size, shown in
hex, decimal, octal, and binary at once. Click a base to type in it. Arithmetic
wraps at the word size. AND, OR, XOR, NOT, NAND, and NOR are under Bitwise, and
the Shift menu chooses arithmetic, logical, rotate, or rotate-through-carry
shifts for `<<` and `>>`. Bits switches the keypad to the individual bits of
the word, to flip one with a click.

### Date calculation

The time between two dates (in calendar terms, weeks, weekdays, and more), a
date moved forward or back by years, months, weeks, and days (optionally
business days), and facts about one date. Click a date to pick another from a
small calendar. Click its title to step out to the months of the year, then to
a span of years. From the keyboard, arrows move by day and week, Page Up and
Page Down by month (with Shift, by year), Enter picks, and `T` picks today.

### Converters

Volume, length, weight and mass, temperature, energy, area, speed, time, power,
data, pressure, angle, and currency. Click either value to type into it; the
other follows. All units lists the value in every unit of the category.
Currency uses the European Central Bank's daily reference rates through
[frankfurter.dev](https://frankfurter.dev). Rates are fetched when you open
Currency (at most every few hours) and cached for offline use. No other mode
uses the network.

### Keyboard

| Keys | Where | Does |
| --- | --- | --- |
| `Alt+1` … `Alt+5` | everywhere | Standard, Scientific, Graphing, Programmer, Date calculation |
| `Ctrl+D` | everywhere | Date calculation and back |
| `F1` or `?` | everywhere | Show every shortcut for the current page |
| `Ctrl+C` / `Ctrl+V` | everywhere | Copy the result / paste a number |
| `Ctrl+H` | Standard, Scientific | History |
| `Ctrl+M` `Ctrl+R` `Ctrl+P` `Ctrl+Shift+P` `Ctrl+L` | Standard, Scientific | MS, MR, M+, M−, MC |
| `Ctrl+Q` | everywhere | Quit |
| `0-9` `.` `+` `-` `*` or `x` `/` `^` | calculators | Digits and operators. The numpad works with or without NumLock |
| `Enter` or `=` | calculators | Equals |
| `Esc` / `Delete` / `Backspace` | calculators | Clear / clear entry / delete a digit |
| `F9` (`S` in Standard) | calculators | Toggle the sign |
| `R` `Q` `@` | Standard | 1/x, x², √x |
| `s` `o` `t`, `S` `O` `T` | Scientific | sin, cos, tan and their inverses |
| `l` `n` `q` `#` `r` `@` `y` `!` `%` | Scientific | log, ln, x², x³, 1/x, √x, ʸ√x, n!, mod |
| `p` `e` `E` `(` `)` `\|` | Scientific | π, e, exponent, parentheses, \|x\| |
| `A`–`F` `&` `\|` `^` `~` `<` `>` | Programmer | Hex digits, AND, OR, XOR, NOT, shift left, shift right |
| `F5` `F6` `F7` `F8` | Programmer | HEX, DEC, OCT, BIN |
| `F12` `F2` `F3` `F4` | Programmer | QWORD, DWORD, WORD, BYTE |
| arrows, `+` `-`, `0` | Graphing | Pan, zoom, reset the view |
| `Tab`, `S` | Converters | Switch fields, swap units |

Colors follow the current Omarchy theme (`~/.local/state/omarchy/current/theme/colors.toml`)
and re-tint live when the theme changes. Text follows the desktop text size —
`omarchy display text size`, or GNOME's `text-scaling-factor`.

## Requirements

- Qt 6: `qt6-base` (including Qt Network, for currency rates), `qt6-declarative`
- `xdg-desktop-portal` and a portal backend

The iA Writer Mono font is bundled under the SIL Open Font License 1.1; see
`fonts/OFL.txt`. The font is copyright Information Architects Inc. and based on
IBM Plex, copyright IBM Corp.

## Development

`bin/build` compiles `build/omacalc` with CMake, and `bin/test` builds and runs the tests offscreen. Building needs `cmake`, `make`, and a C++ compiler on top of the Qt packages above. The package itself is built in [omarchy-pkgs](https://github.com/omacom/omarchy-pkgs/tree/master/pkgbuilds/omacalc).
