#include <wiringPi.h>
#include <iostream>

#define SLEEP_TIME 1000
#define DESC_BIT_DURATION 1000 // In milliseconds.
#define LASER 1

#define BIT_MASK 0x01

#pragma pack(1)
struct ConnectionDescriptor {
	int16_t syncInterval;
	uint16_t bitDuration;
	uint8_t durationType;
} desc;

void sleep() {
	delay(SLEEP_TIME);
}

void sync() {
	digitalWrite(1, LOW);
	sleep();
	digitalWrite(1, HIGH);
	sleep();
}

int main() {
	wiringPiSetup();
	pinMode(1, OUTPUT);

	std::cout << "Testing size thing." << std::endl;
	std::cout << sizeof(desc) << std::endl;
	std::cout << sizeof(int16_t) << std::endl;
	std::cout << sizeof(uint16_t) << std::endl;
	std::cout << sizeof(uint8_t) << std::endl;
	std::cout << alignof(desc.syncInterval) << std::endl;
	std::cout << alignof(desc.bitDuration) << std::endl;
	std::cout << alignof(desc.durationType) << std::endl;

	std::cout << "Sending the desc thing..." << std::endl;

	for (unsigned int i = 0; i < sizeof(desc); i++) {
                digitalWrite(LASER, HIGH);
                for (int j = 0; j < 8; j++) {
                        delay(DESC_BIT_DURATION);
                        if ((*((char*)&desc + i) >> j) & BIT_MASK) {
				digitalWrite(LASER, HIGH);
                                continue;
                        }
                        digitalWrite(LASER, LOW);
                }
                delay(DESC_BIT_DURATION);
                digitalWrite(LASER, LOW);
                // Give the other device enough time to prepare for synchronization.
                delay(DESC_BIT_DURATION);
                delay(DESC_BIT_DURATION);
		std::cout << "Finished byte of sending." << std::endl;
        }

	std::cout << "Synchronizing..." << std::endl;
	sync();
	std::cout << "Synchronized." << std::endl;
	digitalWrite(1, LOW);
}
