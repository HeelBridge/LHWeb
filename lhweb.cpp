#include "lhweb.h"

#define VERSION "LHWeb v0.1"

// Constructor - inits config and web server as well
LHWeb::LHWeb(bool dbg): config("lhweb.conf"), httpd(80), telnetd(23){
    debug=dbg;
    readMacAddress();
    // set defaults
    fallback_ssid="LHWeb_"+short_mac;
    fallback_pass="lh"+short_mac;
    fallback_ssid.trim();
    fallback_pass.trim();
    
    telnet_commands = LinkedList<TelnetCmd*>();
}

void LHWeb::begin(){
    addLog("Boot up", false);

    
    // read config file
    if(debug) Serial.print("Loading config ");
    if(debug) Serial.println(config.begin());

    // open UDP Port dor ntp
    Udp.begin(localUdpPort);
    
    // assign default page handlers
    httpd.onNotFound ( [&](){ this->handle404(); } );
    httpd.on ( "/",  [&](){ this->handleRoot(); }  );
    httpd.on ( "/userconfig",  [&](){ this->handleUserConfig(); }  );
    httpd.on ( "/reset",  [&](){ this->handleReset(); } );
    
    httpd.onFileUpload([&](){ this->fileUpload(); } ); // curl -F "file=@css/dropdown.css;filename=/css/dropdown.css" 192.168.4.1/upload
    httpd.on("/upload", HTTP_POST, [&](){ this->onUpload(); } );
    httpd.on("/browse", [&](){ this->handleBrowse(); } );
    httpd.on("/webconfig", [&](){ this->handleWebConfig(); } );
    httpd.on("/showlog", [&](){ this->handleLog(); } );
    httpd.on("/format", [&](){ this->handleFormat(); } );
    
    
    checkFiles();
    
    connect();
    
    // start web server
    httpd.begin();
    
    // start telnet server
    telnetd.begin();
    telnetd.setNoDelay(true);
}



void LHWeb::checkFiles(){
    if(!SPIFFS.exists("/index.tmpl")){
        fsUploadFile = SPIFFS.open("/index.tmpl", "w");
        fsUploadFile.println("<html>\
        <head>\
          <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
        </head>\
        <body>\
          <form method=\"POST\" action=\"/webconfig\">\
            <table>\
              <tr><td>SSID:</td><td><input name=\"wifi_ssid\"></td></tr>\
              <tr><td>Passwort:</td><td><input name=\"wifi_pass\"></td></tr>\
              <tr><td>Hostname:</td><td><input name=\"wifi_host\"></td></tr>\
              <tr><td colspan=2><input type=\"submit\" value=\"Save\"></td></tr>\
            </table>\
          </form>\
        </body></html>");
        fsUploadFile.close();
    }
    if(!SPIFFS.exists("/webconfig.tmpl")){
        fsUploadFile = SPIFFS.open("/webconfig.tmpl", "w");
        fsUploadFile.println("<html>\
        <head>\
          <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\
        </head>\
        <body>\
          {{banner}}\
          Aktuelle WLAN-Daten:<br>\
            <table>\
              <tr><td>SSID:</td><td>{{ssid}}</td></tr>\
              <tr><td>Passwort:</td><td>{{pass}}</td></tr>\
              <tr><td>Hostname:</td><td>{{host}}</td></tr>\
            </table>\
        </body></html>");
        fsUploadFile.close();
    }
}


String LHWeb::SSID(){
    //return "Speedlink-493";
    if(config.exists("wifi_ssid")){
        return config.get("wifi_ssid");
    }else{
        return fallback_ssid;
    }
}
void LHWeb::SSID(String ssid){
    config.add("wifi_ssid", ssid);
}

String LHWeb::Password(){
    //return "EMeuJqhYQUjnxM9z";
    if(config.exists("wifi_pass")){
        return config.get("wifi_pass");
    }else{
        return fallback_pass;
    }
  }
void LHWeb::Password(String pass){
    config.add("wifi_pass", pass);
}

String LHWeb::Hostname(){
    if(config.exists("wifi_hostname")){
        return config.get("wifi_hostname");
    }else{
        return fallback_ssid;
    }
}
void LHWeb::Hostname(String hostname){
    config.add("wifi_hostname", hostname);
}

