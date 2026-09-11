# ZG Browser

A GUI browser for [zg_choir](https://github.com/jfriesne/zg_choir) systems, in the spirit of
muscle's [qt_muscled_browser](https://github.com/jfriesne/muscle/tree/master/platform/qt/qt_muscled_browser) --
but pointed at a ZG system (discovered on the LAN) rather than at a single `muscled` server, and
built on JUCE instead of Qt.

* **Startup:** the app listens for ZG systems on the local network with `zg::SystemDiscoveryClient`
  and lists what it finds (system name, server signature, peer count, addresses).
* **Browsing:** clicking a system opens a live tree of its database. Opening a tree node subscribes
  to that node's children; closing it drops the subscription (and the cached data below it) again,
  so the client only ever holds the part of the database that is on screen -- and that part is
  always up to date, including nodes other clients add or delete while you watch.
* **Selection:** the right-hand pane shows the selected node's `Message` payload (and, for deflated
  payloads, its inflated form as well).
* **Disconnection:** if the system goes away, an overlay says so and `zg::MessageTreeClientConnector`
  keeps trying; when the system comes back the tree is rebuilt and the nodes you had open re-open
  themselves. The header stays live throughout, so you can always go back to the systems list.

## Building

```sh
python3 build.py                     # build for this machine
python3 build.py --debug --run       # debug build, then launch it
python3 build.py --package           # also zip a distributable into build/artefacts
python3 build.py --with-test-server  # also build zg_choir's tree_server (see below)
```

`build.py` needs nothing but Python 3 (standard library only) and CMake 3.22+ with a
C++17 toolchain. It picks a generator (Ninja where available, the Visual Studio default on
Windows), passes the right per-platform options, and builds into `build/<platform>-<arch>/`.
`--help` lists every option, including the platform-specific ones.

Dependencies are fetched by CPM during the configure step, so the first build needs network
access. The script points `CPM_SOURCE_CACHE` at `~/.cache/CPM` (unless you have set it
yourself) so repeat builds -- and the two halves of a macOS universal build -- reuse the
downloads.

The app lands in `build/<platform>-<arch>/zg_browser_artefacts/<config>/`, as
`ZG Browser.app` on macOS, `ZG Browser.exe` on Windows and `ZG Browser` on Linux.

### Platform notes

**macOS** builds `x86_64` and `arm64` in separate trees and `lipo`s them into one universal
bundle, which is then re-signed -- ad-hoc by default, or pass `--macos-signing-identity`.
That re-sign is not optional: `lipo` invalidates the code signature, and macOS refuses to
launch an unsigned arm64 binary. Use `--macos-arch arm64` for a quicker single-architecture
dev build. The minimum deployment target is 11.0 (`--macos-deployment-target`).

**Windows** uses the Visual Studio generator and builds x64 by default
(`--windows-arch Win32|ARM64`).

**Linux** needs the JUCE X11 stack; on Debian/Ubuntu:

```sh
sudo apt install build-essential cmake ninja-build git python3 pkg-config \
     zlib1g-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
     libxcursor-dev libfreetype-dev libfontconfig1-dev
```

### Driving CMake yourself

`build.py` is a convenience, not a requirement -- the project is a plain CMake project:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j8
```

### Dependencies

| Dependency | Version | Why |
|---|---|---|
| [muscle](https://github.com/jfriesne/muscle) | v9.93 | `Message`, networking, the node-tree protocol |
| [zg_choir](https://github.com/jfriesne/zg_choir) | master @ `0069258` (matches muscle v9.93) | discovery, connection, subscriptions |
| [JUCE](https://github.com/juce-framework/JUCE) | 8.0.15 | the UI (`juce_gui_basics` only) |

muscle and zg_choir are fetched source-only and compiled by this project's `CMakeLists.txt` rather
than through their own build files. That is deliberate: their CMakeLists build demos, tools and
tests we don't need, and -- more importantly -- they set ABI-relevant defines (`MUSCLE_USE_PTHREADS`,
`MUSCLE_USE_CPLUSPLUS17`, ...) through directory-scoped flags that would *not* reach this app,
which would silently mismatch struct layouts between the app and the libraries. Here every
consumer inherits the same defines from the `muscle` target.

JUCE is dual-licensed (AGPLv3 / commercial); check that its terms suit your use.

## Trying it out

If you don't have a ZG server to hand, build zg_choir's own test server along with the app
(`--with-test-server`, or `-DZG_BROWSER_BUILD_TEST_SERVER=ON` if you are driving CMake
yourself) and run it:

```sh
python3 build.py --with-test-server
./build/macos-universal/x86_64/zg_tree_server   # advertises the system "test_tree_system"
```

It starts with an empty database; zg_choir's `tree_client` (from the same `tests/` directory) can
fill it with `s srv/machines/alpha`-style commands. Start several servers and they form one system
together.

On macOS 15 and newer the first run asks for permission to use the local network -- ZG's discovery
pings are multicast, so the systems list stays empty if that is denied.

## Layout

| File | Contents |
|---|---|
| `Source/Main.cpp` | `JUCEApplication`, window, muscle's `CompleteSetupSystem` lifetime |
| `Source/MainComponent.*` | owns the callback mechanism + discovery client; switches between the two screens |
| `Source/DiscoveryComponent.*` | the systems list (`IDiscoveryNotificationTarget`) |
| `Source/BrowserComponent.*` | the browser for one system (`ITreeGatewaySubscriber`): subscriptions, tree model, connection state |
| `Source/NodeTreeItem.*` | one node in the `juce::TreeView` |
| `Source/MessagePanel.*` | the `Message` dump for the selected node |
| `Source/MuscleJuce.h` | `muscle::String` <-> `juce::String` and node-path helpers |
| `Source/Theme.h` | the colour palette, shared by the components and the look-and-feel |
| `Source/ZGLookAndFeel.*` | flat dark styling for the stock JUCE controls (buttons, text fields) |
| `build.py` | cross-platform build/package driver (see [Building](#building)) |

Callbacks from zg's network threads reach the JUCE message thread through muscle's own
`JUCECallbackMechanism` (`platform/juce/JUCECallbackMechanism.h`), so all the UI code above runs on
the message thread and needs no locking.
