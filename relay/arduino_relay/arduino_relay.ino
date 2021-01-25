// Pins.
#define PHOTORESISTOR A0

// Serial.
#define BAUD_RATE 9600

// Laser.
#define TRANSMISSION_INTERVAL 1000 // In milliseconds. TODO: Change this to microseconds.
#define RECEIVE_OFFSET 10 // In microseconds.     // TODO: This might not even be necessary.
#define SYNC_POINT 100

#define CLEARANCE 10
#define BUFFER_SIZE 8     // TODO: Change this to a larger buffer size and such.

#define SLEEP delay(TRANSMISSION_INTERVAL);

short baseline;

void setup() {
  // Initialize serial.
  Serial.begin(BAUD_RATE);

  while (!Serial) { }         // Wait for initialization. This is because the serial-to-usb chip is async. This isn't necessary for serial pins.

  // Pins are inputs by default, so no need for pinMode() here.

  pinMode(13, OUTPUT); // Except this one, which we are using for debugging purposes. TODO.

  // Set the baseline brightness to the environmental brightness plus the clearance.
  baseline = analogRead(PHOTORESISTOR) + CLEARANCE;
}

short syncCounter = 0;

short pos = 0;
char charPos = 0;
char buffer[BUFFER_SIZE];

void sendBuffer() {
  // Send the buffer to computer over serial.
  Serial.write(buffer, pos + 1);      // TODO: If this is the only thing in here, then there isn't really a need for a function is there.
  Serial.flush();
}

bool ledState = LOW;

void incrementPos() {   // TODO: You have to calculate the length of the transmission somewhere, because or else you're not going to know when to send the buffer.
  if (charPos == 7) {
      ledState = !ledState;
      digitalWrite(13, ledState);
      charPos = 0;
      if (pos == BUFFER_SIZE - 1) {
        sendBuffer();
        pos = 0;
        return;
      }
      pos++;
      return;
  }
  charPos++;
}

void loop() {
  if (analogRead(PHOTORESISTOR) > baseline) {      // TODO: Use a non-sleep-reliant method to transfer metadata about the connection before attempting the high-speed transfer.
    delayMicroseconds(RECEIVE_OFFSET); // TODO: This probably doesn't do anything because the arduino can't time travel. This probably makes it worse.
    while (true) {
      SLEEP;
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
        incrementPos();                   // TODO: You can put this at the top of the loop so that you avoid writing it twice down here. Not without some changing around of other stuff though. See if you can make this work.
        continue;
      }
      buffer[pos] &= ~(1 << charPos);       // TODO: Maybe make the charPos shifts into some sort of function or something. Is this the most efficient way?
      incrementPos();
    }
  }
}
