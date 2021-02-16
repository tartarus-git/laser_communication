// Includes for the device file handling.
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>

// For GPIO access.
#include <wiringPi.h>

// I2C.
#define CONVERTER_ADDRESS 3

// Button.
#define BUTTON 0				// TODO: Give this an actual pin.

// Loop.
#define LOOP_SLEEP 10							// To make sure that we don't use too much of the processor for nothing.

char* buffer;
int length;

void shoot() {
	
}

int I2CFile;

int main() {
	// Open I2C connection to the Arduino converter.
	I2CFile = open("/dev/i2c-1", O_WRONLY);
	ioctl(I2CFile, I2C_SLAVE, CONVERTER_ADDRESS);

	// Set up WiringPi.
	wiringPiSetup();
	pinMode(BUTTON, INPUT);				// TODO: Do you have to explicitly say this if it's just an input like in arduino?

	// Enter loop and wait for someone to press button.
	while (true) {
		delay(LOOP_SLEEP);
		if (digitalRead(BUTTON)) {
			
		}
	}

	// Close I2C connection.
	close(I2CFile);
}
