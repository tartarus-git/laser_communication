#include <Wire.h>                                           // Needed for I2C communication.

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

void transmit(const char* data, int amount) {
  sync();

  // Send length of next packet.
  for (int bit = 0; bit < 16; bit++) {
    digitalWrite(LASER, (packetLength >> bit) & BIT_MASK);
    SLEEP;
  }

  SLEEP;
  sync();

  // Send the packet.
  for (int bytePos = 0; bytePos < amount; bytePos++) {
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

char buffer[BUFFER_SIZE];

int transmissionPos = 0;
int pos = 0;
bool recDesc = true;
void I2CReceive(int amount) {
  if (recDesc) {
receiveDescriptor:
    while (Wire.available()) {
      if (pos == sizeof(desc) - 1) {
        transmitDescriptor();                           // Send descriptor over laser.
        
        pos = 0;                                        // Prepare for receiving the transmission.
        recTransmission = false;
        goto receiveTransmission;                       // Go receive the transmission.
      }
      ((char*)desc)[pos] = Wire.read();
      pos++;
    }
    return;
  }
  
receiveTransmission:
  while (Wire.available()) {
    buffer[pos] = Wire.read();
    pos++;
    transmissionPos++;
    if (transmissionPos == desc.transmissionLength) {
      transmit(buffer, pos);                                    // Transmit last, potentially incomplete packet.
      
      pos = 0;                                                  // Prepare for receiving descriptor.
      transmissionPos = 0;
      recDesc = true;
      goto receiveDescriptor;
    }
    if (pos == BUFFER_SIZE) {
      transmit(buffer, pos);                                    // Send packet over laser.
      pos = 0;                                                  // Prepare for the next packet over I2C.
    }
  }
}

void setup() {
  // I2C setup.
  Wire.begin(0);
  Wire.onReceive(I2CReceive);

  // Laser setup.
  pinMode(LASER, OUTPUT);
}

void loop() { }
