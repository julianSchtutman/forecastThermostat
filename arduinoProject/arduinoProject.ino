
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h> 
#include <ArduinoJson.h>

#define MAX_TEMPERATURES 48


void setup() {
  // Initialize serial port with baud rate of 9600 and 8N1 configuration
  Serial.begin(9600, SERIAL_8N1);
  
  
  WiFiManager wm;
  bool res;
  String payload;

  //CONNECT TO WIFI
  res = wm.autoConnect("Termostato","password"); // password protected ap

  if(!res) {
      Serial.println("Failed to connect");
      // ESP.restart();
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("connected...yeey :)");
  }

  //HTTP init
  WiFiClient client;
  HTTPClient http;
  int httpCode;

  //GET ACTUAL HOUR
  String url = "http://worldtimeapi.org/api/timezone/America/Argentina/Salta";
  http.begin(client, url);

  httpCode = http.GET();
  if (httpCode > 0) {
    payload = http.getString();
    //Serial.println(payload);
  }else{
    Serial.printf("Error geting actual time - code: %d ",httpCode);
  }

  DynamicJsonDocument doc(500);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  const char* datetime = doc["datetime"];
  Serial.printf("datetime: %s",datetime);
  String hora = String(datetime).substring(11, 13);

  // Imprimir la hora obtenida
  Serial.print(F("Hora extraída: "));
  Serial.println(hora);

  //Get forecast
  
  url = "http://api.open-meteo.com/v1/forecast?latitude=-41.14&longitude=-71.4&timezone=America%2FSao_Paulo&hourly=temperature_2m&forecast_days=2";
  http.begin(client, url);
  httpCode = http.GET();
  if (httpCode > 0) {
    payload = http.getString();
    Serial.println(payload);
  }else{
    Serial.printf("Error geting forecast - code: %d ",httpCode);
  }

  
  DynamicJsonDocument doc2(1000);
  error = deserializeJson(doc2, payload);

  // Obtener el array de temperaturas
  JsonArray temperaturas = doc2["hourly"]["temperature_2m"];
  float temperatures[MAX_TEMPERATURES];
  size_t i = 0;
  
  // Imprimir las temperaturas
  Serial.println(F("Temperaturas obtenidas del JSON:"));
  for (float temperatura : temperaturas) {
      if (i < MAX_TEMPERATURES) {
        temperatures[i++] = temperatura;
       }else {
      break;
    }
  }

  Serial.println(F("Temperaturas almacenadas en el array:"));
  for (size_t j = 0; j < i; j++) {
    Serial.println(temperatures[j]);
  }
  
  
}

void loop() {
  // This program does not require any code in the loop function
}
