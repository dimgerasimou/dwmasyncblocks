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

compiledScripts="battery clock internet kernel keyboard volume weather"
coloredScripts="memory"
dependencyList="networkmanager pamixer"
configDirectory="$HOME/.local/bin/statusbar"

# Functions ----------------------------------------------------------

function printHelp {
	echo "This is a simple script to aid with the installation of dwm-asyncblocks."
	echo "Builds dwmblocks and copies all scripts in their designated directories.\n"
	echo "WARNING: Do not run as root. You will be prompted for your password by sudo.\n"
	echo "	--help               Prints this menu and exits."
	echo "	--no-build           Copies scripts without installing dwmblocks."
	echo "	--clean              Cleans the build directory."
	echo "  --install-clean      Installs dwmblocks then cleans the directory."
	echo "  --uninstall          Uninstalls dwmblocks binary and all scripts."
}

function cleanDir {
	echo "Cleaning directory."
	make clean 1> /dev/null 2> log.txt
}

function removeScripts {
	echo "Removing scripts."
	for script in $compiledScripts; do
		if [[ -s $configDirectory/$script ]]; then
			rm -rf $configDirectory/$script 1> /dev/null 2> log.txt
		fi
	done

	if ! [ "$(ls -A $configDirectory)" ]; then
		rmdir $configDirectory 1> /dev/null 2> log.txt
	fi
}

function copyScripts {
	echo "Copying scripts."

	if [[ ! -d $configDirectory ]]; then
		mkdir $configDirectory 1> /dev/null 2> log.txt
	fi

	for script in $compiledScripts; do
		gcc -O2 -o scripts/$script scripts/src/$script.c 1> log.txt 2> log.txt
		mv scripts/$script $configDirectory/$script 1> /dev/null 2> log.txt
	done

	for script in $coloredScripts; do
		gcc -O2 -o scripts/$script scripts/color/$script.c 1> log.txt 2> log.txt
		mv scripts/$script $configDirectory/$script 1> /dev/null 2> log.txt
	done
}

function dependencyCheck {
	echo "Checking dependencies."
	gr='\x1b[32m'
	red='\x1b[31m'
	nrm='\x1b[0m'
	validDependencies=1
	if [[ -s /usr/bin/pacman ]]; then
		for pkg in $dependencyList; do
			if $(pacman -Qi $pkg &> /dev/null); then
				echo -e "	[$gr ✓ $nrm] $pkg is installed."
			else
				echo -e "	[$red ❌$nrm] $pkg is not installed."
				validDependencies=0
			fi
		done

		if [ $validDependencies == 0 ]; then
			echo "Not all dependencies are installed. Install them manually."
			exit 1
		fi
	else
		read "Not running Arch. Please check that all dependencies are installed. Continue? [y/N]" yn
		case $yn in
			[Yy] ) break;;
			* ) exit 1;;
		esac
	fi
}

function build {
	dependencyCheck

	if [ CONFIG == 0 ]; then
		cleanDir
	fi

	echo "Building dwmblocks."
	sudo make install 1> /dev/null 2> log.txt	
}

function uninstall {
	echo "Uninstalling dwmblocks."
	sudo make uninstall 1> /dev/null 2> log.txt
	cleanDir
	removeScripts
}

# Script--------------------------------------------------------------

BUILD=1
CONFIG=0
UNINSTALL=0
CLEAN=0
INSTALL=0

# Checking arguments:
for var in "$@"
do
	case $var in
		"--help" | "-h")    printhelp
		                    exit;;
		"--no-build")       BUILD=0;;
		"--keep-config")    CONFIG=1;;
		"--clean")          CLEAN=1;;
		"--uninstall")      UNINSTALL=1;;
		"--install-clean")  CLEAN=1
		                    INSTALL=1;;
		*)					echo "Invalid arguments. --help for help"
		                    exit 1;;
	esac
done

if [ $UNINSTALL == 0 ] && [ $CLEAN == 0 ]; then
	INSTALL=1
fi

# Check for root privilages.
if (( $EUID == 0 )); then
	echo "Please do not run as root"
		exit 1
fi

# Delete existing log.
if [[ -s log.txt ]]; then
	rm log.txt
fi


if [ $UNINSTALL == 1 ]; then
	uninstall
fi

if [ $INSTALL == 1 ]; then
	if [ $BUILD == 1 ]; then
		build
	fi
	copyScripts
fi

if [ $CLEAN == 1 ]; then
	cleanDir
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
