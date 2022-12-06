# dwmblocks

This is a status bar designed to work with the suckless dwm.  
It differs from other dwmblocks builds by executing the blocks asynchronously, thus not having the effect of blocks disappearing if they are late on updating.

## Requirments

In order to build dwmblocks the Xlib header files are needed.  
Additionaly, the following packages need to be installed to work with the scripts:
- wireplumber or pamixer
- networkmanager

If you use pulseaudio change wireplumber to pamixer in the install script:

`/install.sh`
```bash
...
dependencyList="networkmanager wireplumber"
...
```
to:

`/install.sh`
```bash
...
dependencyList="networkmanager pamixer"
...
```
## Installation

dwmblocks can be installed using the provided `install.sh` script.  
This will build dwmblocks and additionally copy the blocks' scripts to `~/.local/bin/statusbar`.

Alternetively you can use make to just compile and install dwmblocks:
```bash
sudo make install
```

Additionally, your wm must be able to support colored emoji and color in the output.
In dwm this can be achived with the [status2d patch](https://dwm.suckless.org/patches/status2d/ 'dwm.suckless.org/patches/status2d/') and the usage of a ttf font. See [my dwm build](https://github.com/dimgerasimou/dwm 'github.com/dimgerasimou/dwm'). 

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

To uninstall you can just use the install script with an uninstall modifier:
```bash
./install.sh --uninstall
```
Alternetively you can use make to uninstall the binary
```bash
sudo make uninstall
```
or remove it manualy
```bash
sudo rm -f /usr/local/bin/dwmblocks
```
and then remove the block scripts with
```bash
rm -rf ~/.local/bin/statusbar/
```
