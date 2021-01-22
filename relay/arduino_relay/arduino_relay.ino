// Pins.
#define PHOTORESISTOR 0 // TODO: Find the pin for this. Apparently you can use like A0 and such. Figure out how that works in the context of C++ language spec.
                        // Almost definitely just a macro. Like #define something.

// Serial.
#define BAUD_RATE 9600

// Laser.
#define CLEARANCE 10
#define BUFFER_SIZE 1024

const short baseline; // TODO: See if this const somehow optimizes the greater than baseline + CLEARANCE thing.

void setup() {
  // Initialize serial.
  Serial.begin(BAUD_RATE);

  // Get the baseline environmental brightness.
  baseline = analogRead(PHOTORESISTOR);
}

short pos = 0;
char charPos = 0;
char buffer[BUFFER_SIZE];

bool state = false;

void sendBuffer() {
  // Send the buffer to computer over serial.
  // TODO: Finish this.
}

void incrementPos() {
  if (charPos == 7) {
      charPos = 0;
      if (pos == BUFFER_SIZE) {
        sendBuffer(); // TODO: Make this support non-full buffers as edge case ending things. Use the int as param I think.
        pos = 0;
        continue;
      }
      pos++;
      continue;
    }
    charPos++;
}

void loop() {
  if (analogRead(PHOTORESISTOR) > baseline + CLEARANCE) {
    if (state) { continue; }
    buffer[pos] |= HIGH << charPos;
  } else {
    if (state) {
      incrementPosition();
    }
  }
}
