#ifndef ARDUINO_H
#define ARDUINO_H

#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <stdint.h>
#include <algorithm>

#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HIGH 0x1
#define LOW  0x0
#define SSD1306_SWITCHCAPVCC 0x2
#define WHITE 1
#define SSD1306_WHITE 1

typedef uint8_t byte;

// Forward declarations for functions in .ino
float readTemp();
float logEst(float rate);
void addSample(float s);
float calcMean();
void adjustWeight(float error);
void resetLight();
void resetEstimate();
void fitCorr();
void correctArray(float corr);
float estimateTempLoss();
float estimateTime();
void updateDisplay();

// Mock Serial
class MockSerial {
public:
    void begin(int baud) {}
    void print(const char* s) { std::cout << s; }
    void print(float f, int p = 2) { std::cout << f; }
    void println(const char* s) { std::cout << s << std::endl; }
    void println(float f, int p = 2) { std::cout << f << std::endl; }
    void println(const std::string& s) { std::cout << s << std::endl; }
};
extern MockSerial Serial;

// Mock Wire
class MockWire {
public:
    void begin() {}
};
extern MockWire Wire;

// Mock Adafruit_GFX
class Adafruit_GFX {
public:
    Adafruit_GFX(int w, int h) {}
};

// Mock Adafruit_SSD1306
class Adafruit_SSD1306 : public Adafruit_GFX {
public:
    Adafruit_SSD1306(int w, int h, MockWire* wire, int rst = -1) : Adafruit_GFX(w, h) {}
    bool begin(int type, int addr) { return true; }
    void clearDisplay() {}
    void setTextSize(int s) {}
    void setTextColor(int c) {}
    void setCursor(int x, int y) {}
    void print(const char* s) {}
    void print(float f, int p = 2) {}
    void println(const char* s) {}
    void display() {}
};

#define F(s) s

// Arduino functions
unsigned long millis();
void pinMode(int pin, int mode);
int digitalRead(int pin);
int analogRead(int pin);
void delay(unsigned long ms);

// Math functions (Arduino uses standard C++ math usually)
using std::log;
using std::pow;
using std::exp;

template <typename T, typename T2, typename T3>
T constrain(T x, T2 a, T3 b) {
    if (x < (T)a) return (T)a;
    if (x > (T)b) return (T)b;
    return x;
}

#endif
