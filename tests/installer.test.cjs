const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');

// Exercise the classes shipped to browsers, without importing the dialog's DOM UI.
const bundle = fs.readFileSync(path.join(__dirname, '../website/vendor/install-dialog-CLL1RKXl.js'), 'utf8');
const Transport = new Function(`return (${bundle.slice(bundle.indexOf('class Ca{'), bundle.indexOf('function Ta('))})`)();
const Loader = new Function('ui', `return (${bundle.slice(bundle.indexOf('class Fa{'), bundle.indexOf('const Ua='))})`)(Error);
const terminal = { clean() {}, write() {}, writeLine() {} };

function createDialog(transportClass, loaderClass) {
  const source = bundle.slice(bundle.indexOf('class Na extends r'), bundle.indexOf('Na.styles='));
  const Dialog = new Function('r', 'Ca', 'Fa', 'Ua', 'xe', 'window', 'console', `return (${source})`)(
    class {
      updateComplete = Promise.resolve();
      requestUpdate() {}
    }, transportClass, loaderClass, async () => {}, async () => {}, {},
    { log() {}, error() {} },
  );
  return new Dialog();
}

test('an unresponsive board stops after seven connection attempts', async () => {
  for (const baudrate of [115200, 9600]) {
    const loader = new Loader({
      transport: { getInfo: () => 'test port', async connect() {} },
      baudrate,
      romBaudrate: baudrate,
      terminal,
    });
    loader.constructResetSequence = () => [];
    let attempts = 0;
    loader._connectAttempt = async () => {
      assert.ok(++attempts <= 7, 'connection retries must not use the baud rate');
      return 'No serial data received';
    };

    await assert.rejects(loader.detectChip(), /Failed to connect with the device/);
    assert.equal(attempts, 7);
  }
});

test('a responding board ends connection retries immediately', async () => {
  const loader = new Loader({
    transport: { getInfo: () => 'test port', async connect() {} },
    baudrate: 115200,
    terminal,
  });
  loader.constructResetSequence = () => [];
  let attempts = 0;
  loader._connectAttempt = async () => (++attempts === 2 ? 'success' : 'No response');

  await loader.connect('default_reset', 7, true);
  assert.equal(attempts, 2);
});

test('disconnect cancels a pending read and releases the serial port', async () => {
  const readable = new ReadableStream();
  const writable = new WritableStream();
  let closed = false;
  const transport = new Transport({
    readable,
    writable,
    async open() {},
    async close() {
      assert.equal(readable.locked, false);
      assert.equal(writable.locked, false);
      closed = true;
    },
  });
  await transport.connect();
  const pendingRead = transport.reader.read();
  // Fail immediately instead of waiting forever if the stream remains locked.
  transport.waitForUnlock = async () => assert.equal(readable.locked, false);

  await transport.disconnect();
  assert.deepEqual(await pendingRead, { value: undefined, done: true });
  assert.equal(closed, true);
});

test('a failed serial write releases its lock so the port can close', async () => {
  const writable = new WritableStream({
    write() { throw new Error('Device disconnected'); },
  });
  const transport = new Transport({ writable });

  await assert.rejects(transport.write(new Uint8Array([1, 2, 3])), /Device disconnected/);
  assert.equal(writable.locked, false);
});

test('a failure to close the port becomes a visible installation error', async () => {
  const dialog = createDialog();
  dialog.port = { async close() { throw new Error('Serial port busy'); } };

  await dialog._confirmInstall();
  assert.equal(dialog._installState.state, 'error');
  assert.match(dialog._installState.message, /Serial port busy/);
  assert.ok(dialog._installLogs.some(line => line.includes('Serial port busy')));
});

test('the port reopens only after failed initialization has finished cleanup', async () => {
  const events = [];
  let finishCleanup;
  let startCleanup;
  const cleanupStarted = new Promise(resolve => { startCleanup = resolve; });
  const cleanupDone = new Promise(resolve => { finishCleanup = resolve; });
  const dialog = createDialog(
    class {
      async disconnect() {
        events.push('disconnect');
        startCleanup();
        await cleanupDone;
        events.push('cleanup finished');
      }
    },
    class {
      async main() { throw new Error('No response'); }
    },
  );
  dialog.port = {
    async close() { events.push('close'); },
    async open() { events.push('open'); },
  };

  const install = dialog._confirmInstall();
  await cleanupStarted;
  assert.equal(dialog._installState.state, 'error');
  assert.match(dialog._installState.message, /BOOT/);
  assert.deepEqual(events, ['close', 'disconnect']);
  finishCleanup();
  await install;
  assert.deepEqual(events, ['close', 'disconnect', 'cleanup finished', 'open']);
  assert.ok(dialog._installLogs.some(line => line.includes('BOOT')));
  assert.ok(dialog._installLogs.some(line => line.includes('No response')));
});

test('connection attempts are sent to the live terminal', async () => {
  const lines = [];
  const loader = new Loader({
    transport: { getInfo: () => 'test port', async connect() {} },
    baudrate: 115200,
    terminal: { ...terminal, writeLine: line => lines.push(line) },
  });
  loader.constructResetSequence = () => [];
  let attempts = 0;
  loader._connectAttempt = async () => (++attempts === 2 ? 'success' : 'No response');

  await loader.connect('default_reset', 7, true);
  assert.ok(lines.includes('Connection attempt 1 of 7'));
  assert.ok(lines.includes('Connection attempt 2 of 7'));
  assert.ok(!lines.includes('Connection attempt 3 of 7'));
});

test('installation logs retain recent messages and skip blank lines', () => {
  const dialog = createDialog();
  for (let index = 0; index < 405; index++) dialog._appendInstallLog(`Message ${index}`);
  dialog._appendInstallLog('\r\n');

  assert.equal(dialog._installLogs.length, 400);
  assert.match(dialog._installLogs[0], /Message 5$/);
  assert.match(dialog._installLogs.at(-1), /Message 404$/);
});
