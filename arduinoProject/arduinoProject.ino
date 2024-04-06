
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h> 
#include <ArduinoJson.h>

#define MAX_TEMPERATURES 48

#define MAX_DUTY_CYCLE 80
#define FUTURE_TEMP_HS 12
#define TEMP_THRESHOLD_ALWAYS_ON 10
#define TEMP_THRESHOLD_ALWAYS_OFF 22


WiFiClient client;
int actualHour;
bool heaterOn;

typedef struct{
  float temp0;
  float temp1;
  float temp2;
  float temp3;
  float temp4;
  float temp5;
  float temp6;
  float temp7;
  float temp8;
  float temp9;
  float temp10;
  float temp11;
  float temp12;
  float temp13;
  float temp14;
  float temp15;
  float temp16;
  float temp17;
  float temp18;
  float temp19;
  float temp20;
  float temp21;
  float temp22;
  float temp23;
  int actualHour;
  int actualDutyCycle;
  float futureTemp;
  bool heaterOn;
}TelemetryPack_t;

void setup() {
  String datetime;
  // Initialize serial port with baud rate of 9600 and 8N1 configuration
  Serial.begin(9600, SERIAL_8N1);
  
  WiFiManager wm;
  bool res;

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

  if(!getDateTime(datetime)){
    Serial.printf("Error getting datetime from web");
    return;
  }
  actualHour = (String(datetime).substring(11, 13)).toInt();;

  if(!turnHeaterOn()){
    Serial.printf("Error turning heater on");
    return; 
  }

}

void loop() {
  int newHour;
  String datetime;
  TelemetryPack_t tmy;
  
  static float temperatures[MAX_TEMPERATURES];
  static int dutyCycle;
  static unsigned long turnOnTime = millis();
    
  if(!getDateTime(datetime)){
    Serial.printf("Error getting datetime from web");
    return;
  }
  newHour = (String(datetime).substring(11, 13)).toInt();

  // Imprimir la hora obtenida
  Serial.print(F("New hour: "));
  Serial.println(newHour);

  if(newHour != actualHour){
    
    actualHour = newHour;
    //apagar caldera

    //getForecast
    if(!getForecast(temperatures)){
      Serial.printf("Error getting forecast from web");
      return;
    }

    dutyCycle = calcDutyCycle(temperatures[actualHour + FUTURE_TEMP_HS]);
    Serial.printf("DutyCycle: %d \n", dutyCycle);
    turnOnTime = millis() + (dutyCycle * 100 / (3600 * 1000));
    Serial.printf("Turning on in: %d ms", (dutyCycle * 100 / (3600 * 1000)));

    if(!turnHeaterOff()){
      Serial.printf("Error turning heater off");
      return; 
    }

  } 

  if((millis() > turnOnTime) && !heaterOn){
      if(!turnHeaterOn()){
        Serial.printf("Error turning heater on");
        return; 
    }
  }

  //fill telemetry
  tmy.actualDutyCycle = dutyCycle;
  tmy.actualHour = actualHour;
  tmy.futureTemp = temperatures[actualHour + FUTURE_TEMP_HS];
  tmy.heaterOn = heaterOn;
  tmy.temp0 = temperatures[actualHour];
  tmy.temp1 = temperatures[actualHour + 1];
  tmy.temp2 = temperatures[actualHour + 2];
  tmy.temp3 = temperatures[actualHour + 3];
  tmy.temp4 = temperatures[actualHour + 4];
  tmy.temp5 = temperatures[actualHour + 5];
  tmy.temp6 = temperatures[actualHour + 6];
  tmy.temp7 = temperatures[actualHour + 7];
  tmy.temp8 = temperatures[actualHour + 8];
  tmy.temp9 = temperatures[actualHour + 9];
  tmy.temp10 = temperatures[actualHour + 10];
  tmy.temp11 = temperatures[actualHour + 11];
  tmy.temp12 = temperatures[actualHour + 12];
  tmy.temp13 = temperatures[actualHour + 13];
  tmy.temp14 = temperatures[actualHour + 14];
  tmy.temp15 = temperatures[actualHour + 15];
  tmy.temp16 = temperatures[actualHour + 16];
  tmy.temp17 = temperatures[actualHour + 17];
  tmy.temp18 = temperatures[actualHour + 18];
  tmy.temp19 = temperatures[actualHour + 19];
  tmy.temp20 = temperatures[actualHour + 20];
  tmy.temp21 = temperatures[actualHour + 21];
  tmy.temp22 = temperatures[actualHour + 22];
  tmy.temp23 = temperatures[actualHour + 23];
  
  sendToInflux(tmy);
  
  delay(60000); //sleep 5 minutes

}

int calcDutyCycle(float futureTemp){
  if (futureTemp >= TEMP_THRESHOLD_ALWAYS_OFF) return 0;
  if (futureTemp <= TEMP_THRESHOLD_ALWAYS_ON) return MAX_DUTY_CYCLE;
  
  return (int)((-MAX_DUTY_CYCLE / (TEMP_THRESHOLD_ALWAYS_OFF - TEMP_THRESHOLD_ALWAYS_ON)) * (futureTemp - TEMP_THRESHOLD_ALWAYS_ON)+ MAX_DUTY_CYCLE + 0.5);  
}


bool getDateTime(String& datetime){
    String url = "http://worldtimeapi.org/api/timezone/America/Argentina/Salta";
    HTTPClient http;
    int httpCode;
    String payload;
    
    http.begin(client, url);
    httpCode = http.GET();
    
    if (httpCode > 0) {
      payload = http.getString();
    }else{
      Serial.printf("Error geting actual time - code: %d ",httpCode);
      http.end();
      return false;
    }

    DynamicJsonDocument doc(500);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("deserializeJson() for date/time failed: "));
      Serial.println(error.f_str());
      http.end();
      return false;
    }

    datetime = doc["datetime"].as<String>();
    http.end();
    return true;
}

bool getForecast(float* forecast){
    HTTPClient http;
    int httpCode;
    String url;
    String payload;

    url = "http://api.open-meteo.com/v1/forecast?latitude=-41.14&longitude=-71.4&timezone=America%2FSao_Paulo&hourly=temperature_2m&forecast_days=2";
    http.begin(client, url);
    httpCode = http.GET();
    
    if (httpCode > 0) {
      payload = http.getString();
    }else{
      Serial.printf("Error geting forecast - code: %d ",httpCode);
      http.end();
      return false;
    }  
    
    DynamicJsonDocument doc2(1000);
    DeserializationError error = deserializeJson(doc2, payload);

    if (error) {
      Serial.print(F("deserializeJson() for forecast failed: "));
      Serial.println(error.f_str());
      http.end();
      return false;
    }

    // Obtener el array de temperaturas
    JsonArray temperaturas = doc2["hourly"]["temperature_2m"];
    
    size_t i = 0;
  
    for (float temperatura : temperaturas) {
      if (i < MAX_TEMPERATURES) {
        forecast[i++] = temperatura;
       }
      else {
      break; 
      }
    }

    http.end();
    return true;
}

bool sendToInflux(TelemetryPack_t tmy){
  return true;
}

bool turnHeaterOn(){
  heaterOn = true;
  return true;
}

bool turnHeaterOff(){
  heaterOn = false;
  return true;
}
