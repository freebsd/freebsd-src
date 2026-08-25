libdir                 = @libdir@
datarootdir            = @datarootdir@
datadir                = @datadir@
includedir             = @includedir@
system_includedir      = @SYSTEM_INCLUDEDIR@
system_libdir          = @SYSTEM_LIBDIR@
pkg_default_dir        = @PKG_DEFAULT_PATH@
personality_dir        = @PERSONALITY_PATH@
pkgconfigdir           = $(libdir)/pkgconfig
nodist_pkgconfig_DATA  = libpkgconf.pc

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = -DPERSONALITY_PATH=\"$(personality_dir)\" -DPKG_DEFAULT_PATH=\"$(pkg_default_dir)\" -DSYSTEM_INCLUDEDIR=\"$(system_includedir)\" -DSYSTEM_LIBDIR=\"$(system_libdir)\"

bin_PROGRAMS = pkgconf bomtool spdxtool
lib_LTLIBRARIES = libpkgconf.la

EXTRA_DIST =	pkg.m4 \
		meson.build \
		meson_options.txt \
		pkgconf.wxs.in \
		txt2rtf.py \
		libpkgconf/meson.build \
		libpkgconf/config.h.meson \
		libpkgconf/win-dirent.h \
		t/cli/about.test \
		t/cli/atleast-pkgconfig-version-fail.test \
		t/cli/atleast-pkgconfig-version-pass.test \
		t/cli/atleast-version-fail.test \
		t/cli/atleast-version-inline-constraint.test \
		t/cli/atleast-version-inline-overrides.test \
		t/cli/atleast-version-pass.test \
		t/cli/cflags-libs.test \
		t/cli/debug.test \
		t/cli/define-variable.test \
		t/cli/dont-define-prefix.test \
		t/cli/dump-personality-loaded.test \
		t/cli/dump-personality.test \
		t/cli/env-allow-system-cflags-default.test \
		t/cli/env-allow-system-cflags.test \
		t/cli/env-allow-system-libs-default.test \
		t/cli/env-allow-system-libs.test \
		t/cli/env-dont-relocate-paths.test \
		t/cli/env-early-trace.test \
		t/cli/env-log.test \
		t/cli/env-maximum-traverse-depth.test \
		t/cli/env-msvc-syntax.test \
		t/cli/env-only.test \
		t/cli/env-top-build-dir.test \
		t/cli/env.test \
		t/cli/exact-version-fail.test \
		t/cli/exact-version-inline-constraint.test \
		t/cli/exact-version-inline-overrides.test \
		t/cli/exact-version-pass.test \
		t/cli/exists-missing.test \
		t/cli/exists-valid.test \
		t/cli/fragment-filter.test \
		t/cli/help.test \
		t/cli/internal-cflags.test \
		t/cli/keep-system-cflags-flag.test \
		t/cli/keep-system-libs-flag.test \
		t/cli/list-package-names.test \
		t/cli/log-file.test \
		t/cli/max-version-fail.test \
		t/cli/max-version-inline-constraint.test \
		t/cli/max-version-inline-overrides.test \
		t/cli/max-version-pass.test \
		t/cli/maximum-traverse-depth.test \
		t/cli/missing-package.test \
		t/cli/missing-package-error.test \
		t/cli/modversion.test \
		t/cli/no-arguments.test \
		t/cli/no-cache.test \
		t/cli/no-provides.test \
		t/cli/no-uninstalled.test \
		t/cli/path.test \
		t/cli/personality.test \
		t/cli/prefix-variable.test \
		t/cli/print-provides.test \
		t/cli/print-requires-private.test \
		t/cli/print-requires.test \
		t/cli/print-variables.test \
		t/cli/shared.test \
		t/cli/short-errors.test \
		t/cli/silence-errors.test \
		t/cli/simulate.test \
		t/cli/solution.test \
		t/cli/unknown-option.test \
		t/cli/validate-warning.test \
		t/cli/validate.test \
		t/cli/variable.test \
		t/cli/verbose.test \
		t/cli/version.test \
		t/cli/with-path.test \
		t/basic/arbitrary-path.test \
		t/basic/builtin-pkg-config-exists.test \
		t/basic/builtin-pkg-config-pc_path-variable.test \
		t/basic/builtin-pkgconf-exists.test \
		t/basic/builtin-pkgconf-pc_path-variable.test \
		t/basic/cflags-shared.test \
		t/basic/cflags-shared-static.test \
		t/basic/define-prefix.test \
		t/basic/define-prefix-child-prefix-env.test \
		t/basic/define-prefix-child-prefix.test \
		t/basic/direct-query-with-pkg_config_path.test \
		t/basic/env-tuple.test \
		t/basic/exists-cflags-env.test \
		t/basic/exists-cflags.test \
		t/basic/exists-nonexistent.test \
		t/basic/exists-version-greater-than-with-tilde.test \
		t/basic/exists-version-greater-than.test \
		t/basic/exists-version-less-than-with-tilde.test \
		t/basic/exists-version-malformed.test \
		t/basic/exists-version-minimum.test \
		t/basic/exists-version-with-tilde.test \
		t/basic/incomplete-cflags.test \
		t/basic/incomplete-libs.test \
		t/basic/keep-system-libs.test \
		t/basic/keep-system-libs-2.test \
		t/basic/libs.test \
		t/basic/libs-cflags.test \
		t/basic/libs-cflags-version.test \
		t/basic/libs-cflags-version-multiple.test \
		t/basic/libs-cflags-version-multiple-comma.test \
		t/basic/libs-cflags-version-not.test \
		t/basic/libs-cflags-version-not-provided.test \
		t/basic/libs-env.test \
		t/basic/libs-only.test \
		t/basic/libs-shared.test \
		t/basic/libs-shared-static.test \
		t/basic/malformed-empty-package.test \
		t/basic/modversion-fullpath.test \
		t/basic/modversion-noflatten.test \
		t/basic/modversion-uninstalled.test \
		t/basic/modversion-verbose-prefix.test \
		t/basic/noargs.test \
		t/basic/nocflags.test \
		t/basic/nolibs.test \
		t/basic/pkg-config-path.test \
		t/basic/print-variables-env.test \
		t/basic/requires-shared.test \
		t/basic/requires-shared-static.test \
		t/basic/single-depth-selectors.test \
		t/basic/static-archive-libs.test \
		t/basic/uninstalled-not.test \
		t/basic/uninstalled.test \
		t/basic/variable-env.test \
		t/basic/variable-no-recursion.test \
		t/basic/variable.test \
		t/basic/version-atleast-1.test \
		t/basic/version-atleast-2.test \
		t/basic/version-exact-1.test \
		t/basic/version-exact-2.test \
		t/basic/version-max-1.test \
		t/basic/version-max-2.test \
		t/ordering/cflags-never-mergeback.test \
		t/ordering/cflags-only.test \
		t/ordering/flag-order-1.test \
		t/ordering/flag-order-2.test \
		t/ordering/flag-order-3.test \
		t/ordering/flag-order-4.test \
		t/ordering/fragment-collision.test \
		t/ordering/framework-1.test \
		t/ordering/framework-2.test \
		t/ordering/framework-3.test \
		t/ordering/idirafter-munge-ordering.test \
		t/ordering/idirafter-ordering.test \
		t/ordering/isystem-munge-ordering.test \
		t/ordering/libs-never-mergeback.test \
		t/parser/argv-parse-1.test \
		t/parser/argv-parse-2.test \
		t/parser/argv-parse-3.test \
		t/parser/c-comments-should-warn.test \
		t/parser/comments-in-fields.test \
		t/parser/comments.test \
		t/parser/dollar-sign-escape.test \
		t/parser/dollar-sign-escape-2.test \
		t/parser/dos-lineendings.test \
		t/parser/escaped-backslashes-in-output.test \
		t/parser/fragment-comment.test \
		t/parser/fragment-escaping-1.test \
		t/parser/fragment-escaping-2.test \
		t/parser/fragment-escaping-3.test \
		t/parser/fragment-groups-composite.test \
		t/parser/fragment-groups.test \
		t/parser/fragment-quoting-2.test \
		t/parser/fragment-quoting-3.test \
		t/parser/fragment-quoting-5.test \
		t/parser/fragment-quoting-7.test \
		t/parser/fragment-quoting.test \
		t/parser/fragment-tree.test \
		t/parser/framework-with-libs.test \
		t/parser/malformed-no-newlines.test \
		t/parser/malformed-quoting.test \
		t/parser/msvc-fragment-quoting.test \
		t/parser/msvc-fragment-render-cflags.test \
		t/parser/multiline-bogus-header.test \
		t/parser/multiline-field.test \
		t/parser/multiline-folding-cflags.test \
		t/parser/multiline-folding-libs.test \
		t/parser/no-trailing-newline.test \
		t/parser/paren-quoting.test \
		t/parser/shell-quoted-output.test \
		t/parser/tilde-quoting-1.test \
		t/parser/tilde-quoting-2.test \
		t/parser/truncated-files-should-fail-to-parse.test \
		t/parser/tuple-dequote.test \
		t/parser/variable-fragment-expansion.test \
		t/parser/variable-whitespace.test \
		t/parser/version-with-whitespace-2.test \
		t/parser/version-with-whitespace-diagnostic.test \
		t/parser/version-with-whitespace.test \
		t/personality/dump-personality.test \
		t/personality/pc-path-var.test \
		t/personality/pc-system-includedirs.test \
		t/personality/pc-system-libdirs.test \
		t/solver/circular-reference-1.test \
		t/solver/circular-reference-2.test \
		t/solver/circular-reference-directpc.test \
		t/solver/compound-foo-metapackage-3.test \
		t/solver/conflicts-solution-error.test \
		t/solver/conflicts-solution-ignore.test \
		t/solver/conflicts-solution-is-fine-standalone.test \
		t/solver/depgraph-break-1.test \
		t/solver/depgraph-break-2.test \
		t/solver/depgraph-break-3.test \
		t/solver/libs-intermediary.test \
		t/solver/libs-metapackage.test \
		t/solver/libs-static-ordering.test \
		t/solver/missing-required-dep.test \
		t/solver/modversion-provides.test \
		t/solver/private-libs-duplication-digraph.test \
		t/solver/private-libs-duplication.test \
		t/solver/provides-bar-any.test \
		t/solver/provides-bar-versioned-equal.test \
		t/solver/provides-bar-versioned-greater-than-equal.test \
		t/solver/provides-bar-versioned-greater-than.test \
		t/solver/provides-bar-versioned-less-than-equal.test \
		t/solver/provides-bar-versioned-less-than-new.test \
		t/solver/provides-bar-versioned-less-than-old.test \
		t/solver/provides-bar-versioned-not-equal-old.test \
		t/solver/provides-bar-versioned-not-equal.test \
		t/solver/provides-foo-any.test \
		t/solver/provides-foo-versioned-exact.test \
		t/solver/provides-foo-versioned-greater-than-equal.test \
		t/solver/provides-foo-versioned-greater-than.test \
		t/solver/provides-foo-versioned-less-than-equal.test \
		t/solver/provides-foo-versioned-less-than.test \
		t/solver/provides-foo-versioned-not-equal.test \
		t/solver/provides-indirect-dependency-versioned.test \
		t/solver/provides-indirect-dependency.test \
		t/solver/provides-print-simple.test \
		t/solver/provides-request-simple.test \
		t/solver/provides-request-simple-no-provides.test \
		t/solver/query-no-space-0.test \
		t/solver/query-no-space-1.test \
		t/solver/query-order-1.test \
		t/solver/query-order-2.test \
		t/solver/requires-cflags-libs-private-1.test \
		t/solver/requires-cflags-libs-private-2.test \
		t/solver/requires-internal-collision.test \
		t/solver/requires-internal-missing-nonstatic-cflags-libs.test \
		t/solver/requires-internal-missing-nonstatic.test \
		t/solver/requires-internal-missing-static-cflags.test \
		t/solver/requires-internal-missing.test \
		t/solver/requires-internal.test \
		t/solver/requires-libs-cflags.test \
		t/solver/requires-libs-static-2.test \
		t/solver/requires-libs-static-pure.test \
		t/solver/requires-libs-static.test \
		t/solver/requires-libs.test \
		t/solver/requires-private-debounce.test \
		t/solver/requires-private-missing.test \
		t/solver/requires-static-cflags.test \
		t/bomtool/about.test \
		t/bomtool/bad-output.test \
		t/bomtool/basic.test \
		t/bomtool/default-time.test \
		t/bomtool/define-variable.test \
		t/bomtool/define-variable-with-dependency.test \
		t/bomtool/help.test \
		t/bomtool/meta-package.test \
		t/bomtool/multiple-packages.test \
		t/bomtool/no-license.test \
		t/bomtool/output-file.test \
		t/bomtool/package-whitespace.test \
		t/bomtool/private-dependency.test \
		t/bomtool/source-date-epoch.test \
		t/bomtool/source-date-epoch-precedence.test \
		t/bomtool/special-chars.test \
		t/bomtool/unknown-option.test \
		t/bomtool/unknown-package.test \
		t/bomtool/version.test \
		t/bomtool/version-operator.test \
		t/bomtool/with-dependency.test \
		t/spdxtool/about.test \
		t/spdxtool/agent-name.test \
		t/spdxtool/agent-name-space.test \
		t/spdxtool/bad-output.test \
		t/spdxtool/basic.test \
		t/spdxtool/basic-with-colon.test \
		t/spdxtool/basic-with-spdx-base-id.test \
		t/spdxtool/creation-id.test \
		t/spdxtool/default-time.test \
		t/spdxtool/define-variable.test \
		t/spdxtool/define-variable-with-dependency.test \
		t/spdxtool/help.test \
		t/spdxtool/meta-package.test \
		t/spdxtool/meta-package-with-colon-spdx-base-id.test \
		t/spdxtool/multi-copyright.test \
		t/spdxtool/multiple-packages.test \
		t/spdxtool/no-license.test \
		t/spdxtool/output-file.test \
		t/spdxtool/package-whitespace.test \
		t/spdxtool/private-dependency.test \
		t/spdxtool/source-date-epoch.test \
		t/spdxtool/source-date-epoch-precedence.test \
		t/spdxtool/special-chars.test \
		t/spdxtool/unknown-option.test \
		t/spdxtool/unknown-package.test \
		t/spdxtool/version.test \
		t/spdxtool/version-operator.test \
		t/spdxtool/with-dependency.test \
		t/sbom/license-file-bar.test \
		t/sbom/license-file-foo.test \
		t/sbom/license-isc.test \
		t/sbom/license-malformed-token.test \
		t/sbom/license-noassertion.test \
		t/sbom/source-tag-bar.test \
		t/sbom/source-tag-foo.test \
		t/symlink/pcfiledir_symlink_absolute_env1.test \
		t/symlink/pcfiledir_symlink_absolute_env2.test \
		t/symlink/pcfiledir_symlink_absolute_env3.test \
		t/symlink/pcfiledir_symlink_absolute_env4.test \
		t/symlink/pcfiledir_symlink_absolute_env5.test \
		t/symlink/pcfiledir_symlink_absolute_env6.test \
		t/symlink/pcfiledir_symlink_absolute_env7.test \
		t/symlink/pcfiledir_symlink_absolute_env8.test \
		t/symlink/pcfiledir_symlink_absolute_env9.test \
		t/symlink/pcfiledir_symlink_absolute_env10.test \
		t/symlink/pcfiledir_symlink_absolute_env11.test \
		t/symlink/pcfiledir_symlink_absolute_env12.test \
		t/symlink/pcfiledir_symlink_relative_env1.test \
		t/symlink/pcfiledir_symlink_relative_env2.test \
		t/symlink/pcfiledir_symlink_relative_env3.test \
		t/symlink/pcfiledir_symlink_relative_env4.test \
		t/symlink/pcfiledir_symlink_relative_env5.test \
		t/symlink/pcfiledir_symlink_relative_env6.test \
		t/symlink/pcfiledir_symlink_relative_env7.test \
		t/symlink/pcfiledir_symlink_relative_env8.test \
		t/symlink/pcfiledir_symlink_relative_env9.test \
		t/symlink/pcfiledir_symlink_relative_env10.test \
		t/symlink/pcfiledir_symlink_relative_env11.test \
		t/symlink/pcfiledir_symlink_relative_env12.test \
		t/sysroot/cflags-rewriting.test \
		t/sysroot/explicit-sysroot.test \
		t/sysroot/idirafter-munge-sysroot.test \
		t/sysroot/isystem-munge-sysroot.test \
		t/sysroot/isystem-sysroot-prefixed.test \
		t/sysroot/no-pc_sysrootdir-rewriting-2.test \
		t/sysroot/no-pc_sysrootdir-rewriting-3.test \
		t/sysroot/no-pc_sysrootdir-rewriting-4.test \
		t/sysroot/no-pc_sysrootdir-rewriting-5.test \
		t/sysroot/no-pc_sysrootdir-rewriting-variable.test \
		t/sysroot/path-segment-awareness.test \
		t/sysroot/skip-rewriting-when-sysroot-dir-is-root-dir.test \
		t/sysroot/sysroot-differs-from-destdir-injects.test \
		t/sysroot/sysroot-equals-destdir-fdo-rules.test \
		t/sysroot/undefined-pc_sysrootdir-should-be-empty.test \
		t/sysroot/uninstalled-rewriting-fdo-rules-with-pc_sysrootdir.test \
		t/sysroot/uninstalled-rewriting-fdo-rules.test \
		t/sysroot/uninstalled-rewriting-pkgconf-1-rules.test \
		t/sysroot/uninstalled-rewriting.test \
		t/sysroot/variable-rewriting-1.test \
		t/sysroot/variable-rewriting-2.test \
		t/tuple/billion-laughs.test \
		t/tuple/case-sensitivity-1.test \
		t/tuple/case-sensitivity-2.test \
		t/tuple/define-variable-override.test \
		t/tuple/define-variable.test \
		t/tuple/duplicate-upsert.test \
		t/tuple/empty-tuple.test \
		t/link-abi/default-c.test \
		t/link-abi/cxx-leaf.test \
		t/link-abi/cxx-leaf-static.test \
		t/link-abi/public-requires.test \
		t/link-abi/private-requires-shared.test \
		t/link-abi/private-requires-static.test \
		t/link-abi/multi-dedup.test \
		tests/win-shim.h \
		tests/api/test-api.h \
		tests/lib-relocatable/lib/pkgconfig/foo.pc \
		tests/lib1/argv-parse-2.pc \
		tests/lib1/billion-laughs.pc \
		tests/lib1/dollar-sign-escape.pc \
		tests/lib1/dollar-sign-escape-2.pc \
		tests/lib1/dos-lineendings.pc \
		tests/lib1/paren-quoting.pc \
		tests/lib1/argv-parse-3.pc \
		tests/lib1/foo.pc \
		tests/lib1/foobar.pc \
		tests/lib1/link-abi-plainc.pc \
		tests/lib1/link-abi-cxx.pc \
		tests/lib1/link-abi-multi.pc \
		tests/lib1/link-abi-public.pc \
		tests/lib1/link-abi-private.pc \
		tests/lib1/license-malformed.pc \
		tests/lib1/unavailable-provider.pc \
		tests/lib1/prefix-foo1.pc \
		tests/lib1/argv-parse.pc \
		tests/lib1/framework-1.pc \
		tests/lib1/framework-with-libs.pc \
		tests/lib1/prefix-foo2.pc \
		tests/lib1/bar.pc \
		tests/lib1/framework-2.pc \
		tests/lib1/private-libs-duplication.pc \
		tests/lib1/baz.pc \
		tests/lib1/incomplete.pc \
		tests/lib1/quotes.pc \
		tests/lib1/case-sensitivity.pc \
		tests/lib1/intermediary-1.pc \
		tests/lib1/static-archive-libs.pc \
		tests/lib1/cflags-libs-only.pc \
		tests/lib1/cflags-shared-test.pc \
		tests/lib1/libs-shared-test.pc \
		tests/lib1/intermediary-2.pc \
		tests/lib1/static-libs.pc \
		tests/lib1/circular-1.pc \
		tests/lib1/missing-require.pc \
		tests/lib1/sysroot-dir.pc \
		tests/lib1/circular-2.pc \
		tests/lib1/multiline.pc \
		tests/lib1/multiline-bogus.pc \
		tests/lib1/multiline-folding.pc \
		tests/lib1/tilde-quoting.pc \
		tests/lib1/circular-3.pc \
		tests/lib1/no-trailing-newline.pc \
		tests/lib1/tilde.pc \
		tests/lib1/top-builddir.pc \
		tests/lib1/comments-in-fields.pc \
		tests/lib1/nocflag.pc \
		tests/lib1/typelibdir.pc \
		tests/lib2/foo.pc \
		tests/lib1/comments.pc \
		tests/lib1/nolib.pc \
		tests/lib3/bar.pc \
		tests/lib1/conflicts.pc \
		tests/lib1/omg-uninstalled.pc \
		tests/lib1/omg-sysroot-uninstalled.pc \
		tests/lib1/isystem.pc \
		tests/lib1/isystem-sysroot-prefixed.pc \
		tests/lib1/idirafter.pc \
		tests/lib1/idirafter-ordering.pc \
		tests/lib1/depgraph-break.pc \
		tests/lib1/cflags-whitespace.pc \
		tests/lib1/cflags-whitespace-trailing.pc \
		tests/lib1/provides.pc \
		tests/lib1/provides-request-simple.pc \
		tests/lib1/flag-order-1.pc \
		tests/lib1/flag-order-3.pc \
		tests/lib1/variable-fragment-expansion.pc \
		tests/lib1/variable-whitespace.pc \
		tests/lib1/fragment-collision.pc \
		tests/lib1/fragment-collision-intermediary.pc \
		tests/lib1/fragment-collision-1.pc \
		tests/lib1/fragment-collision-2.pc \
		tests/lib1/fragment-comment.pc \
		tests/lib1/fragment-escaping-1.pc \
		tests/lib1/fragment-escaping-2.pc \
		tests/lib1/fragment-escaping-3.pc \
		tests/lib1/fragment-quoting.pc \
		tests/lib1/fragment-quoting-2.pc \
		tests/lib1/fragment-quoting-3.pc \
		tests/lib1/fragment-quoting-5.pc \
		tests/lib1/fragment-quoting-7.pc \
		tests/lib1/fragment-groups.pc \
		tests/lib1/fragment-groups-2.pc \
		tests/lib1/fragment-group-a.pc \
		tests/lib1/fragment-group-b.pc \
		tests/lib1/fragment-group-c.pc \
		tests/lib1/malformed-1.pc \
		tests/lib1/malformed-quoting.pc \
		tests/lib1/malformed-version.pc \
		tests/lib1/metapackage.pc \
		tests/lib1/metapackage-1.pc \
		tests/lib1/metapackage-2.pc \
		tests/lib1/metapackage-3.pc \
		tests/lib1/explicit-sysroot.pc \
		tests/lib1/escaped-backslash.pc \
		tests/lib1/cflags-internal.pc \
		tests/lib1/requires-internal.pc \
		tests/lib1/requires-internal-2.pc \
		tests/lib1/requires-internal-missing.pc \
		tests/lib1/requires-internal-collision.pc \
		tests/lib1/requires-shared.pc \
		tests/lib1/requires-shared-dep.pc \
		tests/lib1/tuple-quoting.pc \
		tests/lib1/empty-tuple.pc \
		tests/lib1/orphaned-requires-private.pc \
		tests/lib1/pcfiledir.pc \
		tests/lib1/sysroot-dir-2.pc \
		tests/lib1/sysroot-dir-3.pc \
		tests/lib1/sysroot-dir-4.pc \
		tests/lib1/sysroot-dir-5.pc \
		tests/lib1/child-prefix/pkgconfig/child-prefix-1.pc \
		tests/lib1/cflags-libs-private-a.pc \
		tests/lib1/cflags-libs-private-b.pc \
		tests/lib1/cflags-libs-private-c.pc \
		tests/lib1/truncated.pc \
		tests/lib1/c-comment.pc \
		tests/lib1/duplicate-tuple.pc \
		tests/lib1/flag-whitespace.pc \
		tests/lib1/flag-whitespace-2.pc \
		tests/lib-sbom/meta_package.pc \
		tests/lib-sbom/test1.pc \
		tests/lib-sbom/test2.pc \
		tests/lib-sbom/test3.pc \
		tests/lib-sbom/test4.pc \
		tests/lib-sbom/test5.pc \
		tests/lib-sbom/test6.pc \
		tests/lib-sbom/multi-copyright.pc \
		tests/lib-sbom/nolicense.pc \
		tests/lib-sbom/special.pc \
		tests/lib-sbom/variable-test1.pc \
		tests/lib-sbom/variable-test2.pc \
		tests/lib-sbom/priv-parent.pc \
		tests/lib-sbom/priv-child.pc \
		tests/lib-sbom-files/agent_name.json \
		tests/lib-sbom-files/agent_name_space.json \
		tests/lib-sbom-files/basic.json \
		tests/lib-sbom-files/basic-spdx-base-id.json \
		tests/lib-sbom-files/basic-use-uri.json \
		tests/lib-sbom-files/creation_id.json \
		tests/lib-sbom-files/define_variable.json \
		tests/lib-sbom-files/define_variable_with_dependency.json \
		tests/lib-sbom-files/meta_package.json \
		tests/lib-sbom-files/meta_package-use-uri-spdx-base-id.json \
		tests/lib-sbom-files/multi_copyright.json \
		tests/lib-sbom-files/multiple_packages.json \
		tests/lib-sbom-files/nolicense.json \
		tests/lib-sbom-files/special.json \
		tests/lib-sbom-files/with_dependency.json \
		tests/lib-sbom-files/private_dependency.json \
		tests/lib-sbom-files/bomtool-basic.txt \
		tests/lib-sbom-files/bomtool-define_variable.txt \
		tests/lib-sbom-files/bomtool-define_variable_with_dependency.txt \
		tests/lib-sbom-files/bomtool-meta_package.txt \
		tests/lib-sbom-files/bomtool-multiple_packages.txt \
		tests/lib-sbom-files/bomtool-nolicense.txt \
		tests/lib-sbom-files/bomtool-special.txt \
		tests/lib-sbom-files/bomtool-with_dependency.txt \
		tests/lib-sbom-files/bomtool-private_dependency.txt \
		tests/personality-data/i386-linux-gnu.personality \
		tests/personality-data/sysrooted.personality \
		doc/conf.py \
		doc/extract.py \
		doc/index.rst \
		doc/libpkgconf.rst \
		doc/libpkgconf-argvsplit.rst \
		doc/libpkgconf-audit.rst \
		doc/libpkgconf-cache.rst \
		doc/libpkgconf-client.rst \
		doc/libpkgconf-dependency.rst \
		doc/libpkgconf-fragment.rst \
		doc/libpkgconf-path.rst \
		doc/libpkgconf-pkg.rst \
		doc/libpkgconf-queue.rst \
		doc/libpkgconf-tuple.rst

