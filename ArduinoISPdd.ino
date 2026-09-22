#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 8
#define OLED_SCL 10

#define SEND_PIN 2
#define RCV_PIN 3
#define BUZZER_PIN 7 // Динамик

int touchThreshold = 60; 

#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum RobotState {
  STATE_IDLE,     
  STATE_HAPPY,    
  STATE_TEA,      
  STATE_PINGPONG, 
  STATE_READING,  
  STATE_SLEEP,
  STATE_WAKE_UP,
  STATE_ANGRY     // Сохранили эмоцию, доступна по команде '6'
};

RobotState currentState = STATE_IDLE;

unsigned long lastTouchTime = 0;
unsigned long lastTouchCheck = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastActionTime = 0;
unsigned long hobbyStartTime = 0; 
unsigned long lastSnoreTime = 0; 
unsigned long wakeStartTime = 0; 
unsigned long lightningX = 64; 

// Время бездействия перед хобби — 5 минут (300000 мс)
const unsigned long BORED_TIMEOUT = 300000; 
// Время до сна — 15 минут (900000 мс)
const unsigned long SLEEP_TIMEOUT = 900000; 

bool isBlinking = false;
unsigned long blinkStart = 0;
int eyeOffsetX = 0; 
int eyeOffsetY = 0; 
int pingPongPos = 20; 
int pingPongDir = 4;
int cupOffsetX = 0; 

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(SEND_PIN, OUTPUT);
  digitalWrite(SEND_PIN, LOW);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(OLED_SDA, OLED_SCL);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.display();
  
  lastTouchTime = millis();
  lastActionTime = millis();
  playWakeSound();
  currentState = STATE_WAKE_UP;
  wakeStartTime = millis();
  
  Serial.println("Robot initialized! Send 0-6 in Serial to control animations.");
}

void loop() {
  unsigned long currentMillis = millis();

  // Чтение команд из Serial порта (0-IDLE, 1-HAPPY, 2-TEA, 3-PINGPONG, 4-READING, 5-SLEEP, 6-ANGRY)
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    lastTouchTime = currentMillis; 
    
    if (cmd == '0') {
      currentState = STATE_IDLE;
    } else if (cmd == '1') {
      currentState = STATE_HAPPY;
      playHappySound();
    } else if (cmd == '2') {
      currentState = STATE_TEA;
      hobbyStartTime = currentMillis;
      playHobbySound();
    } else if (cmd == '3') {
      currentState = STATE_PINGPONG;
      hobbyStartTime = currentMillis;
      playHobbySound();
    } else if (cmd == '4') {
      currentState = STATE_READING;
      hobbyStartTime = currentMillis;
      playHobbySound();
    } else if (cmd == '5') {
      currentState = STATE_SLEEP;
      playSleepSound();
    } else if (cmd == '6') {
      currentState = STATE_ANGRY;
      lightningX = random(25, 103);
      playAngrySound();
    }
  }

  // Опрос сенсора касания каждые 50 мс
  if (currentMillis - lastTouchCheck >= 50) {
    lastTouchCheck = currentMillis;
    long touchVal = readCapacitivePin();

    if (touchVal > touchThreshold) {
      // Если держишь или гладишь — постоянно обновляем время касания, чтобы робот радовался
      lastTouchTime = currentMillis;
      lastActionTime = currentMillis;
      
      if (currentState == STATE_SLEEP) {
        playWakeSound();
        currentState = STATE_WAKE_UP;
        wakeStartTime = currentMillis;
      } 
      else if (currentState != STATE_HAPPY && currentState != STATE_WAKE_UP && currentState != STATE_ANGRY) {
        currentState = STATE_HAPPY;
        playHappySound();
      }
    } else {
      // Если палец убрали (или пока держим в режиме HAPPY)
      if (currentState == STATE_HAPPY) {
        // Если прошло больше 1.5 секунд после того, как перестали гладить — возвращаемся в IDLE
        if (currentMillis - lastTouchTime > 1500) { 
          currentState = STATE_IDLE; 
          hobbyStartTime = currentMillis;
        }
      } 
      else if (currentState != STATE_ANGRY) {
        if (currentMillis - lastTouchTime > BORED_TIMEOUT && currentState == STATE_IDLE) {
          int hobby = random(0, 3);
          if (hobby == 0) currentState = STATE_TEA;
          else if (hobby == 1) currentState = STATE_PINGPONG;
          else currentState = STATE_READING;
          
          hobbyStartTime = currentMillis;
          playHobbySound();
        }
        
        if (currentState >= STATE_TEA && currentState <= STATE_READING) {
          if (currentMillis - hobbyStartTime > 30000) {
            currentState = STATE_IDLE;
            lastTouchTime = currentMillis;
          }
        }
        
        if (currentMillis - lastTouchTime > SLEEP_TIMEOUT && currentState != STATE_SLEEP && currentState != STATE_WAKE_UP) {
          playSleepSound();
          currentState = STATE_SLEEP;
        }
      }
    }
  }

  if (currentState == STATE_SLEEP) {
    if (currentMillis - lastSnoreTime > 2500) {
      lastSnoreTime = currentMillis;
      playSnoreSound();
    }
  }

  if (currentState == STATE_IDLE && currentMillis - lastActionTime > 4000) {
    lastActionTime = currentMillis;
    int r = random(0, 5);
    if (r == 0) { eyeOffsetX = -4; eyeOffsetY = 0; }
    else if (r == 1) { eyeOffsetX = 4; eyeOffsetY = 0; }
    else if (r == 2) { eyeOffsetX = 0; eyeOffsetY = -3; }
    else { eyeOffsetX = 0; eyeOffsetY = 0; }
  }

  if (currentState == STATE_IDLE) {
    if (!isBlinking && currentMillis - lastBlinkTime > random(3000, 6000)) {
      isBlinking = true;
      blinkStart = currentMillis;
      lastBlinkTime = currentMillis;
    }
    if (isBlinking && currentMillis - blinkStart > 120) {
      isBlinking = false;
    }
  }

  updateDisplay();
}

