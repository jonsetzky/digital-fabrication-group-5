#include <algorithm>
#include <deque>
#include <iostream>
#include <cmath>

float tempC;
float tempF;
/*
gpio1 (gp0) enable hdriver
gpio2 (gp1) hdriver input
*/
const int PIN_POTENTIOMETER_HIGH = 15;
const int PIN_TEMPSENSOR_HIGH = 14;

const int PIN_MOTOR = 0;
const int PIN_POTENTIOMETER_INPUT = 26;  // ADC0
const int PIN_TEMPSENSOR_INPUT = 28;     // ADC2

const double analogResolution = 1024.0;
/*
  returns potentiometer value in range [0, 1]
*/
double readPotentiometer() {
  /*
  input range
    max counterclockwise 1023
    max counterclockwise on the middle 550
    max clockwise on the middle 490
    max clockwise 4
  */
  double potentiometerRead = (double)analogRead(PIN_POTENTIOMETER_INPUT);
  return (potentiometerRead - 4.0) / 1019.0;
}


std::deque<double> tempHistory;
const int historySize = 256;
const double historySpan = 5000.0;  // 1000 ms
double nextHistoryUpdate = 0.0;

double getAvgTemp() {
  double sum = 0;
  for (double temp : tempHistory)
    sum += temp;

  return sum / tempHistory.size();
}

double readTempSensor(bool isAverage = true) {
  /*
    offset voltage 0.5
    voltage scaling mV/C 10
    output voltage @ 25C
    max value 1023
  */
  const double offsetVoltage = 0.5;
  const double mvPerCelcius = 10.0 / 1000.0;  // 10 mV

  double now = millis() / 1000.0;

  double sensorVoltage = (double)analogRead(PIN_TEMPSENSOR_INPUT) * 3.3 / analogResolution;
  double temperature = (sensorVoltage - offsetVoltage) / mvPerCelcius;

  if (now >= nextHistoryUpdate) {
    nextHistoryUpdate = now + (historySize / historySpan);
    tempHistory.push_back(temperature);
    if (tempHistory.size() >= 128)
      tempHistory.pop_front();
  }

  if (isAverage)
    return getAvgTemp();

  return temperature;
}

unsigned long nextToggle;
bool ledOn = true;
void blinkLed() {
  unsigned long now = millis();
  if (nextToggle <= now) {
    digitalWrite(LED_BUILTIN, ledOn ? HIGH : LOW);
    ledOn = !ledOn;
    nextToggle = 2500 + now;
    digitalWrite(PIN_MOTOR, ledOn ? HIGH : LOW);
  }
}


void setup() {
  Serial.begin(115200);
  analogWriteRange(255);

  pinMode(PIN_POTENTIOMETER_HIGH, OUTPUT);
  pinMode(PIN_TEMPSENSOR_HIGH, OUTPUT);
  digitalWrite(PIN_POTENTIOMETER_HIGH, HIGH);
  digitalWrite(PIN_TEMPSENSOR_HIGH, HIGH);

  pinMode(PIN_MOTOR, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);


  digitalWrite(PIN_MOTOR, HIGH);
}


// phase of n in 0-1 between a and b in exponential relation
// output is clamped to [0, 1]
double powPhaseBetween(double a, double b, double n) {
  double out = pow((float)((n - a) / (b - a)), 2);
  return max(min(out, 1.0), 0.0);
}

unsigned long nextPrint = 0;
int outputPower = 155;
void loop() {
  unsigned long now = millis();
  auto temp = readTempSensor();
  double potentiometer = readPotentiometer();
  double power = readPotentiometer() / 2 + 0.5;


  double rpm01;
  if (potentiometer < (7 / analogResolution)) {
    // automatic rpm
    rpm01 = powPhaseBetween(19.0, 26.0, temp);
  } else {
    // manual rpm
    rpm01 = potentiometer;
  }
  rpm01 = rpm01 < 0.1 ? 0 : rpm01;

  double motorOffset = 0.4;
  double motorPower = rpm01 * (1 - motorOffset) + motorOffset;
  analogWrite(PIN_MOTOR, (int)(motorPower * 255));

  // 0 rpm @ 19C & full rpm @ 26C

  if (nextPrint <= now) {
    nextPrint = now + 500;
    Serial.print("temp: ");
    Serial.print(temp);
    Serial.print(" potentiometer value: ");
    Serial.print(potentiometer);
    Serial.print(" motor power: ");
    Serial.print(motorPower * 100);
    Serial.print("%");
    Serial.print(" rpm: ");
    Serial.println(rpm01 * 185);
  }

  blinkLed();
}