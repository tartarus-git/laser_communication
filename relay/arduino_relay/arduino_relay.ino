// Pins.
#define SOURCE A2
#define PHOTORESISTOR A0
#define CLEARANCE 100
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

void blocking_delay(int amount) {
  for (int i = 0; i < amount; i++) {
    delayMicroseconds(1000);
  }
}

#define DELAY(amount) blocking_delay(amount)

void sleep() {
  if (desc.durationType == MICROSECONDS) {
    delayMicroseconds(desc.bitDuration);
    return;
  }
  DELAY(desc.bitDuration);
}

#define SLEEP sleep()

void sync() {
  while (analogRead(PHOTORESISTOR) > baseline) { }
  while (analogRead(PHOTORESISTOR) <= baseline) { }
  SLEEP;
}

char buffer[BUFFER_SIZE];
uint32_t progress = 0;

int16_t syncCounter = -1;

void setup() {
  pinMode(STATUS, OUTPUT);

  pinMode(SOURCE, OUTPUT);
  digitalWrite(SOURCE, HIGH);
  pinMode(PHOTORESISTOR, INPUT);
  
  // Initialize serial.
  Serial.begin(BAUD_RATE);

  // Wait for initialization. This is because the serial-to-usb chip is async. This isn't necessary for serial pins.
  while (!Serial) { }

  //Serial.println("Hi there.");

  // Set the baseline brightness to the environmental brightness plus the clearance.
  baseline = analogRead(PHOTORESISTOR) + CLEARANCE;
}

void loop() {

  //Serial.println(baseline);

  noInterrupts();

  // Receive the connection descriptor.
  for (int i = 0; i < sizeof(desc); i++) {
    while (analogRead(PHOTORESISTOR) <= baseline) { }
    for (int j = 0; j < 8; j++) {
      DELAY(DESC_BIT_DURATION);
      if (analogRead(PHOTORESISTOR) > baseline) {
        *((char*)&desc + i) |= HIGH << j;
        continue;
      }
      *((char*)&desc + i) &= ~(HIGH << j);      // TODO: Is there any better way to do this?
    }
    // Give the other device enough time to set up a synchronization point.
    DELAY(DESC_BIT_DURATION);
    DELAY(DESC_BIT_DURATION);
  }

  interrupts();

  /*Serial.println("Connection descriptor received. Values:");
  Serial.println(desc.transmissionLength);
  Serial.println(desc.syncInterval);
  Serial.println(desc.bitDuration);
  Serial.println(desc.durationType);
  Serial.flush();*/

  noInterrupts();

  bool ledState = false;
  
  while (true) {
    sync();
    //Serial.println("Synced.");

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

    //interrupts();

    //Serial.println(length);
    //Serial.flush();


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

    // Receive the next byte.
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

    progress += length;
    if (progress == desc.transmissionLength) {
      break;
    }

    interrupts();

    // Output the buffer to serial.
    //Serial.println("Sending thing...");
    
    Serial.write(buffer, length);
    Serial.flush();

    noInterrupts();
  }
}
