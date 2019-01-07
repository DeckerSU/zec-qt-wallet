## kmd-qt-wallet  ##

[kmd-qt-wallet](https://github.com/DeckerSU/zec-qt-wallet/tree/komodo) is a fork of [zec-qt-wallet](https://github.com/ZcashFoundation/zec-qt-wallet) adopted for using with KMD daemon (komodod). it's not a full-node itself, it's just a Qt GUI that interacts with komodo daemon (komodod) via RPC calls.

kmd-qt-wallet used `z_sendmany` for every transaction (t->z, z->t, t->t), as a result when you trying to send funds from your t-address to the same address - no rewards (interest) will be received. As a result you **can't claim interest (rewards)** using this software. But it's very handy anyway.

Installation and compile steps is same as described in zec-qt-wallet section, make sure that you use correct branch for komodo.

![Remotenode](docs/remote_node.png?raw=true)

Coin (assetchain) selection in Settings menu (Ctrl-P) will be available if you start kmd-qt-wallet with `--no-embedded` arg. More info available here - [Connecting to a remote zcashd](https://github.com/ZcashFoundation/zec-qt-wallet/wiki/Troubleshooting-&-FAQ#4-connecting-to-a-remote-zcashd).

_PS: kmd-qt-wallet is NOT an official wallet, and is not affiliated with the Komodo / KomodoPlatform in any way._

# Installation

### Linux

_Ubuntu 18.x_ :

```
git clone https://github.com/DeckerSU/zec-qt-wallet
cd zec-qt-wallet
git checkout komodo
git pull
QT_SELECT=5 qmake 
make -j$(nproc)

```
_Ubuntu 16.x_ :

```
# installing Qt 5.9.1, bcz default Qt 5.5.1 is not good for this project

wget http://download.qt.io/official_releases/qt/5.9/5.9.1/qt-opensource-linux-x64-5.9.1.run
chmod +x qt-opensource-linux-x64-5.9.1.run
./qt-opensource-linux-x64-5.9.1.run
# during installation leave install folder "AS IS", i.e. $HOME/Qt5.9.1

git clone https://github.com/DeckerSU/zec-qt-wallet
cd zec-qt-wallet
git checkout komodo
git pull
$HOME/Qt5.9.1/5.9.1/gcc_64/bin/qmake 
make -j$(nproc)
```

Launch: `./release/kmd-qt-wallet --no-embedded` (in no-embedded mode you should enter Settings (Ctrl-P) and fill connection params, or select needed coin from the list. komodod should be launched before start of kmd-qt-wallet)

_p.s._ Qt versions lower than 5.6.0 are incompatible with this project.

## zec-qt-wallet  ##

zec-qt-wallet is a z-Addr first, Sapling compatible wallet and full node for zcashd that runs on Linux, Windows and macOS.

![Screenshots](docs/screenshot-sub.png?raw=true)

# Installation

Head over to the releases page and grab the latest installers or binary. https://github.com/ZcashFoundation/zec-qt-wallet/releases

### Linux

If you are on Debian/Ubuntu, please download the `.deb` package and install it.
```
sudo dpkg -i linux-deb-zec-qt-wallet-v0.5.6.deb
sudo apt install -f
```

Or you can download and run the binaries directly.
```
tar -xvf zec-qt-wallet-v0.5.6.tar.gz
./zec-qt-wallet-v0.5.6/zec-qt-wallet
```

### Windows
Download and run the .msi installer and follow the prompts. Alternately, you can download the release binary, unzip it and double click on zec-qt-wallet to start.

### macOS
Double-click on the .dmg file to open it, and drag zec-qt-wallet on to the Applications link to install.

## zcashd
zec-qt-wallet needs a Zcash node running zcashd. If you already have a zcashd node running, zec-qt-wallet will connect to it. 

If you don't have one, zec-qt-wallet will start its embedded zcashd node. 

Additionally, if this is the first time you're running zec-qt-wallet or a zcashd daemon, zec-qt-wallet will download the zcash params (~1.7 GB) and configure `zcash.conf` for you. 

Pass `--no-embedded` to disable the embedded zcashd and force zec-qt-wallet to connect to an external node.

## Compiling from source
zec-qt-wallet is written in C++ 14, and can be compiled with g++/clang++/visual c++. It also depends on Qt5, which you can get from [here](https://www.qt.io/download). Note that if you are compiling from source, you won't get the embedded zcashd by default. You can either run an external zcashd, or compile zcashd as well. 

See detailed build instructions [on the wiki](https://github.com/ZcashFoundation/zec-qt-wallet/wiki/Compiling-from-source-code)

### Building on Linux

```
git clone https://github.com/ZcashFoundation/zec-qt-wallet.git
cd zec-qt-wallet
/path/to/qt5/bin/qmake zec-qt-wallet.pro CONFIG+=debug
make -j$(nproc)

./zec-qt-wallet
```

### Building on Windows
You need Visual Studio 2017 (The free C++ Community Edition works just fine). 

From the VS Tools command prompt
```
git clone https://github.com/ZcashFoundation/zec-qt-wallet.git
cd zec-qt-wallet
c:\Qt5\bin\qmake.exe zec-qt-wallet.pro -spec win32-msvc CONFIG+=debug
nmake

debug\zec-qt-wallet.exe
```

To create the Visual Studio project files so you can compile and run from Visual Studio:
```
c:\Qt5\bin\qmake.exe zec-qt-wallet.pro -tp vc CONFIG+=debug
```

### Building on macOS
You need to install the Xcode app or the Xcode command line tools first, and then install Qt. 

```
git clone https://github.com/ZcashFoundation/zec-qt-wallet.git
cd zec-qt-wallet
/path/to/qt5/bin/qmake zec-qt-wallet.pro CONFIG+=debug
make

./zec-qt-wallet.app/Contents/MacOS/zec-qt-wallet
```

### [Troubleshooting Guide & FAQ](https://github.com/ZcashFoundation/zec-qt-wallet/wiki/Troubleshooting-&-FAQ)
Please read the [troubleshooting guide](https://github.com/ZcashFoundation/zec-qt-wallet/wiki/Troubleshooting-&-FAQ) for common problems and solutions.
For support or other questions, tweet at [@zecqtwallet](https://twitter.com/zecqtwallet) or [file an issue](https://github.com/ZcashFoundation/zec-qt-wallet/issues).

_PS: zec-qt-wallet is NOT an official wallet, and is not affiliated with the Zerocoin Electric Coin Company in any way._
