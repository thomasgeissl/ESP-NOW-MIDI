#include "config.h"
#include "enomik_client.h"

#include <Adafruit_MPU6050.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>


// on the dongle: run the print_mac firmware and paste it here
uint8_t peerMacAddress[6] = { 0xCC, 0x8D, 0xA2, 0x8B, 0x85, 0x1C };
enomik::Client _client;

Adafruit_MPU6050 _mpu;
Adafruit_VL53L0X _lox = Adafruit_VL53L0X();



bool _mpu6050Connected = false;
bool _vl53LoxConnected = false;

int _soloCC = -1;


void onNoteOn(byte channel, byte note, byte velocity) {
  Serial.printf("Note On - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);
}

void onNoteOff(byte channel, byte note, byte velocity) {
  Serial.printf("Note Off - Channel: %d, Note: %d, Velocity: %d\n", channel, note, velocity);
}

void onControlChange(byte channel, byte control, byte value) {
  Serial.printf("Control Change - Channel: %d, Control: %d, Value: %d\n", channel, control, value);
}

void onProgramChange(byte channel, byte program) {
  Serial.printf("Program Change - Channel: %d, Program: %d\n", channel, program);
}

void onPitchBend(byte channel, int value) {
  Serial.printf("Pitch Bend - Channel: %d, Value: %d\n", channel, value);
}
void onAfterTouch(byte channel, byte value) {
  Serial.printf("After Touch - Channel: %d, Value: %d\n", channel, value);
}
void onPolyAfterTouch(byte channel, byte note, byte value) {
  Serial.printf("Poly After Touch - Channel: %d, note: %d, Value: %d\n", channel, note, value);
}

bool shouldSendControlChangeMessage(int controller) {
  if (_soloCC <= 0) {
    return true;
  }
  if (_soloCC == controller) {
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  _client.begin();
  _client.addPeer(peerMacAddress);

  // all of these midi handlers are optional, depends on the usecase, very often you just wanna send data and not receive
  // e.g. this can be used for calibration, or maybe you wanna connect an amp via i2s and render some sound
  _client.espnowMIDI.setHandleNoteOn(onNoteOn);
  _client.espnowMIDI.setHandleNoteOff(onNoteOff);
  _client.espnowMIDI.setHandleControlChange(onControlChange);
  _client.espnowMIDI.setHandleProgramChange(onProgramChange);
  _client.espnowMIDI.setHandlePitchBend(onPitchBend);
  _client.espnowMIDI.setHandleAfterTouchChannel(onAfterTouch);
  _client.espnowMIDI.setHandleAfterTouchPoly(onPolyAfterTouch);


  Serial.println("setting up sensors");
  Serial.print("setting mpu 6050 ... ");
  _mpu6050Connected = _mpu.begin();
  if (_mpu6050Connected) {
    _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    Serial.println("done");
  } else {
    Serial.println("error");
  }

  Serial.print("setting vl53l0x ... ");
  _vl53LoxConnected = _mpu.begin();
  if (_vl53LoxConnected) {
    Serial.println("done");
  } else {
    Serial.println("error");
  }
}

void loop() {
  _client.loop();
  if (_mpu6050Connected) {
    sensors_event_t a, g, temp;
    _mpu.getEvent(&a, &g, &temp);
    Serial.print(a.acceleration.x);
    if (shouldSendControlChangeMessage(CC_MPU6050_ACCELERATION_X)) {
      _client.sendControlChange(CC_MPU6050_ACCELERATION_X, map(a.acceleration.x, -10, 10, 0, 127), 1);
    }
    if (shouldSendControlChangeMessage(CC_MPU6050_ACCELERATION_Y)) {
      _client.sendControlChange(CC_MPU6050_ACCELERATION_Y, map(a.acceleration.y, -10, 10, 0, 127), 1);
    }
    if (shouldSendControlChangeMessage(CC_MPU6050_ACCELERATION_Z)) {
      _client.sendControlChange(CC_MPU6050_ACCELERATION_Z, map(a.acceleration.z, -10, 10, 0, 127), 1);
    }
    if (shouldSendControlChangeMessage(CC_MPU6050_ORIENTATION_X)) {
      _client.sendControlChange(CC_MPU6050_ORIENTATION_X, map(g.orientation.x, -5000, 5000, 0, 127), 1);
    }
    if (shouldSendControlChangeMessage(CC_MPU6050_ORIENTATION_Y)) {
      _client.sendControlChange(CC_MPU6050_ORIENTATION_Y, map(g.orientation.y, -5000, 5000, 0, 127), 1);
    }
    if (shouldSendControlChangeMessage(CC_MPU6050_ORIENTATION_Z)) {
      _client.sendControlChange(CC_MPU6050_ORIENTATION_Z, map(g.orientation.z, -5000, 5000, 0, 127), 1);
    }
  }
  return;

  if (_vl53LoxConnected) {
    VL53L0X_RangingMeasurementData_t measure;
    _lox.rangingTest(&measure, false);  // pass in 'true' to get debug data printout!
    if (measure.RangeStatus != 4) {     // phase failures have incorrect data
      Serial.print("Distance (mm): ");
      Serial.println(measure.RangeMilliMeter);
    } else {
      Serial.println(" out of range ");
    }
  }
}