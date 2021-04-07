// For printf.
#include <stdio.h>

// Includes for the device file handling.
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>

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

// I2C.
#define CONVERTER_ADDRESS 3

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

// Storage variable for the file descriptor for the I2C virtual file thing.
int I2CFile;

// Something goes wrong when sending more than 32 bytes at a time because internal buffer on the arduino is 32 bytes for I2C.
// This splits the message up and waits for a small amount of time between each chunk.
// That gives the arduino enough time to increment a few numbers and make everything work.
void writeThroughChunks(char* buffer, int length) {
	int ni = 32;
	for (int i = 0; i < length; i = ni, ni += 32) {
		if (ni > length) {
			write(I2CFile, buffer + i, length - i);
			break;
		}
		write(I2CFile, buffer + i, 32);
		delay(10);
	}
}

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

// Sleep until slave device says that this device can continue.
// Periodically polls slave device to get regular answers.
// If no answer is received because slave is busy with a time critical process, busy is assumed.
bool waitForOk() {
	char buffer;
	while (true) {
		delay(100);
		if (read(I2CFile, &buffer, 1) == 0) { continue; }
		if (buffer) { return false; }
		log("Slave busy.");
		if (pollButton()) {
			log("Button interrupt received. Shutting down transmission and resetting laser card...");
			return true;
		}
	}
}

int main() {
	// Open I2C connection to the Arduino converter.
	if ((I2CFile = open("/dev/i2c-1", O_RDWR)) == -1) {
		log("Failed to open the I2C bus. Quitting...");
		return 0;
	}
	if (ioctl(I2CFile, I2C_SLAVE, CONVERTER_ADDRESS) == -1) {
		log("Failed to open connection to I2C slave device. Quitting...");
		close(I2CFile);
		return 0;
	}

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
	desc.syncInterval = 1;
	desc.bitDuration = 500;
	desc.durationType = MICROSECONDS;

	// Enter loop and wait for someone to press button.
	while (true) {
		delay(LOOP_SLEEP);
		if (pollButton()) {					// If button is held down at this moment, shoot image and start sending.
			log("Shooting image...");
			shoot();
			log("Sending the image over I2C...");
			write(I2CFile, &desc, sizeof(desc));
			int ni = BUFFER_SIZE;
			for (int i = 0; i < IMAGE_SIZE; i = ni, ni += BUFFER_SIZE) {
				if (waitForOk()) { break; }
				log("Packet arrived at photoresistor.");
				if (ni > IMAGE_SIZE) {
					writeThroughChunks((char*)image.data + i, IMAGE_SIZE - i);
					log("Entire image was sucessfully transmitted. Waiting for human input...");
					break;
				}
				writeThroughChunks((char*)image.data + i, BUFFER_SIZE);
			}
			// Send a reset signal to the laser card because either waitForOk() triggered or the entire image has been sent.
                	// In the latter case, this isn't strictly necessary, but in case something went wrong somewhere, why not do it?
			digitalWrite(RESET, HIGH);
                	delay(RESET_WAIT);
                	digitalWrite(RESET, LOW);
		}
	}

	// Close I2C connection.
	// TODO: This can never be reached so it doesn't make any sense to have here.
	close(I2CFile);					// TODO: Make this dispose itself on signal interrupt too.
}