nobase_pkginclude_HEADERS = libpkgconf/bsdstubs.h libpkgconf/iter.h libpkgconf/libpkgconf.h libpkgconf/stdinc.h libpkgconf/libpkgconf-api.h
libpkgconf_la_SOURCES  =	\
		libpkgconf/argvsplit.c		\
		libpkgconf/audit.c		\
		libpkgconf/bsdstubs.c		\
		libpkgconf/buffer.c		\
		libpkgconf/bufferset.c		\
		libpkgconf/bytecode.c		\
		libpkgconf/cache.c		\
		libpkgconf/client.c		\
		libpkgconf/dependency.c		\
		libpkgconf/fileio.c		\
		libpkgconf/fragment.c		\
		libpkgconf/license.c		\
		libpkgconf/output.c		\
		libpkgconf/parser.c		\
		libpkgconf/path.c		\
		libpkgconf/personality.c	\
		libpkgconf/pkg.c		\
		libpkgconf/queue.c		\
		libpkgconf/tuple.c		\
		libpkgconf/variable.c		\
		libpkgconf/version.c
libpkgconf_la_LDFLAGS = -no-undefined -version-info 8:0:0 -export-symbols-regex '^pkgconf_'

dist_man_MANS    = 		\
	man/bomtool.1		\
	man/pkgconf.1		\
	man/pkg.m4.7		\
	man/pc.5		\
	man/pkgconf-personality.5	\
	man/spdxtool.1

