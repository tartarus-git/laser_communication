#include <opencv2/opencv.hpp>			// TODO: This include seems strange, figure out if you can be more precise here.
#include <wiringPi.h>
#include <memory>
#include <cstring>
#include <iostream>			// This needs to be included with fstream because the standard says so. I don't know why
					// iostream isn't just included inside of fstream. TODO: Ask on StackOverflow and fix this in
					// DoorSensor, because I think we don't do it there.
#include <fstream>

// Pins
#define BUTTON 15
#define BUTTON_SOURCE 16

#define LASER 1

// Loop
#define LOOP_SLEEP 10

// Communication
#define DESC_BIT_DURATION 100		// In milliseconds.
#define SLEEP delay(desc.bitDuration)
#define BIT_MASK 0x01
#define SERIAL_DELAY 1000		// In milliseconds.
#define BUFFER_SIZE 1024

void log(const char* message) {
	int len = strlen(message);
	std::unique_ptr<char[]> buffer = std::make_unique<char[]>(len + 1 + 1);
	memcpy(buffer.get(), message, len);
	buffer[len] = '\n';
	buffer[len + 1] = '\0';
	printf(buffer.get());
	// Free the memory inside of the unique pointer. Don't use release() because it only release control of the pointer.
	// I think it's pretty dumb, they should be the other way around.
	buffer.reset();
}

// Since our struct is less complex now, this might not even be necessary, even though I still don't understand why we needed it.
#pragma pack(1)				// TODO: Figure out why the hell this fixes a bug and makes this work (which it does). It shouldn't do anything should it?
struct ConnectionDescriptor {
	int16_t syncInterval;         // The amount of bytes between each synchronization plus 1.
	uint16_t bitDuration;
} desc;

void transmitDescriptor() {
	for (unsigned int i = 0; i < sizeof(desc); i++) {
		digitalWrite(LASER, HIGH);
		for (int j = 0; j < 8; j++) {
			delay(DESC_BIT_DURATION);
			digitalWrite(LASER, (*((char*)&desc + i) >> j) & BIT_MASK);	// TODO: Learn order of operations.
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
        SLEEP;
        digitalWrite(LASER, HIGH);
	SLEEP;
}

int16_t syncCounter = -1;

void transmit(char* data, int length) {
	uint16_t packetLength;
	int ni = BUFFER_SIZE;
	for (int i = 0; i < length; i = ni, ni += BUFFER_SIZE) {
		if (ni > length) { packetLength = length - i; }
		else { packetLength = BUFFER_SIZE; }
		std::cout << "Next packet length: " << packetLength << std::endl;

		sync();

		// Send length of the next packet.
		for (int bit = 0; bit < 16; bit++) {
			digitalWrite(LASER, (packetLength >> bit) & BIT_MASK);
			SLEEP;
		}

		// Sleep so that the Arduino has enough time to start waiting for synchronization.
		SLEEP;
		sync();

		// Send the packet.
		for (int byte = 0; byte < packetLength; byte++) {
			// Synchronize if the time is right.
			if (syncCounter == desc.syncInterval) {
				syncCounter = 0;
				SLEEP;				// Sleep to give Arduino enough time.
				sync();
			} else { syncCounter++; }

			// Send the next byte.
			for (int bit = 0; bit < 8; bit++) {
				digitalWrite(LASER, (data[i + byte] >> bit) & BIT_MASK);
				SLEEP;
			}
		}

		// Reset sync counter.
		syncCounter = -1;

		// Give the relay enough time to send the captured data to the receiver over serial.
		delay(SERIAL_DELAY);
	}
}

cv::VideoCapture cap;

void press() {
	// Load raw image data from file (not the actual RAW format).
	/*std::ifstream f("input.raw", std::ios::binary | std::ios::ate);
	if (f.is_open()) {
		log("Sucessfully opened the image file. Loading contents...");
		int length = f.tellg();
		std::unique_ptr<char[]> buffer = std::make_unique<char[]>(length);
		f.seekg(0, f.beg);
		f.read(buffer.get(), length);
		if (f.gcount() == length) { log("Sucessfully loaded all contents of file."); }
		else {
			log("Could not load the contents of file.");
			f.close();
			return;
		}
		f.close();

		log("Sending data over laser...");
		transmit(buffer.get(), length);			// passing raw array to function is ok because possible exception
								// would go up the call stack and still get handled here.
		return;
	}
	// Log failure and wait for the next button press. Maybe next time it'll work out better.
	log("Could not open the image file.");*/

	// Capture image and send it over the laser.
	log("Capturing image...");
	cv::Mat image;
	cap >> image;
	log("Sucessfully captured image. Resizing...");
	cv::Mat resized;
	cv::resize(image, resized, cv::Size(200, 100));
	log("Resized. Converting to 1bpp...");
	int length = 200 * 100 / 8;
	if ((200 * 100) % 8) { length++; }
	length += 8;
	std::unique_ptr<char[]> buffer = std::make_unique<char[]>(length);
	int matIndex = 0;
	for (int i = 8; i < length; i++) {
		for (int j = 7; j >= 0; j--, matIndex += 3) {
			char avg = (resized.data[matIndex] + resized.data[matIndex + 1] + resized.data[matIndex + 2]) / 3;
			if (avg > 127) {
				buffer[i] |= 1 << j;
				continue;
			}
			buffer[i] &= ~(1 << j);
		}
	}
	log("Converted. Adding dimension tag to the buffer...");
	*(int32_t*)buffer.get() = 200;
	*(int32_t*)(buffer.get() + 4) = 100;
	log("Added. Sending data over laser...");
	transmit(buffer.get(), length);
}


int main() {
	// Setup.
	wiringPiSetup();
	piHiPri(99);		// Give this thread the highest possible priority. Good for timing, only works while elevated.
	pinMode(LASER, OUTPUT);

	// Initialize camera with OpenCV.
	cap = cv::VideoCapture(0);
	if (!cap.isOpened()) {			// TODO: Obviously put the rest of the stuff inside of the if for effiency.
		log("Failed to initialize the camera. Quitting...");
		return 0;
	}

	// Describe a few things about the connection.
	desc.syncInterval = 1;
	desc.bitDuration = 1;

	log("Transmitting descriptor...");

	// Send the descriptor with a standard protocol and go with the described one from here on out.
	transmitDescriptor();

	log("Transmitted descriptor.");

	bool buttonState = false;
	while (true) {				// TODO: Add a SIGINT intercept thing so that you can dispose properly.
		// Add a delay so as to not use 100% of the CPU (or even just 1 of its cores).
                delay(LOOP_SLEEP);

		//if (digitalRead(BUTTON)) { // TODO: Put this back in.
		if (true) {
			if (buttonState) { continue; }
			press();		// Trigger a press event.
			buttonState = true;
		}
		else {
			buttonState = false;
		}
	}
}
