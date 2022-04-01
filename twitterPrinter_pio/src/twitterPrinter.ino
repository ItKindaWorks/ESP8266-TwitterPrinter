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
#include "qrcode.h"
#include <SoftwareSerial.h>
#include <Bounce2.h>


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

const int buttonPin = D2;
Bounce pushbutton = Bounce(buttonPin, 10);  // 10 ms debounce


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

bool flippedPrinting = true;
bool printQRLinks = true;

SoftwareSerial mySerial;
const int PRINTER_RX = D5;
const int PRINTER_TX = D6;
Adafruit_Thermal printer(&mySerial);     // Pass addr to printer constructor

const char upsideOn[] = { 27, 123, 1, 0};
const char upsideOff[] = { 27, 123, 0, 0};


void setup() {
	Serial.begin(115200);
	delay(500);

	
	/////////////////////////   IO init   /////////////////////////

	mySerial.begin(19200, SWSERIAL_8N1, PRINTER_RX, PRINTER_TX, false, 95, 11);
	delay(500);
	printer.begin();        // Init printer (same regardless of serial type)

	printer.wake();
	printer.feed(2);
	basicPrint("Starting Up Please Wait...", 2, true);

    pinMode(buttonPin, INPUT_PULLUP);

	/////////////////////////   Network Init   /////////////////////////

	//startup the wifi and web server
	startWifi(true);
	loadOtherCfg();
	configPage.fillConfig(&config);
	configPage.begin(config.hostname);
	server.on("/", HTTP_GET, handleStatus);
	server.on("/topicUpdate", HTTP_POST, handleTopicUpdate);
	server.on("/flippedPrinting", HTTP_POST, handleToggleFlipPrint);
	server.on("/printQRLinks", HTTP_POST, handleToggleQRLink);

	//UNCOMMENT THIS LINE TO ENABLE MQTT CALLBACK
	myESP.setCallback(callback);
	myESP.addSubscription(topic);
	myESP.addSubscription("/home/devicePing");

	server.begin();                            // Actually start the server
	
	//print a little signature ;)
	printer.wake();
	if(flippedPrinting){mySerial.write(upsideOff, 4);}
	printQR("https://youtube.com/itkindaworks");
	defaultPrintSettings();
	printer.sleep();


	basicPrint("Booting Finished", 2, true);

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
                if(pushbutton.update()){
					if (pushbutton.risingEdge()) {
						String outTopic = String(topic);
					outTopic += "/signal";
					myESP.publish(outTopic.c_str(), "1");
					}
				}
			}

			if(justReconnected){
				//post to mqtt if connected
				if(espHelperStatus == FULL_CONNECTION){
					myESP.publish("/home/IP", String(String(myESP.getHostname()) + " -- " + String(myESP.getIP()) ).c_str() );
				}
				Serial.print("Connected to network with IP: ");
				Serial.println(myESP.getIP());

				if(flippedPrinting == true){
					basicPrint(myESP.getIP().c_str(), 0, true);
					basicPrint("Connected to network with IP: ", 2, true);
				}
				else{
					basicPrint("Connected to network with IP: ", 0, true);
					basicPrint(myESP.getIP().c_str(), 2, true);
				}

				justReconnected = false;
			}

		}

		//loop delay can be down to 1ms if needed. Shouldn't go below that though
		//(ie dont use 'yield' if it can be avoided)
		delay(5);
	}

}

/**
 * @brief mqtt callback function
 * 
 * @param topic 	topic string
 * @param payload 	payload string (NOT NULL TERMINATED)
 * @param length 	length of the payload
 */
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

	if(topicStr.equals("/home/devicePing")){
		postDevicePing();
		return;
	}

	//make sure the printer is defaulted
	defaultPrintSettings();

//header and footer strings
char* header = "\
-----------------------\n\
------ New Tweet ------\n\
-----------------------";
char* footer = "-----------------------";

	//invert the order things are printed in if the
	//text is to be printed in reverse order
	if(flippedPrinting == true){
		thermalPrint(footer, strlen(footer), 2);
		if(printQRLinks){printBarcodeLink(payloadStr);}
		thermalPrint(newPayload, length, 2);
		thermalPrint(header, strlen(header), 3);
	}	
	else{
		thermalPrint(header, strlen(header), 2);
		thermalPrint(newPayload, length, 2);
		if(printQRLinks){printBarcodeLink(payloadStr);}
		thermalPrint(footer, strlen(footer), 3);
	}	
}

