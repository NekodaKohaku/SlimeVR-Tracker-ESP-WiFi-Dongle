"""Host regression tests for the production receiver (requires Python 3 and clang++/g++).
Only Arduino/network/storage dependencies are stubbed; receiver methods are compiled
from src/TrackerUdpReceiver.cpp. No ESP32 hardware is required.
"""
from pathlib import Path
import re, shutil, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
compiler = shutil.which('clang++') or shutil.which('g++')
if not compiler:
    raise SystemExit('A C++17 compiler is required')
def without_includes(text):
    return re.sub(r'^\s*#(?:include|pragma once).*$', '', text, flags=re.M)
stubs = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
using std::size_t;
uint32_t clockMs = 100;
uint32_t millis() { return clockMs; }
struct Logger { void println(const char*) {} template<class... T> void printf(const char*, T...) {} } Serial;
struct IPAddress {
 int value = 0;
 IPAddress(int v = 0): value(v) {}
 bool operator==(const IPAddress& rhs) const { return value == rhs.value; }
 std::string toString() const { return std::to_string(value); }
};
struct AsyncUDPPacket {
 std::vector<uint8_t> bytes; IPAddress ip; uint16_t port;
 uint8_t* data() { return bytes.data(); } size_t length() { return bytes.size(); }
 IPAddress remoteIP() { return ip; } uint16_t remotePort() { return port; }
};
struct AsyncUDP {
 bool listen(int) { return true; }
 template<class T> void onPacket(T) {}
 void writeTo(const uint8_t*, size_t, IPAddress, uint16_t) {}
};
constexpr int WIFI_AP=1, WIFI_AP_STA=2, WIFI_IF_AP=1, WIFI_PS_NONE=0;
struct Wifi {
 void mode(int) {} bool softAP(const char*,const char*,int,int,int) { return true; }
 int scanNetworks(bool,bool) { return 0; } void scanDelete() {}
 int channel(int) { return 1; } int RSSI(int) { return -50; }
} WiFi;
struct wifi_config_t { struct { int beacon_interval, dtim_period; } ap; };
void esp_wifi_get_config(int,wifi_config_t*) {} void esp_wifi_set_config(int,wifi_config_t*) {} void esp_wifi_set_ps(int) {}
enum class ErrorCodes { NO_ERROR, AP_START_FAILED, UDP_LISTEN_FAILED };
struct Configuration {
 static Configuration& getInstance() { static Configuration c; return c; }
 bool getOrCreateTrackerId(const uint8_t* mac,uint8_t& id) { id=mac[0]; return true; }
};
struct PacketHandling {
 std::vector<std::array<uint8_t,15>> packets;
 std::vector<std::pair<uint8_t,bool>> status;
 static PacketHandling& getInstance() { static PacketHandling p; return p; }
 void insert(const uint8_t* p) { std::array<uint8_t,15> a; memcpy(a.data(),p,15); packets.push_back(a); }
 void setTrackerOnline(uint8_t id,bool online) { status.emplace_back(id,online); }
 void setBattery(uint8_t,uint8_t,uint16_t) {} void setTemp(uint8_t,uint8_t) {}
 void setRssi(uint8_t,int8_t) {} void setSensorInfo(uint8_t,uint8_t,uint8_t) {}
 void setFirmware(uint8_t,uint8_t,uint8_t,uint16_t,uint8_t,uint8_t,uint8_t) {}
};
'''
tests = r'''
void putFloat(std::vector<uint8_t>& b, size_t at, float f) {
 uint32_t bits; memcpy(&bits,&f,4);
 for(int i=0;i<4;i++) b[at+i]=static_cast<uint8_t>(bits>>(24-8*i));
}
int16_t acceleration(const std::array<uint8_t,15>& b) { int16_t v; memcpy(&v,&b[9],2); return v; }
int main() {
 TrackerUdpReceiver r;
 uint8_t a[6]={0}, b[6]={1}; bool isNew;
 assert(r.findOrAddPeer(a, IPAddress(2), 6969, isNew)==0 && isNew);
 r.m_peers[0].connected=true;
 assert(r.findOrAddPeer(b, IPAddress(2), 6969, isNew)==1 && isNew);
 assert(!r.m_peers[0].endpointBound && !r.m_peers[0].connected);
 assert(r.findPeerByEndpoint(IPAddress(2),6969)==1);
 assert(r.findPeerByEndpoint(IPAddress(2),1234)==-1);
 auto& status=PacketHandling::getInstance().status;
 assert(status.size()==1 && status[0].first==0 && !status[0].second);
 // A can rebind to a different IP without stealing B's data.
 assert(r.findOrAddPeer(a,IPAddress(3),6969,isNew)==0 && !isNew);
 assert(r.findPeerByEndpoint(IPAddress(2),6969)==1);
 assert(r.findPeerByEndpoint(IPAddress(3),6969)==0);
 auto& p=r.m_peers[0];
 std::vector<uint8_t> accel(25);
 putFloat(accel,12,1.0f); accel[24]=0; r.handleAccel(p,accel.data(),accel.size());
 putFloat(accel,12,2.0f); accel[24]=1; r.handleAccel(p,accel.data(),accel.size());
 std::vector<uint8_t> rotation(31); putFloat(rotation,26,1.0f);
 rotation[12]=0; r.handleRotation(p,rotation.data(),rotation.size());
 rotation[12]=1; r.handleRotation(p,rotation.data(),rotation.size());
 auto& packets=PacketHandling::getInstance().packets;
 assert(packets.size()==2 && acceleration(packets[0])==128 && acceleration(packets[1])==256);
 // Invalid sensor IDs cannot alias another sensor or index outside the cache.
 accel[24]=16; r.handleAccel(p,accel.data(),accel.size());
 rotation[12]=16; r.handleRotation(p,rotation.data(),rotation.size());
 assert(packets.size()==2 && p.accelFixed[0][0]==128);
 r.handleAccel(p,accel.data(),24); r.handleRotation(p,rotation.data(),30);
 assert(packets.size()==2);
 // A handshake/rebind clears acceleration from the old session.
 r.findOrAddPeer(a,IPAddress(4),6969,isNew);
 assert(p.accelFixed[0][0]==0 && p.accelFixed[1][0]==0);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp)/'receiver.cpp'
    config = without_includes((root/'src/WifiDongleConfig.h').read_text())
    header = without_includes((root/'src/TrackerUdpReceiver.h').read_text()).replace('private:', 'public:')
    impl = without_includes((root/'src/TrackerUdpReceiver.cpp').read_text())
    source.write_text(stubs + config + header + impl + tests)
    binary = Path(tmp)/'receiver-test'
    subprocess.run([compiler,'-std=c++17','-fsanitize=address,undefined','-g',str(source),'-o',str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: endpoint reassignment, source port, per-sensor acceleration, invalid IDs, short packets, session reset')
