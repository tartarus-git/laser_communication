// Pins.
#define PHOTORESISTOR A0

// Serial.
#define BAUD_RATE 9600

// Laser.
#define TRANSMISSION_INTERVAL 100 // In microseconds.
#define SYNC_POINT 100

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

short syncCounter = 0;

short pos = 0;
char charPos = 0;
char buffer[BUFFER_SIZE];

void sendBuffer() {
  // Send the buffer to computer over serial.
  Serial.write(buffer, pos);
}

void incrementPos() {   // TODO: You have to calculate the length of the transmission somewhere, because or else you're not going to know when to send the buffer.
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

bool isSyncing;
void resync() {
  isSyncing = false;
}

void loop() {
  if (analogRead(PHOTORESISTOR)) {      // TODO: Use a non-sleep-reliant method to transfer metadata about the connection before attempting the high-speed transfer.
    while (true) {
      delayMicroseconds(TRANSMISSION_INTERVAL);
      if (analogRead(PHOTORESISTOR)) {
        buffer[pos] |= HIGH << charPos;
        incrementPos();
        continue;
      }
      incrementPos();
      if (syncCounter == SYNC_POINT) {
        syncCounter = 0;
        isSyncing = true;
        attachInterrupt(digitalPinToInterrupt(PHOTORESISTOR), resync, RISING);
        while (isSyncing) { }
        delayMicroseconds(TRANSMISSION_INTERVAL);       // TODO: Add some offset define so that this isn't too close to the edge of a value.
                                                        // Of course, only if it's a problem.
        continue;
      }
      syncCounter++;
    }
  }
}
