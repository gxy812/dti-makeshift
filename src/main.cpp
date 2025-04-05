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
MotorPins motorL{-1, 15, 16};
MotorPins motorR{-1, 17, 18};
const int led_R = 4;
const int led_G = 5;
const int led_B = 6;
const int led2_R = 39;
const int led2_G = 40;
const int led2_B = 41;

const int sensor_FL = 37;
const int sensor_FR = 38;
const int sensor_BL = 37;
const int sensor_BR = 36;

bool isBlocked_FL = false;
bool isBlocked_FR = false;
bool isBlocked_BL = false;
bool isBlocked_BR = false;

BLEServer *pServer = nullptr;
BLECharacteristic *pDirectionCharacterisitic = nullptr;
BLECharacteristic *pIRCharacterisitic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint64_t ms_last_motor_update = 0;

const char *SERVICE_UUID = "05816886-9304-4973-8176-34e49bb6dbab";
const char *DIRECTION_CHARACTERISTIC_UUID =
  "3210b38d-583c-4127-9bbb-a3161716dae7";
const char *IR_CHARACTERISTIC_UUID = "4b4da85c-af00-412b-ad32-dc8a4492b574";

const int MOTOR_SPEED = 255;

enum Direction { None, Forward, TurnLeft, TurnRight, Backward };

Direction movingDirection = None;

void setupMotor(MotorPins motor) {
    // pinMode(motor.en, OUTPUT);
    pinMode(motor.in1, OUTPUT);
    pinMode(motor.in2, OUTPUT);
}

void setMotor(MotorPins motor, int power) {
    if (power == 0) {
        digitalWrite(motor.in1, HIGH);
        digitalWrite(motor.in2, HIGH);
        return;
    }
    bool positive = power > 0;
    digitalWrite(motor.in1, positive);
    digitalWrite(motor.in2, !positive);
    // analogWrite(motor.en, abs(power));
}

// return error code
int move(Direction dir) {
    // TODO: Motor integration and max PWM value
    switch (dir) {
    case None:
        setMotor(motorL, 0);
        setMotor(motorR, 0);
        break;
    case Forward:
        if (isBlocked_FL || isBlocked_FR) {
            break;
        }
        setMotor(motorL, MOTOR_SPEED);
        setMotor(motorR, MOTOR_SPEED);
        break;
    case TurnLeft:
        if (isBlocked_FL) {
            break;
        }
        setMotor(motorL, MOTOR_SPEED);
        setMotor(motorR, 0);
        break;
    case TurnRight:
        if (isBlocked_FR) {
            break;
        }
        setMotor(motorL, 0);
        setMotor(motorR, MOTOR_SPEED);
        break;
    case Backward:
        if (isBlocked_BL || isBlocked_BR) {
            break;
        }
        setMotor(motorL, -MOTOR_SPEED);
        setMotor(motorR, -MOTOR_SPEED);
        break;
    default:
        Serial.println("Strange bluetooth value!");
        return 1;
    }
    return 0;
}

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
        auto receivedValue = static_cast<Direction>(value[0]);
        if (!move(receivedValue)) {
            ms_last_motor_update = millis();
        }
        Serial.println(receivedValue);
    }
};

void setup() {
    pinMode(sensor_FL, INPUT_PULLUP);
    pinMode(sensor_FR, INPUT_PULLUP);
    pinMode(sensor_BL, INPUT_PULLUP);
    pinMode(sensor_BR, INPUT_PULLUP);
    pinMode(led_R, OUTPUT);
    pinMode(led_G, OUTPUT);
    pinMode(led_B, OUTPUT);
    pinMode(led2_R, OUTPUT);
    pinMode(led2_G, OUTPUT);
    pinMode(led2_B, OUTPUT);

    setupMotor(motorL);
    setupMotor(motorR);

    Serial.begin(115200);
    BLEDevice::init("Wall");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pDirectionCharacterisitic = pService->createCharacteristic(
      DIRECTION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
    pDirectionCharacterisitic->setCallbacks(new CharacteristicCallbacks());
    pDirectionCharacterisitic->addDescriptor(new BLE2902());
    pIRCharacterisitic = pService->createCharacteristic(
      IR_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE |
                                BLECharacteristic::PROPERTY_NOTIFY |
                                BLECharacteristic::PROPERTY_INDICATE);
    pIRCharacterisitic->addDescriptor(new BLE2902());
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0);
    BLEDevice::startAdvertising();
}

void loop() {
    isBlocked_FL = !digitalRead(sensor_FL);
    isBlocked_FR = !digitalRead(sensor_FR);
    isBlocked_BL = !digitalRead(sensor_BL);
    isBlocked_BR = !digitalRead(sensor_BR);
    static uint64_t ms_last_characteristic_update = 0;
    static byte r = 0;
    static byte g = 0;
    static byte b = 0;
    if (deviceConnected && millis() - ms_last_characteristic_update > 2000) {
        pIRCharacterisitic->setValue(
          String((byte)isBlocked_FR + (byte)isBlocked_FL).c_str());
        pIRCharacterisitic->notify();
        ms_last_characteristic_update = millis();
    }
    // stop motors after last command
    if (millis() - ms_last_motor_update > 500) {
        move(None);
    }
    static uint64_t ms_last_led_update = 0;
    if (millis() - ms_last_led_update > 500) {
        digitalWrite(led_R, r % 3 == 0);
        digitalWrite(led_G, g % 2 == 0);
        digitalWrite(led_B, b % 5 == 0);
        digitalWrite(led2_R, r % 3 == 1);
        digitalWrite(led2_G, g % 2 == 1);
        digitalWrite(led2_B, b % 5 == 1);
        r++;
        g++;
        b++;
        ms_last_led_update = millis();
    }
}
