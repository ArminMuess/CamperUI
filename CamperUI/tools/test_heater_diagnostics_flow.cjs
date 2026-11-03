const fs=require('fs'),vm=require('vm'),assert=require('assert');
const root=require('path').resolve(__dirname,'..')+'/';
const bytes=fs.readFileSync(root+'docs/flows.json');
const nodes=JSON.parse(bytes.toString(bytes[0]===255 && bytes[1]===254 ? 'utf16le':'utf8').replace(/^\uFEFF/,''));
const code=nodes.find(n=>n.id==='ef5df576.d9abc8').func;
// Exercise the real VanPi HTTP response builder, including its historical heatglow key.
const globals=new Map(Object.entries({autoterm1:true,usbport_autoterm1:1,
 heatvolt_autoterm1:13,heatfan_autoterm1:4200,heatpump_autoterm1:1.5,heattemp_autoterm1:144}));
const ctx={global:{get:k=>globals.get(k),set:(k,v)=>globals.set(k,v)},msg:{}};
const lines=code.split('\n').filter(l=>/autoterm1\s*=/.test(l));
for(const l of lines){const key=l.match(/global.get\(["'`]([^"'`]+)["'`]\)/);if(key)globals.set(key[1],true);}
const result=vm.runInNewContext('(function(){'+code+'})()',ctx);
assert(result.payload.autoterm1,'VanPi fixture must enable the first Autoterm');
const h=result.payload.autoterm1;
assert.equal(Number(h.heatvolt),13);assert.equal(Number(h.heatfan),4200);
assert.equal(Number(h.heatglow),1.5);assert.equal(Number(h.heattemp),144);
assert(!('heatpump' in h),'Bundled API uses heatglow for fuel pump Hz');
console.log('PASS: real VanPi /heater response maps V/rpm/Hz/C without extra scaling.');
