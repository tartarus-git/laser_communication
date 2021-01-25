#include <wiringPi.h>
#include <iostream> // TODO: Research about including this, and then probably include it because C++ standard and whatnot. Why does stdio.h work instead of this for DoorSensor?
#include <cstring>
#include <fstream>
#include <memory>
#include <chrono>
#include <thread>

// Transmission
#define TRANSMISSION_INTERVAL 1000000		// This is in microseconds.
#define SLEEP std::this_thread::sleep_for(std::chrono::microseconds(TRANSMISSION_INTERVAL))		// TODO: Is this the proper way. I was just winging it.

#define SYNC_POINT 100

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

//#define BIT_MASK 0x01
#define BIT_MASK 0b00000001		// TODO: Using this for now, but after debugging, we should switch back to hex because it looks nicer.

void sendByte(char data) {
	digitalWrite(LASER, data & BIT_MASK);
	SLEEP;
	for (int i = 1; i < 7; i++) {
		digitalWrite(LASER, (data >> i) & BIT_MASK);
		SLEEP;
	}
	digitalWrite(LASER, data >> 7);
	SLEEP;
}

short syncCounter = 0;

void synchronize() {
	digitalWrite(LASER, LOW);			// How much power does this take. Does it make sense to check the value first or would that be inefficient. TODO.
	SLEEP;
	syncCounter = 0;
	digitalWrite(LASER, HIGH);
	SLEEP;
}

void transmit(char* data, int length) {
	// Send the data over the laser to the recieving device.
	digitalWrite(LASER, HIGH);
	log("Set high for com start.");
	SLEEP;
	for (int i = 0; i < length; i++) {
		if (syncCounter == SYNC_POINT) {
			synchronize();
		}
		else {					// TODO: Change the format so the else is on the line above. It looks better, even though it isn't consistant with VS.
			syncCounter++;
		}
		sendByte(data[i]);
		log("Sent a byte of data.");
	}
	log("Done sending the data. Setting back to low.");
	// Reset the laser pin back to low so we don't trigger another receive on the Arduino.
	digitalWrite(LASER, LOW);
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
