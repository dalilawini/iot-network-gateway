arduino-cli compile   --fqbn esp32:esp32:esp32   --build-property compiler.cpp.extra_flags=" -Idisplay -Iui -Iwireless"   -v .

arduino-cli upload -p COM4 --fqbn esp32:esp32:esp32 .