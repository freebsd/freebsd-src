#!/bin/sh
# =============================================================================
# UOS MediaTek - Master Device Test Runner
# =============================================================================
# Runs all device tests and generates a compatibility report.
#
# Usage (on running UOS instance):
#   sh run_all_tests.sh [--verbose] [--test uart|gpio|mmc|ethernet|usb|all]
#
# Can be copied to the target via:
#   scp -P 2222 -r tools/mediatek/tests/ root@localhost:/uos-tests/
# =============================================================================

VERBOSE=0
RUN_TEST="all"
TEST_DIR="$(dirname "$0")"
REPORT_FILE="/tmp/uos_compat_report_$(date +%Y%m%d_%H%M%S).txt"
PASS=0
FAIL=0
SKIP=0

# Parse args
for arg in "$@"; do
    case "$arg" in
        --verbose) VERBOSE=1 ;;
        --test) shift; RUN_TEST="$1" ;;
    esac
done

# ---- Helpers ----
GREEN='\033[0;32m'; RED='\033[0;31m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; NC='\033[0m'

log()     { echo "$*" | tee -a "$REPORT_FILE"; }
info()    { printf "${BLUE}[TEST]${NC}  %s\n" "$*" | tee -a "$REPORT_FILE"; }
pass()    { printf "${GREEN}[PASS]${NC}  %s\n" "$*" | tee -a "$REPORT_FILE"; PASS=$((PASS+1)); }
fail()    { printf "${RED}[FAIL]${NC}  %s\n" "$*" | tee -a "$REPORT_FILE"; FAIL=$((FAIL+1)); }
skip()    { printf "${YELLOW}[SKIP]${NC}  %s\n" "$*" | tee -a "$REPORT_FILE"; SKIP=$((SKIP+1)); }

run_test() {
    local name="$1"
    local script="$TEST_DIR/test_${name}.sh"
    [ "$RUN_TEST" != "all" ] && [ "$RUN_TEST" != "$name" ] && return
    log ""
    info "=== Testing: $name ==="
    if [ ! -f "$script" ]; then
        skip "$name test script not found: $script"
        return
    fi
    if sh "$script" $( [ "$VERBOSE" = "1" ] && echo "--verbose" ); then
        pass "$name"
    else
        fail "$name (exit code: $?)"
    fi
}

# ---- Header ----
log ""
log "========================================================"
log "  UOS MediaTek Device Compatibility Test Report"
log "  Generated: $(date)"
log "  Hostname:  $(hostname)"
log "  Kernel:    $(uname -r)"
log "  Machine:   $(uname -m)"
log "========================================================"

# ---- System info ----
log ""
info "=== System Information ==="
log "$(dmesg | grep -E 'cpu[0-9]|CPU|MediaTek|mt8395|MEDIATEK' | head -20 || true)"

# ---- Run individual tests ----
run_test uart
run_test gpio
run_test i2c
run_test mmc
run_test ethernet
run_test usb
run_test pcie
run_test display
run_test bionic

# ---- Summary ----
TOTAL=$((PASS + FAIL + SKIP))
log ""
log "========================================================"
log "  RESULTS: $PASS/$TOTAL passed, $FAIL failed, $SKIP skipped"
log "========================================================"
log ""
log "Full report saved to: $REPORT_FILE"
log ""

if [ "$FAIL" -gt 0 ]; then
    exit 1
else
    exit 0
fi