pkgconf_LDADD    = libpkgconf.la
pkgconf_SOURCES  = \
	cli/main.c				\
	cli/core.c				\
	cli/getopt_long.c			\
	cli/renderer-msvc.c
pkgconf_CPPFLAGS = -I$(top_srcdir)/libpkgconf -I$(top_srcdir)/cli
noinst_HEADERS   = \
	libpkgconf/path.h					\
	cli/core.h						\
	cli/getopt_long.h					\
	cli/renderer-msvc.h					\
	cli/spdxtool/core.h					\
	cli/spdxtool/generate.h				\
	cli/spdxtool/serialize.h			\
	cli/spdxtool/simplelicensing.h		\
	cli/spdxtool/software.h				\
	cli/spdxtool/util.h

bomtool_LDADD    = libpkgconf.la
bomtool_SOURCES  = \
	cli/bomtool/main.c				\
	cli/getopt_long.c
bomtool_CPPFLAGS = -I$(top_srcdir)/libpkgconf -I$(top_srcdir)/cli -I$(top_srcdir)/cli/bomtool

spdxtool_LDADD    = libpkgconf.la
spdxtool_SOURCES  = \
	cli/spdxtool/main.c		\
	cli/spdxtool/core.c		\
	cli/spdxtool/software.c		\
	cli/spdxtool/serialize.c	\
	cli/spdxtool/simplelicensing.c	\
	cli/spdxtool/util.c		\
	cli/spdxtool/generate.c		\
	cli/getopt_long.c