String LHWeb::NTPServer(){
    if(config.exists("wifi_ntp")){
        return config.get("wifi_ntp");
    }else{
        return fallback_ntp;
    }
}
void LHWeb::NTPServer(String ntp){
    config.add("wifi_ntp", ntp);
}

int LHWeb::TimeZone(){
    if(config.exists("wifi_tz")){
        return config.get("wifi_tz").toInt();
    }else{
        return fallback_tz;
    }
}
void LHWeb::TimeZone(int tz){
    config.add("wifi_tz", (String)tz);
}

// Read MAC address and store in varialbles (mac_address and short_mac)
void LHWeb::readMacAddress(){
    WiFi.macAddress(MAC_array);
    for (int i = 0; i < WL_MAC_ADDR_LENGTH; ++i){
        sprintf(MAC_char,"%02X",MAC_array[i]);
        mac_address += MAC_char;
        if(i<WL_MAC_ADDR_LENGTH-1){
            mac_address += ":";
      }
      if(i>=WL_MAC_ADDR_LENGTH-3){ 
            sprintf(MAC_char,"%02X",MAC_array[i]);
            short_mac += MAC_char; 
      }
    }    
}


// Connect to AP 
// Can also open its own AP if connection fails 
void LHWeb::connect(bool fallback_AP){
    char ssid[100]; SSID().toCharArray(ssid, 100);
    char pass[100]; Password().toCharArray(pass, 100);

    WiFi.disconnect();
    WiFi.softAPdisconnect(false);
    delay(100);
    addLog((String)"Connecting to '" + ssid+"'", false);
    if(debug) Serial.print("Password: '");
    if(debug) Serial.print(pass);
    if(debug) Serial.println("'");
    WiFi.begin ( ssid, pass );
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    int counter=0;
    while ( WiFi.status() != WL_CONNECTED && WiFi.status() != WL_NO_SSID_AVAIL && counter++<100 ) {
        delay ( 200 );
        if(debug) Serial.print ( "." );
    }
    if(debug) Serial.println ( );
    
    if( WiFi.status() == WL_CONNECTED ){
        addLog("connected to wifi", false);

        // register at DNS
        char host[100]; Hostname().toCharArray(host, 100);
        int err_dns=0;
        while(err_dns<6){
            if ( !MDNS.begin ( host ) ) {
                addLog("Error setting  up MDNS", false);
                err_dns++;
                delay(500);
            }else{
                addLog ( "MDNS responder started" , false);
                err_dns=99;
                MDNS.addService("http", "tcp", 80);
            }       
        }
      
        // Start time sync
        //setSyncProvider( (time_t(*)()) &LHWeb::getNtpTime);
        setTime(getNtpTime());
        last_time_sync=millis();
      

        addLog("Time set", false);
    
        dumpConnectionInfo();
    }else{
        if(fallback_AP) startAP();
    }

}


// Writes general info to serial port
void LHWeb::dumpConnectionInfo(){
    if(!debug){ return; }
    Serial.print("SSID:        ");
    Serial.println(WiFi.SSID());
    Serial.print("IP-Address:  ");
    Serial.println(WiFi.localIP());
    Serial.print("Subnet-Mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway:     ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS:         ");
    Serial.println(WiFi.dnsIP());
    Serial.print("AP:          ");
    Serial.println(WiFi.BSSIDstr());
    Serial.print("RSSI:        ");
    Serial.println(WiFi.RSSI());
    Serial.print("hostname:    ");
    Serial.println(Hostname());
    Serial.print("MAC address: ");
    Serial.println(mac_address);
}

// run own AP
void LHWeb::startAP(){
    char ssid[100]; fallback_ssid.toCharArray(ssid, 100);
    char pass[100]; fallback_pass.toCharArray(pass, 100);

    addLog("Starting AP.", false);
    WiFi.disconnect();
    delay(200);
    WiFi.mode(WIFI_OFF);
    delay(200);
    WiFi.begin(ssid);      
    delay(200);
    WiFi.disconnect();
    delay(200);
    WiFi.mode(WIFI_AP);
    delay(200);
    WiFi.softAP(ssid, pass);
    WiFi.mode(WIFI_AP);
    delay(200);
    
    IPAddress myIP = WiFi.softAPIP();
    addLog((String)"AP IP address: " + myIP.toString(), false);    
    addLog((String)"SSID:          " + ssid, false);
    addLog((String)"Password:      " + pass, false);
}


