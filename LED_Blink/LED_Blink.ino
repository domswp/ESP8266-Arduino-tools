// Blink LED built-in (GPIO2)
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);  // LED_BUILTIN = GPIO2
  Serial.begin(115200);
  Serial.println("ESP8266 berhasil terhubung!");
}

void loop() {
  digitalWrite(LED_BUILTIN, LOW);   // LED ON (active low!)
  Serial.println("LED ON");
  delay(1000);
  
  digitalWrite(LED_BUILTIN, HIGH);  // LED OFF
  Serial.println("LED OFF");
  delay(1000);
}
