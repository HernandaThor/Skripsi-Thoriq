#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include <Wire.h>
#include <ModbusMaster.h>
#include "esp_adc_cal.h"
#include <ArduinoJson.h>

//WiFi & MQTT
const char* ssid = "wifiyangdigunakan";
const char* password = "";
const char* mqtt_server = "servermqtt";
const int mqtt_port = portyangdigunakan;
const char* mqtt_topic = "topikyangdipublish";

const char* mqtt_user = "usermqtt";    
const char* mqtt_pass = "passwordmqtt";    

WiFiClient espClient;
PubSubClient client(espClient);

// ==================== LCD ====================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ==================== RS485 PZEM ====================
#define RS485_1_RX 16
#define RS485_1_TX 17
HardwareSerial RS485_1(1);
ModbusMaster pzem1;

// ==================== RS485 Autonics TM2/TM4 ====================
#define RS485_2_RX 19
#define RS485_2_TX 18
HardwareSerial RS485_2(2);
ModbusMaster nodeTM2;
ModbusMaster nodeTM4;

// ==================== DHT21 ====================
#define DHT_PIN 4
#define DHT_TYPE DHT21
DHT dht(DHT_PIN, DHT_TYPE);

// ==================== LM35 ====================
#define LM35_1 32
#define LM35_2 33
#define LM35_3 36
esp_adc_cal_characteristics_t adc_cal;

// ==================== Data Variables ====================
float voltage1, current1, power1, energy1;
float tempDHT, humDHT;
float temp1, temp2, temp3;
float tm2_ch1, tm2_ch2;
float tm4_ch1, tm4_ch2, tm4_ch3, tm4_ch4;

// ==================== Shunt Setting ====================
enum ShuntValue { Shunt100A = 0x0000, Shunt50A = 0x0001, Shunt200A = 0x0002 };
void setShunt(HardwareSerial &port, uint8_t addr, ShuntValue shunt) {
  uint16_t u16CRC = 0xFFFF;
  uint8_t packet[8];
  packet[0] = addr; packet[1] = 0x06; packet[2] = 0x00; packet[3] = 0x02;
  packet[4] = (shunt >> 8) & 0xFF; packet[5] = shunt & 0xFF;
  for (int i = 0; i < 6; i++) {
    u16CRC ^= packet[i];
    for (int j = 0; j < 8; j++)
      u16CRC = (u16CRC & 1) ? (u16CRC >> 1) ^ 0xA001 : (u16CRC >> 1);
  }
  packet[6] = u16CRC & 0xFF; packet[7] = (u16CRC >> 8) & 0xFF;
  port.write(packet, 8);
  delay(200);
}

// ==================== Fungsi Baca Sensor Analog ====================
float readLM35(int pin) {
  uint32_t adc_reading = analogRead(pin);
  uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_cal);
  return voltage / 10.0; // 10mV per °C
}

// ==================== Setup ====================
void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  // RS485 PZEM
  RS485_1.begin(9600, SERIAL_8N1, RS485_1_RX, RS485_1_TX);
  pzem1.begin(0x01, RS485_1);
  setShunt(RS485_1, 0x01, Shunt50A);

  // RS485 Autonics
  RS485_2.begin(9600, SERIAL_8N1, RS485_2_RX, RS485_2_TX);
  nodeTM2.begin(1, RS485_2);  // Slave ID TM2
  nodeTM4.begin(2, RS485_2);  // Slave ID TM4

  // DHT
  dht.begin();

  // LM35 ADC calibration
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_cal);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());

  // MQTT
  client.setServer(mqtt_server, mqtt_port);

  // LCD Splash
  lcd.setCursor(4, 1); lcd.print("Peltier");
  lcd.setCursor(2, 2); lcd.print("Monitoring v5");
  delay(2000);
  lcd.clear();
}

// ==================== MQTT Reconnect ====================
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_pass))
      Serial.println("connected.");
    else {
      Serial.print("failed, rc="); Serial.print(client.state());
      delay(2000);
    }
  }
}

