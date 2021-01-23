#include <wiringPi.h>
//#include <iostream> // TODO: Research about including this, and then probably include it because C++ standard and whatnot. Why does stdio.h work instead of this for DoorSensor?
#include <cstring>
#include <fstream>
#include <memory>
#include <chrono>
#include <thread>

// Transmission
#define TRANSMISSION_INTERVAL 1		// TODO: Change the unit of this. This is probably going to need to be in nanoseconds or at least microseconds or something.
#define SLEEP std::this_thread::sleep_for(std::chrono::microseconds(TRANSMISSION_INTERVAL))

// Pins
#define BUTTON 15
#define BUTTON_SOURCE 16

#define LASER 1

// Loop
#define LOOP_SLEEP 10

void log(const char* message) {
	int len = strlen(message);
	std::unique_ptr<char[]> buffer = std::make_unique<char[]>(len + 1);
	memcpy(buffer.get(), message, len);
	buffer[len] = '\n';
	printf(buffer.get());
	buffer.reset(); 	// This frees the memory and sets the pointer inside of the unique_ptr to null. Don't use release(), it won't free the memory, just release
				// responsibility for it from the unique_ptr.
}

#define BIT_MASK 0xF1

void setSignal(bool data) {
	if (data) { digitalWrite(LASER, HIGH); }
	else { digitalWrite(LASER, LOW); }
}

void sendByte(char data) {
	setSignal(data & BIT_MASK);
	SLEEP;
	for (int i = 1; i < 7; i++) {
		setSignal((data >> i) & BIT_MASK);
		SLEEP;
	}
	setSignal(data >> 7);
	SLEEP;
}

void transmit(char* data, int length) {
	// Send the data over the laser to the recieving device.
	setSignal(true);
	SLEEP;
	for (int i = 0; i < length; i++) {
		sendByte(data[i]);
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
