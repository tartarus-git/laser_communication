// Pins.
#define LASER A0

// Laser.
#define DESC_BIT_DURATION 100
#define BUFFER_SIZE 1024
#define BIT_MASK 0x01
#define SERIAL_DELAY 1000

#define MICROSECONDS true
#define MILLISECONDS false

// Contains connection information which is used to set up the high-speed connection.
struct ConnectionDescriptor {
  uint32_t transmissionLength;                              // The length of the entire following transmission.
  int16_t syncInterval;                                     // The amount of bytes between each synchronization plus 1.
  uint16_t bitDuration;
  uint8_t durationType;                                     // True for microseconds, false for milliseconds.
} desc;

void transmitDescriptor() {
  for (int bytePos = 0; bytePos < sizeof(desc); bytePos++) {
    digitalWrite(LASER, HIGH);
    for (int bitPos = 0; bitPos < 8; bitPos++) {
      delay(DESC_BIT_DURATION);
      digitalWrite(LASER, (*((char*)&desc + bytePos) >> bitPos) & BIT_MASK);
    }
    delay(DESC_BIT_DURATION);
    digitalWrite(LASER, LOW);
    // Give the other device enough time to prepare for synchronization.
    delay(DESC_BIT_DURATION);
    delay(DESC_BIT_DURATION);
  }
}

void sleep() {
  if (desc.durationType == MICROSECONDS) {
    delayMicroseconds(desc.bitDuration);
    return;
  }
  delay(desc.bitDuration);        // TODO: This was just a work-around from the start anyway, can I use 1 number for the delay?
}

#define SLEEP sleep()

void sync() {
  digitalWrite(LASER, LOW);
  SLEEP;
  digitalWrite(LASER, HIGH);
  SLEEP;
}

int16_t syncCounter = -1;

void transmit(const char* data) {
  uint16_t packetLength;
  int ni = BUFFER_SIZE;
  for (int i = 0; i < desc.transmissionLength; i = ni; ni += BUFFER_SIZE) {
    if (ni > desc.transmissionLength) { packetLength = desc.transmissionLength - i; }
    else { packetLength = BUFFER_SIZE; }

    sync();

    // Send length of next packet.
    for (int bit = 0; bit < 16; bit++) {
      digitalWrite(LASER, (packetLength >> bit) & BIT_MASK);
      SLEEP;
    }

    SLEEP;                        // Sleep so that the other Arduino has enough time to start waiting for synchronization.
    sync();

    // Send the packet.
    for (int bytePos = 0; bytePs < packetLength; bytePos++) {
      // Synchronize if the time is right.
      if (syncCounter == desc.syncInterval) {
        syncCounter = 0;
        SLEEP;                    // Sleep to give the other Arduino enough time.
        sync();
      } else { syncCounter++; }

      // Send the next byte.
      for (int bitPos = 0; bitPos < 8; bitPos++) {
        digitalWrite(LASER, (data[i + bytePos] >> bitPos) & BIT_MASK);
        SLEEP;
      }

      // Reset sync counter;
      syncCounter = -1;

      // Give the relay enough time to send the captured data to the receiver over serial.
      delay(SERIAL_DELAY);
    }
  }
}

void setup() {
  pinMode(LASER, OUTPUT);

  // Set up the values for the connection descriptor.
  desc.syncInterval = 1;
  desc.bitDuration = 1;
  desc.durationType = MILLISECONDS;
}

void loop() {
  // TODO: Wait for input from I2C connection to raspi. Then set the transmission length and start sending. When done, repeat.
}
