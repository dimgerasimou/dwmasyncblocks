# dwmblocks

This is a status bar designed to work with [dwm](https://dwm.suckless.org/ 'dwm').  
dwmasyncblocks works asynchronously, thus no more frustration for blocks "freezing" the bar.

## Requirments

In order to build dwmblocks the Xlib header files are needed (for Arch, the `libx11` package).

## Installation

Use make to compile and install dwmblocks (make sure you edit config.h to
match your setup beforehand):
```bash
sudo make clean install
```

## Description

With dwmblocks, you can divide the status bar into multiple blocks, thus not having to rerun
every single script everytime the bar updates, giving you control on how often, moreover with
signals you can update any block on demand. It is done by sending dwmblocks a signal:
```bash
pkill -RTMIN+SIG_NO dwmblocks
```
where `SIG_NO` is the signal number of the corresponding block. `SIGUSR1` can be used to
update all blocks together. Moreover dwmblocksctl is included, where instead of trying to
remember the signal number of the block, you can use its name.

Dwmasyncblocks, comes in handy in cases where a block takes several seconds to execute. In
this case, instead of every block waiting for that to end, all blocks can update
independandly thus no "freezing" and no disappearing of blocks.

Blocks are clickable, and that can be utilized by using the "BLOCK_BUTTON" environment
variable in the scripts, to be used with a dwm patched with [statuscmd](https://dwm.suckless.org/patches/statuscmd/ 'statuscmd').

## Usage

To set dwmasyncblocks as the statusbar, it mast be run in the background on startup. Add the
following to the startup script of your window manager (or `.xinitrc` if you use that).
```bash
dwmblocks &
```

### Configuration

The blocks aswell some options can be defined in the `config.h` file:
```c
#define CLICKABLE_BLOCKS 1     // Allows the blocks to be clickable.
#define LEADING_DELIMITER 0    // Places a delimiter at the front of the bar.
#define DELIMITER ""           // The string to be used as the delimiter.

const Block blocks[] = {
	/*Command                  Update Interval   Update Signal */
    ...
	{ "/path/to/script",       10,                10 },
    ...
};
```
The update interval is in seconds, and if set to 0, that means the block wont be updated
automaticly. If the signal is set to 0, then it renders the block unclickable

Everytime the `config.h` file is editted, dwmblocks must be (re)compiled.

## Uninstallation

Use make to uninstall the binary
```bash
sudo make uninstall
```

## Credits
When i finished the project with some minor things missing, i happened to find UtkarshVerma's
[dwmblocks-async](https://github.com/UtkarshVerma/dwmblocks-async 'dwmblocks-async') dwmblocks,
where it is basicly the same as mine.