spdxtool_CPPFLAGS = -I$(top_srcdir)/libpkgconf -I$(top_srcdir)/cli -I$(top_srcdir)/cli/spdxtool

API_TEST_PROGS =		\
	test-api-audit		\
	test-api-buffer		\
	test-api-bytecode	\
	test-api-client		\
	test-api-dependency	\
	test-api-fileio		\
	test-api-fragment	\
	test-api-license	\
	test-api-path-utils	\
	test-api-personality	\
	test-api-queue		\
	test-api-serialize	\
	test-api-tuple		\
	test-api-variable	\
	test-api-version

noinst_PROGRAMS = test-runner $(API_TEST_PROGS)

test_runner_LDADD = libpkgconf.la
test_runner_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/libpkgconf -I$(top_srcdir)/tests/api
test_runner_SOURCES = \
	cli/core.c \
	cli/getopt_long.c \
	cli/renderer-msvc.c \
	tests/test-runner.c

api_test_ldadd = libpkgconf.la
api_test_cppflags = -I$(top_srcdir) -I$(top_srcdir)/libpkgconf -I$(top_srcdir)/tests/api

test_api_audit_LDADD = $(api_test_ldadd)
test_api_audit_SOURCES = tests/api/test-audit.c
test_api_audit_CPPFLAGS = $(api_test_cppflags)

