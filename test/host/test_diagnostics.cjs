const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../../docs/app.js'), 'utf8');
const start = source.indexOf('async function refreshDiagnostics()');
const end = source.indexOf('const imuNames', start);
let response;
const table = {rows: [], replaceChildren(){this.rows=[];}, insertRow(){
 const row={cells:[],insertCell(){const cell={};this.cells.push(cell);return cell;}};
 this.rows.push(row);return row;
}};
const context={sendCommand:async()=>response,$:()=>table,t:key=>key,state:{},updateSensorChoices(){},refreshSensors:async()=>{}};
vm.createContext(context);vm.runInContext(source.slice(start,end),context);
(async()=>{
 response='Diagnostics v1: columns\nD,0,1,-52,200,4,3,145,12,2,0,0\nDiagnostics end';
 await context.refreshDiagnostics();assert.equal(table.rows.length,1);
 assert.equal(table.rows[0].cells[5].textContent,'0, 1');
 assert.equal(table.rows[0].cells.length,11);
 response='Diagnostics v1: columns\nD,0,1,<script>,1,2,3,4,5,6,7,8\nDiagnostics end';
 await context.refreshDiagnostics();assert.equal(table.rows.length,0);
 response='Unknown command. Type help.';await context.refreshDiagnostics();
 assert.equal(table.rows[0].cells[0].textContent,'diagUnavailable');
 response='Diagnostics v1: columns\nD,0,1,-52,1,2,1,4,5,6,7,8';
 await context.refreshDiagnostics();assert.equal(table.rows[0].cells[0].textContent,'diagUnavailable');
 response='Diagnostics v1: columns\nDiagnostics end';await context.refreshDiagnostics();assert.equal(table.rows.length,0);
 console.log('PASS: diagnostics parsing, sensor mask, malformed row, old firmware, truncated reply, empty list');
})().catch(e=>{console.error(e);process.exitCode=1;});
