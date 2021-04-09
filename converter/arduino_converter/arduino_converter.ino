#include <Wire.h>                                           // Needed for I2C communication.

// Serial.
#define BAUD_RATE 115200

// Laser card protocol.
#define CARD_0 12
#define CARD_1 11
#define CARD_2 10
#define CARD_3 9
#define CARD_4 8
#define CARD_5 7
#define CARD_6 6
#define CARD_7 5

#define CARD_TRIGGER 3                                      // Has to be either 2 or 3 because we're using it for interrupts.

#define CARD_FREE_FLAG A3

// Pins.
#define RESET A2                                            // Raspberry Pi sets this HIGH when this card should reset.
#define RESET_WAIT 100                                      // In milliseconds.
#define LASER A0
#define LED A5
#define STATUS 13

// Laser.
#define DESC_BIT_DURATION 100                               // The time delay used for transmitting the descriptor. Milliseconds.
#define BUFFER_SIZE 1024                                    // This needs to be set across all devices by hand. No system in place.
#define BIT_MASK 0x01
#define SERIAL_DELAY 500

#define MICROSECONDS true
#define MILLISECONDS false

// Function to debug parts of the code where interrupts are disabled.
bool statusPrevState = false;
void toggleStatus() {
  digitalWrite(STATUS, statusPrevState = !statusPrevState);
}

// Buffer for receiving laser card protocol and sending through laser.
volatile char buffer[BUFFER_SIZE];                          // Volatile tells compiler not to cache values because variable could change outside of compilers scope.
volatile int bufferPos = 0;
volatile uint16_t transmissionPos = 0;

// Interrupt triggers when data is dumped from master. Reads all the data out and increments counters.
void laserCardProtocolDumpInterrupt() {
  if (digitalRead(CARD_0)) { buffer[bufferPos] |= HIGH; } else { buffer[bufferPos] &= 0b11111110; }
  if (digitalRead(CARD_1)) { buffer[bufferPos] |= HIGH << 1; } else { buffer[bufferPos] &= 0b11111101; }
  if (digitalRead(CARD_2)) { buffer[bufferPos] |= HIGH << 2; } else { buffer[bufferPos] &= 0b11111011; }
  if (digitalRead(CARD_3)) { buffer[bufferPos] |= HIGH << 3; } else { buffer[bufferPos] &= 0b11110111; }
  if (digitalRead(CARD_4)) { buffer[bufferPos] |= HIGH << 4; } else { buffer[bufferPos] &= 0b11101111; }
  if (digitalRead(CARD_5)) { buffer[bufferPos] |= HIGH << 5; } else { buffer[bufferPos] &= 0b11011111; }
  if (digitalRead(CARD_6)) { buffer[bufferPos] |= HIGH << 6; } else { buffer[bufferPos] &= 0b10111111; }
  if (digitalRead(CARD_7)) { buffer[bufferPos] |= HIGH << 7; } else { buffer[bufferPos] &= 0b01111111; }
  // TODO: Maybe do direct digital reads here through that other way, the pins might already be packaged inside of a number there.
  // That would mean you would have to do a lot less here.
  bufferPos++;
  transmissionPos++;
}

// Reset functions for future additions if I want it.
void resetBuffer() { bufferPos = 0; }
void resetTransmission() { transmissionPos = 0; }

void setup() {
  // Debugging setup.
  pinMode(STATUS, OUTPUT);
  
  // Laser setup.
  pinMode(LASER, OUTPUT);

  // LED setup.
  pinMode(LED, OUTPUT);

  // Reset pin setup.
  pinMode(RESET, INPUT);

  // Serial setup.
  Serial.begin(BAUD_RATE);
  while (!Serial) { }                                       // Wait for serial to initialize.
  
  // Laser card protocol setup.
  pinMode(CARD_0, INPUT);
  pinMode(CARD_1, INPUT);
  pinMode(CARD_2, INPUT);
  pinMode(CARD_3, INPUT);
  pinMode(CARD_4, INPUT);
  pinMode(CARD_5, INPUT);
  pinMode(CARD_6, INPUT);
  pinMode(CARD_7, INPUT);
  pinMode(CARD_TRIGGER, INPUT);
  pinMode(CARD_FREE_FLAG, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(CARD_TRIGGER), laserCardProtocolDumpInterrupt, RISING);
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

// DELAY macro for easy redefinition across program.
#define DELAY(amount) blocking_delay(amount)

// Macro to make digitalWrite have almost exactly the same duration as analogRead. Useful for communication with other device.
#define LASER_WRITE(state) digitalWrite(LASER, state); digitalWrite(LED, state); delayMicroseconds(96)

// Transmit the connection descriptor through the laser using the standard description bit duration.
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

// Sleep function which selects the correct delay function based on the connection descriptor.
void sleep() {
  if (desc.durationType == MICROSECONDS) {
    delayMicroseconds(desc.bitDuration);
    return;
  }
  DELAY(desc.bitDuration);
}

#define SLEEP sleep()                                       // Macro so that I can avoid function overhead if I want to in future.

// Send out a synchronization pulse through the laser that helps the other device stay in time with this device.
// This is necessary because laser communication is purely one-directional.
void sync() {
  LASER_WRITE(LOW);
  SLEEP;
  LASER_WRITE(HIGH);
  SLEEP;
}

// Counter to keep track of when to synchronize.
int16_t syncCounter = -1;

// Transmit a packet of data across the laser using connection metadata from the connection descriptor.
void transmit(const volatile char* data, uint16_t amount) {
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

// Check if the reset pin has been pushed. If yes, reset everything and prepare for next transmission.
bool pollReset() {
  if (digitalRead(RESET)) {
    resetBuffer();
    resetTransmission();
    digitalWrite(CARD_FREE_FLAG, LOW);
    delay(RESET_WAIT);
    Serial.println("Reset triggered, prepared for new transmission.");

    // Blink light so that the status is visible to someone without having to look at serial.
    digitalWrite(LED, LOW);
    delay(500);
    digitalWrite(LED, HIGH);
    delay(2000);
    digitalWrite(LED, LOW);
    
    return true;
  }
  return false;
}

void loop() {
  LASER_WRITE(LOW);
  
  Serial.println("Awaiting transmission...");
  Serial.flush();

  //isReady = true;                                             // Activate data transfer.
  digitalWrite(CARD_FREE_FLAG, HIGH);
  
  // Receive connection descriptor through laser card protocol and relay it through laser.
  while (true) {
    delay(100);
    if (pollReset()) { return; }                                // Because returning starts the loop() function again.
    if (bufferPos == sizeof(desc)) {
      digitalWrite(CARD_FREE_FLAG, LOW);                        // Prevent further data from being transmitted from master while busy.
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
  Serial.flush();

  digitalWrite(CARD_FREE_FLAG, HIGH);                         // Reactivate data transfer so that new data can come in from master.
  
  // Receive transmission and relay it through the laser.
  while (true) {
    //Serial.println(bufferPos);
    //Serial.flush();
    delay(100);
    if (pollReset()) { return; }
    if (bufferPos == BUFFER_SIZE) {
      digitalWrite(CARD_FREE_FLAG, LOW);                      // Turn of data transfer while busy.
      noInterrupts();
      transmit(buffer, bufferPos);
      interrupts();
      Serial.println(transmissionPos);
      Serial.flush();
      resetBuffer();
      digitalWrite(CARD_FREE_FLAG, HIGH);                     // Reactivate data transfer.
      continue;
    }
    if (transmissionPos == desc.transmissionLength) {
      digitalWrite(CARD_FREE_FLAG, LOW);                      // Turn of data transfer.
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
