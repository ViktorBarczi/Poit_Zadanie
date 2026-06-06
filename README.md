# IoT Distance Alarm System

This project is an IoT warning and alarm system for distance monitoring. The system uses real hardware to measure the distance of an object and activates an alarm when the measured distance is lower than the configured threshold.

The project uses an ESP32-C3 board, an HY-SRF05 ultrasonic sensor, an RGB LED and a buzzer. The server application is written in Python using the Flask framework and runs in a Linux environment inside VirtualBox, which functionally replaces a Raspberry Pi. Communication between the server and the ESP32-C3 board is implemented through USB Serial.

## Features

- Distance measurement using the HY-SRF05 ultrasonic sensor
- ESP32-C3 hardware control
- RGB LED visual alarm indication
- Buzzer sound alarm
- Configurable alarm distance threshold
- Configurable measurement interval
- Configurable normal LED color and alarm LED color
- Web interface with Open, Start, Stop and Close buttons
- Live value display
- Chart visualization
- Gauge indicator
- Alarm-only measurement list
- SQLite database archiving
- CSV file logging
- USB Serial communication between Flask and ESP32-C3

## System Architecture

User  
↓  
Web browser  
↓ HTTP  
Python Flask server in VirtualBox Linux  
↓ USB Serial  
ESP32-C3  
↓  
HY-SRF05 sensor, RGB LED, buzzer

The ESP32-C3 is responsible for reading the sensor value and controlling the RGB LED and buzzer.

The Flask server is responsible for the web interface, communication with ESP32-C3, database storage and CSV archiving.

## Hardware Components

- ESP32-C3 development board
- HY-SRF05 ultrasonic distance sensor
- RGB LED
- Buzzer
- Resistors
- Jumper wires
- USB cable

## Pin Configuration

| Component | ESP32-C3 Pin |
|---|---|
| HY-SRF05 TRIG | GPIO 7 |
| HY-SRF05 ECHO | GPIO 8 |
| RGB LED Red | GPIO 10 |
| RGB LED Green | GPIO 20 |
| RGB LED Blue | GPIO 21 |
| Buzzer | GPIO 3 |

> Note: The ECHO pin of the HY-SRF05 sensor can output 5 V. The ESP32-C3 uses 3.3 V logic, so a voltage divider should be used on the ECHO pin.

## Serial Commands

The Flask server sends simple Serial commands to the ESP32-C3.

| Command | Description |
|---|---|
| `O` | Open / initialize system |
| `S` | Start monitoring |
| `T` | Stop monitoring |
| `C` | Close / deactivate system |
| `P` | Get status |
| `I1000` | Set measurement interval to 1000 ms |
| `D30` | Set alarm threshold to 30 cm |
| `G1` - `G4` | Set normal LED color |
| `A1` - `A4` | Set alarm LED color |

Example message from ESP32-C3:

DATA:24.15,30.00,1,MONITORING

## Project Structure

Poit_Zadanie/  
├── esp32_sensor_node/  
│   └── esp32_sensor_node.ino  
├── raspberry_pi/  
│   ├── app.py  
│   ├── requirements.txt  
│   ├── templates/  
│   │   └── index.html  
│   └── data/  
│       ├── measurements.db  
│       └── measurements.csv  
└── README.md

## Installation and Running

### 1. Upload Arduino firmware

Open the Arduino file from the `esp32_sensor_node` folder in Arduino IDE.

Select the correct ESP32-C3 board and port, then upload the firmware.

### 2. Configure VirtualBox USB

Connect the ESP32-C3 board to the computer using USB.

In VirtualBox enable USB controller:

Settings → USB → Enable USB Controller

Add the ESP32-C3 board as a USB filter. It may appear as:

- Espressif USB JTAG/serial
- Silicon Labs CP210x
- QinHeng CH340
- USB Serial

Inside the Linux virtual machine, check the connected Serial port:

ls /dev/ttyACM* /dev/ttyUSB*

The board usually appears as:

/dev/ttyACM0

or:

/dev/ttyUSB0

### 3. Install Python dependencies

Go to the server folder:

cd raspberry_pi

Create a virtual environment:

python3 -m venv venv

Activate it:

source venv/bin/activate

Install dependencies:

pip install -r requirements.txt

### 4. Run Flask server

python app.py

Open the web application:

http://127.0.0.1:5000

## Usage

1. Connect the ESP32-C3 board.
2. Start the Linux virtual machine.
3. Run the Flask server.
4. Open the web interface in a browser.
5. Click **Open** to initialize the system.
6. Set the measurement interval, alarm threshold and LED colors.
7. Click **Start** to begin monitoring.
8. Watch the live distance value, chart and gauge.
9. Alarm measurements are displayed in the alarm list.
10. Click **Stop** to stop monitoring.
11. Click **Close** to deactivate the system.

## Data Archiving

The application stores measured data in two ways.

### SQLite database

The SQLite database is used as the main archive for the web application. It stores measured values, alarm states and system events.

### CSV file

The CSV file is used as an additional file archive. It can be opened in Excel, LibreOffice Calc or used for external data processing.

## Alarm Logic

The measured distance is compared with the configured threshold value.

If the distance is lower than the configured threshold, the alarm is active.  
If the distance is equal to or higher than the threshold, the alarm is inactive.

When the alarm is active, the RGB LED changes to the selected alarm color and the buzzer is turned on.

When the alarm is inactive, the RGB LED uses the selected normal color and the buzzer is turned off.

## Technologies Used

- ESP32-C3
- Arduino / C++
- HY-SRF05 ultrasonic sensor
- Python
- Flask
- HTML
- CSS
- JavaScript
- Chart.js
- SQLite
- CSV
- VirtualBox Linux

## Documentation

The project includes technical documentation with system architecture, communication protocol, hardware description, software description, database structure, user manual, developer manual, testing description and UML/Mermaid diagrams.

## Author

Author: Viktor Barczi  
Project: IoT Distance Alarm System
