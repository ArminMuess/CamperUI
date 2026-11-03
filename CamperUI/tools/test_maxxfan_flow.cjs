const fs = require('fs');
const assert = require('assert/strict');
const repo = require('path').resolve(__dirname, '..');
const raw = fs.readFileSync(repo + '/docs/flows.json');
const nodes = JSON.parse(raw.toString(raw[0] === 0xff ? 'utf16le' : 'utf8').replace(/^\uFEFF/, ''));
function invoke(name, msg, context) {
  const node = nodes.find(n => n.type === 'function' && n.name === name && n.func.includes('maxxfan'));
  assert(node, name);
  const sent = [];
  const globals = {get: key => context[key], set: (key, value) => {context[key] = value;}};
  const result = new Function('msg', 'global', 'node', 'Buffer', 'setTimeout', node.func)(
    msg, globals, {send: value => sent.push(value)}, Buffer, () => {});
  return result;
}
// Execute the actual VanPi functions from the supplied reference flow, no network/device.
for (const power of [false, true]) {
  const context = {maxxfan_auto: true, maxxfan_power: power, maxxfan_speed: 3, maxxfan_vent: 'open'};
  invoke('set auto', {payload: 'auto', topic: 'maxxfan_auto'}, context);
  assert.equal(context.maxxfan_auto, true, 'auto endpoint cannot disable Auto');
  const powerMessage = {payload: 'toggle', topic: 'maxxfan_power'};
  invoke('parse input', powerMessage, context);
  invoke('set temp', powerMessage, context);
  invoke('check vent', powerMessage, context);
  assert.equal(context.maxxfan_power, false, 'Power exits Auto with fan off');
  assert.equal(context.maxxfan_auto, false, 'Power clears Auto');
  assert.equal(context.maxxfan_vent, 'close', 'Power also closes the tracked lid');
  invoke('parse input', {payload: 'toggle', topic: 'maxxfan_power'}, context);
  assert.equal(context.maxxfan_power, true, 'second Power enters Manual');
}
const closed = {maxxfan_auto: false, maxxfan_power: false, maxxfan_vent: 'open'};
invoke('check vent', {payload: 'vent'}, closed);
assert.equal(closed.maxxfan_vent, 'close', 'off fan with open lid can close via vent');
console.log('PASS: original VanPi flow confirms enable-only Auto, Power exit from Auto (running/stopped), Manual restart, lid close. No device commands sent.');
