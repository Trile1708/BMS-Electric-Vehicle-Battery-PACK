Battery Management System for Electric Vehicle Battery Pack
Overview

This project develops a Battery Management System (BMS) for an electric vehicle battery pack using an LFP 15S5P configuration. The system is designed to monitor, protect, and manage the battery pack during charging and discharging operation.

The BMS uses the BQ76940 battery monitoring IC to measure individual cell voltages, pack voltage, current, and temperature. An STM32G030 microcontroller is used as the main controller to process battery data, detect faults, estimate battery status, and control the charge/discharge MOSFETs.

An ESP32 module is integrated for wireless monitoring. It receives data from the STM32 through UART and sends the system parameters to Blynk and Google Sheets for real-time monitoring and data logging.

Main Features
Monitor 15 series-connected LFP cells
Measure pack voltage, cell voltage, current, and temperature
Estimate battery State of Charge (SOC)
Detect overvoltage, undervoltage, overcurrent, short circuit, and overtemperature faults
Control charge and discharge MOSFETs for battery protection
Support cell balancing
Send monitoring data to Blynk and Google Sheets through ESP32
System Architecture

The positive terminal of the battery pack is connected directly to the charger or load power line. The negative terminal passes through the BMS, including the current shunt and MOSFET protection circuit. This design allows the BMS to measure charge/discharge current and disconnect the battery pack when a fault occurs.

Current Project Status

The charging mode has been tested successfully, including voltage measurement, current monitoring, temperature monitoring, UART communication, Blynk display, and Google Sheets data logging.

The discharge test with the electric motor has not been completed yet because the motor control system is still under development.

Hardware Used
LFP 15S5P battery pack
BQ76940 battery monitoring IC
STM32G030 microcontroller
ESP32 Wi-Fi module
Current shunt resistor
Charge/discharge MOSFET circuit
Temperature sensors


Author

This project is developed by Le Phuc Tri-K22 HCMUTE student as part of an automotive engineering student project focused on battery management for electric vehicle applications.
