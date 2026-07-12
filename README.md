# Analog Clock For ESP32C3 Mini

A remix of two projects, the hardware is taken from the most excellent ESP32-Plane-Radar project, (https://github.com/MatixYo/ESP32-Plane-Radar) 
I suggest you build and get this working before trying this….

The code is forked from this beauty, https://github.com/MilovdZee/Analog_Clock-esp32

Notable changes from the original are;

1) Pin assignments updated to reflect the actual hardware being used.
2) Limit the WiFi power to give a more stable AP and reception
3) All in one script slightly modified to run on Linux Mint.
4) Quickly tapping the BOOT button will cycle on to the next clock face.


## All in one

run `./compile.sh`. This will do the following:
- download the arduino-cli
- download old version of the ESP32 core (otherwise it won't fit)
- create the SPIFFS partition data
- compile the sketch
- upload the data and sketch to the board
- If the script complains that arduino-cli is missing, manually download it from here https://docs.arduino.cc/arduino-cli/installation/ and copy into a newly created bin folder in here.

## Update CA
- echo | openssl s_client -servername raw.githubusercontent.com -connect raw.githubusercontent.com:443 -showcerts | grep -E "subject=|issuer="
- Take highest depth: depth=2 C=US, O=Internet Security Research Group, CN=ISRG Root X1
  => ISRG Root X1
  => cat /etc/ssl/certs/ISRG_Root_X1.pem
- use this cert as the CA

## Setup

- Write the LittleFS
- Write the program using the Arduino IDE
- Connect your computer or phone to th 'RondKlokje' wifi network
- Open http://192.168.4.1
- Goto 'wifi' and enter the credentials

## 3D print files

Use the files from here https://github.com/MatixYo/ESP32-Plane-Radar

## Setup LittleFS filesystem

```
~/.arduino15/packages/esp32/tools/mklittlefs/4.0.2-db0513a/mklittlefs \
  --page 256 \
  --size 0x1d0000 \
  --block 4096 \
  --create data data.img
```

```
~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool \
  --port /dev/ttyACM0 \
  --baud 921600 \
  write_flash 0x230000 data.img

## ToDo
Remove the OTA update code.
```
