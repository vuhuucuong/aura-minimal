const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const test = require('node:test');

test('firmware outlook handles precipitation periods, missing data, staleness and localization', t => {
  const compiler = process.env.CXX || 'g++';
  const probe = spawnSync(compiler, ['--version']);
  if (probe.error?.code === 'ENOENT') return t.skip('A C++ compiler is required for firmware checks');
  assert.equal(probe.status, 0, probe.error?.message);
  const root = path.resolve(__dirname, '..');
  const include = process.env.ARDUINOJSON_INCLUDE || path.join(root, '.build/sketchbook/libraries/ArduinoJson/src');
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'aura-outlook-'));
  const binary = path.join(temporary, process.platform === 'win32' ? 'outlook.exe' : 'outlook');
  const args = ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', path.join(root, 'firmware/aura_minimal')];
  if (fs.existsSync(path.join(include, 'ArduinoJson.h'))) args.push('-DAURA_TEST_RESPONSE', '-I', include);
  else t.diagnostic('API parser checks require ArduinoJson 7.4.1; build dependencies or set ARDUINOJSON_INCLUDE');
  try {
    const compile = spawnSync(compiler, [...args, path.join(__dirname, 'weather-outlook.cpp'), '-o', binary], { encoding: 'utf8' });
    assert.equal(compile.status, 0, compile.stdout + compile.stderr);
    const run = spawnSync(binary, [], { encoding: 'utf8' });
    assert.equal(run.status, 0, run.stdout + run.stderr);
    t.diagnostic(run.stdout.trim());
  } finally {
    fs.rmSync(temporary, { recursive: true, force: true });
  }
});
