
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h> 
#include <ArduinoJson.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define MAX_TEMPERATURES 48

#define MAX_DUTY_CYCLE 80
#define FUTURE_TEMP_HS 12
#define TEMP_THRESHOLD_ALWAYS_ON 10
#define TEMP_THRESHOLD_ALWAYS_OFF 22

#define DEVICE "ESP8266"
#define INFLUXDB_URL "http://64.227.117.215:8086"
#define INFLUXDB_TOKEN "Vq8yr2dIKNtFB0g39S4tQP0lcCkzNUAeA_XzZA3gwExDdXNRML6FyrGMcG5pMbgpqSCf4SM5R1wljMWKWYq1Hg=="
#define INFLUXDB_ORG "ed1eda531d831a2f"
#define INFLUXDB_BUCKET "thermostat"
#define TZ_INFO "UTC-3"


WiFiClient client;
int actualHour;
bool heaterOn;
InfluxDBClient influxClient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
Point sensor("Thermostat_TMY");  
float temperatures[MAX_TEMPERATURES];
WiFiUDP ntpUDP;

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
  int getHour;
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
  
  if(!getActualHour(getHour)){
    Serial.printf("Error getting time from web");
    ESP.restart();
  }
  
  actualHour = getHour;

  // Check influx server connection
  if (influxClient.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(influxClient.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(influxClient.getLastErrorMessage());
    return;
  }

  if(!turnHeaterOn()){
    Serial.printf("Error turning heater on");
    return; 
  }

  //getForecast
  if(!getForecast(temperatures)){
    Serial.printf("Error getting forecast from web");
    return;
  }

}

void loop() {
  int newHour;
  String datetime;
  TelemetryPack_t tmy;
  
  
  static int dutyCycle = 100;
  static unsigned long turnOnTime = millis();
    
  if(!getActualHour(newHour)){
    Serial.printf("Error getting time from web");
    return;
  }

  if(newHour != actualHour){
    Serial.printf("Cambio de hora! \n");
    actualHour = newHour;
    //apagar caldera

    //getForecast
    if(!getForecast(temperatures)){
      Serial.printf("Error getting forecast from web");
      return;
    }

    dutyCycle = calcDutyCycle(temperatures[actualHour + FUTURE_TEMP_HS]);
    Serial.printf("DutyCycle: %d \n", dutyCycle);
    turnOnTime = millis() + 3600000 - (dutyCycle * (3600 * 1000) / 100);
    Serial.printf("Turning on in: %d ms", 3600000 - (dutyCycle * (3600 * 1000) /100));

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
  
  if(!sendToInflux(tmy)){
    Serial.printf("Error writing in influx");
  }
  
  delay(10000); //sleep 5 minutes

}

int calcDutyCycle(float futureTemp){
  if (futureTemp >= TEMP_THRESHOLD_ALWAYS_OFF) return 0;
  if (futureTemp <= TEMP_THRESHOLD_ALWAYS_ON) return MAX_DUTY_CYCLE;
  
  return (int)((-MAX_DUTY_CYCLE / (TEMP_THRESHOLD_ALWAYS_OFF - TEMP_THRESHOLD_ALWAYS_ON)) * (futureTemp - TEMP_THRESHOLD_ALWAYS_ON)+ MAX_DUTY_CYCLE + 0.5);  
}


bool getActualHour(int& hour) {
    NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000);  // -3*3600 para Argentina (UTC-3)
    timeClient.begin();

    // Intentar actualizar el tiempo
    if (!timeClient.update()) {
        return false;  // Falló la actualización del tiempo
    }

    String datetime = timeClient.getFormattedTime();
    
    // Validar el formato y extraer la hora
    if (datetime.length() < 2) {
        return false;  // Formato inesperado
    }
    
    hour = datetime.substring(0, 2).toInt();
    
    // Validar que la hora esté en un rango válido
    if (hour < 0 || hour > 23) {
        return false;
    }

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
        forecast[i] = temperatura;
        printf("i:%d temp:%f \n",i, temperatura);
        i++; 
       }
      else {
      break; 
      }
    }

    http.end();
    return true;
}

bool sendToInflux(TelemetryPack_t telemetria){
  sensor.clearFields();
  sensor.addField("temp_0", telemetria.temp0);
  sensor.addField("temp_1", telemetria.temp1);
  sensor.addField("temp_2", telemetria.temp2);
  sensor.addField("temp_3", telemetria.temp3);
  sensor.addField("temp_4", telemetria.temp4);
  sensor.addField("temp_5", telemetria.temp5);
  sensor.addField("temp_6", telemetria.temp6);
  sensor.addField("temp_7", telemetria.temp7);
  sensor.addField("temp_8", telemetria.temp8);
  sensor.addField("temp_9", telemetria.temp9);
  sensor.addField("temp_10", telemetria.temp10);
  sensor.addField("temp_11", telemetria.temp11);
  sensor.addField("temp_12", telemetria.temp12);
  sensor.addField("temp_13", telemetria.temp13);
  sensor.addField("temp_14", telemetria.temp14);
  sensor.addField("temp_15", telemetria.temp15);
  sensor.addField("temp_16", telemetria.temp16);
  sensor.addField("temp_17", telemetria.temp17);
  sensor.addField("temp_18", telemetria.temp18);
  sensor.addField("temp_19", telemetria.temp19);
  sensor.addField("temp_20", telemetria.temp20);
  sensor.addField("temp_21", telemetria.temp21);
  sensor.addField("temp_22", telemetria.temp22);
  sensor.addField("temp_23", telemetria.temp23);
  sensor.addField("actual_hour", telemetria.actualHour);
  sensor.addField("actual_duty_cycle", telemetria.actualDutyCycle);
  sensor.addField("future_temp", telemetria.futureTemp);
  if(telemetria.heaterOn){
    sensor.addField("heater_state", "Encendida");
    sensor.addField("heater_on", 1);  
  }else{
    sensor.addField("heater_state", "Apagada");
    sensor.addField("heater_on", 0);  
  }
  

  // Print what are we exactly writing
  //Serial.print("Writing: ");
  //Serial.println(sensor.toLineProtocol());

  // Write point
  if (!influxClient.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(influxClient.getLastErrorMessage());
    return false;
  }
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
