// SPDX-License-Identifier: BSD-2-Clause
// Exercise orchestration with a tiny Git repository and fake build tools.
// These tests do not compile FreeBSD or prove that an image boots.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const source = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const parent = fs.realpathSync(os.tmpdir());
const temp = fs.mkdtempSync(path.join(parent, 'ameos-release-test-'));
const repo = path.join(temp, 'repo');
const shell = process.env.AMEOS_SH || (process.platform === 'win32'
  ? path.join(process.env.ProgramFiles || 'C:\\Program Files', 'Git/bin/sh.exe')
  : 'sh');
const posix = value => process.platform === 'win32'
  ? value.replaceAll('\\', '/').replace(/^([A-Za-z]):/, (_, drive) => `/${drive.toLowerCase()}`)
  : value;
let passed = 0;

function run(command, args, options = {}) {
  const env = { ...process.env, ...options.env };
  if (process.platform === 'win32' && command === shell) {
    env.PATH = `/mingw64/bin:/usr/bin:/bin:${env.PATH}`;
  }
  const result = spawnSync(command, args, {
    cwd: repo, encoding: 'utf8', timeout: 30000, ...options, env,
  });
  if (result.error) throw result.error;
  assert.equal(result.signal, null, `process interrupted: ${result.stderr}`);
  return result;
}

function git(...args) {
  const result = run('git', [
    '-c', 'core.autocrlf=false', '-c', 'core.hooksPath=/dev/null',
    '-c', 'commit.gpgsign=false', '-c', 'user.name=AmeOS Tests',
    '-c', 'user.email=tests@example.invalid', ...args,
  ]);
  assert.equal(result.status, 0, result.stderr);
  return result.stdout.trim();
}

function write(relative, content) {
  const target = path.join(repo, relative);
  fs.mkdirSync(path.dirname(target), { recursive: true });
  fs.writeFileSync(target, content);
}

function check(name, callback) {
  callback();
  passed += 1;
  console.log(`ok ${passed} - ${name}`);
}

let sequence = 0;
function invoke(args = [], env = {}, work = path.join(temp, `build-${++sequence}`)) {
  const existed = fs.existsSync(work);
  const command = [posix(path.join(repo, 'ameos/scripts/build-release.sh')),
    '--workdir', posix(work), ...args];
  // Set PATH inside MSYS too: its startup can prepend the bundled tools.
  const shellArgs = env.AMEOS_TEST_BIN
    ? ['-c', 'PATH="$AMEOS_TEST_BIN:$PATH"; export PATH; . "$0"', ...command]
    : command;
  return {
    ...run(shell, shellArgs, { env }),
    work,
    existed,
  };
}

function reject(result, pattern) {
  assert.notEqual(result.status, 0, result.stdout);
  assert.match(result.stderr, pattern);
  assert.equal(fs.existsSync(result.work), result.existed, 'rejected command changed path existence');
}

