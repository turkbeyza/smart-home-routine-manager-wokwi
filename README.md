# smart-home-routine-manager-wokwi
AVR C-based Home Routine Management System project developed in Wokwi
# 🏠 Smart Home Routine Manager

A time-based embedded automation system to control home appliances — built with **Arduino Mega** in **Wokwi Simulator**, using **AVR C**.

## 📌 Project Overview
This system allows users to schedule and manually control devices like air conditioners, lights, alarms, and coffee makers using a **keypad** and **OLED display**. It works **offline**, making it reliable, affordable, and ideal for non-tech-savvy users.

## 🧠 Features
- ⏰ Time-based scheduling of appliances
- 🔘 Manual on/off control with keypad
- 📟 OLED display showing status and sensor data
- 🔔 Alarm functionality using buzzer
- 🌡️ Real-time temperature, humidity, and light monitoring

## 🔧 Hardware Components
- Arduino Mega 2560
- DS1307 RTC module (real-time clock)
- DHT-22 temperature & humidity sensor
- LDR (light-dependent resistor)
- OLED Display (I2C)
- 4x4 Keypad
- Buzzer
- LEDs or relays (for appliance simulation)

## 🧑‍💻 Software Details
- **Language**: AVR C
- **Platform**: Wokwi Online Simulator
- **Main Files**:
  - `main.c`: Contains core scheduling logic and device control
  - `diagram.json`: Wokwi wiring configuration
  - `libraries.txt`: External libraries used

## 📸 Project Screenshots
See `/screenshots` folder for images of the system running in simulation.

## 📚 Project Structure
