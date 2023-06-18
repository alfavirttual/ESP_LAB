#include <Arduino.h>
#include <DHT.h>
#include <GyverHUB.h>
#include <SimplePortal.h>
#include <EEPROM.h>

#define DHT_SENSOR_PIN D4
#define DHT_SENSOR_TYPE DHT11
#define LIGHT_SENSOR_PIN A0 
#define BUZZER_PIN D8
#define LED_PIN D2
#define BUTTON_PIN D1
#define LED_R_PIN D6
#define LED_G_PIN D7
#define LED_B_PIN D5
#define BUTTON_HOTSPOT_ON_PIN D3

// Отключение не используемых модулей библиотеки GyverHUB
#define GH_NO_PORTAL    // открытие сайта из памяти esp
#define GH_NO_FS        // работа с файлами (включая ОТА!)
#define GH_NO_OTA       // ОТА файлом с приложения
#define GH_NO_OTA_URL   // ОТА по URL

struct Settings_connections{
  char SSID[32] = "";
  char pass[32] = "";
  char login[32] = "";
  char pass_mqtt[32] = "";
  char host[32] = "";
  char port[8] = "";
} settings;

DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);
GyverHUB hub("Home", "ESP8266", "");  // префикс, имя, иконка

unsigned long light = 0;
unsigned int  bright = 0;
float temperature = 0;
float humidity = 0;
bool led_on_off = false;
bool rgb_led_on_off = false;
bool flag = false;
GHcolor color = 0x0ED92D;


void sensor_polling(float* temp, float* hum, unsigned long* light);
void RGB_set(GHcolor color);

void build() {

  hub.BeginWidgets();
  hub.WidgetSize(50);
  hub.Display(F("Temperature"), String(temperature, 1) + "°C", F("Temperature"), 0x2000ff, 1, 38);
  hub.Display(F("Humidity"), String(humidity, 1) + "%", F("Humidity"), 0x2000ff, 1, 38);
  hub.EndWidgets();

  hub.BeginWidgets();
  hub.Gauge(F("Illumination"), light, "%", F("Illumination"), 0, 100, 1, 0xffdd3f);
  hub.EndWidgets();

  hub.BeginWidgets();
  hub.WidgetSize(25);
  if(hub.SwitchIcon(F("Light"), &led_on_off, F("light"), "", 0xffd200)){
    if(led_on_off){
      analogWrite(LED_PIN, bright);
      tone(BUZZER_PIN, 1500, 300);
    }
    else {
      analogWrite(LED_PIN, 0);
      for(int i = 0; i < 2; i++){
        tone(BUZZER_PIN, 1500, 300);
        delay(700);
      }
    }
  }
  hub.WidgetSize(75);
  if(hub.Slider(F("Slider"), &bright, GH_UINT8, F("Brightness"), 0, 100, 1, 0xffd200)){
    led_on_off ? analogWrite(LED_PIN, bright) : analogWrite(LED_PIN, 0);
  }     
  hub.EndWidgets();

  hub.BeginWidgets();
  hub.WidgetSize(50);
  if(hub.SwitchIcon(F("RGB_ON"), &rgb_led_on_off, F("RGB"), "", color)){
    rgb_led_on_off ? RGB_set(color) : RGB_set(0);
  }
  if(hub.Color(F("Color"), &color, F("Color"))){
    rgb_led_on_off ? RGB_set(color) : RGB_set(0);
  }
  hub.EndWidgets();

  hub.BeginWidgets();
  if(hub.Button(F("Buzzer"), 0, F("Buzzer"), 0xFF0024, 30)){
      tone(BUZZER_PIN, 2000, 300);
  }

  hub.EndWidgets();

}

void setup() {

    pinMode(LED_PIN, OUTPUT);
    pinMode(LED_R_PIN, OUTPUT);
    pinMode(LED_G_PIN, OUTPUT);
    pinMode(LED_B_PIN, OUTPUT); 
    pinMode(BUTTON_PIN, INPUT);
    pinMode(BUTTON_HOTSPOT_ON_PIN, INPUT_PULLUP);
    delay(3000);
    Serial.begin(9600);
    EEPROM.begin(168);

    if(!digitalRead(BUTTON_HOTSPOT_ON_PIN)){
      portalRun(); 
  
      if (portalStatus() == SP_SUBMIT) {
        strcpy(settings.SSID, portalCfg.SSID);
        strcpy(settings.pass, portalCfg.pass);
        strcpy(settings.login, portalCfg.login);
        strcpy(settings.port, portalCfg.port);
        strcpy(settings.host, portalCfg.host);
        strcpy(settings.pass_mqtt, portalCfg.pass_mqtt);

        EEPROM.put(0, settings);
        EEPROM.commit();
        }
    }

    EEPROM.get(0, settings);

    dht_sensor.begin();
    sensor_polling(&temperature, &humidity, &light);


    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.SSID, settings.pass);

    //WiFi.begin("WIFI.NET", "0713850347");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    for(int i = 0; i < 3; i++){
      tone(BUZZER_PIN, 3000, 300);
      delay(700);
    }

    Serial.println("Ready!");

    // hub.setupMQTT("test.mosquitto.org", 1883);
    
    hub.setupMQTT(settings.host, atoi(settings.port), settings.login, settings.pass_mqtt);
    
    hub.onBuild(build);     // подключаем билдер
    hub.begin();            // запускаем систему
    hub.tick();
    if(hub.online()) tone(BUZZER_PIN, 1000, 500);

}

void loop() {
  static GHtimer tmr(2000); // Обновлять показания с датчиков каждые 2 секунды
    if (tmr.ready()) {
      sensor_polling(&temperature, &humidity, &light);
      hub.sendUpdate("Temperature,Humidity,Illumination");
      
    }
  
  if(!digitalRead(BUTTON_PIN) && flag == false){
    static GHtimer tmr(300);
      if (tmr.ready()){
        if(!led_on_off){
          bright = 100;
          digitalWrite(LED_PIN, 1);
          tone(BUZZER_PIN, 1500, 300);
        }
        else{
          bright = 0;
          digitalWrite(LED_PIN, 0);
        }
        led_on_off = !led_on_off;
        hub.sendUpdate("Slider,Light");
        flag = true;
    }
  }
  else if(digitalRead(BUTTON_PIN)) flag = false;

  hub.tick();  // обязательно тикаем тут
}

void sensor_polling(float* temp, float* hum, unsigned long* light){
  *hum = dht_sensor.readHumidity();
  *temp = dht_sensor.readTemperature();
  *light = int(100 - ((analogRead(LIGHT_SENSOR_PIN) / 1024.0) * 100));

  if(isnan(*temp) || isnan(*hum)){
    Serial.println("Faled to read from DHT sensor!");
    for(int i = 0; i < 5; i++){
      tone(BUZZER_PIN, 4500, 300);
      delay(500);
    }
  }
}

void RGB_set(GHcolor color){
  analogWrite(LED_R_PIN, color.r);
  analogWrite(LED_G_PIN, color.g);
  analogWrite(LED_B_PIN, color.b);
}