try {
  fs.mkdirSync(repo);
  git('init', '-q');
  write('Makefile', '# Test-only source tree; never compile this fixture.\n');
  write('sys/conf/newvers.sh', 'TYPE="FreeBSD"\nREVISION="16.0"\nBRANCH="CURRENT"\n');
  git('add', '.');
  git('commit', '-qm', 'fixture base');
  const base = git('rev-parse', 'HEAD');
  for (const relative of ['scripts/build-release.sh', 'profiles/base-amd64.conf',
    'profiles/base-amd64.src.conf', 'profiles/compact-amd64.conf',
    'profiles/compact-amd64.src.conf']) {
    write(`ameos/${relative}`, fs.readFileSync(path.join(source, relative), 'utf8'));
  }
  write('ameos/base-revision', `${base}\n`);
  git('add', '.');
  git('commit', '-qm', 'fixture integration');
  const head = git('rev-parse', 'HEAD');
  const script = posix(path.join(repo, 'ameos/scripts/build-release.sh'));

  check('POSIX shell syntax', () => {
    assert.equal(run(shell, ['-n', script]).status, 0);
  });
  check('plan selects committed HEAD and creates no files', () => {
    const result = invoke(['--plan']);
    assert.equal(result.status, 0, result.stderr);
    assert.ok(result.stdout.includes(`Source snapshot: ${head}`));
    assert.ok(result.stdout.includes(`Pinned FreeBSD base: ${base}`));
    assert.equal(fs.existsSync(result.work), false);
    assert.equal(git('status', '--porcelain'), '');
  });
  check('help does not require a workdir', () => {
    const result = run(shell, [script, '--help']);
    assert.equal(result.status, 0);
    assert.match(result.stdout, /Usage:/);
  });
  check('invalid jobs are rejected', () => {
    for (const jobs of ['0', '-1', '2x', '01', '']) {
      reject(invoke(['--plan', '--jobs', jobs]), /positive integer/);
    }
  });
  check('unsafe and relative paths are rejected', () => {
    for (const work of ['relative', '/', '/tmp/../ameos', '/tmp/ameos space', '/tmp/a;id']) {
      reject(invoke(['--plan'], {}, work), /absolute path|simple path components/);
    }
  });
  check('source directory cannot contain build output', () => {
    reject(invoke(['--plan'], {}, path.join(repo, 'output')), /outside the source checkout/);
  });
  check('existing directories and their contents are preserved', () => {
    const existing = path.join(temp, 'existing');
    fs.mkdirSync(existing);
    fs.writeFileSync(path.join(existing, 'sentinel'), 'keep');
    const result = invoke(['--plan'], {}, existing);
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /already exists/);
    assert.equal(fs.readFileSync(path.join(existing, 'sentinel'), 'utf8'), 'keep');
  });
  check('missing option values and unknown options are rejected', () => {
    reject(invoke(['--jobs']), /missing value/);
    reject(invoke(['--unknown']), /unknown argument/);
  });
  check('dirty checkout warns in plan and stops a real build', () => {
    write('uncommitted.txt', 'not part of HEAD\n');
    const plan = invoke(['--plan']);
    assert.equal(plan.status, 0);
    assert.match(plan.stdout, /uncommitted files/);
    reject(invoke(), /checkout is dirty/);
    fs.unlinkSync(path.join(repo, 'uncommitted.txt'));
  });
  check('missing or malformed base revision is rejected', () => {
    write('ameos/base-revision', `${'1'.repeat(40)}\n`);
    reject(invoke(['--plan']), /pinned base is not an ancestor/);
    write('ameos/base-revision', 'not-a-commit\n');
    reject(invoke(['--plan']), /invalid base revision/);
    git('checkout', '--', 'ameos/base-revision');
  });
  check('unsupported target is rejected', () => {
    write('ameos/profiles/base-amd64.conf', 'TARGET=arm64\nTARGET_ARCH=aarch64\nKERNCONF=GENERIC\n');
    reject(invoke(['--plan']), /unsupported target/);
    git('checkout', '--', 'ameos/profiles/base-amd64.conf');
  });
  check('invalid base metadata is rejected before make runs', () => {
    write('sys/conf/newvers.sh', 'TYPE="FreeBSD"\nREVISION="invalid"\nBRANCH="CURRENT"\n');
    reject(invoke(['--plan']), /invalid FreeBSD revision/);
    git('checkout', '--', 'sys/conf/newvers.sh');
  });

  // Simulate the FreeBSD host, leaving Git and archive extraction real.
  const bin = path.join(temp, 'bin');
  fs.mkdirSync(bin);
  function tool(name, content) {
    const target = path.join(bin, name);
    fs.writeFileSync(target, `#!/bin/sh\nset -eu\n${content}\n`, { mode: 0o755 });
  }
  tool('uname', 'case "$1" in -s) echo FreeBSD ;; -m) echo amd64 ;; *) echo "FreeBSD test amd64" ;; esac');
  tool('id', 'echo 1002');
  tool('df', 'printf "Filesystem 1024-blocks Used Available Capacity Mounted\\nfixture 67108864 0 %s 0%% /\\n" "${AMEOS_TEST_FREE_KIB:-67108864}"');
  tool('sysctl', 'echo 3176628224');
  tool('swapinfo', 'echo "Device 1K-blocks Used Avail Capacity"');
  for (const name of ['sha256', 'sha512', 'makefs', 'mkimg', 'etdump']) {
    tool(name, 'echo fixture-checksum');
  }
  tool('make', `
printf '%s|%s|%s|%s\\n' "$*" "$MAKEOBJDIRPREFIX" "\${MAKEFLAGS-unset}" "$SOURCE_DATE_EPOCH" >> "$AMEOS_TEST_CALLS"
stage=
dest=
release=
for arg do
  case "$arg" in
  buildworld|buildkernel|release|install) stage=$arg ;;
  DESTDIR=*) dest=\${arg#DESTDIR=} ;;
  OSRELEASE=*) release=\${arg#OSRELEASE=} ;;
  esac
done
if [ "$stage" = "\${AMEOS_TEST_FAIL_STAGE-}" ]; then
  echo "simulated $stage failure" >&2
  exit 17
fi
if [ "$stage" = install ] && [ "\${AMEOS_TEST_NO_MEDIA-}" != yes ]; then
  printf 'fixture ISO\\n' > "$dest/$release-disc1.iso"
  printf 'fixture USB\\n' > "$dest/$release-memstick.img"
fi`);
  const calls = path.join(temp, 'calls.txt');
  const env = {
    AMEOS_TEST_BIN: posix(bin),
    AMEOS_TEST_CALLS: posix(calls),
  };
  check('native check works as an ordinary user without creating output', () => {
    const result = invoke(['--check'], env);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /Host resources:/);
    assert.equal(fs.existsSync(result.work), false);
    assert.equal(fs.existsSync(calls), false);
  });
  check('disk reserve prevents starting a build', () => {
    reject(invoke([], { ...env, AMEOS_TEST_FREE_KIB: '1024' }), /less than 2048 MiB/);
    assert.equal(fs.existsSync(calls), false);
  });
  check('unknown profiles and conflicting modes are rejected', () => {
    reject(invoke(['--plan', '--profile', '../bad']), /unknown build profile/);
    reject(invoke(['--plan', '--check']), /mutually exclusive/);
  });
  check('compact profile selects its own source options and filename prefix', () => {
    const result = invoke(['--plan', '--profile', 'compact-amd64']);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /AmeOS-0.1-dev-compact-amd64/);
  });
  check('a failed world build stops all later stages', () => {
    const result = invoke([], { ...env, AMEOS_TEST_FAIL_STAGE: 'buildworld' });
    assert.equal(result.status, 17, `${result.stdout}\n${result.stderr}`);
    assert.match(fs.readFileSync(path.join(result.work, 'buildworld.log'), 'utf8'), /simulated buildworld failure/);
    assert.equal(fs.existsSync(path.join(result.work, 'buildkernel.log')), false);
    assert.equal(fs.readFileSync(calls, 'utf8').trim().split('\n').length, 1);
  });
  check('successful orchestration isolates output and records provenance', () => {
    fs.writeFileSync(calls, '');
    const result = invoke([], { ...env, MAKEFLAGS: 'host-override' });
    assert.equal(result.status, 0, `${result.stdout}\n${result.stderr}`);
    assert.match(result.stdout, /boot testing is still required/);
    const records = fs.readFileSync(calls, 'utf8').trim().split('\n');
    assert.equal(records.length, 4);
    for (const record of records) {
      assert.ok(record.includes(`${posix(result.work)}/obj`));
      assert.match(record, /\|unset\|[0-9]+$/);
    }
    for (const record of records.slice(2)) {
      for (const flag of ['NOPORTS=yes', 'NOPKG=yes', 'NOPKGBASE=yes']) {
        assert.ok(record.includes(flag));
      }
      assert.ok(record.includes('REVISION=16.0 BRANCH=CURRENT'));
    }
    assert.ok(records[3].includes(`DESTDIR=${posix(result.work)}/artifacts`));
    const info = fs.readFileSync(path.join(result.work, 'artifacts/BUILD-INFO.txt'), 'utf8');
    assert.ok(info.includes(`source_commit=${head}`));
    assert.ok(info.includes(`freebsd_base=${base}`));
    assert.ok(fs.existsSync(path.join(result.work, 'artifacts/CHECKSUM.SHA256')));
    assert.equal(git('status', '--porcelain'), '');
  });
  check('missing media is a failure even when make succeeds', () => {
    const result = invoke([], { ...env, AMEOS_TEST_NO_MEDIA: 'yes' });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /missing disc1 ISO/);
  });
  console.log(`${passed} checks passed; no FreeBSD compilation was performed.`);
} finally {
  // Delete only the exact fixture created above, within the resolved temp root.
  assert.equal(path.dirname(fs.realpathSync(temp)), parent);
  assert.ok(path.basename(temp).startsWith('ameos-release-test-'));
  fs.rmSync(temp, { recursive: true, force: true });
}
