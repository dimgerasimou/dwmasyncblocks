# dwmblocks

This is a status bar designed to work with the suckless dwm.  
It differs from other dwmblocks builds by executing the blocks asynchronously, thus not having the effect of blocks disappearing if they are late on updating.

## Requirments

In order to build dwmblocks the Xlib header files are needed.  

## Installation

Use make to just compile and install dwmblocks:
```bash
sudo make clean install
```

## Usage

Blocks can be defined to run in `config.h` file, and any executable that prints to stdout can be ran.  
There is an Update Interval that is the interval in seconds that the blocks will update.  
Some scripts e.g. volume dont need to be updated constantly, but just once at launch and then every time volume properties are altered.  
This can be done, by passing the corresponding Update Signal to dwmblocks, and then dwmblocks will update that block.
To pass an Update Signal to dwmblocks can be done by passing the RTMIN signal + the Update Signal number. For example for signal number 10:
```bash
pkill -RTMIN+10 dwmblocks
```
Everytime the `config.h` file is editted, dwmblocks must be (re)compiled.

## Uninstallation

Use make to uninstall the binary
```bash
sudo make uninstall
```