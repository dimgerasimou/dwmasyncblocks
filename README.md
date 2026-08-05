# dwmblocks

Modular, asynchronous status bar for [dwm](https://dwm.suckless.org/). Each
block runs independently on its own schedule, so a slow block never freezes
the rest of the bar.

## Requirements

Xlib headers (Arch: `libx11`).

## Installation

Edit `config.h` to match your setup, then:
```bash
sudo make clean install
```
Installs the binary, man page, and shell completions (bash, zsh, fish).

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

Run in the background at startup, e.g. from `.xinitrc`:
```bash
dwmblocks &
```

Update a block on demand, by name or by raw signal:
```bash
dwmblocks --update volume
pkill -RTMIN+10 dwmblocks
```

```
--all               update all blocks
--restart           restart the daemon
--list              list configured block names
--update <block>    update a block by name
```

Blocks are clickable: with dwm patched for
[statuscmd](https://dwm.suckless.org/patches/statuscmd/), clicking a block
sets `BLOCK_BUTTON` in its script's environment to the button number.

## Uninstallation

```bash
sudo make uninstall
```

## Credits

Based on UtkarshVerma's
[dwmblocks-async](https://github.com/UtkarshVerma/dwmblocks-async).
