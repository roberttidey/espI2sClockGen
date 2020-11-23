//clock gen using i2s output on GPIO3 (RX0)
// R.J.Tidey 11/11/2020

#define ESP8266
#include <arduino.h>
#include "BaseConfig.h"
#include "i2sTXcircular.h"

#define PULSEFILE_PREFIX "pulse"

void setMSArrayItem(uint16_t bufIx, String line) {
	int i,j;
	uint16_t spaceBits = 16;
	uint16_t markBits = 16;
	uint16_t repeat = 0;
	i = 0;
	j = line.indexOf(',', i);
	if(j>0) {
		markBits = line.substring(i,j).toInt();
		i = j + 1;
		j = line.indexOf(',', i);
		if(j>0) {
			spaceBits = line.substring(i,j).toInt();
			i = j + 1;
			j = line.indexOf(',', i);
			if(j<0) j = 255;
			if(j>0) {
				repeat = line.substring(i,j).toInt();
			}
		}
	}
	Serial.println("setMS " + String(bufIx) + " " + String(markBits) + " " + String(spaceBits) + " " + String(repeat));
	i2scSetMSArrayItem(bufIx, markBits, spaceBits, repeat);
}

void setPulses(String filename) {
	String line = "";
	int config = 0;
	uint16_t count32 = 512;
	uint16_t oneShot = 0;
	uint16_t bufIx = 0;
	uint32_t clockDiv1 = 1000000;
	int16_t div2 = -1;
	uint16_t pulseFormat;
	
	File f = FILESYS.open(filename, "r");
	if(f) {
		while(f.available()) {
			line =f.readStringUntil('\n');
			line.replace("\r","");
			if(line.length() > 0 && line.charAt(0) != '#') {
				switch(config) {
					case 0: clockDiv1 = line.toInt();break;
					case 1: div2 = line.toInt();break;
					case 2: count32 = line.toInt();break;
					case 3: oneShot = line.toInt();
							i2scInit(count32, clockDiv1, div2, oneShot);
							break;
					case 4: pulseFormat = line.toInt();
							break;
					default : 
						switch(pulseFormat) {
							case 0:
								//repeated mark space format
								setMSArrayItem(bufIx, line);
								bufIx++;
								break;
						}
						break;
				}
				config++;
			}
		}
		f.close();
		i2scBegin();
	}
}

//action get pulse files
void handleGetPulseFiles() {
	String fileList;
	String filename;
	Dir dir = FILESYS.openDir("/");
	while (dir.next()) {
		filename = dir.fileName();
		if(filename.indexOf(PULSEFILE_PREFIX) == 1) {
			fileList += filename.substring(1) + "<BR>";
		}
	}
	server.send(200, "text/html", fileList);
}

void handleSetClock() {
	String ret;
	uint16_t markBits = server.arg("markbits").toInt();
	uint16_t spaceBits = server.arg("spacebits").toInt();
	uint16_t count32 = server.arg("count32").toInt();
	uint16_t oneShot = server.arg("oneshot").toInt();
	uint32_t clockDiv1 = server.arg("clockDiv1").toInt();
	int16_t div2 = server.arg("div2").toInt();
	i2scInit(count32, clockDiv1, div2, oneShot);
	i2scSetMSArrayItem(0, markBits, spaceBits, 0);
	i2scBegin();
	ret = "Hz = " + String(i2scGetRealBitClock());
	Serial.println(ret);
	server.send(200, "text/html", ret);
}

void handleSetPulses() {
	String filename = server.arg("filename");
	if(filename.charAt(0) != '/') filename = "/" + filename;
	setPulses(filename);
	server.send(200, "text/html", "Hz = " + String(i2scGetRealBitClock()));
}

void setupStart() {
}

// add any extra webServer handlers here
void extraHandlers() {
	server.on("/setclock", handleSetClock);
	server.on("/setpulses", handleSetPulses);
	server.on("/getpulsefiles", handleGetPulseFiles);
}

void setupEnd() {
	i2scInit(512, 1000000, -1, 0);
	i2scSetMSArrayItem(0, 16, 16, 0);
	i2scBegin();
}

void loop() {
	server.handleClient();
	wifiConnect(1);
	delaymSec(10);
}
