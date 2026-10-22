const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(path.join(__dirname,'../../docs/app.js'),'utf8');
function part(start,end){const at=source.indexOf(start);assert(at>=0);return source.slice(at,source.indexOf(end,at));}
const context={};vm.createContext(context);
vm.runInContext(part('function parseKeyValues(','function updateInfo(')+part('function parseChannelReport(','function renderChannels('),context);
function report(state='ready'){
 return `Channels v1\nstate: ${state}\ngeneration: 1\nage_ms: 2\ncurrent: 6\nsaved: 0\nrecommended: 11\nap_total: 1\nallowed_mask: 8191\n`+
 Array.from({length:13},(_,i)=>`C,${i+1},${i===0?1:0},${i===0?-40:0},0\n`).join('')+'Channels end';
}
const parse=context.parseChannelReport;
assert.equal(parse(report()).rows.length,13);
assert.equal(parse(report()).recommended,11);
assert.equal(parse(report('empty').replace('recommended: 11','recommended: 0').replace('ap_total: 1','ap_total: 0')).state,'empty');
assert.equal(parse(report().replace('Channels end','')),null);
assert.equal(parse(report().replace('C,13,0,0,0\n','')),null);
assert.equal(parse(report().replace('C,13,','C,12,')),null);
assert.equal(parse(report().replace('state: ready','state: unsafe')),null);
assert.equal(parse(report().replace('recommended: 11','recommended: 14')),null);
assert.equal(parse(report().replace('generation: 1','generation: 4294967296')),null);
assert.equal(parse(report().replace('C,1,1,-40,0','C,1,-1,-40,0')),null);
assert.equal(parse(report().replace('C,1,1,-40,0','C,1,1,-128,0')),null);
console.log('PASS: channel parsing, malformed/truncated data, numeric bounds and duplicate rows');
