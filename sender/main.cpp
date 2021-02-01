#include <wiringPi.h>
#include <iostream> // TODO: Research about including this, and then probably include it because C++ standard and whatnot. Why does stdio.h work instead of this for DoorSensor?
#include <cstring>
#include <fstream>
#include <memory>
#include <chrono>
#include <thread>

// Pins
#define BUTTON 15
#define BUTTON_SOURCE 16

#define LASER 1

// Loop
#define LOOP_SLEEP 10

void log(const char* message) {
	int len = strlen(message);
	std::unique_ptr<char[]> buffer = std::make_unique<char[]>(len + 1 + 1);
	memcpy(buffer.get(), message, len);
	buffer[len] = '\n';
	buffer[len + 1] = '\0';
	printf(buffer.get());
	buffer.reset(); 	// This frees the memory and sets the pointer inside of the unique_ptr to null. Don't use release(), it won't free the memory, just release
				// responsibility for it from the unique_ptr.
}

#define DESC_BIT_DURATION 100		// milliseconds.

struct ConnectionDescriptor {
	int16_t syncInterval;         // The amount of bytes between each synchronization plus 1.
	uint16_t bitDuration;
	uint8_t durationType;
} desc;

void sleep() {
	if (desc.durationType) {
		std::this_thread::sleep_for(std::chrono::microseconds(desc.bitDuration));		// TODO: Is this the proper way to do this?
		return;
	}
	delay(desc.bitDuration);
}

#define BIT_MASK 0x01

void transmitDescriptor() {
	log("Entered descriptor function.");
	for (unsigned int i = 0; i < sizeof(desc); i++) {
		digitalWrite(LASER, HIGH);
		for (int j = 0; j < 8; j++) {
			delay(DESC_BIT_DURATION);
			if ((*((char*)&desc + i) >> j) & BIT_MASK) {				// TODO: This can technically be optimized, but will you do it?
				digitalWrite(LASER, HIGH);
				continue;
			}
			digitalWrite(LASER, LOW);
		}
		delay(DESC_BIT_DURATION);
		digitalWrite(LASER, LOW);
		// Give the other device enough time to prepare for synchronization.
		delay(DESC_BIT_DURATION);
		delay(DESC_BIT_DURATION);
	}
}

void sync() {
        digitalWrite(LASER, LOW);
        sleep();
        digitalWrite(LASER, HIGH);
        sleep();
}

#define BUFFER_SIZE 1024

int16_t syncCounter = -1;

void transmit(char* data, int length) {
	printf("Length gotten: %d\n", length);
	int ni = BUFFER_SIZE;
	uint16_t amount;
	for (int i = 0; i < length; i = ni, ni += BUFFER_SIZE) {
		if (ni > length) {
			amount = length - i;
		} else {
			amount = BUFFER_SIZE;
		}
// TODO: Make sure the buffer doesn't overflow on the arduino because of this length transmit here.
		// Send the length of the next packet.
		printf("Length of next packet: %d\n", amount);
		sync();

		for (int bit = 0; bit < 16; bit++) {
			digitalWrite(LASER, (amount >> bit) & BIT_MASK);
			sleep();		// TODO: Make all other parts that are like this be like this. This is a step up from what you were doing before.
		}

		// Send the packet.
		for (int byte = 0; byte < amount; byte++) {
			// Synchronize if the time is right.
			if (syncCounter == desc.syncInterval) {
				sync();
				syncCounter = 0;
			} else { syncCounter++; }

			// Send the next byte.
			for (int bit = 0; bit < 8; bit++) {
				if ((data[byte] >> bit) & BIT_MASK) {		// TODO: This is technically inefficient. Do you want to fix this?
					digitalWrite(LASER, HIGH);
					sleep();
					continue;
				}
				digitalWrite(LASER, LOW);
				sleep();
			}
		}
	}
}

void press() {
	// Load raw image data from file (not the actual RAW format).
	std::ifstream f("input.raw", std::ios::binary | std::ios::ate);
	if (f.is_open()) {
		log("Sucessfully opened the image file. Loading contents...");
		int length = f.tellg();
		std::unique_ptr<char[]> buffer = std::make_unique<char[]>(length);
		f.seekg(0, f.beg);
		f.read(buffer.get(), length);
		if (f.gcount() == length) {
			log("Sucessfully loaded all contents of file.");
		}
		else {
			log("Could not load the contents of file.");
			f.close();
			return;
		}
		f.close();

		char* test = buffer.get();
		printf("Width gotten: %d\n", *(int*)test);
		printf("Height gotten: %d\n", *(int*)(test + 4));

		log("Sending data over laser...");
		transmit(buffer.get(), length);		// TODO: This get() here is okay right? If some sort of exception occurs, were safe, even in the other function right?

		return;
	}
	log("Could not open the image file.");
}


int main() {
	// Setup.
	wiringPiSetup();
	pinMode(LASER, OUTPUT);

	desc.syncInterval = 1;
	desc.bitDuration = 100;
	desc.durationType = false;			// false = milliseconds, true = microseconds. TODO: Make defines for this.

	transmitDescriptor();

	log("Transmitted descriptor.");

	bool buttonState = false;
	while (true) {
		// Add a delay so as to not use 100% of the CPU (or even just 1 of its cores).
                delay(LOOP_SLEEP);

		//if (digitalRead(BUTTON)) { // TODO: Put this back in.
		if (true) {
			if (buttonState) { continue; }
			// Trigger a press event.
			press();
			buttonState = true;
		}
		else {
			buttonState = false;
		}
	}
}
