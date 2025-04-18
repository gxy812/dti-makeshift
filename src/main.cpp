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
#include <FastLED.h>

const uint8_t NUM_LEDS = 18;

struct MotorPins {
    const uint8_t in1;
    const uint8_t in2;
};

// TODO: Pin choices
MotorPins motorL{15, 16};
MotorPins motorR{17, 18};

const uint8_t LED_DATA = 4;
CRGB *leds;

const uint8_t sensor_F = 5;
const uint8_t sensor_B = 6;

bool isBlocked_F = false;
bool isBlocked_B = false;

BLEServer *pServer = nullptr;
BLECharacteristic *pDirectionCharacterisitic = nullptr;
BLECharacteristic *pIRCharacterisitic = nullptr;
BLECharacteristic *pRGBCharacterisitic = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint64_t ms_last_motor_update = 0;

const char *SERVICE_UUID = "05816886-9304-4973-8176-34e49bb6dbab";
const char *DIRECTION_CHARACTERISTIC_UUID =
  "3210b38d-583c-4127-9bbb-a3161716dae7";
const char *IR_CHARACTERISTIC_UUID = "4b4da85c-af00-412b-ad32-dc8a4492b574";
const char *RGB_CHARACTERISTIC_UUID = "01d3636d-4cfb-46d8-890d-ac30f7fc5ac8";

const int MOTOR_SPEED = 0xFF;

enum Direction { None, Forward, TurnLeft, TurnRight, Backward };

// MOTOR CONTROL

void setupMotor(MotorPins motor) {
    pinMode(motor.in1, OUTPUT);
    pinMode(motor.in2, OUTPUT);
}

// Control motor spin
// Speed control not implemented
// Can potentially change to sending PWM to direction pins for speed control
void setMotor(MotorPins motor, int16_t power) {
    // Enable both directions to lock the motor in place - braking effect
    if (power == 0) {
        digitalWrite(motor.in1, HIGH);
        digitalWrite(motor.in2, HIGH);
        return;
    }
    bool positive = power > 0;
    digitalWrite(motor.in1, positive);
    digitalWrite(motor.in2, !positive);
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
        if (isBlocked_F) {
            break;
        }
        setMotor(motorL, MOTOR_SPEED);
        setMotor(motorR, MOTOR_SPEED);
        break;
    case TurnLeft:
        setMotor(motorL, MOTOR_SPEED);
        setMotor(motorR, 0xF0);
        break;
    case TurnRight:
        setMotor(motorL, 0xF0);
        setMotor(motorR, MOTOR_SPEED);
        break;
    case Backward:
        if (isBlocked_B) {
            break;
        }
        setMotor(motorL, -0xF0);
        setMotor(motorR, -0xF0);
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

class DirectionCallbacks : public BLECharacteristicCallbacks {
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

class RGBCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *prop) {
        // Gets array of bytes of type string
        auto value = prop->getValue();
        if (value.length() < 3)
            return;
        uint32_t color = 0;
        for (int i = 0; i < 3; ++i) {
            auto receivedValue = static_cast<uint32_t>(value[i]);
            Serial.println(receivedValue, 16);
            for (int j = 0; j < i; ++j) {
                receivedValue <<= 8;
            }
            color |= receivedValue;
        }
        Serial.println(color, 16);
        for (int i = 0; i < NUM_LEDS; ++i) {
            leds[i].setColorCode(color);
        }
    }
};

void setup() {
    pinMode(sensor_F, INPUT_PULLUP);
    pinMode(sensor_B, INPUT_PULLUP);

    setupMotor(motorL);
    setupMotor(motorR);
    leds = new CRGB[NUM_LEDS];
    FastLED.addLeds<NEOPIXEL, 4>(leds, NUM_LEDS);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].setColorCode(0x7F7F7F);
    }

    Serial.begin(115200);

    BLEDevice::init("Wall");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pDirectionCharacterisitic = pService->createCharacteristic(
      DIRECTION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
    pDirectionCharacterisitic->setCallbacks(new DirectionCallbacks());
    pDirectionCharacterisitic->addDescriptor(new BLE2902());

    pIRCharacterisitic = pService->createCharacteristic(
      IR_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE |
                                BLECharacteristic::PROPERTY_NOTIFY |
                                BLECharacteristic::PROPERTY_INDICATE);
    pIRCharacterisitic->addDescriptor(new BLE2902());

    pRGBCharacterisitic = pService->createCharacteristic(
      RGB_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE);
    pRGBCharacterisitic->setCallbacks(new RGBCallbacks());
    pRGBCharacterisitic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0);
    BLEDevice::startAdvertising();
}

void loop() {
    isBlocked_F = !digitalRead(sensor_F);
    isBlocked_B = !digitalRead(sensor_B);
    static uint64_t ms_last_characteristic_update = 0;
    if (deviceConnected && millis() - ms_last_characteristic_update > 2000) {
        byte block_flags = 0;
        block_flags |= isBlocked_F;
        block_flags |= (byte)isBlocked_B << 1;
        pIRCharacterisitic->setValue(String(block_flags).c_str());
        pIRCharacterisitic->notify();
        ms_last_characteristic_update = millis();
    }
    // stop motors after last command
    if (millis() - ms_last_motor_update > 500) {
        move(None);
    }
    FastLED.show();
}
