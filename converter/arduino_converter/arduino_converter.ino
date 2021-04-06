#include <Wire.h>                                           // Needed for I2C communication.

// Serial.
#define BAUD_RATE 115200

// I2C.
#define I2C_ADDRESS 3

// Pins.
#define LASER A0
#define STATUS 13

// Laser.
#define DESC_BIT_DURATION 100                               // The time delay used for transmitting the descriptor. Milliseconds.
#define BUFFER_SIZE 1024
#define BIT_MASK 0x01
#define SERIAL_DELAY 500

#define MICROSECONDS true
#define MILLISECONDS false

char buffer[BUFFER_SIZE];
int bufferPos = 0;
uint16_t transmissionPos = 0;

// Receive event callback for I2C, triggers through interrupt every time data comes in.
void I2CReceive(int amount) {
  Wire.readBytes(buffer + bufferPos, amount);
  bufferPos += amount;
  transmissionPos += amount;
}

// Put these inside of functions for future expandability. Not strictly necessary,
void resetBuffer() { bufferPos = 0; }
void resetTransmission() { transmissionPos = 0; }

// Request callback for I2C, responsible for telling the sender when it can send the next packet.
bool isReady = false;
void I2CRequest() {
  Wire.write((char*)&isReady, 1);                                  // Send the status flag to the master.
}

void setup() {
  // Debugging setup.
  pinMode(STATUS, OUTPUT);
  
  // Laser setup.
  pinMode(LASER, OUTPUT);

  // Serial setup.
  Serial.begin(BAUD_RATE);
  while (!Serial) { }                                       // Wait for serial to initialize.
  
  // I2C setup.
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(I2CReceive);                               // Register callback for receiving data.
  Wire.onRequest(I2CRequest);                               // Register callback for listening for data requests.
}

// Contains connection information which is used to set up the high-speed connection.
struct ConnectionDescriptor {
  uint32_t transmissionLength;                              // The length of the entire following transmission.
  int16_t syncInterval;                                     // The amount of bytes between each synchronization plus 1.
  uint16_t bitDuration;
  uint8_t durationType;                                     // True for microseconds, false for milliseconds.
} desc;

// Custom delay function which doesn't use the millis interrupt. This is for operation while interrupts are paused.
void blocking_delay(int amount) {
  for (int i = 0; i < amount; i++) {
    delayMicroseconds(1000);
  }
}

#define DELAY(amount) blocking_delay(amount)

// Macro to make digitalWrite have almost exactly the same duration as analogRead. Useful for communication with other device.
#define LASER_WRITE(state) digitalWrite(LASER, state); delayMicroseconds(98)

void transmitDescriptor() {
  for (int bytePos = 0; bytePos < sizeof(desc); bytePos++) {
    LASER_WRITE(HIGH);
    for (int bitPos = 0; bitPos < 8; bitPos++) {
      DELAY(DESC_BIT_DURATION);
      LASER_WRITE((*((char*)&desc + bytePos) >> bitPos) & BIT_MASK);
    }
    DELAY(DESC_BIT_DURATION);
    LASER_WRITE(LOW);
    // Give the other device enough time to prepare for synchronization.
    DELAY(DESC_BIT_DURATION);
    DELAY(DESC_BIT_DURATION);
  }
}

void sleep() {
  if (desc.durationType == MICROSECONDS) {
    delayMicroseconds(desc.bitDuration);
    return;
  }
  DELAY(desc.bitDuration);
}

#define SLEEP sleep()                                       // Define so that I can avoid function overhead if I want to in future.

void sync() {
  LASER_WRITE(LOW);
  SLEEP;
  LASER_WRITE(HIGH);
  SLEEP;
}

int16_t syncCounter = -1;

void transmit(const char* data, uint16_t amount) {
  sync();

  // Send length of next packet.
  for (int bitPos = 0; bitPos < 16; bitPos++) {
    LASER_WRITE((amount >> bitPos) & BIT_MASK);
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
      LASER_WRITE((data[bytePos] >> bitPos) & BIT_MASK);
      SLEEP;
    }
  }

  // Reset sync counter;
  syncCounter = -1;

  // Give the relay enough time to send the captured data to the receiver over serial.
  DELAY(SERIAL_DELAY);
}

void loop() {
  Serial.println("Awaiting transmission...");

  isReady = true;                                             // Activate data transfer.
  
  // Receive connection descriptor through I2C and relay it through laser.
  while (true) {
    Serial.flush();       // Why does there need to be something here so that everything works? Does constant looping interfere with interrupts or something?
    if (bufferPos == sizeof(desc)) {
      isReady = false;                                        // Prevent further data transfer until laser transfer is complete.
      desc = *(ConnectionDescriptor*)buffer;
      noInterrupts();
      transmitDescriptor();
      interrupts();
      resetBuffer();
      resetTransmission();
      break;
    }
  }

  Serial.println("Descriptor received. The following transmission is this many bytes long:");
  Serial.println(desc.transmissionLength);
  Serial.println("Progress:");

  isReady = true;                                             // Reactivate data transfer.

  // Receive transmission and relay it through the laser.
  while (true) {
    Serial.flush();
    if (bufferPos == BUFFER_SIZE) {
      isReady = false;
      noInterrupts();
      transmit(buffer, bufferPos);
      interrupts();
      Serial.println(transmissionPos);
      resetBuffer();
      isReady = true;
      continue;
    }
    if (transmissionPos == desc.transmissionLength) {
      isReady = false;
      noInterrupts();
      transmit(buffer, bufferPos);
      interrupts();
      resetBuffer();
      resetTransmission();
      break;
    }
  }

  Serial.println(desc.transmissionLength);
  Serial.println("Transmission is complete.");
}
