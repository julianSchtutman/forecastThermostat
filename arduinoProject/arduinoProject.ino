
#include <WiFiManager.h> 

void setup() {
  // Initialize serial port with baud rate of 9600 and 8N1 configuration
  Serial.begin(9600, SERIAL_8N1);
  WiFiManager wm;
  bool res;

  res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

  if(!res) {
      Serial.println("Failed to connect");
      // ESP.restart();
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("connected...yeey :)");
  }

  // Print message to serial console
  Serial.print("Hello World!");
}

void loop() {
  // This program does not require any code in the loop function
}
