#pragma once
extern "C" {
static inline void pinMode(unsigned char, int) {}
static inline unsigned long micros() { return 0; }
static inline int digitalPinToInterrupt(int pin) { return pin; }
static inline void attachInterruptArg(int, void (*)(void *), void *, int) {}
static inline void detachInterrupt(int) {}
}
#define INPUT 0
#define CHANGE 1
