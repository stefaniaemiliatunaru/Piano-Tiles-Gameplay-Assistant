#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int buttonAsistent = 2;
const int buttonAutonom = 3;

const int ledPins[] = {7, 6, 5, 4};
const int sensorPins[] = {A0, A1, A2, A3};
const int impulsPins[] = {10, 11, 12, 13};
const int buzzerPin = 8;

const int tones[] = {262, 330, 392, 523};
const int thresholds[] = {300, 40, 30, 120};

enum GameState { MENU, ASISTENT, AUTONOM };
GameState currentState = MENU;

int currentScore = 0;
int highScore = 0;
int greseli = 0;
int greseliRamase = 5;

unsigned long lastMistakeTime = 0;
unsigned long lastDetectionTimes[4] = {0, 0, 0, 0};
unsigned long blackTileStartTimes[4] = {0, 0, 0, 0};
bool blackTileDetected[4] = {false, false, false, false};
unsigned long lastScoreUpdateTime = 0;
bool startedScoring = false;
bool waitingForFirstBlack = true;

unsigned long asistentStartTime = 0;
unsigned long lastTimeDisplayUpdate = 0;

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  pinMode(buttonAsistent, INPUT);
  pinMode(buttonAutonom, INPUT);

  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(sensorPins[i], INPUT);
    pinMode(impulsPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
    digitalWrite(impulsPins[i], HIGH);
  }

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  displayOpeningScreen();
  displayMenu();
}

void loop() {
  if (currentState == MENU) {
    if (digitalRead(buttonAsistent) == HIGH) {
      delay(100);
      if (digitalRead(buttonAsistent) == HIGH) {
        // trecere in modul asistent
        selectAsistent();
      }
    }
    if (digitalRead(buttonAutonom) == HIGH) {
      delay(100);
      if (digitalRead(buttonAutonom) == HIGH) {
        // trecere in modul autonom
        selectAutonom();
      }
    }
  } else if (currentState == ASISTENT) {
    // ruleaza logica modului asistent
    runAsistentMode();
    unsigned long now = millis();
    if (now - lastTimeDisplayUpdate >= 1000) {
      // actualizare display o data pe secunda
      lastTimeDisplayUpdate = now;
      if (currentState == ASISTENT) {
        // afiseaza greselile si timpul
        displayMistakesAndHighscore();  
      }
    }

  } else if (currentState == AUTONOM) {
    // ruleaza logica modului autonom
    runAutonomMode();  
  }
}

void displayOpeningScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  PIANO TILES");
  lcd.setCursor(0, 1);
  lcd.print("   ASSISTANT");
  // ecran de start cu delay
  delay(3000);  
}

void displayMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MOD 1: ASISTENT");
  lcd.setCursor(0, 1);
  lcd.print("MOD 2: AUTONOM");
  // oprire toate iesirile cand suntem in meniu
  stopAllOutputs();  
}

void selectAsistent() {
  currentState = ASISTENT;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mod selectat:");
  lcd.setCursor(0, 1);
  lcd.print("ASISTENT");
  delay(3000);

  greseli = 0;
  greseliRamase = 5;
  asistentStartTime = millis();
  lastTimeDisplayUpdate = 0;

  for (int i = 0; i < 4; i++) {
    // resetare detectii
    blackTileDetected[i] = false;   
    blackTileStartTimes[i] = 0;
  }

  displayMistakesAndHighscore();
}

void selectAutonom() {
  currentState = AUTONOM;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mod selectat:");
  lcd.setCursor(0, 1);
  lcd.print("AUTONOM");
  delay(3000);

  currentScore = 0;
  waitingForFirstBlack = true;
  startedScoring = false;

  displayScores();
}

void displayMistakesAndHighscore() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Incercari: ");
  lcd.print(greseliRamase);

  unsigned long seconds = (millis() - asistentStartTime) / 1000;
  lcd.setCursor(0, 1);
  lcd.print("Timp: ");
  lcd.print(seconds);
  lcd.print("s");
}

void displayScores() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scor: ");
  lcd.print(currentScore);
  lcd.setCursor(0, 1);
  lcd.print("Highscore: ");
  lcd.print(highScore);
}

