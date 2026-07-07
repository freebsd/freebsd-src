Papagan — Development progress and build notes

What I added in this commit:
- A minimal Qt/C++ papagan-dock application (src/papagan-dock) with a QML UI.
- A build script (build-scripts/build_dock.sh) that runs qmake + make and installs the binary to /usr/local/bin by default.
- A ggml adapter stub (usr/local/bin/papagan-ggml-adapter.py) that can call an external LLM binary (placeholder).

How to build the dock locally (FreeBSD x86_64 VM):
1) Install Qt5 development packages: pkg install qt5
2) Run the build script from repository root: sh build-scripts/build_dock.sh
3) Run the QML UI (for quick tests): qmlscene src/papagan-dock/qml/main.qml

Notes:
- The adapter expects an external LLM binary to exist; this is a stub to ease later integration with ggml/llama.cpp.
- For packaging via ports, the ports/papagan-dock/Makefile exists and will pick up the built binary from src/.
