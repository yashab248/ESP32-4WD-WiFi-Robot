
#include <WiFi.h>
#include <WebServer.h>

// ==========================================
// MOTOR DRIVER PINS
// ==========================================

// Left motors: TB6612 channel A
const int PWMA = 23;
const int AIN2 = 21;
const int AIN1 = 22;

// Right motors: TB6612 channel B
const int BIN1 = 18;
const int BIN2 = 17;
const int PWMB = 19;

// LEDs
const int redLED = 26;
const int blueLED = 25;
const int yellowLED = 33;

// ==========================================
// WIFI
// ==========================================

const char* ssid = "ROVER_WIFI";
const char* password = "rover123";

WebServer server(80);

// ==========================================
// MOTOR / DRIVE STATE
// ==========================================
const int MIN_SPEED = 60;
const int MAX_SPEED = 255;

int motorSpeed = 150;

int leftMotorSpeed = 0;
int rightMotorSpeed = 0;

int throttle = 0;  // 1 = forward, -1 = reverse, 0 = stopped
int steering = 0;  // -100 = left, 0 = centre, 100 = right
int pivot = 0;     // -1 = left, 1 = right, 0 = none

bool motorsActive = false;

unsigned long lastCommandTime = 0;

const int STEERING_DEADZONE = 8;
const unsigned long WATCHDOG_TIMEOUT = 450;

// ==========================================
// STOP MOTORS
// ==========================================

void stopMotors() {

  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);

  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, LOW);

  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);

  digitalWrite(redLED, HIGH);
  digitalWrite(blueLED, HIGH);
  digitalWrite(yellowLED, HIGH);

  leftMotorSpeed = 0;
  rightMotorSpeed = 0;

  throttle = 0;
  steering = 0;
  pivot = 0;

  motorsActive = false;
}

// ==========================================
// SET LEFT MOTOR
// Positive = forward
// Negative = backward
// ==========================================

void setLeftMotor(int speed) {

  speed = constrain(speed, -255, 255);

  if (speed > 0) {

    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);

  }
  else if (speed < 0) {

    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);

  }
  else {

    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);

  }

  analogWrite(PWMA, abs(speed));

  leftMotorSpeed = speed;
}

// ==========================================
// SET RIGHT MOTOR
// Positive = forward
// Negative = backward
// ==========================================

void setRightMotor(int speed) {

  speed = constrain(speed, -255, 255);

  if (speed > 0) {

    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);

  }
  else if (speed < 0) {

    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);

  }
  else {

    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);

  }

  analogWrite(PWMB, abs(speed));

  rightMotorSpeed = speed;
}

// ==========================================
// DRIVE FORWARD / REVERSE WITH STEERING
// ==========================================


void drive(int direction, int steeringValue) {

  int adjustedSteering = steeringValue;

  // Reverse steering needs the opposite motor mix.
  if (direction == -1) {
    adjustedSteering = -adjustedSteering;
  }

  int leftFactor = 100 + adjustedSteering;
  int rightFactor = 100 - adjustedSteering;

  int maxFactor = max(leftFactor, rightFactor);

  int leftPWM = motorSpeed * leftFactor / maxFactor;
  int rightPWM = motorSpeed * rightFactor / maxFactor;

  // Keep both motors above the minimum while moving.
  leftPWM = max(leftPWM, MIN_SPEED);
  rightPWM = max(rightPWM, MIN_SPEED);

  setLeftMotor(direction * leftPWM);
  setRightMotor(direction * rightPWM);

  digitalWrite(redLED, LOW);
  digitalWrite(blueLED, HIGH);
  digitalWrite(yellowLED, LOW);

  motorsActive = true;
}


// ==========================================
// PIVOT LEFT / RIGHT
// ==========================================

void rotateLeft() {

  if (motorSpeed == 0) {
    stopMotors();
    return;
  }

  // Left side backward.
  // Right side forward.

  setLeftMotor(-motorSpeed);
  setRightMotor(motorSpeed);

  digitalWrite(redLED, LOW);
  digitalWrite(blueLED, LOW);
  digitalWrite(yellowLED, HIGH);

  motorsActive = true;
}

void rotateRight() {

  if (motorSpeed == 0) {
    stopMotors();
    return;
  }

  // Left side forward.
  // Right side backward.

  setLeftMotor(motorSpeed);
  setRightMotor(-motorSpeed);

  digitalWrite(redLED, LOW);
  digitalWrite(blueLED, LOW);
  digitalWrite(yellowLED, HIGH);

  motorsActive = true;
}