// tries to reconnect to the wifi network
// when it was called 5 times without success, an AP will be started
void LHWeb::reconnect(){
    if(tries_reconnect++ >5){
        //startAP();
    }else{
        connect(false);
    }    
}


String LHWeb::processCommand(String cmd, String key, String val, String par){
    command_parameter=par;
    
    String ret="";
    if(cmd=="?"){
        ret+="\n";
        ret+="? Commands:\n";
        ret+="?   ? - shows this help screen\n";
        ret+="?   version - shows version information\n";
        ret+="?   config - sets or shows a config setting\n";
        ret+="?     * without parameter it shows all config settings\n";
        ret+="?     * with one paramter it shows the specified setting\n";
        ret+="?     * with two paramters it sets the specified setting to the given value\n";
        ret+="?     usage: var [<variable> [<value>]]\n";
        ret+="?       example: config wifi_ssid ESP_Net\n";
        ret+="?       example: config wifi_ssid\n";
        ret+="?       example: config\n";
        ret+="?     list of internal variables:\n";
        ret+="?       wifi_ssid - SSID of the WIFI network\n";
        ret+="?       wifi_pass - Password for the WIFI network\n";
        ret+="?       wifi_hostname - name of the ESP module\n";
        ret+="?       wifi_ntp - Name of NTP server\n";
        ret+="?       wifi_tz - Time zone (offset in hours)\n";
        ret+="?   reset - Restarts the ESP module\n";
        ret+="?   set - set state of device/channel\n";       
        ret+="?     usage: set <channel> <state>\n";        
        ret+="?       example: set 0 on\n";
        ret+="?   channel - shows availabe commands of given channel or all channels\n";
        ret+="?     * without parameter it shows all available channels\n";
        ret+="?     * with one parameter it shows all available commands of given channel\n";
        ret+="?       example: channel 0\n";
        ret+="?       example: channel\n";
        ret+="?   rssi - shows wifi quality\n";
        ret+="\n";
    }else if(cmd=="config"){
        if(key==""){
            for(int i=0; i<config.size(); i++){
                LHConfig::ConfigPair *conf=config.get(i);
                ret+="config "+conf->key+" "+conf->val+"\n";
            }            
        }else if(val==""){
            for(int i=0; i<config.size(); i++){
                LHConfig::ConfigPair *conf=config.get(i);
                if(key==conf->key){
                    ret+="config "+conf->key+" "+conf->val+"\n";
                }
            }                
        }else{
            config.add(key, val);
            config.save();
            ret+="OK\n";
        }
    }else if(cmd=="reset"){
        system_restart();
    }else if(cmd=="version"){
        ret="version ";
        ret+=VERSION;
        ret+="\n";
    }else if(cmd=="set"){
        if(val==""){
            ret="ERROR Parameter missing\n";
        }else{
            bool found=false;
            for(int i=0; i<telnet_commands.size(); i++){
                TelnetCmd *tel=telnet_commands.get(i);
                if(key==tel->channel && val==tel->command){
                    tel->func();
                    found=true;
                }
            }
            if(found){
                ret="OK\n";
            }else{
                ret="ERROR command not registered\n";
            }
        }
    }else if(cmd=="channel"){
        if(key==""){
            String lastChan="";
            for(int i=0; i<telnet_commands.size(); i++){
                TelnetCmd *tel=telnet_commands.get(i);
                if(String(tel->channel) != lastChan){
                    ret+="channel ";
                    ret+=tel->channel;
                    ret+="\n";
                    lastChan=tel->channel;
                }
            }
        }else if(val==""){
            bool found=false;
            for(int i=0; i<telnet_commands.size(); i++){
                TelnetCmd *tel=telnet_commands.get(i);
                if(key==tel->channel){
                    ret+="channel ";
                    ret+=tel->channel;
                    ret+=" ";
                    ret+=tel->command;
                    ret+="\n";
                    found=true;
                }
            }
            if(!found){
                ret="ERROR channel not found\n";
            }
        }
    }else if(cmd=="rssi"){
        ret="rssi "
        ret+=WiFi.RSSI()
        ret+="\n\n";
    }else{
        ret="ERROR unknown command\n";
    }
    
    return ret;
}

