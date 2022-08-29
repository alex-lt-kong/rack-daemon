# rack-monitor

A tool intended to run on embedded systems to:

1. Read temperature values from four DS18B20 temperature sensors;
1. Control two Sunon cooling fans based on ambient and cabinet temperature readings with a IRF520 MOS Driver Module;
1. Detect cabinet door opening with a reed switch (a.k.a. door sensor);
1. Read from an USB IPCam and show real-time image;

## Environment and dependency

* Common libs: `apt install libsqlite3-dev`

* `Pigpio`: used to manipulate GPIO pins.
```
git clone https://github.com/joan2937/pigpio
cd ./pigpio
mkdir ./build
cd ./build
cmake ../
make
make install
```

* Add path to `LD_LIBRARY_PATH`: `export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/:/usr/local/lib/aarch64-linux-gnu/`.

* `OpenCV`: Install following [this link](https://github.com/alex-lt-kong/q-rtsp-viewer#opencv-installation-and-reference).


# Software

<p float="left">
    <img src="./images/desktop.png" height="650" alt="Desktop GUI" />    
    <img src="./images/smartphone.png" height="650" alt="Mobile GUI" />
</p>

# Hardware

## Wiring Diagram
### Door sensor
<img src="./images/wiring-diagram.png" alt="Wiring Diagram" />

### Fans controller
Undocumented

## Raspberry Pi Rack Mount Design

<img src="./images/rackmount-design.png" alt="Raspberry Pi Rackmount Design" />

## Components
### IRF520 MOS Driver Module
<img src="./images/mos-driver-module.jpeg" alt="IRF520 MOS Driver Module" width="300" />

### Sunon Cooling Fan x2
<img src="./images/sunon-cooling-fan.jpg" alt="Sunon Cooling Fan" width="300" />

### Door sensor
<img src="./images/door-sensor.jpg" alt="Door sensor" width="300" />

## Installation
### Raspberry Pi and its rackmount
<img src="./images/raspberry-pi.jpg" height="450" alt="Raspberry Pi" />    

### IPCam
<img src="./images/ipcam.jpg" height="450" alt="IPCam" />

### Cooling Fans
<img src="./images/cooling-fans.jpg" height="450" alt="Cooling Fans" />

### Temperature sensor

### Door sensor



