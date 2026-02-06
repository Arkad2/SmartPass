#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN   10   // SDA на RC522
#define RST_PIN  9    // RST на RC522

// Светодиоды
#define LED_RED    5  // D5
#define LED_GREEN  6  // D6
#define LED_YELLOW 7  // D7

MFRC522 mfrc522(SS_PIN, RST_PIN);

bool lastIn = false;  // чередование IN/OUT
String serialBuffer;  // буфер для входящих строк от M5

void setLeds(bool r, bool g, bool y) {
  digitalWrite(LED_RED,    r ? HIGH : LOW);
  digitalWrite(LED_GREEN,  g ? HIGH : LOW);
  digitalWrite(LED_YELLOW, y ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);   // UART к M5StickC Plus2
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);

  // Режим ожидания карты — жёлтый
  setLeds(false, false, true);
}

void loop() {
  // 1. Обрабатываем входящие сообщения от M5 (статус)
  handleSerialFeedback();

  // 2. Читаем карту
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial())   return;

  // Собираем UID в HEX без пробелов
  String uidStr;
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase();

  // Чередуем IN / OUT
  String event = lastIn ? "OUT" : "IN";
  lastIn = !lastIn;

  // Формат строки для M5: CARDUID,EVENT
  Serial.print("CARD");
  Serial.print(uidStr);
  Serial.print(",");
  Serial.println(event);

  // После отправки оставляем жёлтый (ждём ответ от M5)
  setLeds(false, false, true);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(200);
}

// ===== Приём обратной связи от M5 =====
void handleSerialFeedback() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        processFeedbackLine(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}

void processFeedbackLine(const String &line) {
  // Ожидаемые варианты:
  // "OK 24FE055C IN"
  // "ERR CARD_NOT_FOUND 24FE055C"
  // "ERR 400 24FE055C"
  if (line.startsWith("OK")) {
    // Успешное событие -> зелёный на короткое время
    setLeds(false, true, false);
    delay(300);
    setLeds(false, false, true);  // обратно в жёлтый
  } else if (line.startsWith("ERR")) {
    // Любая ошибка -> красный
    setLeds(true, false, false);
    delay(500);
    setLeds(false, false, true);  // обратно в жёлтый
  }
}
