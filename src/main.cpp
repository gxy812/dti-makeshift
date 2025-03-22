/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at
  https://RandomNerdTutorials.com/esp32-web-bluetooth/ Permission is hereby
  granted, free of charge, to any person obtaining a copy of this software and
  associated documentation files. The above copyright notice and this permission
  notice shall be included in all copies or substantial portions of the
  Software.
*/
#include <Arduino.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

struct MotorPins {
    const int en;
    const int in1;
    const int in2;
};

// TODO: Pin choices
MotorPins motorL{3, 4, 5};
MotorPins motorR{6, 7, 8};

BLEServer *pServer = nullptr;
BLECharacteristic *pDirectionCharacterisitic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// TODO : Generate UUIDS
const char *SERVICE_UUID = "";
const char *DIRECTION_CHARACTERISTIC_UUID = "";

const int MOTOR_SPEED = 255;

enum Direction { None, Forward, TurnLeft, TurnRight, Backward };

Direction movingDirection = None;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *_) {
        deviceConnected = true;
        Serial.println("Connected!");
    }

    void onDisconnect(BLEServer *_) {
        deviceConnected = false;
        Serial.println("Disconnected!");
        // restart advertising
        pServer->startAdvertising();
    }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *prop) {
        auto value = prop->getValue();
        if (value.length() <= 0)
            return;
        auto receivedValue = static_cast<int>(value[0]);
        // TODO: Motor integration and max PWM value
        switch (receivedValue) {
        case None:
            setMotor(motorL, 0);
            setMotor(motorR, 0);
            break;
        case Forward:
            setMotor(motorL, MOTOR_SPEED);
            setMotor(motorR, MOTOR_SPEED);
        case TurnLeft:
            setMotor(motorL, MOTOR_SPEED);
            setMotor(motorR, 0);
        case TurnRight:
            setMotor(motorL, 0);
            setMotor(motorR, MOTOR_SPEED);
        case Backward:
            setMotor(motorL, -MOTOR_SPEED);
            setMotor(motorR, -MOTOR_SPEED);
        default:
            Serial.println("Strange bluetooth value!");
        }
    }
};

void setMotor(MotorPins motor, int power) {
    bool positive = power >= 0;
    digitalWrite(motor.in1, positive);
    digitalWrite(motor.in2, !positive);
    analogWrite(motor.en, abs(power));
}

void setup() {
    Serial.begin(115200);
    BLEDevice::init("Wall");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pDirectionCharacterisitic = pService->createCharacteristic(
      DIRECTION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
    pDirectionCharacterisitic->setCallbacks(new CharacteristicCallbacks());
    pDirectionCharacterisitic->addDescriptor(new BLE2902());
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0);
    BLEDevice::startAdvertising();
}

void loop() {}