test_api_buffer_LDADD = $(api_test_ldadd)
test_api_buffer_SOURCES = tests/api/test-buffer.c
test_api_buffer_CPPFLAGS = $(api_test_cppflags)

test_api_bytecode_LDADD = $(api_test_ldadd)
test_api_bytecode_SOURCES = tests/api/test-bytecode.c
test_api_bytecode_CPPFLAGS = $(api_test_cppflags)

test_api_client_LDADD = $(api_test_ldadd)
test_api_client_SOURCES = tests/api/test-client.c
test_api_client_CPPFLAGS = $(api_test_cppflags)

test_api_dependency_LDADD = $(api_test_ldadd)
test_api_dependency_SOURCES = tests/api/test-dependency.c
test_api_dependency_CPPFLAGS = $(api_test_cppflags)

test_api_fileio_LDADD = $(api_test_ldadd)
test_api_fileio_SOURCES = tests/api/test-fileio.c
test_api_fileio_CPPFLAGS = $(api_test_cppflags)

test_api_fragment_LDADD = $(api_test_ldadd)
test_api_fragment_SOURCES = tests/api/test-fragment.c
test_api_fragment_CPPFLAGS = $(api_test_cppflags)

test_api_license_LDADD = $(api_test_ldadd)
test_api_license_SOURCES = tests/api/test-license.c
test_api_license_CPPFLAGS = $(api_test_cppflags)

