#include <HomeSpan.h>

// Пины
#define RELAY1_PIN     22
#define RELAY2_PIN     18
#define BUTTON1_PIN    16
#define BUTTON2_PIN     5
#define DOOR_PIN       17

#define RELAY_TIMEOUT    (30UL * 60 * 1000)
#define DEBOUNCE_DELAY   500

// ─────────────────────────────────────────────────
// Relay с возможностью инверсии

struct Relay : Service::Switch {
  SpanCharacteristic *power;
  int pin;
  bool invert;
  bool isOn = false;
  unsigned long startTime = 0;

  Relay(int pin, bool invert=false) : Service::Switch() {
    this->pin = pin;
    this->invert = invert;

    power = new Characteristic::On(0, true);

    // Если invert==true, выключаем реле LOW, иначе HIGH
    digitalWrite(pin, invert ? LOW : HIGH);
    delay(10);
    pinMode(pin, OUTPUT);
  }

  boolean update() override {
    bool val = power->getNewVal();
    if(invert) val = !val;
    digitalWrite(pin, val ? HIGH : LOW);
    if(val && !isOn) {
      isOn = true;
      startTime = millis();
    }
    if(!val) isOn = false;
    return true;
  }

  void loop() override {
    if(isOn && millis() - startTime >= RELAY_TIMEOUT) {
      power->setVal(false);
      update();
    }
  }

  void toggle() {
    power->setVal(!power->getVal());
    update();
  }
};

Relay *relay1;
Relay *relay2;

// ─────────────────────────────────────────────────
// Door sensor

struct DoorSensor : Service::ContactSensor {
  SpanCharacteristic *state;
  int last = HIGH;

  DoorSensor() : Service::ContactSensor() {
    pinMode(DOOR_PIN, INPUT_PULLUP);
    state = new Characteristic::ContactSensorState(1);
    last = digitalRead(DOOR_PIN);
  }

  void loop() override {
    int cur = digitalRead(DOOR_PIN);
    if(cur != last) {
      state->setVal(cur == LOW ? 0 : 1);
      last = cur;
    }
  }
};

// ─────────────────────────────────────────────────
// Кнопки с debounce

void checkButtons() {
  static bool l1 = HIGH, l2 = HIGH;
  static unsigned long t1 = 0, t2 = 0;
  unsigned long now = millis();

  bool b1 = digitalRead(BUTTON1_PIN);
  bool b2 = digitalRead(BUTTON2_PIN);

  if(b1 != l1 && now - t1 > DEBOUNCE_DELAY) {
    t1 = now; l1 = b1;
    if(b1 == LOW) relay1->toggle();
  }
  if(b2 != l2 && now - t2 > DEBOUNCE_DELAY) {
    t2 = now; l2 = b2;
    if(b2 == LOW) relay2->toggle();
  }
}

// ─────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Гарантируем OFF состояние до pinMode
  digitalWrite(RELAY1_PIN, LOW);
  pinMode(RELAY1_PIN, OUTPUT);
  digitalWrite(RELAY2_PIN, LOW);
  pinMode(RELAY2_PIN, OUTPUT);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

  homeSpan.setWifiCredentials("AirPort", "QWEqwe1234567222");
  homeSpan.begin(Category::Bridges, "ESP32 Inverted Relays");

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Name("Relay 1");
    relay1 = new Relay(RELAY1_PIN, true);

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Name("Relay 2");
    relay2 = new Relay(RELAY2_PIN, true);

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Name("Door Sensor");
    new DoorSensor();
}

void loop() {
  checkButtons();
  homeSpan.poll();
}
