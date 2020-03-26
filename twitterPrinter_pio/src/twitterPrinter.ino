/*
	Copyright (c) 2020 ItKindaWorks All right reserved.
    github.com/ItKindaWorks

    This File is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This File is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with This File.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "ESPHelper_base.h"
#include "Adafruit_Thermal.h"

#include <SoftwareSerial.h>


#define AFFIRM_REQ(SUB_TEXT, SUB_DATA) String("<html>\
	<header>\
	<title>Updated!</title>\
	</header>\
	<body>\
	<p><strong>" SUB_TEXT " " + SUB_DATA + "</strong></br>\
	</body>\
	<meta http-equiv=\"refresh\" content=  \"2; url=./\"  />\
	</html>")

//invalid input http str
const String INVALID_REQUEST_STR = String("<html>\
<header>\
<title>Bad Input</title>\
</header>\
<body>\
	<p><strong>400: Invalid Request</strong></br>\
</body>\
<meta http-equiv=\"refresh\" content=  \"0; url=./\"  />\
</html>");

/////////////////////////   Pin Definitions   /////////////////////////



/////////////////////////   Networking   /////////////////////////

//bool set to true when the system has just connected or reconnected. Allows us to ping the IP
//or do whatever else is necessary when we connect to wifi
bool justReconnected = true;



/////////////////////////   Timers   /////////////////////////


/////////////////////////   Sensors   /////////////////////////



/////////////////////////   Config Keys    /////////////////////////

char topic[64];
char statusTopic[64];


/////////////////////////   Other   /////////////////////////

const bool FLIPPED_PRINTING = true;

SoftwareSerial mySerial;
const int PRINTER_RX = D5;
const int PRINTER_TX = D6;
Adafruit_Thermal printer(&mySerial);     // Pass addr to printer constructor

const char upsideOn[] = { 27, 123, 1, 0};
const char upsideOff[] = { 27, 123, 0, 0};


void setup() {
	Serial.begin(115200);
	delay(500);
	// This line is for compatibility with the Adafruit IotP project pack

	// NOTE: SOME PRINTERS NEED 9600 BAUD instead of 19200, check test page.
	mySerial.begin(19200, SWSERIAL_8N1, PRINTER_RX, PRINTER_TX, false, 95, 11);

	delay(500);

	//Serial1.begin(19200); // Use this instead if using hardware serial
	printer.begin();        // Init printer (same regardless of serial type)
	
	/////////////////////////   IO init   /////////////////////////


	// Set text justification (right, center, left) -- accepts 'L', 'C', 'R'
	printer.wake();


	if(FLIPPED_PRINTING){mySerial.write(upsideOn, 4);}

	printer.feed(2);
	printer.justify('C');
	printer.println("Starting Up Please Wait...");
	printer.feed(2);
	printer.justify('L');
	printer.sleep();      // Tell printer to sleep
	/////////////////////////   Network Init   /////////////////////////

	//startup the wifi and web server
	startWifi(true);
	loadOtherCfg();
	configPage.fillConfig(&config);
	configPage.begin(config.hostname);
	server.on("/", HTTP_GET, handleStatus);
	server.on("/topicUpdate", HTTP_POST, handleTopicUpdate);

	//UNCOMMENT THIS LINE TO ENABLE MQTT CALLBACK
	myESP.setCallback(callback);
	myESP.addSubscription(topic);

	server.begin();                            // Actually start the server
	

	// Set text justification (right, center, left) -- accepts 'L', 'C', 'R'
	printer.wake();
	printer.justify('C');
	printer.println("Booting Finished");
	printer.feed(2);
	printer.justify('L');
	printer.sleep();      // Tell printer to sleep
}

void loop(){
	while(1){
		/////////////////////////   Background Routines   /////////////////////////


		/////////////////////////   Network Handling   /////////////////////////

		//get the current status of ESPHelper
		int espHelperStatus = myESP.loop();
		manageESPHelper(espHelperStatus);

		if(espHelperStatus >= WIFI_ONLY){

			if(espHelperStatus == FULL_CONNECTION){
				//full connection loop code here
			}

			if(justReconnected){
				//post to mqtt if connected
				if(espHelperStatus == FULL_CONNECTION){
					myESP.publish("/home/IP", String(String(myESP.getHostname()) + " -- " + String(myESP.getIP()) ).c_str() );
				}
				Serial.print("Connected to network with IP: ");
				Serial.println(myESP.getIP());
				justReconnected = false;
			}

		}

		//loop delay can be down to 1ms if needed. Shouldn't go below that though
		//(ie dont use 'yield' if it can be avoided)
		delay(5);
	}

}

//MQTT callback
void callback(char* topic, uint8_t* payload, unsigned int length) {
	//convert topic to string to make it easier to work with if complex
	//comparisons are needed.
	String topicStr = topic;

	//fix error in mqtt not including a null terminator and
	//create a string to make the payload easier to work with if complex
	//comparisons are needed.
	char newPayload[500];
	memcpy(newPayload, payload, length);
	newPayload[length] = '\0';
	String payloadStr = newPayload;

	printer.wake();
	printer.justify('C');
	printer.sleep();

	char* header = "\
------------------\n\
--- New Tweet ---\n\
-----------------";
char* footer = "------------------";

	if(FLIPPED_PRINTING == true){
		thermalPrint(footer, strlen(footer), 2);
		thermalPrint(newPayload, length, 2);
		thermalPrint(header, strlen(header), 3);
	}	
	else{
		thermalPrint(header, strlen(header), 2);
		thermalPrint(newPayload, length, 2);
		thermalPrint(footer, strlen(footer), 3);
	}	
}


void thermalPrint(char* text, int len, int linebreaksCount){
	printer.wake();


	const int LINE_LEN = 32;
	// const int LINE_LEN = 10;
	int lineCurChar = 0;

	for(int i = 0; i < len; i++){
		if(text[i] != '\n'){
			int charsLeft = LINE_LEN - lineCurChar;

			if(shouldNewline(&text[i], len-i, charsLeft, LINE_LEN)){
				printer.feed(1);
				lineCurChar = 0;
			}

			printer.print(text[i]);
			lineCurChar++;
		}
		else{
			printer.feed(1);
			lineCurChar = 0;
		}
	}

	printer.feed(linebreaksCount);
	printer.sleep();
}

bool shouldNewline(char* text, int len, int charsLeft, int lineLen){
	if (len < charsLeft){return false;}

	//search for newline or space
	for(int i = 0; i < charsLeft; i++){
		if(text[i] == '\n' || text[i] == ' '){
			return false;
		}
	}

	//check length of word
	int wordSize = 0;
	for(int i = 0; i < len; i++){
		if(text[i] == '\n' || text[i] == ' '){break;}
		wordSize++;
	}

	if(wordSize > lineLen){return false;}
	else{return true;}
}



void loadOtherCfg(){
	ESPHelperFS::begin();

	//Remember to change this to whatever file you want to store/read your custom config from
	const char* file = "/config.json";

	//read the topic from memory (or use a default topic if there is an error)
	loadKey("topic", file, topic, sizeof(topic), "/home/test");
	//create a temporary string to do some concating to eventually assign to the status topic
	String tempTopic = topic;
	tempTopic += "/status";
	tempTopic.toCharArray(statusTopic, sizeof(statusTopic));


	// loadKey("Key", file, someFloat);
	// loadKey("thingUser", file, someStr, sizeof(someStr));

	ESPHelperFS::end();
}




//main config page that allows user to enter in configuration info
void handleStatus() {
  server.send(200, "text/html", \
  String("<html>\
  <header>\
  <title>Device Info</title>\
  </header>\
  <body>\
    <p><strong>System Status</strong></br>\
    Device Name: " + String(config.hostname) + "</br>\
    Connected SSID: " + String(myESP.getSSID()) + "</br>\
    Device IP: " + String(myESP.getIP()) + "</br>\
    Uptime (ms): " + String(millis()) + "</p>\
    <p> </p>\
    \
    \
    <!-- THIS IS COMMENTED OUT \
    <p><strong>Device Variables</strong></p>\
    Temperature: " + String("SOME_VAR") + "</br>\
    UNTIL THIS LINE HERE -->\
    \
    \
  </body>\
  \
  \
	<form action=\"/topicUpdate\" method=\"POST\">\
	Update Topic: <input type=\"text\" name=\"topic\" size=\"64\" value=\"" + String (topic) + "\"></br>\
	<input type=\"submit\" value=\"Submit\"></form>\
  \
  \
  </html>"));
}


//handler function for updating the relay timer
void handleTopicUpdate() {

	const char* keyName = "topic";

	//Remember to change this to whatever file you want to store/read your custom config from
	const char* file = "/config.json";

	//make sure that all the arguments exist and that at least an SSID and hostname have been entered
	if(!server.hasArg(keyName)){
		server.send(400, "text/plain", INVALID_REQUEST_STR);
		return;
	}

	//handle int/float
	// int data = server.arg(keyName).toInt();
	// char buf[20];
	// String tmp = String(data);
	// tmp.toCharArray(buf, sizeof(buf));

	//handle string
	char buf[64];
	server.arg(keyName).toCharArray(buf, sizeof(buf));

	myESP.removeSubscription(topic);

	//save the key to flash and refresh data var with data from flash
	addKey(keyName, buf , file);
	loadKey(keyName, file, topic, sizeof(topic), "/home/test");

	myESP.addSubscription(topic);

	//tell the user that the config is loaded in and the module is restarting
	server.send(200, "text/html",AFFIRM_REQ("Updated Topic To: " , String(topic)));

}