test_api_path_utils_LDADD = $(api_test_ldadd)
test_api_path_utils_SOURCES = tests/api/test-path-utils.c
test_api_path_utils_CPPFLAGS = $(api_test_cppflags)

test_api_personality_LDADD = $(api_test_ldadd)
test_api_personality_SOURCES = tests/api/test-personality.c
test_api_personality_CPPFLAGS = $(api_test_cppflags)

test_api_queue_LDADD = $(api_test_ldadd)
test_api_queue_SOURCES = tests/api/test-queue.c
test_api_queue_CPPFLAGS = $(api_test_cppflags)

test_api_serialize_LDADD = $(api_test_ldadd)
test_api_serialize_SOURCES = \
	tests/api/test-serialize.c \
	cli/spdxtool/core.c \
	cli/spdxtool/software.c \
	cli/spdxtool/serialize.c \
	cli/spdxtool/simplelicensing.c \
	cli/spdxtool/util.c
test_api_serialize_CPPFLAGS = $(api_test_cppflags) -I$(top_srcdir)/cli/spdxtool

test_api_tuple_LDADD = $(api_test_ldadd)
test_api_tuple_SOURCES = tests/api/test-tuple.c
test_api_tuple_CPPFLAGS = $(api_test_cppflags)

test_api_variable_LDADD = $(api_test_ldadd)
test_api_variable_SOURCES = tests/api/test-variable.c
test_api_variable_CPPFLAGS = $(api_test_cppflags)

