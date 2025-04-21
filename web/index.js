'use strict'
// DOM Elements
const connectButton = document.getElementById('connectBleButton');
const disconnectButton = document.getElementById('disconnectBleButton');
const stopButton = document.getElementById('stopButton');
const upButton = document.getElementById('upButton');
const leftButton = document.getElementById('leftButton');
const rightButton = document.getElementById('rightButton');
const downButton = document.getElementById('downButton');
const retrievedValue = document.getElementById('valueContainer');
const bleStateContainer = document.getElementById('bleState');
const timestampContainer = document.getElementById('timestamp');
/** @type {HTMLInputElement} */
const colorSelector = document.getElementById('colorSelector');

//Define BLE Device Specs
var deviceName = 'Wall';
const bleService = "05816886-9304-4973-8176-34e49bb6dbab";
const dirCharacteristic = '3210b38d-583c-4127-9bbb-a3161716dae7';
const irCharacteristic = '4b4da85c-af00-412b-ad32-dc8a4492b574';
const rgbCharacteristic = '01d3636d-4cfb-46d8-890d-ac30f7fc5ac8';

//Global Variables to Handle Bluetooth
var bleServer;
var bleServiceFound;
var irCharacteristicFound;

var activeDirection;
var oldvalue = 0;

// Connect Button (search for BLE Devices only if BLE is available)
connectButton.addEventListener('click', (event) => {
    if (isWebBluetoothEnabled()) {
        connectToDevice();
    }
});

// Disconnect Button
disconnectButton.addEventListener('click', disconnectDevice);

// Write to the ESP32 Direction Characteristic

['mousedown', 'touchstart'].forEach((eventtype) => {
    upButton.addEventListener(eventtype, () => startDirection(1));
    leftButton.addEventListener(eventtype, () => startDirection(2));
    rightButton.addEventListener(eventtype, () => startDirection(3));
    downButton.addEventListener(eventtype, () => startDirection(4));
    stopButton.addEventListener(eventtype, () => {
        stopDirection();
        sendDirection(0);
    });
});
['mouseup', 'touchend', 'touchcancel'].forEach((eventtype) => {
    upButton.addEventListener(eventtype, stopDirection);
    leftButton.addEventListener(eventtype, stopDirection);
    rightButton.addEventListener(eventtype, stopDirection);
    downButton.addEventListener(eventtype, stopDirection);
});
colorSelector.addEventListener('input', sendColor);

function startDirection(direction) {
    stopDirection();
    if (!bleServer || !bleServer.connected) {
        console.error("Bluetooth is not connected. Cannot write to characteristic.")
        window.alert("Bluetooth is not connected.\n Connect to BLE first!")
        return;
    }
    switch (direction) {
        case 2:
        case 3:
            sendDirection(1);
            break;
    }
    activeDirection = setInterval(() => {
        sendDirection(direction);
    }, 200);
}

function stopDirection() {
    if (activeDirection) {
        clearInterval(activeDirection);
        activeDirection = null;
    }
}

// Check if BLE is available in your Browser
function isWebBluetoothEnabled() {
    if (!navigator.bluetooth) {
        console.log("Web Bluetooth API is not available in this browser!");
        bleStateContainer.innerHTML = "Web Bluetooth API is not available in this browser!";
        return false;
    }
    console.log('Web Bluetooth API supported in this browser.');
    return true;
}

// Connect to BLE Device and Enable Notifications
function connectToDevice() {
    console.log('Initializing Bluetooth...');
    let foundDevice = "";
    navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: deviceName }],
        optionalServices: [bleService]
    })
        .then(device => {
            console.log('Device Selected:', device.name);
            foundDevice = device.name;
            bleStateContainer.innerHTML = 'Connecting to device ' + device.name + '. Please wait.';
            bleStateContainer.style.color = "#af6a24";
            bleStateContainer.className = "loader";
            device.addEventListener('gattserverdisconnected', onDisconnected);
            return device.gatt.connect();
        })
        .then(gattServer => {
            bleServer = gattServer;
            console.log("Connected to GATT Server");
            return bleServer.getPrimaryService(bleService);
        })
        .then(service => {
            bleServiceFound = service;
            console.log("Service discovered:", service.uuid);
            bleStateContainer.innerHTML = 'Connected to device ' + foundDevice;
            bleStateContainer.style.color = "#24af37";
            bleStateContainer.className = "";
            disconnectButton.hidden = false;
            connectButton.hidden = true;
            return service.getCharacteristic(irCharacteristic);
        })
        .then(characteristic => {
            console.log("Characteristic discovered:", characteristic.uuid);
            irCharacteristicFound = characteristic;
            characteristic.addEventListener('characteristicvaluechanged', handleCharacteristicChange);
            characteristic.startNotifications();
            return characteristic.readValue();
        })
        .then(value => {
            console.log("Read value: ", value);
            const decodedValue = new TextDecoder().decode(value);
            retrievedValue.innerHTML = decodedValue;
        })
        .catch(error => {
            console.log('Error: ', error);
        })
}

