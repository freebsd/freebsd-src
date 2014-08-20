#!/bin/sh

TEST_PATH=$1

TEST_FRONTEND=ram

${TEST_PATH}/test_frontend ${TEST_FRONTEND}
${TEST_PATH}/test_plottest ${TEST_FRONTEND}
${TEST_PATH}/test_bitmap ${TEST_FRONTEND}
${TEST_PATH}/test_bezier ${TEST_FRONTEND}
${TEST_PATH}/test_path ${TEST_FRONTEND}
${TEST_PATH}/test_polygon ${TEST_FRONTEND}
${TEST_PATH}/test_polystar ${TEST_FRONTEND}
${TEST_PATH}/test_polystar2 ${TEST_FRONTEND}

