
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h> 
#include <ArduinoJson.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define MAX_TEMPERATURES 48

#define MAX_DUTY_CYCLE 80
#define FUTURE_TEMP_HS 6
#define TEMP_THRESHOLD_ALWAYS_ON 6
#define TEMP_THRESHOLD_ALWAYS_OFF 20

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
const unsigned long reconnectInterval = 6000; 
const int maxReconnectAttempts = 30;
const int pinCaldera = 5;
String lastError;

typedef struct{
  int actualHour;
  int actualDutyCycle;
  float futureTemp;
  float actualTemp;
  bool heaterOn;
  String lastError;
  unsigned long uptime;
}TelemetryPack_t;

void setup() {
  int getHour;
  // Initialize serial port with baud rate of 9600 and 8N1 configuration
  Serial.begin(9600, SERIAL_8N1);
  lastError = "None";
  WiFiManager wm;
  bool res;

  pinMode(pinCaldera, OUTPUT);

  if(!turnHeaterOn()){
    Serial.printf("Error turning heater on");
    return;   }

  //CONNECT TO WIFI
  wm.setConfigPortalTimeout(180);
  res = wm.autoConnect("Termostato","password"); // password protected ap

  if(!res) {
      Serial.println("Failed to connect. Restarting");
      ESP.restart();
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("connected...yeey :)");
  }
  
  //codigo de debub para encontrar hora erronea
  //while(1){
  //  if(!getActualHour(getHour)){
  //    Serial.printf("Error getting time from web");
  //    ESP.restart();
  //  }
  //  Serial.printf("La hora es %d \n", getHour);
  //  delay(10000); 
  // }

  
  if(!getActualHour(getHour)){
    Serial.printf("Error getting time from web");
    delay(60000); 
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
    delay(60000); 
    ESP.restart();
  }

  //getForecast
  if(!getForecast(temperatures)){
    Serial.printf("Error getting forecast from web");
    delay(60000); 
    ESP.restart();
  }

}

void loop() {
  int newHour;
  String datetime;
  TelemetryPack_t tmy;
  static int dutyCycle = 100;
  static unsigned long turnOnTime = millis();
    
  checkWiFiAndReconnect();

  if(!getActualHour(newHour)){
    Serial.printf("Error getting time from web");
    lastError = "Error getting hour - " + String(int(millis()/1000));
    ESP.restart();
  }

  if(newHour != actualHour){
    Serial.printf("Cambio de hora! hora actual %d - hora nueva %d \n", actualHour, newHour);
    
    actualHour = newHour;

    //getForecast
    if(!getForecast(temperatures)){
      Serial.printf("Error getting forecast from web");
      lastError = "Error getting forecast - " + String(int(millis()/1000));
      ESP.restart();
    }

    dutyCycle = calcDutyCycle(temperatures[actualHour + FUTURE_TEMP_HS]);
    Serial.printf("DutyCycle: %d \n", dutyCycle);
    turnOnTime = millis() + 3600000 - (dutyCycle * (3600 * 1000) / 100);
    Serial.printf("Turning on in: %d ms", 3600000 - (dutyCycle * (3600 * 1000) /100));

    if(!turnHeaterOff()){
      Serial.printf("Error turning heater off");
      lastError = "Error turning heater off - " + String(int(millis()/1000));
      ESP.restart();
    }

  } 

  if((millis() > turnOnTime) && !heaterOn){
      if(!turnHeaterOn()){
        Serial.printf("Error turning heater on");
        lastError = "Error turning heater on - " + String(int(millis()/1000));
        ESP.restart();
      }
  }

  //fill telemetry
  tmy.actualDutyCycle = dutyCycle;
  tmy.actualHour = actualHour;
  tmy.futureTemp = temperatures[actualHour + FUTURE_TEMP_HS];
  tmy.heaterOn = heaterOn;
  tmy.lastError = lastError;
  tmy.uptime = millis()/1000;
  tmy.actualTemp= temperatures[actualHour];
    
  if(!sendToInflux(tmy)){
    Serial.printf("Error writing in influx");
    lastError = "Error sending tmy";
  }

  delay(60000); //sleep 5 minutes

}

