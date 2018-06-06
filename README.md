![sepia](banner.png "The Sepia banner")

Sepia implements the [Event Stream](https://github.com/neuromorphic-paris/event_stream) specification, and provides components on top of which a communication library with actual event-based cameras can be built.

# install

Within a Git repository, run the commands:

```sh
mkdir -p third_party
cd third_party
git submodule add https://github.com/neuromorphic-paris/sepia.git
git submodule update --init --recursive
```

On __Linux__, an application using Sepia must link to pthread.

# user guides and documentation

User guides and code documentation are held in the [wiki](https://github.com/neuromorphic-paris/sepia/wiki).

# contribute

## development dependencies

Sepia relies on [Premake 4.x](https://github.com/premake/premake-4.x) (x ≥ 3) to generate build configurations. Follow these steps to install it:
  - __Debian / Ubuntu__: Open a terminal and execute the command `sudo apt install premake4`.
  - __macOS__: Open a terminal and execute the command `brew install premake`. If the command is not found, you need to install Homebrew first with the command<br />
  `ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`.

[ClangFormat](https://clang.llvm.org/docs/ClangFormat.html) is used to unify coding styles. Follow these steps to install it:
- __Debian / Ubuntu__: Open a terminal and execute the command `sudo apt install clang-format`.
- __macOS__: Open a terminal and execute the command `brew install clang-format`. If the command is not found, you need to install Homebrew first with the command<br />
`ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`.

## test

To test the library, run from the *sepia* directory:
```sh
premake4 gmake
cd build
make
cd release
./sepia
```

After changing the code, format the source files by running from the *sepia* directory:
```sh
clang-format -i source/sepia.hpp
clang-format -i test/sepia.cpp
```

# license

See the [LICENSE](LICENSE.txt) file for license rights and limitations (GNU GPLv3).
