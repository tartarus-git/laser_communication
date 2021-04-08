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
#define CARD_0 0							// Bit channels for laser card communication.
#define CARD_1 0
#define CARD_2 0
#define CARD_3 0
#define CARD_4 0
#define CARD_5 0
#define CARD_7 0
#define CARD_TRIGGER 0.							// Triggers the dumping of the byte onto the laser card.


// Button.
#define SOURCE 21
#define BUTTON 25

// Reset pin for arduino laser card.
#define RESET 29
#define RESET_WAIT 2000							// In milliseconds.

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

int main() {
	// Set up WiringPi and pins.
	wiringPiSetup();
	pinMode(SOURCE, OUTPUT);
	digitalWrite(SOURCE, HIGH);
	pinMode(BUTTON, INPUT);				// TODO: Do you have to explicitly say this if it's just an input like in arduino?
	pullUpDnControl(BUTTON, PUD_DOWN);
	pinMode(RESET, OUTPUT);

	// Initialize camera.
	cap = cv::VideoCapture(0);
	if (!cap.isOpened()) {
		log("Failed to initialize the camera. Quitting...");
		close(I2CFile);
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
			// Send descriptor.
			int ni = BUFFER_SIZE;
			for (int i = 0; i < IMAGE_SIZE; i = ni, ni += BUFFER_SIZE) {
				log("Packet arrived at photoresistor.");
				if (ni > IMAGE_SIZE) {
					// Send last bit of transmission.
					log("Entire image was sucessfully transmitted. Waiting for human input...");
					break;
				}
				// Send transmission in packets.
			}
			// Send a reset signal to the laser card because either button was pressed or the entire image has been sent.
                	// In the latter case, this isn't strictly necessary, but in case something went wrong somewhere, why not do it?
			digitalWrite(RESET, HIGH);
                	delay(RESET_WAIT);
                	digitalWrite(RESET, LOW);
		}
	}
}