/**
 * @brief basic thermal print function
 * 
 * @param inStr string to be printed
 * @param feedCount number of line feeds after printed text
 * @param defaultFmt should we use the default formatting options
 */
void basicPrint(const char* inStr, int feedCount, bool defaultFmt){
	printer.wake();
	if(defaultFmt){defaultPrintSettings();}
	printer.println(inStr);
	printer.feed(feedCount);
	printer.sleep();      // Tell printer to sleep
}

/**
 * @brief search for and print a qr code to the last twitter t.co/... link in a tweet (usually a subtweet, picture, or link to article)
 * 
 * @param inStr tweet string to search for a link to convert to a qr code
 */
void printBarcodeLink(String inStr){

	//find the index of the last link
	int index = -1;
	if(flippedPrinting == false){index =inStr.lastIndexOf("https://t.co/");}	
	else{index =inStr.indexOf("https://t.co/");}	//flipped printing requires searching from the "start" of the string (the end of the tweet)

	//if nothing was found, end
	if(index == -1){return;}

	//generate a link only string
	String link = inStr.substring(index, index+23);
	
	//wake the printer and prep for printing
	printer.wake();
	if(flippedPrinting){mySerial.write(upsideOff, 4);}

	//print out the qr code
	printQR(link);

	//reset the printer back to default settings and sleep
	defaultPrintSettings();
	printer.sleep();
	
}

/**
 * @brief print a qr code based on a string
 * 
 * @param text piece of text to be converted into a qr code
 */
void printQR(String text){

	//wake the printer and setup formatting
	printer.wake();
	printer.justify('C');
	printer.setCharSpacing(0);

	//set zero line spacing
	mySerial.write(27);
	mySerial.write(51);
	mySerial.write((byte)0);
	
	//print an extra line feed for spacing
	printer.feed(1);

	// Create the QR code
	Serial.println("Generating QR Code");
	QRCode qrcode;
	uint8_t qrcodeData[qrcode_getBufferSize(3)];
	qrcode_initText(&qrcode, qrcodeData, 3, 0, text.c_str());
	

	//print the code 
	Serial.println("printing QR Code\n\n");

	if(flippedPrinting == false){	//handle regular (right side up) printing of the code

		for (int x = 0; x < qrcode.size; x+=2) {	//x+=2 because each printed character represents 2 lines of the qr code
			for (int y = 0; y < qrcode.size; y++) {

					//since each printed char represents 2 lines of the qr code, we have to make sure to print the right character
					//so each of these if statements looks at the qr 'bit' on the current line and the next line of the qr code
					//and figures out which char needs to be printed. 
 					if (qrcode_getModule(&qrcode, x, y) && qrcode_getModule(&qrcode, x+1, y)) 	{mySerial.write(219);}
					else if (!qrcode_getModule(&qrcode, x, y) && qrcode_getModule(&qrcode, x+1, y)) 	{mySerial.write(220);}
					else if (qrcode_getModule(&qrcode, x, y) && !qrcode_getModule(&qrcode, x+1, y)) 	{mySerial.write(223);}
					else if (!qrcode_getModule(&qrcode, x, y) && !qrcode_getModule(&qrcode, x+1, y)) {mySerial.print(" ");}
			}
			printer.feed(1);
		}
	}
	else{
		//when printing upside down we have to start from the bottom of the code and work upwards which is why x starts at qrcode.size
		for (int x = qrcode.size; x >0; x-=2) {		//x-=2 because each printed character represents 2 lines of the qr code
			for (int y = 0; y < qrcode.size; y++) {
					//see note above - this is the same but opposite
					if (qrcode_getModule(&qrcode, x, y) && qrcode_getModule(&qrcode, x-1, y)) 	{mySerial.write(219);}
					else if (!qrcode_getModule(&qrcode, x, y) && qrcode_getModule(&qrcode, x-1, y)) 	{mySerial.write(220);}
					else if (qrcode_getModule(&qrcode, x, y) && !qrcode_getModule(&qrcode, x-1, y)) 	{mySerial.write(223);}
					else if (!qrcode_getModule(&qrcode, x, y) && !qrcode_getModule(&qrcode, x-1, y)) {mySerial.print(" ");}
			}
			printer.feed(1);
		}
	}

	//turn the printer off when done
	printer.sleep();
}

