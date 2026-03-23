# GPS to ROS Odometry Pipeline (ESP32 + ROS 2)

This project implements an end-to-end GPS data pipeline from an ESP32 to ROS 2 for real-time odometry estimation.

The system reads raw NMEA data from a GPS module, parses and filters valid position data on the ESP32, and streams it over UART to a ROS 2 system, where it is converted into odometry and published with TF transforms.

The focus is on reliable serial communication, structured parsing, and integration between embedded firmware and robotics software.

---

## Features

- UART-based GPS interface on ESP32 (9600 baud)
- NMEA sentence parsing using `minmea` library
- Checksum validation for robust data filtering
- Fusion of RMC and GGA sentences for complete GPS fix
- Custom lightweight serial protocol (`$GPS,lat,lon,alt,sats,speed`)
- ROS 2 integration via serial bridge node
- Conversion from GPS coordinates to local Cartesian (ENU frame)
- Real-time odometry and TF publishing

---

## System Architecture

GPS Module → ESP32 (UART1) → Serial Output (UART0) → ROS 2 Computer → Serial Reader Node → Gps Odometry Node → Odometry + TF

### ESP32 (Firmware)
- Reads NMEA data over UART
- Validates checksum
- Parses RMC (lat/lon/speed) and GGA (altitude/satellites)
- Combines both into a single GPS fix
- Transmits formatted data over UART

### ROS 2 Layer
- **serial_reader node**
  - Reads UART data and publishes `/serial_data`

- **gps_odometry node**
  - Subscribes to /serial_data
  - Parses GPS data
  - Converts lat/lon → local Cartesian coordinates
  - Estimates velocity and heading
  - Publishes `/gps_odom` and TF transforms
 
---

## Key Design Decisions

- **Checksum Validation**
  Ensures corrupted NMEA sentences are discarded before parsing.

- **RMC + GGA Fusion**
  Position (RMC) and altitude/satellites (GGA) are combined to form a complete and reliable GPS fix.

- **Streaming Parser Design**
  UART data is processed character-by-character to reconstruct complete NMEA sentences, avoiding buffer overflows and partial reads.

- **Custom Serial Protocol**
  Parsed GPS data is transmitted as a compact `$GPS,...` string for easy parsing on the ROS side.
  <br />**Format: $GPS,lat,lon,alt,satellites,speed**

- **Local Coordinate Conversion**
  Latitude/longitude is converted to a local ENU frame using a fixed origin for compatibility with robotics applications.

- **Velocity-Based Heading Estimation**
  Robot orientation is derived from position change over time instead of relying on GPS heading.


---

## Hardware

| Component | Pin/Connection | Remarks |
|-----------|----------------|---------|
| **ESP32** | – | Main microcontroller |
| **GPS Module** | – | UART output (NMEA, 9600 baud) |
| **GPS TX** → **ESP32 RX** | GPIO16 (UART1_RXD) | Receives NMEA sentences |
| **GPS RX** ← **ESP32 TX** | GPIO17 (UART1_TXD) | (Optional) command output to GPS |
| **ESP32 UART0** | GPIO1 (TXD), GPIO3 (RXD) | Used to send parsed data to ROS computer (115200 baud) |

---

## Future Improvements

- Sensor fusion with IMU (EKF / complementary filter)
- Use of GPS covariance for better odometry estimation
- Non-blocking UART handling with queues
- Integration with ROS navigation stack