int calcDutyCycle(float futureTemp){
  if (futureTemp >= TEMP_THRESHOLD_ALWAYS_OFF) return 0;
  if (futureTemp <= TEMP_THRESHOLD_ALWAYS_ON) return MAX_DUTY_CYCLE;
  
  return (int)((-MAX_DUTY_CYCLE / (TEMP_THRESHOLD_ALWAYS_OFF - TEMP_THRESHOLD_ALWAYS_ON)) * (futureTemp - TEMP_THRESHOLD_ALWAYS_ON)+ MAX_DUTY_CYCLE + 0.5);  
}


bool getActualHour(int& hour) {
    //NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000);  // -3*3600 para Argentina (UTC-3)
    NTPClient timeClient(ntpUDP, "time.google.com", -3 * 3600, 60000);
    timeClient.begin();

    const int maxAttempts = 10; // Máximo número de reintentos
    const unsigned long retryDelay = 2000; // Tiempo entre reintentos (ms)
    
    // Intentar actualizar el tiempo con reintentos
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
        if (timeClient.update()) {
            String datetime = timeClient.getFormattedTime();
            // Validar el formato y extraer la hora
            if (datetime.length() >= 2) {
                hour = datetime.substring(0, 2).toInt();

                // Validar que la hora esté en un rango válido
                if (hour >= 0 && hour <= 23) {
                    return true;  // Éxito
                }
            }

            return false;  // Formato inesperado o valor fuera de rango
        }

        Serial.printf("Intento %d de %d fallido, reintentando...\n", attempt, maxAttempts);
        delay(retryDelay);  // Esperar antes de reintentar
    }

    return false;  // Todos los intentos fallaron
}

bool getForecast(float* forecast){
    HTTPClient http;
    int httpCode;
    String url;
    String payload;

    url = "http://api.open-meteo.com/v1/forecast?latitude=-41.14&longitude=-71.4&timezone=America%2FArgentina%2FSalta&hourly=temperature_2m&forecast_days=2";
    http.begin(client, url);
    httpCode = http.GET();
    
    if (httpCode > 0) {
      payload = http.getString();
    }else{
      Serial.printf("Error geting forecast - code: %d ",httpCode);
      http.end();
      return false;
    }  

    StaticJsonDocument<256> filter;
    filter["hourly"]["temperature_2m"] = true;

    DynamicJsonDocument doc2(12000);
    DeserializationError error = deserializeJson(doc2, payload, DeserializationOption::Filter(filter));

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
  sensor.addField("actual_hour", telemetria.actualHour);
  sensor.addField("actual_duty_cycle", telemetria.actualDutyCycle);
  sensor.addField("future_temp", telemetria.futureTemp);
  sensor.addField("actual_temp", telemetria.actualTemp);
  if(telemetria.heaterOn){
    sensor.addField("heater_state", "Encendida");
    sensor.addField("heater_on", 1);  
  }else{
    sensor.addField("heater_state", "Apagada");
    sensor.addField("heater_on", 0);  
  }
  sensor.addField("lastError", telemetria.lastError);
  sensor.addField("uptime", telemetria.uptime);
  

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
  Serial.print("Prendiendo caldera - ");
  Serial.println(int(millis() / 1000));
  digitalWrite(pinCaldera, LOW);
  return true;
}

bool turnHeaterOff(){
  heaterOn = false;
  Serial.print("Apagando caldera - ");
  Serial.println(int(millis() / 1000));
  digitalWrite(pinCaldera, HIGH);
  return true;
}

void checkWiFiAndReconnect() {
  int reconnectAttempts = 0; 

  if (WiFi.status() != WL_CONNECTED){
      // Bloquear hasta que se reconecte o se alcancen los intentos máximos
      while (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi desconectado. Intentando reconectar...");
        WiFi.reconnect(); // Intentar reconectar
    
        reconnectAttempts++;
        if (reconnectAttempts > maxReconnectAttempts) {
          Serial.println("Máximo número de intentos alcanzado. Reiniciando...");
          ESP.restart(); // Reiniciar el ESP
        }
    
        delay(reconnectInterval); // Esperar antes del próximo intento
      }
    
      Serial.println("Reconexión exitosa!");
  }
}
