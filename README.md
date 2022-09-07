# rack-monitor

A tool intended to run on embedded systems to:

1. Read temperature values from four DS18B20 temperature sensors;
1. Control two Sunon cooling fans based on ambient and cabinet temperature readings with a IRF520 MOS Driver Module;
1. Detect cabinet door opening with a reed switch (a.k.a. door sensor);
1. Read from an USB IPCam and show real-time image;

## Environment and dependency

### Back-end

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

### Front-end

* `npm install`
* `node babelify.js --prod`


## Gallery

### Software

<p float="left">
    <img src="./assets/desktop.png" width="500px" alt="Desktop GUI" />    
    <img src="./assets/smartphone.png" width="250px" alt="Mobile GUI" />
</p>

### Hardware

#### Wiring Diagram
##### Door sensor
<img src="./assets/wiring-diagram.png" alt="Wiring Diagram" />

##### Fans controller
Undocumented

#### Raspberry Pi Rack Mount Design

<img src="./assets/rackmount-design.png" alt="Raspberry Pi Rackmount Design" />

#### Components
##### IRF520 MOS Driver Module
<img src="./assets/mos-driver-module.jpeg" alt="IRF520 MOS Driver Module" width="300" />

##### Sunon Cooling Fan x2
<img src="./assets/sunon-cooling-fan.jpg" alt="Sunon Cooling Fan" width="300" />

##### Door sensor
<img src="./assets/door-sensor.jpg" alt="Door sensor" width="300" />

#### Installation
##### Raspberry Pi and its rackmount
<img src="./assets/raspberry-pi.jpg" height="450" alt="Raspberry Pi" />    

##### IPCam
<img src="./assets/ipcam.jpg" height="450" alt="IPCam" />

##### Cooling Fans
<img src="./assets/cooling-fans.jpg" height="450" alt="Cooling Fans" />
