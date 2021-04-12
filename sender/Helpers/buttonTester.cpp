#include <wiringPi.h>
#include <iostream>

#define SOURCE 21
#define BUTTON 25

int main() {
	wiringPiSetup();
	pinMode(SOURCE, OUTPUT);
	digitalWrite(SOURCE, HIGH);
	pinMode(BUTTON, INPUT);
	pullUpDnControl(BUTTON, PUD_DOWN);
	while (true) {
		std::cout << digitalRead(BUTTON) << std::endl;
	}
}
