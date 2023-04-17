# arduino-ble-sensors
Project for datalogging and display of critical engine data.
Uses multiple sensors connected to arduino and broadcast over BLE to mobile app. 

This is the arduino firmware component and hardware documenation 

## HW
- Arduino NANO 33 BLE
 datasheet: https://docs.arduino.cc/resources/datasheets/ABX00030-datasheet.pdf

- Temperture sensor: OKAY MOTOR Engine Coolant Temperature Sensor with Pigtail Connector for GM https://www.amazon.com/gp/product/B07GBTBCD4
 
 Sensor is far off from spec sheet so I had to characterize it myself with 3 points
 
 SI used calculator here to generate Steinhart-hart coefficients: http://www.useasydocs.com/theory/ntc.htm

- Wideband: AEM UEGO gauge - tested with regular UEGO and X-series

## Dev env
Uses Platform.io (currently 6.1.6 core) which is a plugin for VS Code (currently using v1.77.3)
 
