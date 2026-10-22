"""Compile production HID scheduler with a controllable USB sink."""
from pathlib import Path
import re, shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
def strip(s): return re.sub(r'^\s*#(?:include|pragma once).*$', '', s, flags=re.M)
stubs=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
using std::size_t;
uint32_t nowMs=100;
uint32_t millis(){return nowMs;}
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
struct Print {std::string text;void println(const char* s){text+=s;text+="\n";}template<class... T>void printf(const char* f,T... args){char b[2048];snprintf(b,sizeof(b),f,args...);text+=b;}}Serial;
struct HIDDevice {
 bool accepting=true, connected=true;
 std::vector<std::array<uint8_t,64>> attempts, accepted;
 std::function<void()> duringSend;
 bool ready(){return connected;}
 bool send(const uint8_t* data,size_t n){
  assert(n==64); std::array<uint8_t,64> b; memcpy(b.data(),data,64); attempts.push_back(b);
  if(duringSend) {auto cb=duringSend; duringSend=nullptr; cb();}
  if(accepting) accepted.push_back(b); return accepting;
 }
};
'''
tests=r'''
void pose(PacketHandling& p,uint8_t tracker,uint8_t sensor,uint8_t value){
 uint8_t data[15]={}; data[0]=(tracker<<4)|sensor; data[1]=value; p.insert(data);
}
void step(PacketHandling& p,HIDDevice& h){nowMs++;p.tick(h);}
int main(){
 uint8_t mac[6]={1,2,3,4,5,6};
 PacketHandling p; HIDDevice h;
 p.registerTracker(0,mac);
 pose(p,0,0,11);
 h.accepting=false; step(p,h);
 assert(h.attempts.size()==1 && h.attempts[0][0]==255);
 assert(!p.registeredHid[0] && p.poseDirty[0]);
 // Failed registration must retry before any data for that sensor.
 step(p,h); assert(h.attempts[1][0]==255);
 h.accepting=true; step(p,h); assert(p.registeredHid[0]);
 // Failed motion submission preserves dirty state and refreshes on retry.
 h.accepting=false; step(p,h); assert(h.attempts.back()[0]==1 && h.attempts.back()[2]==11);
 pose(p,0,0,22); h.accepting=true; step(p,h);
 assert(h.accepted.back()[0]==1 && h.accepted.back()[2]==22 && !p.poseDirty[0]);
 // Data arriving during send cannot be cleared by completion of an older pose.
 pose(p,0,0,33); h.duringSend=[&]{pose(p,0,0,44);};step(p,h);
 assert(p.poseDirty[0]);step(p,h); assert(h.accepted.back()[2]==44 && !p.poseDirty[0]);
 // New auxiliary sensor must register before its first pose.
 pose(p,0,1,55);step(p,h); assert(p.registeredHid[16]);
 assert(h.accepted.back()[0]==255 && h.accepted.back()[1]==16);
 step(p,h);assert(h.accepted.back()[0]==1 && h.accepted.back()[1]==16);
 // Critical status survives a failed send; offline motion is not replayed.
 p.setTrackerOnline(0,false);h.accepting=false;step(p,h);
 bool found=false;for(int i=0;i<4;i++) if(h.attempts.back()[i*16]==3) found=true;assert(found);
 h.accepting=true;step(p,h);found=false;
 for(int i=0;i<4;i++) if(h.accepted.back()[i*16]==3) found=true;assert(found);
 auto stats=p.getStats();assert(stats.failedHidReports==4 && stats.coalescedPoses==2 && stats.droppedPackets==0);
 // Every HID ID can hold its latest pose without overflowing a 64-entry FIFO.
 PacketHandling many;HIDDevice sink;
 for(int t=0;t<10;t++){mac[0]=t;many.registerTracker(t,mac);for(int s=0;s<16;s++)pose(many,t,s,1);}
 for(int i=0;i<1000;i++) step(many,sink);
 for(int t=0;t<10;t++)for(int s=0;s<16;s++){int id=s*16+t;assert(many.registeredHid[id] && !many.poseDirty[id]);}
 assert(many.getStats().droppedPackets==0);
 PacketHandling overflow;overflow.registerTracker(0,mac);
 PacketHandling::Packet control={};control.data[0]=3;
 for(unsigned i=0;i<33;i++)overflow.priorityPush(control);
 assert(overflow.getStats().droppedPackets==1 && overflow.trackers[0].controlDrops==1);
 PacketHandling detail;HIDDevice detailHid;mac[0]=0;detail.registerTracker(0,mac);
 detail.setSensorInfo(0,11,0,0,1);detail.setSensorInfo(0,16,0,1,2);
 detail.setTemp(0,128,0);detail.setTemp(0,140,1);
 pose(detail,0,0,20);pose(detail,0,1,30);
 Print output;detail.printSensors(output,0);
 assert(output.text.find("S,0,0,1,0,1,11,128,1")!=std::string::npos);
 assert(output.text.find("S,0,1,1,0,2,16,140,1")!=std::string::npos);
 detailHid.accepting=false;step(detail,detailHid);
 detail.forgetTracker(0);assert(detail.findTracker(0)==-1);
 detailHid.accepting=true;step(detail,detailHid);
 for(const auto& frame:detailHid.accepted)for(int i=0;i<4;i++)assert(frame[i*16]!=255&&frame[i*16]!=1);
 assert(!detail.poseDirty[0]&&!detail.poseDirty[16]);
 overflow.recordRejectedUdp(0);overflow.recordRejectedUdp();
 assert(overflow.getStats().rejectedUdpPackets==2 && overflow.trackers[0].rejectedUdp==1);

}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp); source=p/'scheduler.cpp'
 source.write_text(stubs+strip((root/'src/packetHandling.h').read_text()).replace('private:','public:')+strip((root/'src/packetHandling.cpp').read_text())+tests)
 subprocess.run([shutil.which('clang++') or 'g++','-std=c++17','-fsanitize=address,undefined','-g',str(source),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: registration ordering, retry, latest pose refresh, concurrent update, AUX, status retention, 160 sensor scheduling')
