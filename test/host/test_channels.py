"""Compile production channel survey/state machine with deterministic WiFi stubs."""
from pathlib import Path
import re, shutil, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
def source(name):
    return re.sub(r'^\s*#(?:include|pragma once).*$', '', (root/'src'/name).read_text(), flags=re.M)
stubs = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <array>
#include <vector>
uint32_t clockMs=100;
uint32_t millis(){return clockMs;}
struct Print {void println(const char*){} template<class... T>void printf(const char*,T...){} };
constexpr int WIFI_AP=1,WIFI_AP_STA=2,WIFI_SCAN_FAILED=-2,WIFI_SCAN_RUNNING=-1,ESP_OK=0;
constexpr int WIFI_SECOND_CHAN_NONE=0;
int esp_wifi_set_channel(uint8_t,int){return ESP_OK;}
struct wifi_country_t {unsigned schan=1,nchan=13;};
int esp_wifi_get_country(wifi_country_t*){return ESP_OK;}
void esp_wifi_scan_stop(){}
struct AP {std::array<uint8_t,6> mac;int channel,rssi;};
struct Wifi {
 int stations=0,activeChannel=6,scanResult=WIFI_SCAN_RUNNING,modeValue=WIFI_AP;
 bool modeOk=true;std::vector<AP> aps;
 int softAPgetStationNum(){return stations;}
 int channel(){return activeChannel;}
 int channel(int i){return aps[i].channel;}
 int RSSI(int i){return aps[i].rssi;}
 uint8_t* BSSID(int i){return aps[i].mac.data();}
 void softAPmacAddress(uint8_t* out){memset(out,0,6);}
 bool mode(int value){modeValue=value;return modeOk;}
 int scanNetworks(bool,bool,bool,int){return scanResult;}
 int scanComplete(){return scanResult;}
 void scanDelete(){aps.clear();}
} WiFi;
struct TrackerUdpReceiver {
 bool active=false;
 static TrackerUdpReceiver& getInstance(){static TrackerUdpReceiver r;return r;}
 bool hasActiveTrackers(){return active;}
};
struct Configuration {
 int saved=0;bool writeOk=true;
 static Configuration& getInstance(){static Configuration c;return c;}
 bool setWifiChannel(uint8_t ch){if(!writeOk)return false;saved=ch;return true;}
 int getWifiChannel(){return saved;}
};
'''
tests = r'''
int main(){
 ChannelSurvey s;assert(s.recommend(6)==0);
 s.add(0,-50);s.add(14,-50);s.add(1,0);assert(s.total==0);
 s.add(1,-40);assert(s.total==1&&s.counts[0]==1&&s.strongest[0]==-40);
 assert(s.scores[0]>s.scores[1]&&s.scores[1]>s.scores[3]);
 assert(s.recommend(6)==6&&s.recommend(1)==6);
 assert(s.recommend(6,1)==1&&s.recommend(6,0)==0);
 s.add(6,-80);assert(s.recommend(1)==11);
 auto &a=ChannelAnalysis::getInstance();auto &cfg=Configuration::getInstance();
 WiFi.stations=1;assert(a.start()!=nullptr);WiFi.stations=0;
 TrackerUdpReceiver::getInstance().active=true;assert(a.start()!=nullptr);
 TrackerUdpReceiver::getInstance().active=false;
 assert(a.start()==nullptr&&a.running);assert(a.start()!=nullptr);
 WiFi.stations=1;a.update();assert(!a.running&&!strcmp(a.state,"interrupted"));
 WiFi.stations=0;assert(a.start()==nullptr);a.cancel();assert(!strcmp(a.state,"cancelled"));
 assert(a.start()==nullptr);clockMs+=15001;a.update();assert(!strcmp(a.state,"error"));
 assert(a.start()==nullptr);WiFi.scanResult=WIFI_SCAN_FAILED;a.update();assert(!strcmp(a.state,"error"));
 assert(a.start()!=nullptr);WiFi.scanResult=WIFI_SCAN_RUNNING;
 assert(a.start()==nullptr);
 WiFi.aps={{{0,0,0,0,0,0},1,-20},{{1,0,0,0,0,0},1,-40},{{1,0,0,0,0,0},1,-40},{{2,0,0,0,0,0},6,-80}};
 WiFi.scanResult=4;a.update();assert(!strcmp(a.state,"ready"));
 assert(a.survey.total==2&&a.recommended==11&&WiFi.modeValue==WIFI_AP);
 assert(!a.saveRecommendation(a.generation-1,11));assert(!a.saveRecommendation(a.generation,6));
 cfg.writeOk=false;assert(!a.saveRecommendation(a.generation,11));cfg.writeOk=true;
 assert(a.saveRecommendation(a.generation,11)&&cfg.saved==11&&WiFi.activeChannel==6);
 clockMs+=300001;assert(!a.saveRecommendation(a.generation,11));
 WiFi.scanResult=0;assert(a.start()==nullptr);assert(!strcmp(a.state,"empty")&&a.recommended==0);
 assert(!a.saveRecommendation(a.generation,11));
 WiFi.modeOk=false;assert(a.start()!=nullptr&&!strcmp(a.state,"error"));
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.cpp').write_text(stubs+source('ChannelSurvey.h')+source('ChannelAnalysis.h').replace('private:', 'public:')+source('ChannelAnalysis.cpp')+tests)
    compiler=shutil.which('clang++') or shutil.which('g++')
    subprocess.run([compiler,'-std=c++17','-fsanitize=address,undefined','-g',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('Channel survey and scan lifecycle tests passed')
