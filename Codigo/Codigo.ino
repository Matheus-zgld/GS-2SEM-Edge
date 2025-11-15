#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ============== CONFIGURAÇÕES ==============

// Pinos
#define DHTPIN 4
#define DHTTYPE DHT22
#define LDR_PIN 35
#define STRESS_BUTTON 12
#define JOYSTICK_X 34
#define JOYSTICK_Y 33
#define POT_PIN 32

#define LED_R 25
#define LED_G 26
#define LED_B 27
#define BUZZER_PIN 2
#define SERVO_PIN 13
#define LCD_SDA 21
#define LCD_SCL 22

// Objetos
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servoMotor;

// Variáveis de estado
float temperature = 0;
float humidity = 0;
int ldr_value = 0;
int stress_level = 0;
int joystick_x = 0;
int joystick_y = 0;
int productivity = 0;
int wellbeing_score = 50;

unsigned long last_mqtt = 0;
unsigned long last_decision = 0;
unsigned long last_pause = 0;

// Funções MQTT (via Serial)
void mqtt_publish(const char* topic, const char* payload) {
  Serial.print("[MQTT Publish] ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(payload);
}

void mqtt_subscribe(const char* topic) {
  Serial.print("[MQTT Subscribe] ");
  Serial.println(topic);
}

// Setup
void setup() {
  Serial.begin(115200);
  
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STRESS_BUTTON, INPUT);
  
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();
  lcd.backlight();
  lcd.print("SYNAPSE Simulado");
  lcd.setCursor(0,1);
  lcd.print("(Wokwi)");

  dht.begin();
  
  servoMotor.attach(SERVO_PIN, 1000, 2000);
  servoMotor.write(0);
  
  mqtt_subscribe("synapse/command/user_001/+");
}

// Loop principal
void loop() {
  read_sensors();
  calculate_wellbeing();
  update_display();
  take_decisions();

  static unsigned long last_send = 0;
  if (millis() - last_send > 30000) {
    last_send = millis();
    send_mqtt_data();
  }

  delay(500);
}

// Ler sensores simulados e reais
void read_sensors() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  ldr_value = analogRead(LDR_PIN);
  stress_level = map(analogRead(STRESS_BUTTON), 0, 4095, 0, 5);
  joystick_x = analogRead(JOYSTICK_X);
  joystick_y = analogRead(JOYSTICK_Y);
  productivity = map(analogRead(POT_PIN), 0, 4095, 0, 100);

  Serial.print("Temp: "); Serial.print(temperature, 2);
  Serial.print("°C | Umid: "); Serial.print(humidity, 2);
  Serial.print("% | LDR: "); Serial.print(ldr_value);
  Serial.print(" | Estresse: "); Serial.print(stress_level);
  Serial.print(" | Produtividade: "); Serial.println(productivity);
}

// Calcular Score de bem-estar
void calculate_wellbeing() {
  wellbeing_score = 50;

  if (temperature >= 18 && temperature <= 24) wellbeing_score += 15;
  else if (temperature > 24 && temperature <= 28) wellbeing_score += 10;
  else if (temperature < 18 && temperature >= 14) wellbeing_score += 5;
  else wellbeing_score -= 10;

  if (humidity >= 40 && humidity <= 60) wellbeing_score += 15;
  else if (humidity > 60 && humidity <= 75) wellbeing_score += 5;
  else wellbeing_score -= 5;

  if (ldr_value < 200) wellbeing_score -= 20;
  else if (ldr_value < 500) wellbeing_score += 5;
  else wellbeing_score += 15;

  wellbeing_score -= stress_level * 8;

  if (joystick_x < 1500) wellbeing_score -= 15;
  else if (joystick_x > 2500) wellbeing_score += 10;

  if (joystick_y > 3000) wellbeing_score -= 15;

  if (productivity < 30) wellbeing_score -= 20;
  else if (productivity > 70) wellbeing_score += 10;

  wellbeing_score = constrain(wellbeing_score, 0, 100);

  Serial.print("Bem-Estar Score: ");
  Serial.println(wellbeing_score);
}

// Atualizar display com mostragem do potenciômetro em tempo real
void update_display() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BE:"); lcd.print(wellbeing_score); 
  lcd.print("% T:"); lcd.print((int)temperature); lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print("U:"); lcd.print((int)humidity); 
  lcd.print("% P:"); lcd.print(productivity); lcd.print("%");
}

// Decisões para atuadores
void take_decisions() {
  if (wellbeing_score > 75) {
    set_led_rgb(0, 255, 0); 
    digitalWrite(BUZZER_PIN, LOW);
    servoMotor.write(0);
  } else if (wellbeing_score > 50) {
    set_led_rgb(255, 255, 0);  
    
    if (millis() - last_pause > 3600000) {
      buzzer_alarm(3);
      last_pause = millis();
    }
    
    if (temperature > 26 || humidity > 65)
      servoMotor.write(90);
    else
      servoMotor.write(45);
  } else {
    set_led_rgb(255, 0, 0); 
    buzzer_alarm(5);
    servoMotor.write(90);
  }
}

// Função para controle RGB LED
void set_led_rgb(int r, int g, int b) {
  analogWrite(LED_R, r);
  analogWrite(LED_G, g);
  analogWrite(LED_B, b);
}

// Buzzer
void buzzer_alarm(int num_beeps) {
  for (int i = 0; i < num_beeps; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
}

// Envia MQTT via Serial
void send_mqtt_data() {
  char topic[100];
  char payload[50];

  sprintf(topic, "synapse/bem-estar/user_001/score");
  sprintf(payload, "%d", wellbeing_score);
  mqtt_publish(topic, payload);

  sprintf(topic, "synapse/ambiente/user_001/temp");
  sprintf(payload, "%.1f", temperature);
  mqtt_publish(topic, payload);

  sprintf(topic, "synapse/ambiente/user_001/umidade");
  sprintf(payload, "%.0f", humidity);
  mqtt_publish(topic, payload);

  sprintf(topic, "synapse/ambiente/user_001/luz");
  sprintf(payload, "%d", ldr_value);
  mqtt_publish(topic, payload);

  sprintf(topic, "synapse/produtividade/user_001/prod");
  sprintf(payload, "%d", productivity);
  mqtt_publish(topic, payload);

  Serial.println("Dados MQTT enviados.");
}