void stopAllOutputs() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(ledPins[i], LOW);
    digitalWrite(impulsPins[i], HIGH);
  }
  noTone(buzzerPin);
  digitalWrite(buzzerPin, LOW);
}

void gameOver() {
  lcd.clear();
  // mesaj game over centrat
  lcd.setCursor(3, 0); 
  lcd.print("GAME OVER");

  if (currentState == ASISTENT) {
    unsigned long totalSeconds = (millis() - asistentStartTime) / 1000;
    lcd.setCursor(0, 1);
    lcd.print("Timp: ");
    lcd.print(totalSeconds);
    lcd.print("s");
  }

  delay(5000);

  stopAllOutputs();
  // intoarcere la meniu dupa game over
  currentState = MENU;  
  displayMenu();
  return;
}

void runAsistentMode() {
  for (int i = 0; i < 4; i++) {
    int val = analogRead(sensorPins[i]);
    // detectie tasta neagra
    if (val < thresholds[i]) {  
      if (!blackTileDetected[i]) {
        blackTileDetected[i] = true;
        blackTileStartTimes[i] = millis();
      }

      if (millis() - blackTileStartTimes[i] > 100) {
        // debounce / delay intre detectii
        if (millis() - lastDetectionTimes[i] > 1000) {  
        // creste numarul de greseli
          greseli++;        
          // scade numarul de incercari ramase
          greseliRamase--;  
          // aprinde led pe pozitia opusa
          digitalWrite(ledPins[3 - i], HIGH);  
          // emite sunet corespunzator
          tone(buzzerPin, tones[3 - i]);       
          delay(1000);
          digitalWrite(ledPins[3 - i], LOW);
          noTone(buzzerPin);
          // reseteaza detectia pe pozitia opusa
          lastDetectionTimes[i] = millis();
          blackTileDetected[3 - i] = false;  
          blackTileStartTimes[3 - i] = 0;
          delay(2000);
          // daca nu mai sunt incercari ramase, jocul se termina
          if (greseliRamase <= 0) {  
            gameOver();
            displayMenu();
            return;
          }
        }
      }
    // daca nu mai detecteaza tasta neagra
    } else { 
      if (blackTileDetected[i]) {
        blackTileDetected[i] = false;
        blackTileStartTimes[i] = 0;
      }
    }
  }
}

void runAutonomMode() {
  currentScore = 0;
  displayScores();
  delay(5000);

  int delay1 = 80;
  int delay2 = 75;
  int count = 0;

  waitingForFirstBlack = true;

  // asteapta prima apasare corecta
  while (waitingForFirstBlack) {  
    for (int i = 0; i < 4; i++) {
      if (analogRead(sensorPins[i]) < thresholds[i]) {
        waitingForFirstBlack = false;
        lastScoreUpdateTime = millis();
        break;
      }
    }
  }
  // jocul principal
  while (!waitingForFirstBlack) {  
    bool detected = false;

    for (int i = 0; i < 4; i++) {
      if (analogRead(sensorPins[i]) < thresholds[i]) {
        // delay intre apasari
        if (millis() - lastDetectionTimes[i] > 500) {  
          digitalWrite(ledPins[i], HIGH);
          digitalWrite(impulsPins[i], LOW);
          delay(delay2);
          digitalWrite(impulsPins[i], HIGH);
          digitalWrite(ledPins[i], LOW);
          delay(delay1);

          currentScore++;
          // actualizez highscore
          if (currentScore > highScore) {
            highScore = currentScore;  
          }

          lastDetectionTimes[i] = millis();
          lastScoreUpdateTime = millis();
          displayScores();

          count++;
          // creste viteza dupa 25 de puncte
          if (count == 25) {  
            delay1 = 70;
            delay2 = 75;
          }

          detected = true;
          break;
        }
      }
    }
    // daca nu se detecteaza nicio apasare dupa 3 sec, game over
    if (!detected && millis() - lastScoreUpdateTime > 3000) {
      gameOver();
      return;
    }
  }
}
