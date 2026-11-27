# dwmblocks

Modular, asynchronous status bar for [dwm](https://dwm.suckless.org/). A small,
async, clickable implementation, with name based control built in.

## Features

- **Asynchronous blocks**: each block forks independently. A slow one never
  freezes the rest of the bar.
- **Clickable blocks**: with dwm patched for
  [statuscmd](https://dwm.suckless.org/patches/statuscmd/), `BLOCK_BUTTON` is
  set in the script's environment on click.
- **Persistent blocks**: a block can run for the daemon's whole lifetime and
  print a line whenever its state changes, so it updates instantly without
  polling or signals. It is restarted with backoff if it exits.
- **Colours and icon style in one place**: blocks print plain fields, the
  daemon colours them and picks icon or ASCII text, from `config.h` defaults
  overridden by X resources. Changes made with `xrdb` apply instantly.
- **Built-in CLI client**: update, restart, or list blocks by name, no
  separate control binary, no memorizing signal numbers.
- **Shell completions**: bash, zsh, fish.

## Why not the alternatives?

**[dwmblocks](https://github.com/torrinfail/dwmblocks)**, the original, runs
every block synchronously through `popen()` - one slow block freezes the
whole bar - and has no click support.

**[dwmblocks-async](https://github.com/UtkarshVerma/dwmblocks-async)** fixes
that (non-blocking, clickable). This project converges on much the same core
design independently. What it doesn't have: a CLI to address blocks by name
(only raw signal numbers), shell completions and some other QOL improvements.

## Build / install

Edit `config.h` to match your setup, then:
```bash
sudo make clean install
```
Installs the binary, man page, and shell completions (bash, zsh, fish).

### Dependencies

- C compiler (gcc/clang)
- make
- Xlib headers (Arch: `libx11`)

## Configuration

Blocks are defined in `config.h`:
```c
const Block blocks[] = {
	/*Command                  Update Interval   Update Signal   Icons   Persist */
	{ "/path/to/script",       10,                10,             1,      0 },
};
```
- **Update interval**, in seconds. `0` disables automatic updates.
- **Update signal**, an offset from `SIGRTMIN`. Must be unique per block. `0`
  makes the block unclickable and unreachable by name.
- **Icons**, `1` to show the block's `rich` text, `0` for its `plain` text.
- **Persist**, `1` to run the block once, with `BLOCK_PERSIST=1`, and keep it
  running. Each line it prints replaces the block's text. Its interval is
  ignored and updates by signal, `--update` or `--all` don't re-run it, but
  clicks still run the command once with `BLOCK_BUTTON` set; that run's
  output is discarded, since the running copy reports the change itself. If
  it exits, the block is blanked and the command restarted after 1s,
  doubling up to 60s while it keeps failing within 30s of starting. It gets
  `SIGTERM` when dwmblocks exits, and dies with it even if dwmblocks is
  killed, provided the command is a single program (it is run with `exec`).

Colour classes and their defaults live in the `colours` table. `CLR_FMT` and
`CLR_NRM` set the escape written around coloured text (status2d by default).
`CLICKABLE_BLOCKS`, `LEADING_DELIMITER`, `DELIMITER`, `STATUS_SUFFIX`
(text appended after the last block) and `TRIM_TRAILING_SPACES` are also set
here. Rebuild after editing `config.h`.

### X resources

Colours and icon style can be changed at runtime, without a rebuild:
```
statusblocks.clr_bat_crt: #F38BA8
statusblocks.icon_battery: 0
```
The prefix is `XRES_PREFIX`. Each block's current colours are also exported
to it as `BLOCK_CLR_<CLASS>`, e.g. `BLOCK_CLR_CAL=#F38BA8`; unset classes are
left out.

### Block output

A block prints one line of tab-separated `key=value` fields:
```
class=bat_nrm<TAB>rich=<glyph><TAB>plain=B:87%
```
- `class` names an entry in `colours`; unknown or missing classes are
  uncoloured.
- `rich` is shown when the block's icon mode is on, `plain` when it is off.
  If one is missing the other is used; if it is present but empty, nothing
  is shown.
- A new segment starts at each `class=` or repeated key, so one line can hold
  several differently coloured runs.

A block whose chosen text is empty (or only spaces) is hidden, along with
its delimiter, e.g. `rich=<TAB>plain=` for an updates block with nothing
pending. A line that doesn't start with one of these keys is shown as is, so plain
scripts keep working. Output is cut to 70 visible characters.

## Usage

Behavior is configured via flags (see `dwmblocks --help` or the man page).

Quick examples:
```bash
dwmblocks &                 # start the daemon
dwmblocks --update volume   # update one block by name
pkill -RTMIN+10 dwmblocks   # ...or by raw signal
dwmblocks --all             # update all blocks
dwmblocks --restart         # restart the daemon
```

### Running at startup

Add to your `~/.xinitrc` or window manager's autostart:
```bash
dwmblocks &
```

## License

This project is licensed under the GNU General Public License v3.
See the `LICENSE` file for details.

© 2022-2024 Dimitris Gerasimou

## See Also

- [dwm](https://dwm.suckless.org/) - the window manager this bar is built for
- [dwmblocks](https://github.com/torrinfail/dwmblocks) - the original, synchronous implementation
- [dwmblocks-async](https://github.com/UtkarshVerma/dwmblocks-async) - async fork, similar with this project
