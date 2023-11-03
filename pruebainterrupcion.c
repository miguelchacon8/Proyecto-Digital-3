#include <stdio.h>
#include <wiringPi.h>

#define BUTTON_PIN 17

void buttonPressed(void) {
    printf("Button pressed!\n");
    delay(500); // Wait for 500ms to debounce
}

int main(void) {
    wiringPiSetupGpio(); // Initialize wiringPi library

    pinMode(BUTTON_PIN, INPUT); // Set button pin as input
    wiringPiISR(BUTTON_PIN, INT_EDGE_RISING, &buttonPressed); // Set up interrupt for button press

    while (1) { // Loop forever
        // Do nothing
    }

    return 0;
}
