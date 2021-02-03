#include <wiringPi.h>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
	wiringPiSetup();
	pinMode(1, OUTPUT);
	piHiPri(99);		// Set the thread priority to the highest possible. Has to be run with sudo for it to take effect.

	// Send a pulse that takes 100 microseconds. This is going to be measured by arduino to see how accurate the pi is.
	auto start = std::chrono::high_resolution_clock::now();
	digitalWrite(1, HIGH);
//	delayMicroseconds(240);
	std::this_thread::sleep_for(std::chrono::microseconds(100));
	digitalWrite(1, LOW);
	std::cout << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count() << std::endl;
}
