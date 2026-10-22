"""Test production tracker-map routines with fault-injectable LittleFS."""
from pathlib import Path
import subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[2]
source=(root/'src/configuration.cpp').read_text()
methods=source[source.index('uint8_t Configuration::getSavedTrackerCount'):source.index('void Configuration::loadDeviceDefaults')]
stubs=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
using std::size_t;
struct Print {void println(const char*){}template<class... T>void printf(const char*,T...){} }Serial;
struct File {
 std::shared_ptr<std::vector<uint8_t>> bytes;size_t offset=0;bool *failWrite=nullptr;
 explicit operator bool()const{return bool(bytes);}
 size_t read(uint8_t* out,size_t n){if(!bytes)return 0;n=std::min(n,bytes->size()-offset);memcpy(out,bytes->data()+offset,n);offset+=n;return n;}
 size_t write(const uint8_t* in,size_t n){if(!bytes || (failWrite && *failWrite))return 0;bytes->resize(std::max(bytes->size(),offset+n));memcpy(bytes->data()+offset,in,n);offset+=n;return n;}
 int available(){return bytes ? int(bytes->size()-offset):0;}void close(){}void flush(){}
};
struct FS {
 std::map<std::string,std::shared_ptr<std::vector<uint8_t>>> files;
 bool failRename=false,failWrite=false;
 bool exists(const char* name){return files.count(name);}
 File open(const char* name,const char* mode,bool=false){
  if(*mode=='r' && !exists(name))return {};
  if(*mode=='w' || !exists(name))files[name]=std::make_shared<std::vector<uint8_t>>();
  return {files[name],*mode=='a'?files[name]->size():0,&failWrite};
 }
 bool remove(const char* name){return files.erase(name)>0;}
 bool rename(const char* from,const char* to){if(failRename||!exists(from))return false;files[to]=files[from];files.erase(from);return true;}
}LittleFS;
struct Configuration {
 std::mutex trackerMutex;uint16_t retiredIds=0;uint8_t removedMacs[16][6]={};
 static constexpr uint8_t maxTrackers=10;
 static constexpr const char *savedTrackerCountPath="/count",*trackerMapPath="/map";
 uint8_t getSavedTrackerCount();void setSavedTrackerCount(uint8_t);
 bool getOrCreateTrackerId(const uint8_t[6],uint8_t&);void resetTrackers();
 bool removeTracker(uint8_t);void printTrackers(Print&);
};
'''
tests=r'''
int main(){
 Configuration c;uint8_t a[6]={1},b[6]={2},d[6]={3},e[6]={4},id;
 assert(c.getOrCreateTrackerId(a,id)&&id==0);
 assert(c.getOrCreateTrackerId(b,id)&&id==1);
 assert(c.getOrCreateTrackerId(d,id)&&id==2);
 assert(c.getSavedTrackerCount()==3);
 auto original=*LittleFS.files["/map"];
 LittleFS.failRename=true;assert(!c.removeTracker(1));assert(*LittleFS.files["/map"]==original);
 LittleFS.failRename=false;LittleFS.failWrite=true;assert(!c.removeTracker(1));assert(*LittleFS.files["/map"]==original);
 LittleFS.failWrite=false;assert(c.removeTracker(1));assert(c.getSavedTrackerCount()==2);
 assert(c.getOrCreateTrackerId(a,id)&&id==0);assert(c.getOrCreateTrackerId(d,id)&&id==2);
 assert(!c.getOrCreateTrackerId(b,id)); // Removed MAC cannot immediately re-register.
 assert(c.getOrCreateTrackerId(e,id)&&id==3); // Removed HID ID is retired this boot.
 assert(c.getSavedTrackerCount()==3);assert(!c.removeTracker(1));
 Configuration rebooted;assert(rebooted.getOrCreateTrackerId(b,id)&&id==1);
 assert(rebooted.getSavedTrackerCount()==4); // Slot becomes reusable only after restart.
 auto intact=*LittleFS.files["/map"];LittleFS.files["/map"]->push_back(99);
 auto corrupt=*LittleFS.files["/map"];assert(!rebooted.removeTracker(2));assert(*LittleFS.files["/map"]==corrupt);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(stubs+methods+tests)
 subprocess.run([shutil.which('clang++') or 'g++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: individual removal, sibling IDs, retired ID/MAC, restart, counts, write/rename failures, corrupt map')
