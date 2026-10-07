#define LED_PIN 2

uint32_t last = 0;     // last LED maathina neram
bool state = false;    // LED ON-a OFF-a

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("BlackBox RTOS booting...");
}

void loop() {
  if (millis() - last >= 500) {   // 500 ms aachaa?
    last = millis();              // neram update
    state = !state;               // ON <-> OFF
    digitalWrite(LED_PIN, state);
    Serial.println(state ? "LED ON" : "LED OFF");
  }
}
