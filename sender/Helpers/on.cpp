#include <wiringPi.h>

int main() {
	wiringPiSetup();
	pinMode(1, OUTPUT);
	digitalWrite(1, HIGH);
	delay(1000);
	digitalWrite(1, LOW);
	// TODO: I don't need to release anything do I?
}
