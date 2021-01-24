// Pins.
#define PHOTORESISTOR A0

// Serial.
#define BAUD_RATE 9600

// Laser.
#define TRANSMISSION_INTERVAL 100 // In microseconds.
#define RECEIVE_OFFSET 10 // In microseconds.     // TODO: This might not even be necessary.
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

void loop() {
  if (analogRead(PHOTORESISTOR) > baseline) {      // TODO: Use a non-sleep-reliant method to transfer metadata about the connection before attempting the high-speed transfer.
    delayMicroseconds(RECEIVE_OFFSET); // TODO: This probably doesn't do anything because the arduino can't time travel. This probably makes it worse.
    while (true) {
      delayMicroseconds(TRANSMISSION_INTERVAL);
      if (syncCounter == SYNC_POINT) {
        syncCounter = 0;
        while (true) {
          if (analogRead(PHOTORESISTOR) > baseline) { break; }
        }
        delayMicroseconds(RECEIVE_OFFSET);    // TODO: This almost definitely doesn't do anything, you can probably remove the offsets because arduino is behind.
        continue;
      }
      syncCounter++;
      if (analogRead(PHOTORESISTOR) > baseline) {
        buffer[pos] |= 1 << charPos;            // TODO: See if it's possible to use HIGH here instead of one for more intent and such.
        incrementPos();                   // TODO: You can put this at the top of the loop so that you avoid writing it twice down here.
        continue;
      }
      buffer[pos] &= ~(1 << charPos);       // TODO: Maybe make the charPos shifts into some sort of function or something. Is this the most efficient way?
      incrementPos();
    }
  }
}