long readCapacitivePin() {
  pinMode(RCV_PIN, OUTPUT);
  digitalWrite(RCV_PIN, LOW);
  delayMicroseconds(10);
  pinMode(RCV_PIN, INPUT);
  
  digitalWrite(SEND_PIN, HIGH);
  long startTime = micros();
  
  while (digitalRead(RCV_PIN) == LOW) {
    if (micros() - startTime > 10000) break;
  }
  long duration = micros() - startTime;
  digitalWrite(SEND_PIN, LOW);
  
  return duration;
}

void updateDisplay() {
  display.clearDisplay();

  switch (currentState) {
    case STATE_WAKE_UP: {
      unsigned long elapsed = millis() - wakeStartTime;
      const unsigned long WAKE_DURATION = 800; 
      
      if (elapsed > WAKE_DURATION) {
        currentState = STATE_IDLE; 
        lastTouchTime = millis();  
        break;
      }

      int currentHeight = map(elapsed, 0, WAKE_DURATION, 2, 30);
      int yPos = 35 - (currentHeight / 2);

      display.fillRoundRect(30, yPos, 24, currentHeight, 6, SSD1306_WHITE);
      display.fillRoundRect(74, yPos, 24, currentHeight, 6, SSD1306_WHITE);
      break;
    }

    case STATE_ANGRY: {
      display.fillRoundRect(30, 26, 24, 18, 4, SSD1306_WHITE);
      display.fillRoundRect(74, 26, 24, 18, 4, SSD1306_WHITE);
      display.fillTriangle(30, 26, 54, 26, 30, 34, SSD1306_BLACK);
      display.fillTriangle(74, 26, 98, 26, 98, 34, SSD1306_BLACK);

      unsigned long m = millis() / 80; 
      if (m % 3 == 0) {
        display.drawLine(lightningX, 0, lightningX - 4, 18, SSD1306_WHITE);
        display.drawLine(lightningX - 4, 18, lightningX + 4, 18, SSD1306_WHITE);
        display.drawLine(lightningX + 4, 18, lightningX - 2, 38, SSD1306_WHITE);
        display.drawLine(lightningX - 2, 38, lightningX + 6, 38, SSD1306_WHITE);
        display.drawLine(lightningX + 6, 38, lightningX, 63, SSD1306_WHITE);
      } else if (m % 3 == 1) {
        display.drawLine(lightningX - 2, 0, lightningX + 2, 16, SSD1306_WHITE);
        display.drawLine(lightningX + 2, 16, lightningX - 6, 22, SSD1306_WHITE);
        display.drawLine(lightningX - 6, 22, lightningX + 1, 42, SSD1306_WHITE);
        display.drawLine(lightningX + 1, 42, lightningX - 3, 63, SSD1306_WHITE);
      }
      break;
    }

    case STATE_SLEEP: {
      display.drawFastHLine(32, 35, 20, SSD1306_WHITE);
      display.drawFastHLine(76, 35, 20, SSD1306_WHITE);

      unsigned long m = millis();
      
      int y1 = 28 - ((m / 300) % 22);
      int x1 = 55 + ((m / 600) % 6) - 3;
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(x1, y1);
      display.print("z");

      int y2 = 24 - (((m + 400) / 300) % 24);
      int x2 = 65 + (((m + 400) / 500) % 8) - 4;
      display.setTextSize(2);
      display.setCursor(x2, y2);
      display.print("Z");
      break;
    }

    case STATE_HAPPY: {
      display.fillCircle(42 + eyeOffsetX, 34 + eyeOffsetY, 10, SSD1306_WHITE);
      display.fillRect(30 + eyeOffsetX, 36 + eyeOffsetY, 24, 10, SSD1306_BLACK);
      
      display.fillCircle(86 + eyeOffsetX, 34 + eyeOffsetY, 10, SSD1306_WHITE);
      display.fillRect(74 + eyeOffsetX, 36 + eyeOffsetY, 24, 10, SSD1306_BLACK);
      break;
    }

    case STATE_TEA: {
      display.fillRoundRect(32, 22, 20, 22, 5, SSD1306_WHITE);
      display.fillRoundRect(76, 22, 20, 22, 5, SSD1306_WHITE);

      cupOffsetX = (millis() / 250) % 6 - 3; 
      int cupX = 54 + cupOffsetX;

      display.fillRect(cupX, 48, 18, 14, SSD1306_WHITE);
      display.drawPixel(cupX - 2, 51, SSD1306_WHITE); 
      display.drawPixel(cupX - 2, 53, SSD1306_WHITE);
      
      display.drawLine(cupX + 5, 45, cupX + 3, 40, SSD1306_WHITE);
      display.drawLine(cupX + 12, 45, cupX + 14, 40, SSD1306_WHITE);
      break;
    }

    case STATE_PINGPONG: {
      int eyeLook = (pingPongDir > 0) ? 3 : -3;
      display.fillRoundRect(32 + eyeLook, 16, 18, 14, 4, SSD1306_WHITE);
      display.fillRoundRect(76 + eyeLook, 16, 18, 14, 4, SSD1306_WHITE);

      for (int y = 38; y < 60; y += 4) {
        display.drawPixel(63, y, SSD1306_WHITE);
        display.drawPixel(64, y, SSD1306_WHITE);
      }

      int paddleOffset = (pingPongPos - 64) / 6;
      display.fillRect(4, 42 + paddleOffset, 3, 12, SSD1306_WHITE);
      display.fillRect(121, 42 - paddleOffset, 3, 12, SSD1306_WHITE);

      display.fillCircle(pingPongPos, 45, 3, SSD1306_WHITE);

      pingPongPos += pingPongDir;
      if (pingPongPos > 116 || pingPongPos < 12) {
        pingPongDir = -pingPongDir; 
        tone(BUZZER_PIN, 2000, 20); 
      }
      delay(25); 
      break;
    }

    case STATE_READING: { 
      int readEyeOffsetX = ((millis() / 250) % 3) * 3 - 3; 

      display.fillRoundRect(32 + readEyeOffsetX, 26, 20, 16, 4, SSD1306_WHITE);
      display.fillRoundRect(76 + readEyeOffsetX, 26, 20, 16, 4, SSD1306_WHITE);

      int paperY = 44 + ((millis() / 300) % 2); 

      display.drawRect(36, paperY, 56, 18, SSD1306_WHITE);
      display.drawFastHLine(40, paperY + 4, 20, SSD1306_WHITE);
      display.drawFastHLine(64, paperY + 4, 24, SSD1306_WHITE);
      display.drawFastHLine(40, paperY + 9, 48, SSD1306_WHITE);
      display.drawFastHLine(40, paperY + 14, 32, SSD1306_WHITE);
      break;
    }

    default: 
      if (isBlinking) {
        display.drawFastHLine(34, 35, 20, SSD1306_WHITE);
        display.drawFastHLine(74, 35, 20, SSD1306_WHITE);
      } else {
        display.fillRoundRect(30 + eyeOffsetX, 20 + eyeOffsetY, 24, 30, 6, SSD1306_WHITE);
        display.fillRoundRect(74 + eyeOffsetX, 20 + eyeOffsetY, 24, 30, 6, SSD1306_WHITE);
      }
      break;
  }

  display.display();
}

void playHappySound() {
  tone(BUZZER_PIN, 1200, 60);
  delay(80);
  tone(BUZZER_PIN, 1800, 100);
}

void playWakeSound() {
  tone(BUZZER_PIN, 800, 50);
  delay(60);
  tone(BUZZER_PIN, 1400, 80);
}

void playSleepSound() {
  tone(BUZZER_PIN, 1000, 100);
  delay(120);
  tone(BUZZER_PIN, 600, 150);
}

void playHobbySound() {
  tone(BUZZER_PIN, 1500, 40);
  delay(50);
  tone(BUZZER_PIN, 1100, 40);
}

void playSnoreSound() {
  tone(BUZZER_PIN, 130, 180);
  delay(200);
  tone(BUZZER_PIN, 90, 250);
}

void playAngrySound() {
  tone(BUZZER_PIN, 300, 120);
  delay(140);
  tone(BUZZER_PIN, 200, 180);
}