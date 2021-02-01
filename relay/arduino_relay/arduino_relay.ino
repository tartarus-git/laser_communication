// Pins.
#define PHOTORESISTOR A0
#define CLEARANCE 10

// Serial.
#define BAUD_RATE 9600
#define BUFFER_SIZE 1024

// Laser.
short baseline;

#define DESC_BIT_DURATION 100    // milliseconds.

struct ConnectionDescriptor {
  int16_t syncInterval;         // The amount of bytes between each synchronization plus 1.
  uint16_t bitDuration;
  uint8_t durationType;
} desc;

void sleep() {
  if (desc.durationType) {
    delayMicroseconds(desc.bitDuration);
    return;
  }
  delay(desc.bitDuration);
}

int16_t syncCounter = -1;

void sync() {
  while (analogRead(PHOTORESISTOR) > baseline) { }
  while (analogRead(PHOTORESISTOR) <= baseline) { }
  sleep();
}

void setup() {
  // Initialize serial.
  Serial.begin(BAUD_RATE);

  // Wait for initialization. This is because the serial-to-usb chip is async. This isn't necessary for serial pins.
  while (!Serial) { }

  // Set the baseline brightness to the environmental brightness plus the clearance.
  baseline = analogRead(PHOTORESISTOR) + CLEARANCE;

  Serial.println("Waiting to receive connection descriptor.");
  Serial.flush();

  // Receive the connection descriptor.
  for (int i = 0; i < sizeof(desc); i++) {
    while (analogRead(PHOTORESISTOR) <= baseline) { }
    for (int j = 0; j < 8; j++) {
      delay(DESC_BIT_DURATION);
      if (analogRead(PHOTORESISTOR) > baseline) {
        *((char*)&desc + i) |= HIGH << j;
        continue;
      }
      *((char*)&desc + i) &= ~(HIGH << j);      // TODO: Is there any better way to do this?
    }
    // Give the other device enough time to set up a synchronization point.
    delay(DESC_BIT_DURATION);
    delay(DESC_BIT_DURATION);
  }

  char buffer[BUFFER_SIZE];

  while (true) {
    Serial.println("Synchronizing...");
    
    sync();

    // Receive the length of the upcoming data transmission.
    uint16_t length;      // TODO: No sync here right? It only is possible once actually sending data right?
    for (uint16_t i = 0; i < 16; i++) {
      if (analogRead(PHOTORESISTOR) > baseline) {
        length |= (uint16_t)HIGH << i;
        sleep();
        continue;
      }
      length &= ~((uint16_t)HIGH << i);
      sleep();
    }

    Serial.println("Length received:");
    Serial.println(length);

    Serial.println("Receiving data.");

    // Receive the data.
    for (int i = 0; i < length; i++) {
      // Synchronize if the time is right.
      if (syncCounter == desc.syncInterval) {
        sync();
        syncCounter = 0;
      } else { syncCounter++; }

      // Receive the next bit.
      for (int j = 0; j < 8; j++) {
        if (analogRead(PHOTORESISTOR) > baseline) {
          buffer[i] |= HIGH << j;
          sleep();
          continue;
        }
        buffer[i] &= ~(HIGH << j);
        sleep();
      }
    }

    // Output the buffer to serial.
    //Serial.write(buffer, length);    TODO: Disabled for debugging purposes. Enable later.
  }
}

void loop() { }
