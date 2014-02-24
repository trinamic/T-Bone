var b = require('bonescript');

var ledPin1 = "P8_11";
var ledPin2 = "P8_12";
var PWMPin1 = "P9_16";
var PWMPin2 = "P9_14";
var PWMPin3 = "P8_19";
var CS_5041_PIN = "P8_13";
var CS_4361_1_PIN = "P8_15";
var CS_4361_2_PIN = "P8_17";
var CS_4361_3_PIN = "P8_18";

b.pinMode(ledPin1, b.OUTPUT);
b.pinMode(ledPin2, b.OUTPUT);

b.pinMode(PWMPin1, b.OUTPUT);
b.pinMode(PWMPin2, b.OUTPUT);
b.pinMode(PWMPin3, b.OUTPUT);

b.pinMode(CS_5041_PIN, b.OUTPUT);
b.pinMode(CS_4361_1_PIN, b.OUTPUT);
b.pinMode(CS_4361_2_PIN, b.OUTPUT);
b.pinMode(CS_4361_3_PIN, b.OUTPUT);

var state = b.LOW;
b.digitalWrite(ledPin1, state);
b.digitalWrite(ledPin2, state);
b.digitalWrite(PWMPin1, state);
b.digitalWrite(PWMPin2, state);
b.digitalWrite(PWMPin3, state);
b.digitalWrite(CS_5041_PIN, state);
b.digitalWrite(CS_4361_1_PIN, state);
b.digitalWrite(CS_4361_2_PIN, state);
b.digitalWrite(CS_4361_3_PIN, state);

setInterval(toggle, 1000);

function toggle() {
    if(state == b.LOW) state = b.HIGH;
    else state = b.LOW;
    b.digitalWrite(PWMPin1, state);
    b.digitalWrite(PWMPin2, state);
    b.digitalWrite(PWMPin3, state);
}