test_api_version_LDADD = $(api_test_ldadd)
test_api_version_SOURCES = tests/api/test-version.c
test_api_version_CPPFLAGS = $(api_test_cppflags)

dist_doc_DATA = CONTRIBUTING.md DCO README.md AUTHORS COPYING

m4datadir              = $(datadir)/aclocal
m4data_DATA            = pkg.m4

CLEANFILES =	$(EXTRA_PROGRAMS)

check: pkgconf test-runner $(API_TEST_PROGS)
	$(builddir)/test-api-audit$(EXEEXT)
	$(builddir)/test-api-buffer$(EXEEXT)
	$(builddir)/test-api-bytecode$(EXEEXT)
	$(builddir)/test-api-client$(EXEEXT)
	$(builddir)/test-api-dependency$(EXEEXT)
	$(builddir)/test-api-fileio$(EXEEXT)
	$(builddir)/test-api-fragment$(EXEEXT)
	$(builddir)/test-api-license$(EXEEXT)
	$(builddir)/test-api-path-utils$(EXEEXT)
	$(builddir)/test-api-personality$(EXEEXT)
	$(builddir)/test-api-queue$(EXEEXT)
	$(builddir)/test-api-serialize$(EXEEXT)
	$(builddir)/test-api-tuple$(EXEEXT)
	$(builddir)/test-api-variable$(EXEEXT)
	$(builddir)/test-api-version$(EXEEXT)
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/basic
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/cli
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/link-abi
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/ordering
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/parser
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/personality
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/solver
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/sbom
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/bomtool
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/spdxtool
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/symlink
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/sysroot
	$(builddir)/test-runner$(EXEEXT) --test-fixtures=$(top_srcdir)/tests --tool-dir="$(PWD)" $(top_srcdir)/t/tuple

pkgconf-final-stamp:
	@echo
	@echo "============================================================================"
	@echo "WARNING: Autotools support is deprecated and will be removed in pkgconf 3.1."
	@echo "Please migrate to Meson or Muon."
	@echo "See README.md for instructions on building with Meson."
	@echo "============================================================================"
	@echo

all: pkgconf-final-stamp
install-exec-hook: pkgconf-final-stamp
