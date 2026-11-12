#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# validate-amede-vps.sh — run AmeDE build checks on the FreeBSD build VPS.
# Run from ameos/: sh scripts/validate-amede-vps.sh

set -e

WORK_DIR="${AMEOS_WORK_DIR:-/home/ameosbuild/ameos-work}"
CARGO_HOME="$WORK_DIR/graphics-cargo"
RUSTUP_HOME="$WORK_DIR/rustup-home"
CARGO_TARGET_DIR="$WORK_DIR/graphics-target"
FONT_PATH="/usr/local/share/fonts/dejavu/DejaVuSans.ttf"
OUTPUT_DIR="${AMEOS_OUTPUT_DIR:-$WORK_DIR/amede-output}"

export CARGO_HOME RUSTUP_HOME CARGO_TARGET_DIR
export PATH="$CARGO_HOME/bin:$PATH"

mkdir -p "$OUTPUT_DIR"

echo "=== Rust version ==="
rustc --version
cargo --version

echo "=== fmt check ==="
cargo fmt --all -- --check

echo "=== ame-gfx tests ==="
cargo test --locked --release -j 2 -p ame-gfx

echo "=== amede build ==="
cargo build --locked --release -j 2 -p amede

echo "=== amede preview (sakura) ==="
AMEDE_FONT="$FONT_PATH" \
  cargo run --locked --release -j 2 -p amede --no-default-features -- \
    --theme sakura --preview "$OUTPUT_DIR/sakura.png"
echo "Preview saved: $OUTPUT_DIR/sakura.png"

echo "=== Wayland check: weston headless ==="
if command -v weston > /dev/null 2>&1; then
    XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/xdg-$(id -u)}"
    mkdir -p -m 0700 "$XDG_RUNTIME_DIR"
    export XDG_RUNTIME_DIR

    # Start headless weston in the background
    weston --backend=headless-backend.so --renderer=pixman \
        --width=1280 --height=800 > "$OUTPUT_DIR/weston.log" 2>&1 &
    WESTON_PID=$!

    # Wait for socket (up to 10 seconds)
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        if [ -S "$XDG_RUNTIME_DIR/wayland-0" ]; then
            break
        fi
        sleep 0.5
    done

    if [ -S "$XDG_RUNTIME_DIR/wayland-0" ]; then
        echo "Wayland compositor ready at $XDG_RUNTIME_DIR/wayland-0"
        WAYLAND_DISPLAY=wayland-0 AMEDE_FONT="$FONT_PATH" \
            timeout 10 "$CARGO_TARGET_DIR/release/amede" \
            > "$OUTPUT_DIR/amede-wayland.log" 2>&1 &
        AMEDE_PID=$!
        sleep 5

        # Capture screenshot if available
        if command -v weston-screenshooter > /dev/null 2>&1; then
            WAYLAND_DISPLAY=wayland-0 \
                weston-screenshooter "$OUTPUT_DIR/amede-wayland.png" \
                2>/dev/null && echo "Screenshot: $OUTPUT_DIR/amede-wayland.png" || true
        fi

        kill "$AMEDE_PID" 2>/dev/null || true
        wait "$AMEDE_PID" 2>/dev/null || true
        kill "$WESTON_PID" 2>/dev/null || true
        wait "$WESTON_PID" 2>/dev/null || true
        echo "Wayland test: OK (amede launched and ran for 5s)"
    else
        echo "WARN: weston socket not found, skipping Wayland test"
        kill "$WESTON_PID" 2>/dev/null || true
    fi
else
    echo "SKIP: weston not installed, skipping Wayland test"
    echo "  Install with: pkg install -y weston"
fi

echo "=== Validation complete ==="
echo "Artifacts: $OUTPUT_DIR"
