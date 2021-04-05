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
#define IMAGE_WIDTH 200
#define IMAGE_HEIGHT 100
#define IMAGE_SIZE IMAGE_WIDTH * 3 * IMAGE_HEIGHT

// I2C.
#define CONVERTER_ADDRESS 3

// Button.
#define BUTTON 0				// TODO: Give this an actual pin.

// Laser.
#define BUFFER_SIZE 1024

#define MILLISECONDS false
#define MICROSECONDS true

// Loop.
#define LOOP_SLEEP 10							// To make sure that we don't use too much of the processor for nothing.

void log(const char* message) {
        int len = strlen(message);
	char* buffer = new char[len + 1 + 1];
        memcpy(buffer, message, len);
        buffer[len] = '\n';
        buffer[len + 1] = '\0';
        printf(buffer);
	delete[] buffer;
}

cv::VideoCapture cap;
cv::Mat image;

void shoot() {
	log("Capturing image...");
        cv::Mat original;
        cap >> original;
        log("Sucessfully captured image. Resizing...");
        cv::resize(original, image, cv::Size(IMAGE_WIDTH, IMAGE_HEIGHT));
}

#pragma pack(1)
struct ConnectionDescriptor {
	uint32_t transmissionLength;					// The length of the entire following transmission.
	int16_t syncInterval;						// The amount of bytes between each synchronization plus 1.
	uint16_t bitDuration;
	uint8_t durationType;						// True for microseconds, false for milliseconds.
} desc;

int I2CFile;

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

int main() {
	// Open I2C connection to the Arduino converter.
	if ((I2CFile = open("/dev/i2c-1", O_WRONLY)) == -1) {
		log("Failed to open the I2C bus. Quitting...");
		return 0;
	}
	if (ioctl(I2CFile, I2C_SLAVE, CONVERTER_ADDRESS) == -1) {
		log("Failed to open connection to I2C slave device. Quitting...");
		close(I2CFile);
		return 0;
	}

	// Set up WiringPi.
	wiringPiSetup();
	pinMode(BUTTON, INPUT);				// TODO: Do you have to explicitly say this if it's just an input like in arduino?

	// Initialize camera.
	cap = cv::VideoCapture(0);
	if (!cap.isOpened()) {
		log("Failed to initialize the camera. Quitting...");
		close(I2CFile);
		return 0;
	}

	// Connection descriptor setup.
	desc.transmissionLength = IMAGE_SIZE;
	printf("%d\n", IMAGE_SIZE);
	desc.syncInterval = 1;
	desc.bitDuration = 10;
	desc.durationType = MICROSECONDS;

	// Enter loop and wait for someone to press button.
	while (true) {
		delay(LOOP_SLEEP);
		if (true) {
			shoot();
			log("Sending the image over I2C...");
			write(I2CFile, &desc, sizeof(desc));
			int ni = BUFFER_SIZE;
			for (int i = 0; i < IMAGE_SIZE; i = ni, ni += BUFFER_SIZE) {
				delay(1000);
				log("Chunk done.");
				if (ni > IMAGE_SIZE) {
					writeThroughChunks((char*)image.data + i, IMAGE_SIZE - i);
					break;
				}
				writeThroughChunks((char*)image.data + i, BUFFER_SIZE);
			}
		}
		break;
	}

	// Close I2C connection.
	close(I2CFile);
}
