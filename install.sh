# !/bin/bash
#  ____   ____
# |  _ \ / ___|    Dimitris Gerasimou (dimgerasimou)
# | | | | |  _     <https://github.com/dimgerasimou>
# | |_| | |_| |
# |____/ \____|
#
# Script that aids with the installation of dwm-asyncblocks. Can build dwmblocks, and copy
# the scripts needed for it to run at their designated directories.

# Global variables ---------------------------------------------------

USEPULSEAUDIO=0
SCRIPTS="memory internet keyboard battery kernel time date volume"

# Paths --------------------------------------------------------------

DESTDIR="/usr/local/bin"
SRC="src"
SSRC="$SRC/scripts"
BIN="bin"
SDIR="$HOME/.local/bin/dwmblocks"

# Build options ------------------------------------------------------

CC="cc"
CFLAGS="-pedantic -Wall -Wno-deprecated-declarations -Os"
LDFLAGS="-lX11"
SCRIPTSCFLAGS="-Wall -Os"

# Functions ----------------------------------------------------------

function printHelp {
	echo "This is a simple script to aid with the installation of dwm-asyncblocks."
	echo "Builds dwmblocks and copies all scripts in their designated directories."
	echo ""
	echo "WARNING: Do not run as root. You will be prompted for your password by sudo."
	echo ""
	echo "	help               Prints this menu and exits."
	echo "	options            Prints the build options."
	echo "	clean              Cleans the build directory."
	echo "	install            Installs dwmblocks and the scripts."
	echo "	uninstall          Uninstalls dwmblocks binary and all scripts."
	echo "	pulseaudio         Copies the pulseaudio script for volume."
}

function printOptions {
	echo "Options:"
	echo ""
	echo "Scripts: $SCRIPTS"
	echo "Destination directory: $DESTDIR"
	echo "Scripts directory: $SDIR"
	echo "Compiler: $CC"
}

function clean {
	echo "Cleaning directory."
	rm -rf bin 1> /dev/null 2> log.txt
	rm -f *.o *.gch 1> /dev/null 2> log.txt
}

function removeScripts {
	echo "Removing scripts."
	for script in $SCRIPTS; do
		if [[ -s $SDIR/$script ]]; then
			rm -f $SDIR/$script 1> /dev/null 2> log.txt
		fi
	done

	rmdir --ignore-fail-on-non-empty $SDIR 1> /dev/null 2> log.txt
}

function buildScripts {
	echo "Compiling scripts."

	mkdir -p $BIN 1> /dev/null 2> log.txt
	
	for script in $SCRIPTS; do
		$CC $SCRIPTSCFLAGS -o $BIN/$script $SSRC/$script.c
		if (( $? != 0 )); then
			echo "Failed to compile $script."
			if [ -e log.txt ]; then
				rm log.txt
			fi
			exit 1
		fi
	done

	if [ $USEPULSEAUDIO == 1 ]; then
		$CC $SCRIPTSCFLAGS -o $BIN/volume $SSRC/volume-pa.c
		if (( $? != 0 )); then
			echo "Failed to compile volume-pa."
			if [ -e log.txt ]; then
				rm log.txt
			fi
			exit 1
		fi
	fi
}

function copyScripts {
	echo "Copying scripts."

	mkdir -p $SDIR 1> /dev/null 2> log.txt

	for script in $SCRIPTS; do
		cp -f $BIN/$script $SDIR/$script 1> /dev/null 2> log.txt
	done	
}

function buildDwmblocks {
	mkdir -p $BIN 1> /dev/null 2> log.txt
	$CC -o $BIN/dwmblocks $SRC/main.c $CFLAGS $LDFLAGS
	if (( $? != 0 )); then
		echo "Failed to compile dwmblocks."
		if [ -e log.txt ]; then
			rm log.txt
		fi
		exit 1
	fi
}

function installDwmblocks {
	sudo mkdir -p $DESTDIR 1> /dev/null 2> log.txt
	sudo cp -f $BIN/dwmblocks $DESTDIR 1> /dev/null 2> log.txt
	sudo chmod 755 $DESTDIR/dwmblocks 1> /dev/null 2> log.txt
}

function uninstallDwmblocks {
	sudo rm -f $DESTDIR/dwmblocks 1> /dev/null 2> log.txt
}

# Script--------------------------------------------------------------

UNINSTALL=0
CLEAN=0
INSTALL=0

# Checking arguments:
for var in "$@"
do
	case $var in
		"help" | "-h")    printHelp
		                  exit;;
		"options")        printOptions
		                  exit;;
		"uninstall")      UNINSTALL=1;;
		"install")        INSTALL=1;;
		"clean")          CLEAN=1;;
		"pulseaudio")     USEPULSEAUDIO=1;;
		*)                echo "Invalid arguments. Enter [help] for help"
		                  exit 1;;
	esac
done

# Check for root privilages.
if (( $EUID == 0 )); then
	echo "Please do not run as root"
		exit 1
fi

# Delete existing log.
if [[ -s log.txt ]]; then
	rm log.txt
fi

if  (( $CLEAN == 1 )); then
	clean
fi

if  (( $UNINSTALL == 1 )); then
	echo "Uninstalling dwmblocks."
	uninstallDwmblocks
	echo "Removing scripts."
	removeScripts
elif (( !($CLEAN == 1 && $INSTALL == 0) )); then
	echo "Building dwmblocks."
	buildDwmblocks
	echo "Buidling scripts."
	buildScripts
	if (( $INSTALL == 1)); then
		copyScripts
		installDwmblocks
	fi
fi

if [[ -s log.txt ]]; then
		echo "Finished with errors."
		exit 1
else
	if [ -e log.txt ]; then
		rm log.txt
	fi
	echo "Done!"
fi
