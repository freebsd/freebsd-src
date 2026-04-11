#!/bin/bash
# =============================================================================
# UOS - Unified Test Runner
# =============================================================================
# Purpose: Execute all tests for MediaTek, Qualcomm, and RISC-V architectures.
# =============================================================================

set -e

BLUE='\033[0;34m'; GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
FREEBSD_SRC="${FREEBSD_SRC:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"

echo -e "${BLUE}======================================================================${NC}"
echo -e "${BLUE}  UOS Unified Cross-Architecture Test Runner                          ${NC}"
echo -e "${BLUE}======================================================================${NC}"

# Find all test scripts following the pattern
TEST_SCRIPTS=$(find "$FREEBSD_SRC/tools" -type f -path "*/tests/test_*.sh")
TOTAL=0
PASSED=0
FAILED=0

for SCRIPT in $TEST_SCRIPTS; do
    TOTAL=$((TOTAL+1))
    echo -e "\n${BLUE}>>> Running Test: $(basename "$SCRIPT")${NC}"
    echo "Path: $SCRIPT"
    if bash "$SCRIPT"; then
        echo -e "${GREEN}[PASS]${NC} $(basename "$SCRIPT")"
        PASSED=$((PASSED+1))
    else
        echo -e "${RED}[FAIL]${NC} $(basename "$SCRIPT")"
        FAILED=$((FAILED+1))
    fi
done

echo -e "\n${BLUE}======================================================================${NC}"
echo -e "Test Summary:"
echo -e "  Total:  $TOTAL"
echo -e "  Passed: ${GREEN}$PASSED${NC}"
echo -e "  Failed: ${RED}$FAILED${NC}"
echo -e "${BLUE}======================================================================${NC}"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
