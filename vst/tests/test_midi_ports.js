const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const html = fs.readFileSync(`${__dirname}/../source/ui.html`, 'utf8');
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const ports = ['Port 0', 'Port 1'];

function openEditor(postMessage) {
  const element = () => ({
    children: [], style: {}, classList: {add() {}, remove() {}},
    appendChild(child) { this.children.push(child); },
    set innerHTML(value) { this.children = []; },
    getBoundingClientRect() { return {left: 0, bottom: 0}; }
  });
  const elements = new Map();
  const document = {
    getElementById(id) {
      if (!elements.has(id)) elements.set(id, element());
      return elements.get(id);
    },
    createElement: element,
    addEventListener(event, callback) { callback(); }
  };
  const context = vm.createContext({
    document, setTimeout() {},
    window: {webkit: {messageHandlers: {tram8: {postMessage}}}}
  });
  vm.runInContext(script, context);
  return {tram8: vm.runInContext('tram8', context), document};
}

for (const selected of [-1, 0, 1]) {
  let output = selected;
  const messages = [];
  const postMessage = message => {
    messages.push(message);
    if (message.type === 'setMidiPort') output = message.index;
  };
  for (let reopen = 0; reopen < 2; reopen++) {
    messages.length = 0;
    const {tram8, document} = openEditor(postMessage);
    const label = document.getElementById('midi-port-label');
    const cell = document.getElementById('midi-port-cell');
    assert.deepEqual(messages.map(message => message.type), ['ready']);
    messages.length = 0;
    tram8.setMidiPorts(ports);
    assert.equal(output, selected, 'enumeration must preserve the output');
    assert.equal(messages.length, 0, 'enumeration must not post setMidiPort');
    assert.equal(label.textContent, '(...)', 'wait for the processor selection');
    tram8.setMidiPort(selected);
    assert.equal(label.textContent, selected < 0 ? '(none)' : ports[selected]);
    tram8.setMidiPorts(ports);
    assert.equal(messages.length, 0, 'selection replies and refreshes are read-only');
    cell.onclick({currentTarget: cell});
    const popup = document.getElementById('popup-root').children[1];
    assert.equal(popup.children[selected + 1].className, 'popup-item active');
  }
}

const messages = [];
const {tram8, document} = openEditor(message => messages.push(message));
tram8.setMidiPort(1); // A reply can arrive before enumeration completes.
tram8.setMidiPorts(ports);
assert.equal(document.getElementById('midi-port-label').textContent, ports[1]);
messages.length = 0;
for (const index of [0, -1, 1]) {
  const cell = document.getElementById('midi-port-cell');
  cell.onclick({currentTarget: cell});
  const popup = document.getElementById('popup-root').children[1];
  popup.children[index + 1].onclick({stopPropagation() {}});
  assert.equal(messages.length, 1, 'only an explicit selection posts a command');
  assert.equal(messages[0].type, 'setMidiPort');
  assert.equal(messages[0].index, index);
  tram8.setMidiPort(index);
  assert.equal(document.getElementById('midi-port-label').textContent,
    index < 0 ? '(none)' : ports[index]);
  messages.length = 0;
}
tram8.setMidiPorts([]);
assert.equal(document.getElementById('midi-port-label').textContent, '(unavailable)');
tram8.setMidiPort(-1);
assert.equal(document.getElementById('midi-port-label').textContent, '(none)');
assert.equal(messages.length, 0);
console.log('MIDI port enumeration and editor reopen regressions passed');