/**
 * @brief sets the printer back to the expected default formatting
 * 
 */
void defaultPrintSettings(){
	printer.wake();
	printer.setDefault();
	if(flippedPrinting){mySerial.write(upsideOn, 4);}
	printer.justify('C');
	printer.feed(1);
	printer.sleep();
}

/**
 * @brief print text using word wrapping
 * 
 * @param text ptr to string to be printed
 * @param len size of the string
 * @param linebreaksCount number of breaks to insert after the printed text
 */
void thermalPrint(char* text, int len, int linebreakCount){
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

	printer.feed(linebreakCount);
	printer.sleep();
}

/**
 * @brief figures out whether a string needs to wrap to the next line to keep from breaking up words
 * 
 * @param text string to check (should generally be used with a given str index, ie &text[i] ) 
 * @param len total length of the string
 * @param charsLeft characters left in the line
 * @param lineLen total line length
 * @return true if we need to insert a newline now
 * @return false if no newline is needed at the moment
 */
bool shouldNewline(char* text, int len, int charsLeft, int lineLen){
	if (len < charsLeft){return false;}

	//search for newline or space
	for(int i = 0; i < charsLeft; i++){
		if(text[i] == '\n' || text[i] == ' '){
			//if we find a newline/space before the end of the line, then we dont need a newline
			//manually inserted now
			return false;
		}
	}

	//find length of word 
	int wordSize = 0;
	for(int i = 0; i < len; i++){
		if(text[i] == '\n' || text[i] == ' '){break;}
		wordSize++;
	}

	//if the word is longer than a line, ignore the newline and just let the printer wrap it
	if(wordSize > lineLen){return false;}

	//otherwise, if we have gotten here then yes we do need to insert a newline now
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


	int inverted = 0;
	int qrCode = 0;

	loadKey("flippedPrinting", file, &inverted);
	loadKey("printQRLinks", file, &qrCode);
	
	if(inverted){flippedPrinting = true;}
	else{flippedPrinting = false;}

	if(qrCode){printQRLinks = true;}
	else{printQRLinks = false;}

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
  </br>\
  </br>\
  \
  \
  	<form action=\"/printQRLinks\" method=\"POST\">\
	Print QR Code: " + String(printQRLinks ? "Enabled" : "Disabled") + " <input type=\"submit\" value=\"Toggle QR Printing\"></form>\
  \
  \
  </br>\
  </br>\
  \
  \
  	<form action=\"/flippedPrinting\" method=\"POST\">\
	Flipped text printing: " + String(flippedPrinting ? "Enabled" : "Disabled") + " <input type=\"submit\" value=\"Toggle Flipped Printing\"></form>\
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

void handleToggleFlipPrint(){
	const char* keyName = "flippedPrinting";

	//Remember to change this to whatever file you want to store/read your custom config from
	const char* file = "/config.json";

	flippedPrinting = !flippedPrinting;

	//save the key to flash and refresh data var with data from flash
	addKey(keyName, flippedPrinting ? (char*)"1" : (char*)"0" , file);

	//tell the user that the config is loaded in and the module is restarting
	server.send(200, "text/html",AFFIRM_REQ("Flipped printing has been ", String(flippedPrinting ? "Enabled" : "Disabled")));
}

void handleToggleQRLink(){
	const char* keyName = "printQRLinks";

	//Remember to change this to whatever file you want to store/read your custom config from
	const char* file = "/config.json";

	printQRLinks = !printQRLinks;

	//save the key to flash and refresh data var with data from flash
	addKey(keyName, printQRLinks ? (char*)"1" : (char*)"0" , file);

	//tell the user that the config is loaded in and the module is restarting
	server.send(200, "text/html",AFFIRM_REQ("QR Code printing has been ", String(printQRLinks ? "Enabled" : "Disabled")));
}
