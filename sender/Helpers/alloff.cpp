#include <wiringPi.h>

int main() {
	wiringPiSetup();
	pinMode(1, OUTPUT);
	digitalWrite(1, HIGH);
}
