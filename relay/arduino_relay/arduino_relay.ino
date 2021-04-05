// Pins.
#define PHOTORESISTOR A0
#define CLEARANCE 10
#define STATUS 13                                   // Built-in status LED on the Arduino is on pin 13.

// Serial.
#define BAUD_RATE 115200
#define BUFFER_SIZE 1024

// Laser.
#define DESC_BIT_DURATION 100                       // In milliseconds.

#define MICROSECONDS true
#define MILLISECONDS false

short baseline;

struct ConnectionDescriptor {
  uint32_t transmissionLength;                      // The length of the entire following transmission.
  int16_t syncInterval;                             // The amount of bytes between each synchronization plus 1.
  uint16_t bitDuration;
  uint8_t durationType;                             // True for microseconds, false for milliseconds.
} desc;

void sleep() {
  if (desc.durationType == MICROSECONDS) {
    delayMicroseconds(desc.bitDuration);
    return;
  }
  delay(desc.bitDuration);        // TODO: This was just a work-around from the start anyway, can I use 1 number for the delay?
}

#define SLEEP sleep()

void sync() {
  while (analogRead(PHOTORESISTOR) > baseline) { }
  while (analogRead(PHOTORESISTOR) <= baseline) { }
  SLEEP;
}

int16_t syncCounter = -1;

void setup() {
  pinMode(STATUS, OUTPUT);

  pinMode(PHOTORESISTOR, INPUT);
  
  // Initialize serial.
  Serial.begin(BAUD_RATE);

  // Wait for initialization. This is because the serial-to-usb chip is async. This isn't necessary for serial pins.
  while (!Serial) { }

  // TODO: Make the connection descriptor a reoccuring send. This will avoid having to restart over and over all the time.

  // Set the baseline brightness to the environmental brightness plus the clearance.
  baseline = analogRead(PHOTORESISTOR) + CLEARANCE;

  // Receive the connection descriptor.
  for (int i = 0; i < sizeof(desc); i++) {
    //while (analogRead(PHOTORESISTOR) <= baseline) { }
    while (!digitalRead(PHOTORESISTOR)) { }
    //delay(1);
    for (int j = 0; j < 8; j++) {
      delay(DESC_BIT_DURATION);
      if (digitalRead(PHOTORESISTOR)) {
        *((char*)&desc + i) |= HIGH << j;
        continue;
      }
      *((char*)&desc + i) &= ~(HIGH << j);      // TODO: Is there any better way to do this?
    }
    // Give the other device enough time to set up a synchronization point.
    delay(DESC_BIT_DURATION);
    delay(DESC_BIT_DURATION);
  }

  Serial.println("Connection descriptor received. Values:");
  Serial.println(desc.transmissionLength);
  Serial.println(desc.syncInterval);
  Serial.println(desc.bitDuration);
  Serial.println(desc.durationType);

  char buffer[BUFFER_SIZE];

  bool ledState = false;
  
  while (true) {
    sync();

    // Receive the length of the upcoming data transmission.
    uint16_t length;
    for (int i = 0; i < 16; i++) {
      if (analogRead(PHOTORESISTOR) > baseline) {
        length |= HIGH << i;
        SLEEP;
        continue;
      }
      length &= ~(HIGH << i);
      SLEEP;
    }

    // Invert state of the status LED to show that the correct length was retrieved. This is just for debugging.
    if (length == BUFFER_SIZE) {
      digitalWrite(13, ledState = !ledState);
    }

    // Synchronize again before attempting to receive data.
    sync();

    // Receive data.
    uint16_t lastIndex = length - 1;
    for (int i = 0; i < lastIndex; i++) {
      // Synchronize if the time is right.
      if (syncCounter == desc.syncInterval) {
        syncCounter = 0;
        sync();
      } else { syncCounter++; }

      // Receive the next byte.
      for (int j = 0; j < 8; j++) {
        if (analogRead(PHOTORESISTOR) > baseline) {
          buffer[i] |= HIGH << j;
          SLEEP;
          continue;
        }
        buffer[i] &= ~(HIGH << j);
        SLEEP;
      }
    }
    // Last round extra to avoid unnecessary SLEEP at the end.
    // Synchronize if the time is right.
    if (syncCounter == desc.syncInterval) {
      syncCounter = 0;
      sync();
    } else { syncCounter++; }

    // Reeive the next byte.
    for (int j = 0; j < 7; j++) {
      if (analogRead(PHOTORESISTOR) > baseline) {
        buffer[length] |= HIGH << j;
        SLEEP;
        continue;
      }
      buffer[length] &= ~(HIGH << j);
      SLEEP;
    }
    if (analogRead(PHOTORESISTOR) > baseline) { buffer[length] |= HIGH << 7; }
    else { buffer[length] &= ~(HIGH << 7); }

    // Reset syncCounter.
    syncCounter = -1;

    // Output the buffer to serial.
    Serial.write(buffer, length);
    Serial.flush();     // TODO: Do I actually need this here. Figure this stuff out.
  }
}

void loop() { }
