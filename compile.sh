#!/bin/bash

PORT=/dev/ttyACM0

# Parse optional flags
NO_WRITE=false
for arg in "$@"; do
  if [ "$arg" == "--noWrite" ]; then
    NO_WRITE=true
  fi
done

PATH=./bin:$PATH
if [[ ! -f bin/arduino-cli ]]; then
  echo "Downloading and installing the arduino-cli..."
  curl -fsSL https://githubusercontent.com | sh
fi

# TOTAL CACHE ERASE
# This clears the global cache that was causing the symbol mismatch
echo "Erasing all Arduino caches..."
rm -rf ~/.cache/arduino/sketches
rm -rf ~/.cache/arduino/cores
rm -rf ./build_cache

# Isolated Local Storage for Tools
LOCAL_DATA="/tmp/arduino_clock_cli"
mkdir -p "$LOCAL_DATA"
mkdir -p "./build_cache"
CLI_CONFIG="--config-dir $LOCAL_DATA"

# Ensure core 2.0.17 is installed locally
if [[ ! -d "$LOCAL_DATA/packages/esp32/hardware/esp32/2.0.17" ]]; then
  echo "Installing private ESP32 Core 2.0.17..."
  arduino-cli $CLI_CONFIG core update-index
  arduino-cli $CLI_CONFIG core install esp32:esp32@2.0.17
fi

echo "Creating data image for in SPIFFS partition..."
/tmp/arduino_clock_cli/packages/esp32/tools/mklittlefs/3.0.0-gnu12-dc7f933/mklittlefs \
  --page 256 \
  --size 0x1d0000 \
  --block 4096 \
  --create data data.img

if [ "$NO_WRITE" = false ]; then
  echo "Write data image to SPIFFS partition..."
  python3 /tmp/arduino_clock_cli/packages/esp32/tools/esptool_py/4.5.1/esptool.py \
    --port $PORT \
    --baud 921600 \
    write_flash 0x230000 data.img
else
  echo "Skipping SPIFFS flash (--noWrite enabled)"
fi

# Compile with clean build path
echo "Compiling..."
arduino-cli $CLI_CONFIG compile --fqbn esp32:esp32:esp32c3 \
  -j 0 \
  --build-path "./build_cache" \
  --libraries ~/Arduino/libraries \
  --build-property "build.extra_flags=-D CORE_DEBUG_LEVEL=0 -D BitOrder=uint8_t" \
  --build-property "build.partitions=partitions" \
  ./Analog_Clock-esp32c3.ino

if [ "$NO_WRITE" = false ]; then
  echo "Uploading firmware..."
  arduino-cli $CLI_CONFIG upload \
    --fqbn esp32:esp32:esp32c3 \
    --port $PORT \
    --input-dir "./build_cache"
else
  echo "Skipping firmware flash (--noWrite enabled)"
fi

