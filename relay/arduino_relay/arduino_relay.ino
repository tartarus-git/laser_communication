// Pins.
#define PHOTORESISTOR A0
#define CLEARANCE 10

// Serial.
#define BAUD_RATE 115200
#define BUFFER_SIZE 1024

// Laser.
short baseline;

#define DESC_BIT_DURATION 100    // milliseconds.      // TODO: Reduce the duration and try to maintain a stable communication.

struct ConnectionDescriptor {
  int16_t syncInterval;         // The amount of bytes between each synchronization plus 1.
  uint16_t bitDuration;
  uint8_t durationType;
} desc;

void sleep() {
  if (desc.durationType) {
    delayMicroseconds(desc.bitDuration - 100);      // This is to account for analogReads, which take 100 microseconds.
                                                    // Obviously this means you can't use this for anything else except analogReads.
                                                    // BTW: You can totally make analogRead faster, so don't worry about this too much.
    return;
  }
  delay(desc.bitDuration);
}

int16_t syncCounter = -1;

void sync() {
  while (analogRead(PHOTORESISTOR) > baseline) { }
  while (analogRead(PHOTORESISTOR) <= baseline) { }
  sleep();
  //delayMicroseconds(desc.bitDuration / 2);
}

void setup() {
  pinMode(13, OUTPUT);
  
  // Initialize serial.
  Serial.begin(BAUD_RATE);

  // Wait for initialization. This is because the serial-to-usb chip is async. This isn't necessary for serial pins.
  while (!Serial) { }

  // Set the baseline brightness to the environmental brightness plus the clearance.
  baseline = analogRead(PHOTORESISTOR) + CLEARANCE;

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

  bool ledState = false;
  
  while (true) {
    sync();

    // Receive the length of the upcoming data transmission.
    uint16_t length;      // TODO: No sync here right? It only is possible once actually sending data right?
    for (uint16_t i = 0; i < 16; i++) {
      if (analogRead(PHOTORESISTOR) > baseline) {
        length |= HIGH << i;
        sleep();
        continue;
      }
      length &= ~(HIGH << i);
      sleep();
    }

    //Serial.println(length);
    //Serial.flush();
    if (length == BUFFER_SIZE) {
      digitalWrite(13, ledState = !ledState);
    }

    // Temp:
    sync();

    // Receive the data.
    for (int i = 0; i < length; i++) {
      // Synchronize if the time is right.
      if (syncCounter == desc.syncInterval) {
        syncCounter = 0;
        sync();
      } else { syncCounter++; }

      // Receive the next bit.
      for (int j = 0; j < 8; j++) {
        if (analogRead(PHOTORESISTOR) > baseline) {
          buffer[i] |= HIGH << j;
          sleep();
          continue;
        }
        buffer[i] &= ~(HIGH << j);
        sleep();          // TODO: See if you can make this not happen on the last loop to save time for serial stuff.
      }
    }

    // Reset syncCounter.
    syncCounter = -1;

    // TODO: Eventually you're gonna need to put in some extra time at the end of each packet for the serial sending stuff.

    // Output the buffer to serial.
    Serial.write(buffer, length);
    Serial.flush();     // TODO: Do I actually need this here. Figure this stuff out.
  }
}

void loop() { }
