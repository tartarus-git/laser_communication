#define PHOTORESISTOR A0
#define SOURCE A2

void setup() {
  pinMode(SOURCE, OUTPUT);
  digitalWrite(SOURCE, HIGH);
  pinMode(PHOTORESISTOR, INPUT);

  Serial.begin(115200);
  while (!Serial) { }
}

void loop() {
  Serial.println(analogRead(PHOTORESISTOR));
  delay(100);
}
