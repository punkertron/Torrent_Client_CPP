# Torrent Client

---

## Contents

1. [Project idea](#project-idea)
2. [How To Build](#how-to-build)
3. [Qt Version](#qt-version)
4. [Command-line](#command-line)
5. [Libraries Used](#libraries-used)
6. [TODO](#todo)

---

### Project idea
The main goal of writing a multithreaded Torrent Client in C++ is to gain in-depth knowledge of networking and peer-to-peer protocols while having the flexibility to create either a command-line or graphical user interface version using Qt.

---

### How To Build
To build library (libtorrent_core.a) and two programs (command-line and Qt), simply use the following command in your terminal (you may have to install curl, libcrypto and Qt5/Qt6):
``` bash
cmake -B build/ -S . && make -C build/ -j$(nproc)
```
Two programmes will appear in the `build/` directory:
- **torrent_client** - command-line program.
- **Torrent Client Qt** - graphical user interface (GUI).

Developed on Debian 12.

---

### Qt Version
![QT_version](assets/QT_version.gif)

---

### Command-line
![terminal_version](assets/terminal_version.gif)

The program supports the following command-line options:

| Options | Alternative | Description                             | Default            |
|---------|-------------|-----------------------------------------|--------------------|
| -t      | --torrent   | Location of the .torrent                | REQUIRED           |
| -d      | --directory | Where to save file                      | ~/Downloads (or current directory if ~/Downloads doesn't exist)           |
| -l      | --logs      | Enable logs (./logs/logs.txt)           | false              |
| -h      | --help      | Print arguments and their descriptions  |                    |

---

### Libraries Used
- [**Qt**](https://www.qt.io/): C++ framework for creating graphical user interfaces.
- [**libcurl**](https://curl.se/libcurl/): Use curl to retrieve peer list.
- [**Crypto++**](https://www.cryptopp.com/): For creating sha1.
- [**bencode.hpp**](https://github.com/jimporter/bencode.hpp): Lightweight lib for parsing and generating [**bencoded**](https://en.wikipedia.org/wiki/Bencode) data.
- [**spdlog**](https://github.com/gabime/spdlog): C++ logging library.
- [**cxxopts**](https://github.com/jarro2783/cxxopts): Lightweight C++ command line option parser.

---

### TODO
- Seeding.
- Resuming a download.
- Downloading multi-file Torrents.
- UDP support when receiving a list of peers.
