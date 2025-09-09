#!/bin/bash

# sudo snap install arduino-cli
# curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
# sudo mv bin/arduino-cli /usr/local/bin/
# arduino-cli version

# arduino-cli core update-index
# arduino-cli core install esp32:esp32
# sudo apt install libstdc++6 libgcc-s1




BOARD="esp32:esp32:lolin_s2_mini"   # FQBN for ESP32-S2 Mini
EXAMPLES_DIR="../examples"           # relative to script location

passed=0
failed=0

# find all sketch directories containing an .ino file
for sketchdir in $(find "$EXAMPLES_DIR" -name '*.ino' -exec dirname {} \; | sort -u); do
    echo "🔧 Compiling: $sketchdir"
    if arduino-cli compile --fqbn $BOARD "$sketchdir"; then
        echo "✅ Success: $sketchdir"
        ((passed++))
    else
        echo "❌ Failed: $sketchdir"
        ((failed++))
    fi
done

echo "========================"
echo "Compilation finished!"
echo "✅ Passed: $passed"
echo "❌ Failed: $failed"