function onDisconnected(event) {
    stopDirection();
    console.log('Device Disconnected:', event.target.device?.name);
    bleStateContainer.innerHTML = "Device disconnected";
    bleStateContainer.style.color = "#d13a30";
    disconnectButton.hidden = true;
    connectButton.hidden = false;

    connectToDevice();
}

function blockFlagsToText(flags) {
    const front = flags & 0b01;
    const back = flags & 0b10;
    let text = "";
    upButton.hidden = false;
    downButton.hidden = false;
    if (front) {
        text += "Front is blocked.\n";
        upButton.hidden = true;
    }
    if (back) {
        text += "Back is blocked.\n";
        downButton.hidden = true;
    }
    return text;
}

function handleCharacteristicChange(event) {
    const newValueReceived = new TextDecoder().decode(event.target.value);
    console.log("Characteristic value changed: ", newValueReceived);
    retrievedValue.innerHTML = blockFlagsToText(newValueReceived);
}

function sendDirection(value) {
    // do not create error popups here - inevitably some commands will sneak through after disconnection
    if (!bleServer || !bleServer.connected) {
        stopDirection();
        console.error("Bluetooth is not connected. Cannot write to characteristic.")
        return;
    }
    bleServiceFound.getCharacteristic(dirCharacteristic)
        .then(characteristic => {
            console.log("Found the direction characteristic: ", characteristic.uuid);
            const data = new Uint8Array([value]);
            return characteristic.writeValueWithoutResponse(data);
        })
        .then(() => {
            console.log("Value written to direction:", value);
        })
        .catch(error => {
            stopDirection();
            console.error("Error writing to the direction characteristic: ", error);
        });
}

/** 
 * @param {Event} event
*/
function sendColor(event) {
    if (!bleServer || !bleServer.connected) {
        console.error("Bluetooth is not connected. Cannot write to characteristic.")
        window.alert("Bluetooth is not connected. Cannot write to characteristic. \n Connect to BLE first!")
        return;
    }
    let value = event.target.value.slice(1);
    value = parseInt(value, 16);
    bleServiceFound.getCharacteristic(rgbCharacteristic)
        .then(characteristic => {
            console.log("Found the color characteristic: ", characteristic.uuid);
            const data = new Uint32Array([value]);
            return characteristic.writeValueWithoutResponse(data);
        })
        .then(() => {
            // latestValueSent.innerHTML = value;
            console.log("Value written to color:", value.toString(16));
        })
        .catch(error => {
            console.error("Error writing to the color characteristic: ", error);
        });
}

function disconnectDevice() {
    console.log("Disconnect Device.");
    if (!bleServer || !bleServer.connected) {
        // Throw an error if Bluetooth is not connected
        console.error("Bluetooth is not connected.");
        window.alert("Bluetooth is not connected.");
        return;
    }
    if (!irCharacteristicFound) {
        console.log("No characteristic found to disconnect.");
        return;
    }
    irCharacteristicFound.stopNotifications()
        .then(() => {
            console.log("Notifications Stopped");
            return bleServer.disconnect();
        })
        .then(() => {
            console.log("Device Disconnected");
        })
        .catch(error => {
            console.log("An error occurred:", error);
        });
}

// Utility function
async function colorCycle() {
    let r = 0;
    let g = 0;
    let b = 0;
    while (true) {
        for (let i = 0; i <= Math.PI; i += Math.PI * 0.01) {
            let value = 0;
            r = Math.cos(i) * 255;
            g = Math.cos(i - Math.PI / 2) * 255;
            b = Math.cos(i - Math.PI) * 255;
            r = Math.floor(Math.max(0, r));
            g = Math.floor(Math.max(0, g));
            b = Math.floor(Math.max(0, b));
            value |= r << 16;
            value |= g << 8;
            value |= b;
            value = value.toString(16).padStart(6, 0);
            value = '#' + value;
            colorSelector.value = value;
            colorSelector.dispatchEvent(new Event('input'));
            await new Promise(resolve => setTimeout(resolve, 40));
        }
    }
}