// ==========================================
// APPLY CONTROLLER INPUTS
// ==========================================

void applyControls() {

  // Forward or reverse has priority over pivot buttons.

  if (throttle != 0) {

    drive(throttle, steering);

  }

  // Pivot only when joystick is centred.

  else if (abs(steering) <= STEERING_DEADZONE) {

    if (pivot == -1) {
      rotateLeft();
    }
    else if (pivot == 1) {
      rotateRight();
    }
    else {
      stopMotors();
    }

  }

  else {

    stopMotors();

  }
}

// ==========================================
// PROCESS WEBSITE COMMANDS
// ==========================================

void handleCommand() {

  if (!server.hasArg("throttle") ||
      !server.hasArg("steer") ||
      !server.hasArg("pivot") ||
      !server.hasArg("speed")) {

    server.send(400, "text/plain", "Missing controller data");
    return;
  }

  throttle = constrain(server.arg("throttle").toInt(), -1, 1);
  steering = constrain(server.arg("steer").toInt(), -100, 100);
  pivot = constrain(server.arg("pivot").toInt(), -1, 1);
  motorSpeed = constrain(server.arg("speed").toInt(), MIN_SPEED, MAX_SPEED);

  // Refresh watchdog.

  lastCommandTime = millis();

  // Apply the new controller inputs.

  applyControls();

  server.send(200, "text/plain", "OK");
}

// ==========================================
// WEBSITE
// ==========================================

