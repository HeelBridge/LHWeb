#ifndef LHWEB_H
#define LHWEB_H

#include <ESP8266WiFi.h>
#include <LHConfig.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <TimeLib.h> 
#include <WiFiUdp.h>


extern "C" {
#include "user_interface.h"
}

extern "C" {
#include "c_types.h"
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "spi_flash.h"
}

#define MAX_SRV_CLIENTS 10
typedef std::function< void(void)> THandlerFunction;

class LHWeb{
  public:
    String mac_address="";
    String short_mac="";

    LHConfig config;
    LinkedList<String> log;
    ESP8266WebServer httpd;
    
    class TelnetCmd {
    public:
        const char* channel;
        const char* command;
        THandlerFunction func;
    };
    LinkedList<TelnetCmd*> telnet_commands;
    WiFiClient telnetClients[MAX_SRV_CLIENTS];
    WiFiServer telnetd;
    
  private:
    uint8_t MAC_array[WL_MAC_ADDR_LENGTH];
    char MAC_char[4];
    bool debug=false;
    String fallback_ssid="";
    String fallback_pass="";
    String fallback_ntp="0.pool.ntp.org";
    int fallback_tz=0;
    
    int tries_reconnect=0;

    // Things for NTP
    WiFiUDP Udp;
    unsigned int localUdpPort = 8888;
    #define NTP_PACKET_SIZE  48
    byte packetBuffer[NTP_PACKET_SIZE];

    String uploadError;
    File fsUploadFile;

    unsigned long last_time_sync=0;

    String serial_input_string="";
    bool serial_input_complete=false;
  public:

    // Constructor - inits config and web server as well
    LHWeb(bool dbg=false);
    void begin();
    void checkFiles();
    String SSID();
    void SSID(String ssid);
    String Password();
    void Password(String pass);
    String Hostname();
    void Hostname(String hostname);
    String NTPServer();
    void NTPServer(String ntp);
    int TimeZone();
    void TimeZone(int tz);

    // Read MAC address and stor in varialbles (mac_address and short_mac)
    void readMacAddress();

    // Connect to AP 
    // Can also open its own AP if connection fails 
    void connect(bool fallback_AP=true);

    // Writes general info to serial port
    void dumpConnectionInfo();

    // run own AP
    void startAP();

    // tries to reconnect to the wifi network
    // when it was called 5 times without success, a AP will be started
    void reconnect();

    // checks to see if we are still conencted to the wifi network
    // if conenction was lost it will try to reconnect
    // also handles all client requests.
    void doWork();

    // callback function for time synchronization
    time_t getNtpTime();

    // send an NTP request to the time server at the given address
    void sendNTPpacket();


    // register urls and telnet commands to local functions
    // uri - web uri eg "/lighton"
    // function - pointer to a function to call
    // channel - the channel number of the device.
    // set - the telnet command to react to
    void on(const char* uri, const char* channel, const char* command, THandlerFunction func);
    void sendStatus(const char* channel, const char* state);

    String timeStamp();
    void addLog(String entry, bool remote=true);

    void handle404();

    String parseTemplate(String html_file, LHConfig &data);
    String parseTemplateString(String tmpl_str, LHConfig &data);

    void handleRoot();
    void handleWebConfig();
    void handleLog();
    void redirect(String uri);
    void handleUserConfig();
    void handleReset();
    String string2hex(String in);
    void fileUpload();
    void onUpload();

    void dumpFileList();
    void dumpFile(String file_name);

    String getContentType(String filename);
    void handleBrowse();
    void handleFormat();

    void resetConfigToDefaults();

    size_t fs_size();
    uint8_t boardID();

    String sizing(size_t value);
    
    String processCommand(String cmd, String key, String val);
    String processInput(String input);
    
    void broadcast(String msg);
};



#endif