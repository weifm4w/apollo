load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "ipopt",
    includes = ["include"],
    linkopts = [
        "-lipopt",
    ],
    strip_include_prefix = "include",
)
