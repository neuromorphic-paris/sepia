![sepia](sepiaBanner.png "The Sepia banner")

# Sepia

Sepia implements the [EventStream](https://github.com/neuromorphic-paris/eventStream) specification, and provides components on top of which a communication library with actual event-based cameras can be built.

# Installation

## Dependencies

Sepia relies on [Premake 4.x](https://github.com/premake/premake-4.x) (x ≥ 3), which is a utility to ease the install process. Follow these steps:
  - __Debian / Ubuntu__: Open a terminal and execute the command `sudo apt-get install premake4`.
  - __Fedora__: Open a terminal and execute the command `sudo dnf install premake`. Then, run<br />
  `echo '/usr/local/lib' | sudo tee /etc/ld.so.conf.d/neuromorphic-paris.conf > /dev/null`.
  - __OS X__: Open a terminal and execute the command `brew install premake`. If the command is not found, you need to install Homebrew first with the command<br />
  `ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`.

## Install

To install the source, go to the *sepia* directory and run `premake4 install`.
The library files are installed in */usr/local/include*.

## Uninstall

To uninstall the library, run `premake4 uninstall` from the *sepia* directory.

## Test

To test the library, run the following commands:
  - Go to the *sepia* directory and run `premake4 gmake && cd build && make`.
  - Run the executable *Release/sepiaTest*.

# User guides and documentation

User guides and code documentation are held in the [wiki](https://github.com/neuromorphic-paris/sepia/wiki).

# License

See the [LICENSE](LICENSE.txt) file for license rights and limitations (GNU GPLv3).
