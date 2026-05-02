name: Build LV-nordpool-monitor

on:
  workflow_dispatch:
  push:
    branches: [ main, master ]
  pull_request:
    branches: [ main, master ]

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout source
        uses: actions/checkout@v4

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'

      - name: Cache PlatformIO
        uses: actions/cache@v4
        with:
          path: |
            ~/.cache/pip
            ~/.platformio/.cache
            ~/.platformio/packages
            ~/.platformio/platforms
          key: ${{ runner.os }}-pio-${{ hashFiles('platformio.ini') }}
          restore-keys: |
            ${{ runner.os }}-pio-

      - name: Install PlatformIO
        run: pip install --upgrade platformio

      - name: Show PlatformIO version
        run: pio --version

      - name: Clean build directory
        run: rm -rf .pio/build

      - name: Build firmware
        run: pio run -e LV-nordpool-monitor

      - name: Build merged flash image
        run: pio run -e LV-nordpool-monitor -t build_merged

      - name: Show build artifacts
        run: ls -la .pio/build/LV-nordpool-monitor/

      - name: Upload firmware artifacts
        uses: actions/upload-artifact@v4
        with:
          name: LV-nordpool-monitor
          path: |
            .pio/build/LV-nordpool-monitor/firmware.bin
            .pio/build/LV-nordpool-monitor/merged-flash.bin
            .pio/build/LV-nordpool-monitor/bootloader.bin
            .pio/build/LV-nordpool-monitor/partitions.bin
          if-no-files-found: warn