String LHWeb::processInput(String input){
    int sep1 = input.indexOf(' ');
    int sep2 = input.indexOf(' ', sep1+1);
    int sep3 = input.indexOf(' ', sep2+1);

    String cmd = input.substring(0, sep1);
    String key = input.substring(sep1+1, sep2);
    String val = input.substring(sep2+1, sep3);
    String par = input.substring(sep3+1);

    if(sep3<0) par="";
    if(sep2<0) val="";
    if(sep1<0) key="";

    return processCommand(cmd, key, val, par);
}

// checks to see if we are still conencted to the wifi network
// if conenction was lost it will try to reconnect
// also handles all client requests.
void LHWeb::doWork(){
    unsigned long sync_interval=600000;
    
    // Check WIFI connection state
    if(timeStatus()==timeNotSet || timeStatus()==timeNeedsSync){
        sync_interval=30000;
    }
    if( millis()<last_time_sync || millis()-last_time_sync>sync_interval){
        setTime(getNtpTime());
        last_time_sync=millis();
    }
    
    if(WiFi.status()==WL_CONNECTION_LOST){
        addLog("connection lost", false);
        reconnect();
    }else{
        httpd.handleClient();
    }

    // Handle Serial communication
    if(debug){
        while (Serial.available()) {
            char inChar = (char)Serial.read();
            //Serial.println(inChar);
            if (inChar == '\n') {
                serial_input_complete = true;
            }else{
                serial_input_string += inChar;
            }

        }

        if(serial_input_complete){         
            serial_input_string.trim();
            Serial.print(processInput(serial_input_string));
            
            serial_input_string="";
            serial_input_complete=false;
        }
    }
    
    
    // Handle telnet communication    
    uint8_t i;
    if (telnetd.hasClient()){
        for(i = 0; i < MAX_SRV_CLIENTS; i++){
            if (!telnetClients[i] || !telnetClients[i].connected()){
                if(telnetClients[i]) telnetClients[i].stop();
                telnetClients[i] = telnetd.available();
                continue;
            }
        }
        //no free spot
        WiFiClient telnetClient = telnetd.available();
        telnetClient.stop();
    }
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
        if (telnetClients[i] && telnetClients[i].connected()){
            if(telnetClients[i].available()){
                String str="";
                while(telnetClients[i].available()){
                    str+=(char)telnetClients[i].read();
                }
                str.trim();
                telnetClients[i].print(processInput(str));
                delay(10);
            }
        }
    }

    
    // process timer
    if( timer_time>0 && timer_time<=millis() ){
        timer_time=0;
        timer_function();
    }
    
}



void LHWeb::broadcast(String msg){
    if(debug) Serial.println(msg);
    
    for(uint8_t i = 0; i < MAX_SRV_CLIENTS; i++){
        if (telnetClients[i] && telnetClients[i].connected()){
            telnetClients[i].println(msg);
        }
    }    
}


