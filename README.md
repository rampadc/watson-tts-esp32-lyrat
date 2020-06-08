# Watson Text-to-Speech with ESP32-ADF

[![License](https://img.shields.io/badge/License-Apache2-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0) 

A super basic example of [ESP32-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/index.html) using IBM Watson Text-to-Speech. This README will be improved.

## Installation

Install [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) and [ESP32-ADF](https://docs.espressif.com/projects/esp-adf/en/latest/index.html) using the `latest`/master branch, and exporting the environment variables before trying out this repo.

## Running it

1. Run `idf.py menuconfig` to configure IBM Watson Text-to-Speech by filling out the endpoint, basic auth string (containing the APIKey) and voice selection.
2. Run `idf.py build` to build.
3. Run `idf.py flash -p [PORT]` to flash to your device. Hold Boot, and press Reset once to instantiate flashing if something resembles `Connecting ....._____...` is shown.
4. RUn `idf.py monitor -p [PORT]` to watch debug logs.

This repo was built using ESP32-LyraT board.

## Improving it

Lots of code were copied and pasted from ESP-ADF examples to get this to barely work.

Known problems:

- Fuzzy/overlapped audio at the start, sounding like frames were not being buffered correctly

## License

This project is licensed under the Apache 2 License - see the [LICENSE](LICENSE) file for details