const char webpage[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">

<title>Rover Controller</title>

<style>

* {
  box-sizing: border-box;
  -webkit-user-select: none;
  user-select: none;
  -webkit-touch-callout: none;
}

body {
  background: #101820;
  color: white;
  font-family: Arial, sans-serif;
  text-align: center;
  margin: 0;
  padding: 20px;
  overflow-x: hidden;
}

h1 {
  color: #4ade80;
  margin-bottom: 5px;
  letter-spacing: 2px;
}

.subtitle {
  color: #94a3b8;
  margin-top: 5px;
}

.status {
  display: inline-block;
  background: #263746;
  border-radius: 20px;
  padding: 10px 24px;
  margin: 15px;
  font-weight: bold;
  color: #4ade80;
}

.controller {
  max-width: 650px;
  margin: 20px auto;
  background: #182632;
  border: 1px solid #334155;
  border-radius: 28px;
  padding: 25px 15px;
}

.controller-layout {
  display: flex;
  justify-content: space-around;
  align-items: center;
  gap: 20px;
  flex-wrap: wrap;
}

/* ANALOG JOYSTICK */

.joystick-section {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.section-label {
  color: #94a3b8;
  font-size: 13px;
  font-weight: bold;
  letter-spacing: 2px;
  margin-bottom: 18px;
}

.joystick {
  width: 190px;
  height: 190px;
  border-radius: 50%;
  background: #263746;
  border: 3px solid #475569;
  position: relative;
  touch-action: none;
  overflow: hidden;
}

.joystick::before {
  content: "";
  position: absolute;
  width: 2px;
  height: 100%;
  background: #475569;
  left: 50%;
  top: 0;
}

.joystick::after {
  content: "";
  position: absolute;
  height: 2px;
  width: 100%;
  background: #475569;
  left: 0;
  top: 50%;
}

.joystick-knob {
  width: 55px;
  height: 55px;
  border-radius: 50%;
  background: #4ade80;
  border: 4px solid #bbf7d0;
  position: absolute;
  left: 50%;
  top: 50%;
  transform: translate(-50%, -50%);
  box-shadow: 0 0 18px #4ade8070;
  z-index: 2;
  pointer-events: none;
}

.steering-label {
  margin-top: 14px;
  color: #4ade80;
  font-size: 13px;
}

/* ACTION BUTTONS */

.action-section {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.button-cluster {
  display: grid;
  grid-template-columns: repeat(3, 80px);
  grid-template-rows: repeat(3, 80px);
  gap: 5px;
}

.action-button {
  border: 3px solid #475569;
  border-radius: 50%;
  background: #263746;
  color: white;
  font-size: 29px;
  font-weight: bold;
  width: 80px;
  height: 80px;
  touch-action: none;
  cursor: pointer;
  transition: transform 0.08s;
}

.action-button:active,
.action-button.pressed {
  transform: scale(0.92);
  filter: brightness(1.5);
}

.button-y {
  grid-column: 2;
  grid-row: 1;
  color: #4ade80;
}

.button-x {
  grid-column: 1;
  grid-row: 2;
  color: #60a5fa;
}

.button-b {
  grid-column: 3;
  grid-row: 2;
  color: #f87171;
}

.button-a {
  grid-column: 2;
  grid-row: 3;
  color: #4ade80;
}

/* SPEED CONTROL */

.speed-section {
  max-width: 400px;
  margin: 30px auto 10px;
  background: #263746;
  border-radius: 18px;
  padding: 18px;
}

.speed-section h3 {
  margin-top: 0;
}

input[type="range"] {
  width: 100%;
  accent-color: #4ade80;
  cursor: pointer;
}

.speed-value {
  color: #4ade80;
  font-weight: bold;
}

.emergency {
  margin-top: 20px;
  background: #dc2626;
  border: none;
  color: white;
  border-radius: 14px;
  padding: 16px 35px;
  font-size: 16px;
  font-weight: bold;
  letter-spacing: 1px;
  touch-action: none;
}

.emergency:active {
  background: #991b1b;
}

.instructions {
  color: #94a3b8;
  font-size: 12px;
  line-height: 1.8;
  margin-top: 25px;
}

@media (max-width: 420px) {

  .controller {
    padding: 20px 8px;
  }

  .controller-layout {
    gap: 25px;
  }

  .joystick {
    width: 165px;
    height: 165px;
  }

  .button-cluster {
    grid-template-columns: repeat(3, 57px);
    grid-template-rows: repeat(3, 57px);
  }

  .action-button {
    width: 57px;
    height: 57px;
  }

}


/* LANDSCAPE PHONE CONTROLLER */
@media (orientation: landscape) {

  body {
    padding: 8px;
    margin: 0;
  }

  h1 {
    font-size: 22px;
    margin: 5px 0 10px;
  }

  .controller {
    width: 100%;
    max-width: 850px;
    margin: 8px auto;
    padding: 12px 20px;
    box-sizing: border-box;
  }

  /* Joystick on the left, buttons on the right */
  .controller-layout {
    display: flex;
    flex-direction: row;
    flex-wrap: nowrap;
    justify-content: space-between;
    align-items: center;
    width: 100%;
    gap: 20px;
  }

  /* LEFT THUMB */
  .joystick {
    width: 155px;
    height: 155px;
    flex-shrink: 0;
  }

  .joystick-knob {
    width: 52px;
    height: 52px;
  }

  /* RIGHT THUMB */
  .button-cluster {
    display: grid;
    grid-template-columns: repeat(3, 80px);
    gap: 5px;
    flex-shrink: 0;
  }

  .action-button {
    width: 80px;
    height: 80px;
    font-size: 21px;
  }

  /* SPEED CONTROL */
  .speed-section {
    margin: 8px auto;
    padding: 10px;
    max-width: 850px;
  }
}

/* Short landscape screens */
@media (orientation: landscape) and (max-height: 450px) {

  h1 {
    font-size: 18px;
    margin: 2px 0 5px;
  }

  .controller {
    padding: 6px 16px;
    margin: 4px auto;
  }

  .joystick {
    width: 125px;
    height: 125px;
  }

  .joystick-knob {
    width: 45px;
    height: 45px;
  }

  .button-cluster {
    grid-template-columns: repeat(3, 50px);
    gap: 4px;
  }

  .action-button {
    width: 50px;
    height: 50px;
    font-size: 19px;
  }

  .speed-section {
    margin: 4px auto;
    padding: 6px;
  }
}

</style>

</head>

<body>

<h1>ROVER CONTROL CENTER</h1>

<p class="subtitle">ESP32 WIRELESS CONTROLLER</p>

<div class="status" id="status">
  STOPPED
</div>

<div class="controller">

  <div class="controller-layout">

    <!-- LEFT ANALOG JOYSTICK -->

    <div class="joystick-section">

      <div class="section-label">STEERING</div>

      <div class="joystick" id="joystick">

        <div class="joystick-knob" id="joystickKnob"></div>

      </div>

      <div class="steering-label" id="steeringLabel">
        CENTRE
      </div>

    </div>

    <!-- ACTION BUTTONS -->

    <div class="action-section">

      <div class="section-label">MOVEMENT</div>

      <div class="button-cluster">

        <button class="action-button button-y"
                data-button="forward">Y</button>

        <button class="action-button button-x"
                data-button="left">X</button>

        <button class="action-button button-b"
                data-button="right">B</button>

        <button class="action-button button-a"
                data-button="reverse">A</button>

      </div>

    </div>

  </div>

  <!-- SPEED SLIDER -->

  <div class="speed-section">

    <h3>
      MOTOR SPEED:
      <span class="speed-value" id="speedValue">150</span>
    </h3>

    <input
      type="range"
      id="speed"
      min="60"
      max="255"
      value="150"
    >

    <p style="font-size:12px;color:#94a3b8">
      Adjust the maximum motor power
    </p>

  </div>

  <button class="emergency" id="emergency">
    EMERGENCY STOP
  </button>

</div>

<div class="instructions">

  <p>Y = FORWARD &nbsp; | &nbsp; A = REVERSE</p>

  <p>X = ROTATE LEFT &nbsp; | &nbsp; B = ROTATE RIGHT</p>

  <p>Move the joystick left or right to steer.</p>

  <p>Release all controls to stop the rover.</p>

</div>

<script>

// ==========================================
// CONTROLLER STATE
// ==========================================

let steering = 0;

let forwardPressed = false;
let reversePressed = false;
let leftPressed = false;
let rightPressed = false;

let emergencyStopped = false;

let requestInFlight = false;

const joystick = document.getElementById("joystick");
const joystickKnob = document.getElementById("joystickKnob");

const steeringLabel = document.getElementById("steeringLabel");

const speedSlider = document.getElementById("speed");
const speedValue = document.getElementById("speedValue");

const statusText = document.getElementById("status");

// ==========================================
// JOYSTICK
// ==========================================

let joystickPointer = null;

function updateJoystick(event) {

  const rect = joystick.getBoundingClientRect();

  const centerX = rect.width / 2;

  const maxTravel = rect.width * 0.34;

  let x = event.clientX - rect.left - centerX;

  x = Math.max(-maxTravel, Math.min(maxTravel, x));

  // Convert horizontal joystick position to -100 to +100.

  steering = Math.round((x / maxTravel) * 100);

  // Dead zone.

  if (Math.abs(steering) < 8) {
    steering = 0;
    x = 0;
  }

  joystickKnob.style.transform =
    `translate(calc(-50% + ${x}px), -50%)`;

  if (steering < 0) {
    steeringLabel.textContent = "STEERING LEFT";
  }
  else if (steering > 0) {
    steeringLabel.textContent = "STEERING RIGHT";
  }
  else {
    steeringLabel.textContent = "CENTRE";
  }
}

function resetJoystick() {

  steering = 0;

  joystickKnob.style.transform =
    "translate(-50%, -50%)";

  steeringLabel.textContent = "CENTRE";
}

joystick.addEventListener("pointerdown", event => {

  event.preventDefault();

  if (joystickPointer !== null) return;

  joystickPointer = event.pointerId;

  joystick.setPointerCapture(event.pointerId);

  updateJoystick(event);

});

joystick.addEventListener("pointermove", event => {

  if (event.pointerId !== joystickPointer) return;

  event.preventDefault();

  updateJoystick(event);

});

function releaseJoystick(event) {

  if (event.pointerId !== joystickPointer) return;

  joystickPointer = null;

  resetJoystick();

}

joystick.addEventListener("pointerup", releaseJoystick);
joystick.addEventListener("pointercancel", releaseJoystick);
joystick.addEventListener("lostpointercapture", releaseJoystick);

// ==========================================
// ACTION BUTTONS
// ==========================================

function setButton(name, pressed) {

  if (name === "forward") forwardPressed = pressed;
  if (name === "reverse") reversePressed = pressed;
  if (name === "left") leftPressed = pressed;
  if (name === "right") rightPressed = pressed;

  const button = document.querySelector(
    `[data-button="${name}"]`
  );

  if (button) {
    button.classList.toggle("pressed", pressed);
  }

}

document.querySelectorAll("[data-button]").forEach(button => {

  const name = button.dataset.button;

  button.addEventListener("pointerdown", event => {

    event.preventDefault();

    button.setPointerCapture(event.pointerId);

    if (emergencyStopped) {
      emergencyStopped = false;
    }

    setButton(name, true);

  });

  function releaseButton(event) {

    if (event && event.pointerId !== undefined &&
        button.hasPointerCapture(event.pointerId) === false) {
      // A lost capture still releases the button.
    }

    setButton(name, false);

  }

  button.addEventListener("pointerup", releaseButton);
  button.addEventListener("pointercancel", releaseButton);
  button.addEventListener("lostpointercapture", releaseButton);

});

// ==========================================
// GET CURRENT CONTROL VALUES
// ==========================================

function getThrottle() {

  if (forwardPressed && !reversePressed) return 1;

  if (reversePressed && !forwardPressed) return -1;

  return 0;

}

function getPivot() {

  // Pivot buttons only work when:
  // 1. Neither forward nor reverse is pressed.
  // 2. The joystick is centred.

  if (getThrottle() !== 0) return 0;

  if (Math.abs(steering) > 8) return 0;

  if (leftPressed && !rightPressed) return -1;

  if (rightPressed && !leftPressed) return 1;

  return 0;

}

// ==========================================
// SEND COMMAND TO ESP32
// ==========================================

async function sendCommand() {

  if (requestInFlight) return;

  requestInFlight = true;

  const throttleValue = emergencyStopped ? 0 : getThrottle();

  const steeringValue = emergencyStopped ? 0 : steering;

  const pivotValue = emergencyStopped ? 0 : getPivot();

  const speed = speedSlider.value;

  const url =
    `/drive?throttle=${throttleValue}` +
    `&steer=${steeringValue}` +
    `&pivot=${pivotValue}` +
    `&speed=${speed}`;

  try {

    const response = await fetch(url, {
      cache: "no-store"
    });

    if (!response.ok) {
      throw new Error("ESP32 command failed");
    }

    if (emergencyStopped ||
        (throttleValue === 0 && pivotValue === 0)) {

      statusText.textContent = "STOPPED";
      statusText.style.color = "#4ade80";

    }
    else if (throttleValue === 1) {

      statusText.textContent = "FORWARD";
      statusText.style.color = "#4ade80";

    }
    else if (throttleValue === -1) {

      statusText.textContent = "REVERSE";
      statusText.style.color = "#4ade80";

    }
    else if (pivotValue === -1) {

      statusText.textContent = "ROTATING LEFT";
      statusText.style.color = "#60a5fa";

    }
    else if (pivotValue === 1) {

      statusText.textContent = "ROTATING RIGHT";
      statusText.style.color = "#f87171";

    }

  }
  catch (error) {

    statusText.textContent = "CONNECTION LOST";
    statusText.style.color = "#f87171";

  }
  finally {

    requestInFlight = false;

  }

}

// ==========================================
// SPEED SLIDER
// ==========================================

speedSlider.addEventListener("input", () => {

  speedValue.textContent = speedSlider.value;

});

// ==========================================
// EMERGENCY STOP
// ==========================================

document.getElementById("emergency").addEventListener("click", () => {

  emergencyStopped = true;

  forwardPressed = false;
  reversePressed = false;
  leftPressed = false;
  rightPressed = false;

  document.querySelectorAll("[data-button]").forEach(button => {
    button.classList.remove("pressed");
  });

  resetJoystick();

  sendCommand();

});

// ==========================================
// SEND CONTROLLER DATA EVERY 100ms
// ==========================================

// Keeps the ESP32 watchdog refreshed while controls are active.

setInterval(sendCommand, 100);

// Send an initial stop command.

sendCommand();

// ==========================================
// SAFETY: STOP IF PAGE LOSES FOCUS
// ==========================================

function releaseAllControls() {

  forwardPressed = false;
  reversePressed = false;
  leftPressed = false;
  rightPressed = false;

  emergencyStopped = true;

  document.querySelectorAll("[data-button]").forEach(button => {
    button.classList.remove("pressed");
  });

  resetJoystick();

  sendCommand();

}

window.addEventListener("blur", releaseAllControls);

document.addEventListener("visibilitychange", () => {

  if (document.hidden) {
    releaseAllControls();
  }

});

// Prevent long-press context menus.

document.addEventListener("contextmenu", event => {
  event.preventDefault();
});

</script>

</body>
</html>

)rawliteral";

// ==========================================
// SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(redLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);

  stopMotors();

  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("ROVER WIFI STARTED!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, []() {

    server.send(200, "text/html", webpage);

  });

  server.on("/drive", HTTP_GET, handleCommand);

  server.begin();

  Serial.println("WEB SERVER STARTED!");
  Serial.println("XBOX-STYLE CONTROLLER READY!");

}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {

  server.handleClient();

  // SAFETY WATCHDOG

  if (motorsActive &&
      millis() - lastCommandTime > WATCHDOG_TIMEOUT) {

    stopMotors();

    Serial.println("WATCHDOG: MOTORS STOPPED!");

  }

}