// callback function for time synchronization
time_t LHWeb::getNtpTime(){
    while (Udp.parsePacket() > 0) ; // discard any previously received packets
    addLog("Transmit NTP Request", false);
    sendNTPpacket();
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500) {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE) {
            addLog("Received NTP Response", false);
            Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
            unsigned long secsSince1900;
            // convert four bytes starting at location 40 to a long integer
            secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer[43];
            return secsSince1900 - 2208988800UL + TimeZone() * SECS_PER_HOUR;
        }
    }
    addLog("No NTP Response", false);
    return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void LHWeb::sendNTPpacket(){
    IPAddress address;
    WiFi.hostByName( NTPServer().c_str(), address);
    // set all bytes in the buffer to 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;
    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:                 
    Udp.beginPacket(address, 123); //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}

String LHWeb::timeStamp(){
    char ts[32];
    sprintf(ts,"%04d-%02d-%02dT%02d:%02d:%02d", year(), month(), day(), hour(), minute(), second() );
    return ts;
  }

void LHWeb::addLog(String entry, bool remote){
    if(remote){
        IPAddress ip = httpd.client().remoteIP();
        entry=timeStamp()+" "+ip.toString()+" "+entry;    
    }else{
        entry=timeStamp()+" "+entry;      
    }
    log.add(entry);
    if(debug) Serial.println(entry);
    while(log.size()>100){ log.shift(); }
}



void LHWeb::handle404(){
    String file_name=httpd.uri();
    //Serial.println(file_name);
    if(SPIFFS.exists(file_name)){
        addLog( (String)"Access file "+httpd.uri() , true);
        File file = SPIFFS.open(file_name, "r");
        size_t sent = httpd.streamFile(file, getContentType(file_name));
        file.close();
    }else{
        addLog( (String)"Error 404: "+httpd.uri() , true);
        String message = "File Not Found\n\n";
        message += "URI: ";
        message += httpd.uri();
        message += "\nMethod: ";
        message += ( httpd.method() == HTTP_GET ) ? "GET" : "POST";
        message += "\nArguments: ";
        message += httpd.args();
        message += "\n";
        
        for ( uint8_t i = 0; i < httpd.args(); i++ ) {
            message += " " + httpd.argName ( i ) + ": " + httpd.arg ( i ) + "\n";
        }
    
        httpd.send ( 404, "text/plain", message );
    }
}



String LHWeb::parseTemplate(String html_file, LHConfig &data){
    String out="";
    String tag;
    char c;

    // h - html
    // o - 1st open curly
    // s - space
    // t - tag
    // c - 1st closing curly
    char state='h';
    
    if(!SPIFFS.exists(html_file)){ 
        addLog("Error Template "+html_file+" does not exist", false);
        return ""; 
    }
    
    File f = SPIFFS.open(html_file, "r");
    while(c=f.read()){
      if(c==255){ break; }   
      //Serial.print(c);
      //Serial.print(" ");
      //Serial.print(state);
      //Serial.print(" ");
      if( state=='h' ){
        if(c=='{'){
          state='o';
        }else{
          out+=c;
        }
      }else if(state=='o'){
        if(c=='{'){
          state='s';
          tag="";
        }else{
          out+="{";
          out+=c;
          state='h';
        }
      }else if(state=='s'){
        if(c=='}'){
          state='c';
        }else if(c!=' '){
          state='t';
          tag+=c;
        }
      }else if(state=='t'){
        if(c==' ' || c=='}'){
          if(tag!=""){
            if(data.exists(tag)){
              out+=data.get(tag);
            }
            tag="";
          }
          if(c==' '){ state='s'; }
          if(c=='}'){ state='c'; }
        }else{
          tag+=c;
        }
      }else if(state=='c'){
        if(c!='}'){ out+=c; }
        state='h';
      }
      //Serial.print(state);
      //Serial.print(" ");
      //Serial.println(tag);
    }
    f.close();

    data.clean();

    return out;
}


String LHWeb::parseTemplateString(String tmpl_str, LHConfig &data){
    String out="";
    String tag;
    char c;
    int idx=0;

    // h - html
    // o - 1st open curly
    // s - space
    // t - tag
    // c - 1st closing curly
    char state='h';
    
    while(c=tmpl_str[idx++]){
      if(c==255 || c==0){ break; }   
      //Serial.print(c);
      //Serial.print(" ");
      //Serial.print(state);
      //Serial.print(" ");
      if( state=='h' ){
        if(c=='{'){
          state='o';
        }else{
          out+=c;
        }
      }else if(state=='o'){
        if(c=='{'){
          state='s';
          tag="";
        }else{
          out+="{";
          out+=c;
          state='h';
        }
      }else if(state=='s'){
        if(c=='}'){
          state='c';
        }else if(c!=' '){
          state='t';
          tag+=c;
        }
      }else if(state=='t'){
        if(c==' ' || c=='}'){
          if(tag!=""){
            if(data.exists(tag)){
              out+=data.get(tag);
            }
            tag="";
          }
          if(c==' '){ state='s'; }
          if(c=='}'){ state='c'; }
        }else{
          tag+=c;
        }
      }else if(state=='c'){
        if(c!='}'){ out+=c; }
        state='h';
      }
      //Serial.print(state);
      //Serial.print(" ");
      //Serial.println(tag);
    }

    return out;
  }




void LHWeb::handleRoot(){
    addLog("Access /",true);
    
    LHConfig data("");    
    data.add("ssid", WiFi.SSID() );
    data.add("bssid", WiFi.BSSIDstr() );
    data.add("rssi", String(WiFi.RSSI()) );
    data.add("mac", mac_address );
    data.add("ip", WiFi.localIP().toString() );
    data.add("mask", WiFi.subnetMask().toString() );
    data.add("gw", WiFi.gatewayIP().toString() );
    data.add("dns", WiFi.dnsIP().toString() );
    data.add("hostname", Hostname() );
    data.add("flash_mem", sizing(fs_size())+"B" );
    data.add("board_id", String(boardID()) );

    httpd.send(200, "text/html", parseTemplate("/index.tmpl", data) );
}


void LHWeb::handleWebConfig(){
    String banner="";
    
    addLog( (String)"Access "+httpd.uri() , true);
    if(httpd.method()==HTTP_POST){
        String ssid=httpd.arg("wifi_ssid");
        String pass=httpd.arg("wifi_pass");
        String host=httpd.arg("wifi_host");
        //Serial.println(ssid);
        config.add("wifi_ssid", ssid);
        config.add("wifi_pass", pass);
        config.add("wifi_hostname", host);
        config.save();
        //config.dump();
        addLog("Config saved", false);
        banner="<div class=\"w3-container w3-section w3-green\"> \
            <span onclick=\"this.parentElement.style.display='none'\" class=\"w3-closebtn\">x</span> \
            <h3>OK</h3> \
            <p>Neue Config wurde gespeichert.</p> \
            </div>";
    }

    LHConfig data("");
    data.add("ssid", SSID() );
    data.add("pass", Password() );
    data.add("host", Hostname() );
    data.add("banner", banner);
    httpd.send(200, "text/html", parseTemplate("/webconfig.tmpl", data) );

}


void LHWeb::handleLog(){
    addLog("Access /showlog",true);
    String message = "";
    for(int i=0; i<log.size(); i++){
        if(debug) Serial.print("Log: ");
        if(debug) Serial.print(i);
        if(debug) Serial.print(" - ");
        if(debug) Serial.println(log.get(i));
        message += String("<tr><td>")+log.get(i)+"</td></tr>\n";
    }

    LHConfig data("");
    data.add("log", message);

    httpd.send(200, "text/html", parseTemplate( "/log.tmpl", data) );
}


void LHWeb::redirect(String uri){
    httpd.sendHeader("Location", uri);
    httpd.send ( 301, "text/html", uri );
}


void LHWeb::handleUserConfig(){
    addLog( (String)"Access "+httpd.uri() , true);
    String message = "";
    String banner="";


    if(httpd.method()==HTTP_POST){
        // Save new Config
        for ( uint8_t i = 0; i < httpd.args(); i++ ) {
            String argNam=httpd.argName(i);
            String argVal=httpd.arg(i);
            String key;
            String val;
            String num;
            if(argNam.substring(0,3)=="key"){
                num=argNam.substring(4,10);
                key=argVal;
                val=httpd.arg( (String("val_")+String(num)).c_str() );
                key.trim();
                val.trim();
                if(debug) Serial.println( key+"="+val );
                if(key!=""){
                    config.add(key, val);
                }          
            }
        }
        config.save();

        banner="<div class=\"w3-container w3-section w3-green\"> \
            <span onclick=\"this.parentElement.style.display='none'\" class=\"w3-closebtn\">x</span> \
            <h3>OK</h3> \
            <p>Neue Config wurde gespeichert.</p> \
            </div>";

    }
    

    LHConfig::ConfigPair* c;
    int i;
    for(i=0; i<config.size(); i++){
        c=config.get(i);
        if(c->key=="wifi_ssid" || c->key=="wifi_pass" || c->key=="wifi_hostname" ){
                continue;
        }
        message += "<tr>";
        message += (String)"<td><input class=\"w3-input\" type=\"text\" name=\"key_"+String(i)+"\" value=\""+c->key+"\"></td>";
        message += (String)"<td><input class=\"w3-input\" type=\"text\" name=\"val_"+String(i)+"\" value=\""+c->val+"\"></td>";
        message += "<tr>\n";
    }
    for(int j=0; j<2; j++,i++){
        message += "<tr>";
        message += (String)"<td><input class=\"w3-input\" type=\"text\" name=\"key_"+String(i)+"\" value=\"\"></td>";
        message += (String)"<td><input class=\"w3-input\" type=\"text\" name=\"val_"+String(i)+"\" value=\"\"></td>";
        message += "<tr>\n";
    }

    LHConfig data("");
    data.add("banner", banner);
    data.add("userconfig", message);
    httpd.send(200, "text/html", parseTemplate( "/userconfig.tmpl", data) );
}


void LHWeb::handleReset(){
    addLog( (String)"Access "+httpd.uri() , true);
    httpd.send ( 200, "text/plain", "Resetting\n" );
    delay(200);
    system_restart();
}



String LHWeb::string2hex(String in){
    String s="";
    char hex[8];
    bool first=true;
    
    for (int i = 0; i < in.length(); ++i) {
        if(first){
                first=false;
        }else{
                s+=" ";
        }
        sprintf(hex, "%02x", in[i]);
        s+=hex;
    }
    
    return s;
}



void LHWeb::fileUpload(){
    int bytes_written;
    if(httpd.uri() != "/upload") return;
    HTTPUpload& upload = httpd.upload();
    if(upload.status == UPLOAD_FILE_START){
        String filename = upload.filename;
        addLog("Upload "+filename, true);
        if(!filename.startsWith("/")) filename = "/"+filename;
        if(debug) Serial.print("handleFileUpload Name: "); 
        if(debug) Serial.println(filename);
        fsUploadFile = SPIFFS.open(filename, "w");
        filename = String();
        uploadError="";
    } else if(upload.status == UPLOAD_FILE_WRITE){
        if(debug) Serial.print("handleFileUpload Data: ");
        if(debug) Serial.print(upload.currentSize);
        if(fsUploadFile){
            bytes_written = fsUploadFile.write(upload.buf, upload.currentSize);
            if(debug) Serial.print("   Bytes written: ");
            if(debug) Serial.println(bytes_written);
        }else{
            if(debug) Serial.println("Nothing written");
            uploadError="Error writing file";
        }
    } else if(upload.status == UPLOAD_FILE_END){
        if(fsUploadFile){
            fsUploadFile.close();
        }else{
            if(debug) Serial.println("File Error");
            addLog("Upload error");
        }
        if(debug) Serial.print("handleFileUpload Size: "); 
        if(debug) Serial.println(upload.totalSize);
        addLog("Upload complete "+upload.filename+" "+String(upload.totalSize) , true);
    }
}

void LHWeb::onUpload(){
    httpd.sendHeader("Connection", "close");
    httpd.sendHeader("Access-Control-Allow-Origin", "*");

    httpd.send(200, "text/plain", uploadError);     
}


void LHWeb::dumpFileList(){
    Dir dir = SPIFFS.openDir("/");
    while(dir.next()){
        File entry = dir.openFile("r");
        if(debug) Serial.print(entry.name());
        if(debug) Serial.print("    ");
        if(debug) Serial.println(entry.size());
        entry.close();
    }
}

void LHWeb::dumpFile(String file_name){
    char c;
    File file = SPIFFS.open(file_name, "r");
    while( c = file.read() ) { 
        if(c==255){ break; }   
        if(debug) Serial.print(c);
    }
    file.close();
    if(debug) Serial.println();
}


String LHWeb::getContentType(String filename){
    if(httpd.hasArg("download")) return "application/octet-stream";
    else if(filename.endsWith(".htm")) return "text/html";
    else if(filename.endsWith(".html")) return "text/html";
    else if(filename.endsWith(".css")) return "text/css";
    else if(filename.endsWith(".js")) return "application/javascript";
    else if(filename.endsWith(".png")) return "image/png";
    else if(filename.endsWith(".gif")) return "image/gif";
    else if(filename.endsWith(".jpg")) return "image/jpeg";
    else if(filename.endsWith(".ico")) return "image/x-icon";
    else if(filename.endsWith(".svg")) return "image/svg+xml";
    else if(filename.endsWith(".xml")) return "text/xml";
    else if(filename.endsWith(".pdf")) return "application/x-pdf";
    else if(filename.endsWith(".zip")) return "application/x-zip";
    else if(filename.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
}

void LHWeb::handleBrowse(){
    if(debug){
        if(debug) Serial.println("browse "+httpd.uri());
        for (uint8_t i=0; i<httpd.args(); i++){
        if(debug) Serial.println(httpd.argName(i) + ": " + httpd.arg(i) );
        }
    }
    
    if( !httpd.hasArg("cmd") ){
        addLog("Access /browse",true);
        String message = "";
        Dir dir = SPIFFS.openDir("/");
  
        String html_string;
        String plain_string;
        char c;
        char buf[10];
        while(dir.next()){
            File entry = dir.openFile("r");
            bool isDir = false;
            plain_string=entry.name();
            message += "<tr>";
            message += String("<td><a href=\"")+entry.name()+"\">"+entry.name()+"</a></td>";
            message += String("<td>")+sizing(entry.size())+"B</td>";
            html_string="";
            for(int i=0; i<plain_string.length(); i++){
                c=plain_string[i];
                if( c>='0' && c<='9' || c>='A' && c<='Z' || c>='a' && c<='z'){
                    html_string+=c;
                }else{
                    sprintf(buf, "%02X", c);
                    html_string+="%";
                    html_string+=buf;
                }
            }
            message += String("<td><a href=\"/browse?cmd=del&file=")+html_string+"\">X</a></td>";
            message += "</tr>\n";
            //addLog(String("  ")+entry.name() );
            entry.close();
        }
        LHConfig data("");
        data.add("file_list", message);
  
        httpd.send(200, "text/html", parseTemplate( "/browse.tmpl", data) );
    }else if(httpd.arg("cmd")=="del"){        
        String file_name = httpd.arg("file");
        addLog("Delete "+file_name, true);
        if(!SPIFFS.exists(file_name)){
            if(debug) Serial.println("File does not exist");
            return httpd.send(404, "text/plain", "FileNotFound");
        }else{
            int ret=SPIFFS.remove(file_name);
            if(debug) Serial.println("File rmoved: "+String(ret));
            redirect("/browse");
        }
    }else if(httpd.arg("cmd")=="show"){
        String file_name = httpd.arg("file");
        redirect(file_name);
    }
}



void LHWeb::handleFormat(){
    SPIFFS.format();
    return httpd.send(404, "text/plain", "File System has been formated");
}


void LHWeb::resetConfigToDefaults(){
    SPIFFS.remove("lhweb.conf");
    if(debug) dumpFileList();
    config.add("wifi_pass", "");
    config.add("wifi_ssid", "");
    if(debug) Serial.println(config.save());
    if(debug) config.dump();
    if(debug) dumpFileList();
}


size_t LHWeb::fs_size(){
    // returns the flash chip's size, in BYTES
    uint32_t id = spi_flash_get_id();
    uint8_t mfgr_id = id & 0xff;
    uint8_t type_id = (id >> 8) & 0xff; // not relevant for size calculation
    uint8_t size_id = (id >> 16) & 0xff; // lucky for us, WinBond ID's their chips as a form that lets us calculate the size
    return 1 << size_id; 
}


uint8_t LHWeb::boardID(){
    // returns the flash chip's size, in BYTES
    uint32_t id = spi_flash_get_id();
    uint8_t mfgr_id = id & 0xff;
    uint8_t type_id = (id >> 8) & 0xff; // not relevant for size calculation
    
    return type_id; 
}

String LHWeb::sizing(size_t value){
    double val=value;
    char prefixes[]={' ', 'K','M','G','T'};
    int pre=0;
    while(val>1024){
        val/=1024;
        pre++;
    }
    if(pre==0){
        return String(value);      
    }else if(pre<=4){
        return String((int)val)+prefixes[pre];
    }else{
        return String((int)val)+"*1024^"+String(pre);
    }
}


void LHWeb::sendStatus(const char* channel, const char* state){
    broadcast((String)"state "+channel+" "+state);
}

void LHWeb::on(const char* uri, const char* channel, const char* command, THandlerFunction func){
    httpd.on(uri, func);
    TelnetCmd *tel = new TelnetCmd();
    tel->channel = channel;
    tel->command = command;
    tel->func = func;
    telnet_commands.add(tel);
}


void LHWeb::deleteTimer(){
    timer_time=0;
}

void LHWeb::setTimer(unsigned long int delay, THandlerFunction func){
    timer_time=millis()+delay*1000;
    timer_function=func;
}

String LHWeb::getParameter(){
    if(command_parameter!=""){
        return command_parameter;
    }else if(httpd.args()>0){
        return httpd.arg(0);
    }else{
        return String();
    }
}

