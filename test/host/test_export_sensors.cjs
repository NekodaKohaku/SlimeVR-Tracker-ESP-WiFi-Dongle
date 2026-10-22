const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync(path.join(__dirname,'../../docs/app.js'),'utf8');
function part(start,end){const at=source.indexOf(start);assert(at>=0);return source.slice(at,source.indexOf(end,at));}
const context={};vm.createContext(context);
vm.runInContext(part('function parseKeyValues(','function updateInfo(')+part('function parseSensorReport(','function updateSensorChoices(')+part('function createDiagnosticReport(','async function exportDiagnostics('),context);
let rows=context.parseSensorReport('Sensors v1: columns\nS,0,0,1,3,1,11,128,5\nS,0,1,0,-1,-1,-1,-1,0\nSensors end',0);
assert.equal(rows.length,2);assert.equal(rows[1][3],-1);
assert.equal(context.parseSensorReport('Sensors v1: partial',0),null);
assert.equal(context.parseSensorReport('Sensors v1:\nS,1,0,1,0,1,11,128,5\nS,0,16,1,0,1,11,128,5\nSensors end',0).length,0);
const report=context.createDiagnosticReport('Product: SlimeVR WiFi Dongle\nFirmware: v1.1.6\nFlash size: 4 MB\nUSB serial: PRIVATE_SERIAL\nPassword: PRIVATE_PASSWORD\nSSID: PRIVATE_SSID\n','Uptime: 30s\nSoftAP: PRIVATE_SSID\nAP address: PRIVATE_IP\nChannel: 6\nPassword: PRIVATE_PASSWORD\n',[[0,1,-50,100,0,3,1,2,3,0,0]],rows);
const text=JSON.stringify(report);assert(!text.includes('PRIVATE_'));assert.equal(report.device.firmware,'v1.1.6');assert.equal(report.status.channel,'6');assert.equal(report.sensors.length,2);
rows[0][0]=9;assert.equal(report.sensors[0][0],0);
console.log('PASS: sensor parsing, unknown values, truncated response, wrong tracker/ID, export privacy allowlist, snapshot copy');
