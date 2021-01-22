// Pins.
#define PHOTORESISTOR A0

// Serial.
#define BAUD_RATE 9600

// Laser.
#define CLEARANCE 10
#define BUFFER_SIZE 1024

short baseline;

void setup() {
  // Initialize serial.
  Serial.begin(BAUD_RATE);

  // Pins are inputs by default, so no need for pinMode() here.

  // Set the baseline brightness to the environmental brightness plus the clearance.
  baseline = analogRead(PHOTORESISTOR) + CLEARANCE;
}

short pos = 0;
char charPos = 0;
char buffer[BUFFER_SIZE];

bool state = false;

void sendBuffer() {
  // Send the buffer to computer over serial.
  Serial.write(buffer, pos);
}

void incrementPos() {
  if (charPos == 7) {
      charPos = 0;
      pos++;
      if (pos == BUFFER_SIZE) {
        sendBuffer();
        pos = 0;
      }
    }
    charPos++;
}

void loop() {
  if (analogRead(PHOTORESISTOR) > baseline) {
    if (state) { return; }
    buffer[pos] |= HIGH << charPos;
    state = true;
    return;
  }
  if (state) {
    incrementPos();
    state = false;
  }
}
