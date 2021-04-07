#define LASER A0

void setup() {
  pinMode(LASER, OUTPUT);
}

void loop() {
  digitalWrite(LASER, HIGH);
  delay(500);
  digitalWrite(LASER, LOW);
  delay(500);
}
