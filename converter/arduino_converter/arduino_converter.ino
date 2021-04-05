#include <Wire.h>                                           // Needed for I2C communication.

// Serial.
#define BAUD_RATE 115200

// I2C.
#define I2C_ADDRESS 3

// Pins.
#define LASER A0
#define STATUS 13

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
  Serial.write(data, amount);
  Serial.flush();       // TODO: This is temporary. Remove this serial stuff.
  
  sync();

  // Send length of next packet.
  for (int bitPos = 0; bitPos < 16; bitPos++) {
    digitalWrite(LASER, (amount >> bitPos) & BIT_MASK);
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
      digitalWrite(LASER, (data[bytePos] >> bitPos) & BIT_MASK);
      SLEEP;
    }

    // Reset sync counter;
    syncCounter = -1;

    // Give the relay enough time to send the captured data to the receiver over serial.
    delay(SERIAL_DELAY);
  }
}

char buffer[BUFFER_SIZE];
int bufferPos = 0;
uint16_t transmissionPos = 0;
bool thingthing = false;

void I2CReceive(int amount) {
  // buffer size inside wire is 32, make sure to incorporate that into this.
  digitalWrite(STATUS, thingthing = !thingthing);
  Wire.readBytes(buffer + bufferPos, amount);
  bufferPos += amount;
  transmissionPos += amount;
}

void resetBuffer() {
  bufferPos = 0;
}

void resetTransmission() {
  transmissionPos = 0;
}

void setup() {
  // Debugging setup.
  pinMode(STATUS, OUTPUT);
  
  // Laser setup.
  pinMode(LASER, OUTPUT);

  // Serial setup.
  Serial.begin(BAUD_RATE);
  while (!Serial) { }                                           // Wait for serial to initialize.
  
  // I2C setup.
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(I2CReceive);
}

void loop() {
  while (true) {
    Serial.flush();       // Why does there need to be something here so that everything works? Does constant looping interfere with interrupts or something?
    if (bufferPos == sizeof(desc)) {
      desc = *(ConnectionDescriptor*)buffer;
      //transmitDescriptor();
      //digitalWrite(STATUS, HIGH);
      resetBuffer();
      resetTransmission();                // TODO: You should be able to remove this and make it a little bit more efficient, but you're going to need to rewrite some stuff.
      break;
    }
    /*digitalWrite(STATUS, HIGH);
    Serial.println("Thing.");
    if (pos == sizeof(desc) - 1) {
      ((char*)&desc)[pos] = Wire.read();    // Receive the last byte.
      //transmitDescriptor();                                     // Send descriptor over laser.

      pos = 0;                                                  // Prepare for receiving the transmission.
      recDesc = false;
      Serial.println("Received and sent transmission descriptor.");
      Serial.flush();                       // TODO: Is this necessary after println?
      break;                                                    // Go receive the transmission.
    }
receiveDescriptor:
    ((char*)&desc)[pos] = Wire.read();
    pos++;*/
  }

  Serial.println(desc.transmissionLength);
  bool thing = false;
  while (true) {
    Serial.flush();
    //Serial.println(bufferPos);
    if (bufferPos == BUFFER_SIZE) {
      //digitalWrite(STATUS, LOW);
      //digitalWrite(STATUS, thing = !thing);
      //transmit(buffer, bufferPos);
      Serial.println(transmissionPos);
      resetBuffer();
      continue;
    }
    if (transmissionPos == desc.transmissionLength) {
      //transmit(buffer, bufferPos);
      resetBuffer();
      resetTransmission();
      break;
    }
    
    /*if (!Wire.available()) { continue; }
    buffer[pos] = Wire.read();
    pos++;
    transmissionPos++;
    if (transmissionPos == desc.transmissionLength) {
      transmit(buffer, pos);                                    // Transmit last, potentially incomplete packet.
      
      pos = 0;                                                  // Prepare for receiving descriptor.
      transmissionPos = 0;
      recDesc = true;
      Serial.println("Received and sent entire transmission.");
      Serial.flush();
      goto receiveDescriptor;
    }
    // TODO: You might actually have to program in waits so that the laser can transmit everything in time.
    if (pos == BUFFER_SIZE) {
      transmit(buffer, pos);                                    // Send packet over laser.
      pos = 0;                                                  // Prepare for the next packet over I2C.
    }*/
  }

  Serial.println("Transmission is complete.");
}
