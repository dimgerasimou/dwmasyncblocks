# dwmblocks

Modular, asynchronous status bar for [dwm](https://dwm.suckless.org/). A small,
async, clickable implementation, with name based control built in.

## Features

- **Asynchronous blocks**: each block forks independently. A slow one never
  freezes the rest of the bar.
- **Clickable blocks**: with dwm patched for
  [statuscmd](https://dwm.suckless.org/patches/statuscmd/), `BLOCK_BUTTON` is
  set in the script's environment on click.
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
	/*Command                  Update Interval   Update Signal */
	{ "/path/to/script",       10,                10 },
};
```
- **Update interval**, in seconds. `0` disables automatic updates.
- **Update signal**, an offset from `SIGRTMIN`. Must be unique per block. `0`
  makes the block unclickable and unreachable by name.

`CLICKABLE_BLOCKS`, `LEADING_DELIMITER`, `DELIMITER`, and
`TRIM_TRAILING_SPACES` are also set here. Rebuild after any change; there is
no runtime reload.

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
