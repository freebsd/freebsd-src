load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_library(
    name = "hello_lib",
    srcs = [
        "hello.cc",
    ],
    hdrs = [
        "hello.h",
    ],
    visibility = [
        "//src:__pkg__",
    ],
    deps = [
        "@libcbor//:cbor",
    ],
)

cc_test(
    name = "hello_test",
    size = "small",
    srcs = [
        "hello_test.cc",
    ],
    visibility = [
        "//visibility:private",
    ],
    deps = [
        ":hello_lib",
        "@googletest//:gtest_main",
        "@libcbor//:cbor",
    ],
)


cc_binary(
    name = "hello",
    srcs = [
        "main.cc",
    ],
    deps = [
        ":hello_lib",
    ],
)

