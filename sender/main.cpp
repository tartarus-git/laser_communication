// For printf.
#include <stdio.h>

// For GPIO access.
#include <wiringPi.h>

// For camera access and image resizing.
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>

// Image.
#define IMAGE_WIDTH 100
#define IMAGE_HEIGHT 100
#define IMAGE_SIZE IMAGE_WIDTH * 3 * IMAGE_HEIGHT

// Laser card protocol.
#define CARD_0 8							// Bit channels for laser card communication.
#define CARD_1 9
#define CARD_2 7
#define CARD_3 15
#define CARD_4 16
#define CARD_5 1
#define CARD_6 0
#define CARD_7 2

#define CARD_TRIGGER 3							// Triggers the dumping of the byte onto the laser card.
#define CARD_TRIGGER_DURATION 500					// In microseconds.

#define CARD_OPEN_FLAG 12
#define CARD_OPEN_FLAG_POLL_WAIT 10

#define CARD_TRANSMISSION_MIN_WAIT 1000

#define BIT_MASK 0x01

// Button.
#define SOURCE 21
#define BUTTON 25

// Reset pin for arduino laser card.
#define RESET 29

// Laser.
#define BUFFER_SIZE 1024

#define MILLISECONDS false
#define MICROSECONDS true

// Loop.
#define LOOP_SLEEP 10							// To make sure that we don't use too much of the processor for nothing.

// Simple logging code so that I don't constantly have to use printf and newline and such.
void log(const char* message) {
        int len = strlen(message);
	char* buffer = new char[len + 1 + 1];
        memcpy(buffer, message, len);
        buffer[len] = '\n';
        buffer[len + 1] = '\0';
        printf(buffer);
	delete[] buffer;
}

// Set up some variables that OpenCV uses for video capture.
cv::VideoCapture cap;
cv::Mat image;

// Code for shooting an image and resizing it to fit the desired dimensions.
void shoot() {
	log("Capturing image...");
        cv::Mat original;
        cap >> original;
        log("Sucessfully captured image. Resizing...");
        cv::resize(original, image, cv::Size(IMAGE_WIDTH, IMAGE_HEIGHT));
}

// TODO: Had some weird bugs when I left out pragma pack, even though it technically shouldn't change anything in this case, find out why.
#pragma pack(1)
struct ConnectionDescriptor {
	uint32_t transmissionLength;					// The length of the entire following transmission.
	int16_t syncInterval;						// The amount of bytes between each synchronization plus 1.
	uint16_t bitDuration;						// The duration of one single bit within the laser communication.
	uint8_t durationType;						// True for microseconds, false for milliseconds.
} desc;

// This makes sure that the button has to be let go before it can trigger another press event.
bool buttonPrevState = false;
bool pollButton() {
        if (digitalRead(BUTTON)) {
                if (buttonPrevState) { return false; }
                buttonPrevState = true;
                return true;
        }
        buttonPrevState = false;
        return false;
}

// This loads the corresponding bits onto the right pins and dumps each byte at a time to the slave.
void sendToLaserCard(const uchar* buffer, int length) {
	for (int bytePos = 0; bytePos < length; bytePos++) {
		digitalWrite(CARD_0, buffer[bytePos] & BIT_MASK);
		digitalWrite(CARD_1, (buffer[bytePos] >> 1) & BIT_MASK);
		digitalWrite(CARD_2, (buffer[bytePos] >> 2) & BIT_MASK);
		digitalWrite(CARD_3, (buffer[bytePos] >> 3) & BIT_MASK);
		digitalWrite(CARD_4, (buffer[bytePos] >> 4) & BIT_MASK);
		digitalWrite(CARD_5, (buffer[bytePos] >> 5) & BIT_MASK);
		digitalWrite(CARD_6, (buffer[bytePos] >> 6) & BIT_MASK);
		digitalWrite(CARD_7, buffer[bytePos] >> 7);
		digitalWrite(CARD_TRIGGER, HIGH);
		delayMicroseconds(CARD_TRIGGER_DURATION);		// Give the slave enough time to process dump.
		digitalWrite(CARD_TRIGGER, LOW);
		delayMicroseconds(CARD_TRIGGER_DURATION);		// Make sure it's low long enough for the slave to detect a rising edge on the next run.
	}
}

// Wait for the laser card to set the CARD_OPEN_FLAG. This makes the master wait for each packet to be processed by the slave before sending the next one.
bool waitForLaserCardOpen() {
	while(true) {
		if (digitalRead(CARD_OPEN_FLAG)) { return false; }
		if (pollButton()) {
			log("Button press detected. Resetting...");
			digitalWrite(RESET, HIGH);
			while (!digitalRead(CARD_OPEN_FLAG)) { }
			while (digitalRead(CARD_OPEN_FLAG)) { }
			digitalWrite(RESET, LOW);
			return true;
		}
		delay(CARD_OPEN_FLAG_POLL_WAIT);
	}
}

int main() {
	// Set up WiringPi and pins.
	wiringPiSetup();
	pinMode(SOURCE, OUTPUT);
	digitalWrite(SOURCE, HIGH);
	pinMode(BUTTON, INPUT);				// TODO: Do you have to explicitly say this if it's just an input like in arduino?
	pullUpDnControl(BUTTON, PUD_DOWN);
	pinMode(RESET, OUTPUT);

	// Set up connection to the laser card.
	pinMode(CARD_0, OUTPUT);
	pinMode(CARD_1, OUTPUT);
	pinMode(CARD_2, OUTPUT);
	pinMode(CARD_3, OUTPUT);
	pinMode(CARD_4, OUTPUT);
	pinMode(CARD_5, OUTPUT);
	pinMode(CARD_6, OUTPUT);
	pinMode(CARD_7, OUTPUT);
	pinMode(CARD_TRIGGER, OUTPUT);
	pinMode(CARD_OPEN_FLAG, INPUT);

	// Initialize camera.
	cap = cv::VideoCapture(0);
	if (!cap.isOpened()) {
		log("Failed to initialize the camera. Quitting...");
		return 0;
	}

	// Connection descriptor setup.
	desc.transmissionLength = IMAGE_SIZE;
	printf("Image size: %d\n", IMAGE_SIZE);
	desc.syncInterval = 2;
	desc.bitDuration = 250;
	desc.durationType = MICROSECONDS;

	// Enter loop and wait for someone to press button.
	while (true) {
		delay(LOOP_SLEEP);
		if (pollButton()) {					// If button is held down at this moment, shoot image and start sending.
			log("Shooting image...");
			shoot();
			log("Sending image to laser card...");
			sendToLaserCard((uchar*)&desc, sizeof(desc));
			int ni = BUFFER_SIZE;
			for (int i = 0; i < IMAGE_SIZE; i = ni, ni += BUFFER_SIZE) {
				delay(CARD_TRANSMISSION_MIN_WAIT);
				if (waitForLaserCardOpen()) { break; }
				log("Packet arrived at photoresistor.");
				if (ni > IMAGE_SIZE) {
					sendToLaserCard(image.data + i, IMAGE_SIZE - i);
					log("Entire image was sucessfully transmitted. Waiting for human input...");
					break;
				}
				sendToLaserCard(image.data + i, BUFFER_SIZE);
			}
		}
	}
}