// ==================== Pembacaan Sensor ====================
void readSensors() {
  // DHT
  tempDHT = dht.readTemperature();
  humDHT = dht.readHumidity();

  // LM35
  temp1 = readLM35(LM35_1);
  temp2 = readLM35(LM35_2);
  temp3 = readLM35(LM35_3);

  // PZEM-017
  if (pzem1.readInputRegisters(0x0000, 6) == pzem1.ku8MBSuccess) {
    voltage1 = pzem1.getResponseBuffer(0) / 100.0;
    current1 = pzem1.getResponseBuffer(1) / 10.0;
    power1   = ((uint32_t)pzem1.getResponseBuffer(3) << 16 | pzem1.getResponseBuffer(2)) ;
    energy1  = ((uint32_t)pzem1.getResponseBuffer(5) << 16 | pzem1.getResponseBuffer(4)) * 1.0;
  }

  // Autonics TM2 (ID 1)
  if (nodeTM2.readInputRegisters(1000, 1) == nodeTM2.ku8MBSuccess) tm2_ch1 = nodeTM2.getResponseBuffer(0);
  delay(1000);
  if (nodeTM2.readInputRegisters(1006, 1) == nodeTM2.ku8MBSuccess) tm2_ch2 = nodeTM2.getResponseBuffer(0);
  delay(1000);

  // Autonics TM4 (ID 2)
  delay(2000);
  if (nodeTM4.readInputRegisters(1000, 1) == nodeTM4.ku8MBSuccess) tm4_ch1 = nodeTM4.getResponseBuffer(0);
  delay(2000);
  if (nodeTM4.readInputRegisters(1006, 1) == nodeTM4.ku8MBSuccess) tm4_ch2 = nodeTM4.getResponseBuffer(0);
  delay(2000);
  if (nodeTM4.readInputRegisters(1012, 1) == nodeTM4.ku8MBSuccess) tm4_ch3 = nodeTM4.getResponseBuffer(0);
  delay(2000);
  if (nodeTM4.readInputRegisters(1018, 1) == nodeTM4.ku8MBSuccess) tm4_ch4 = nodeTM4.getResponseBuffer(0);
}

// ==================== Publish ke MQTT ====================
void publishData() {
  StaticJsonDocument<512> doc;
  doc["dht_temp"] = tempDHT;
  doc["dht_hum"] = humDHT;
  doc["lm35_t1"] = temp1;
  doc["lm35_t2"] = temp2;
  doc["lm35_t3"] = temp3;

  JsonObject pz1 = doc.createNestedObject("pzem1");
  pz1["voltage"] = voltage1;
  pz1["current"] = current1;
  pz1["power"] = power1;
  pz1["energy"] = energy1;

  JsonObject autonics = doc.createNestedObject("autonics");
  autonics["tm2_ch1"] = tm2_ch1;
  autonics["tm2_ch2"] = tm2_ch2;
  autonics["tm4_ch1"] = tm4_ch1;
  autonics["tm4_ch2"] = tm4_ch2;
  autonics["tm4_ch3"] = tm4_ch3;
  autonics["tm4_ch4"] = tm4_ch4;

  char buffer[512];
  size_t n = serializeJson(doc, buffer);
  client.publish(mqtt_topic, buffer, n);

  Serial.println(buffer);
}

// ==================== LCD Display ====================
unsigned long lastLCDMillis = 0;
const unsigned long intervalLCD = 4000;
int lcdPage = 0;

void displayLCD() {
  lcd.clear();
  if (lcdPage == 0) {
    lcd.setCursor(0, 0); lcd.print("PZ1 V: "); lcd.print(voltage1); lcd.print("V");
    lcd.setCursor(0, 1); lcd.print("I: "); lcd.print(current1); lcd.print("A");
    lcd.setCursor(0, 2); lcd.print("P: "); lcd.print(power1); lcd.print("W");
    lcd.setCursor(0, 3); lcd.print("E: "); lcd.print(energy1); lcd.print("Wh");
  } else if (lcdPage == 1) {
    lcd.setCursor(0, 0); lcd.print("LM35 T1: "); lcd.print(temp1); lcd.print("C");
    lcd.setCursor(0, 1); lcd.print("LM35 T2: "); lcd.print(temp2); lcd.print("C");
    lcd.setCursor(0, 2); lcd.print("LM35 T3: "); lcd.print(temp3); lcd.print("C");
    lcd.setCursor(0, 3); lcd.print("DHT "); lcd.print(tempDHT); lcd.print("C ");
    lcd.print(humDHT); lcd.print("%");
  } else {
    lcd.setCursor(0, 0); lcd.print("TM2-1: "); lcd.print(tm2_ch1); lcd.print("C");
    lcd.setCursor(0, 1); lcd.print("TM2-2: "); lcd.print(tm2_ch2); lcd.print("C");
    lcd.setCursor(0, 2); lcd.print("TM4-1: "); lcd.print(tm4_ch1); lcd.print("C");
    lcd.setCursor(0, 3); lcd.print("TM4-2: "); lcd.print(tm4_ch2); lcd.print("C");
  }
  lcdPage = (lcdPage + 1) % 3;
}

// ==================== Loop ====================
unsigned long lastDataMillis = 0;
const unsigned long intervalData = 60000; // kirim setiap 60 detik

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastDataMillis >= intervalData) {
    lastDataMillis = now;
    readSensors();
    publishData();
  }

  if (now - lastLCDMillis >= intervalLCD) {
    lastLCDMillis = now;
    displayLCD();
  }
}
