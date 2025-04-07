FlowCom Modbus Control Application
Overview
This application is an industrial control interface designed to communicate with and control flow bench equipment using the Modbus RTU protocol. It provides real-time monitoring of flow data and precise servo control capabilities, making it suitable for testing and calibrating flow-related equipment.
Key Features

Dual Serial Communication: Manages two separate serial connections - one for Modbus communication with the flow bench and another for servo control via Pololu Maestro
Real-time Data Monitoring: Polls and displays values from multiple Modbus registers
Automated Testing Sequence: Runs pre-defined test sequences with programmable PWM steps
Data Logging: Records test data to CSV files with metadata and timestamps
Live Channel Data View: Provides a dedicated dialog for monitoring all channel values simultaneously

Technical Implementation
Modbus Communication
The application implements custom Modbus RTU communication rather than using a pre-built library. This approach provides fine-grained control over the protocol implementation:

```
QByteArray MainWindow::createModbusRequest(uint8_t function, uint16_t registerAddr,
                                           uint16_t numRegisters, uint16_t value)
{
    QByteArray request;
    // Use slave ID 0x1C.
    request.append(static_cast<char>(0x1C));
    request.append(static_cast<char>(function));

    // Subtract MODBUS_BASE (40001) from the register.
    uint16_t adjustedReg = registerAddr - MODBUS_BASE; // e.g. 40016 becomes 15
    request.append(static_cast<char>((adjustedReg >> 8) & 0xFF));
    request.append(static_cast<char>(adjustedReg & 0xFF));
    // ...
}
```

This code constructs Modbus RTU frames manually, handling the register offset conversion (Modbus registers traditionally start at 40001 but are transmitted with a zero-based offset), function codes, and CRC calculation.


Servo Control System
The application interfaces with a Pololu Maestro servo controller to provide precise positioning:

```
QByteArray MainWindow::createMaestroCommand(int channel, int pwmValue)
{
    int target = pwmValue * 4;
    QByteArray command;
    command.append(static_cast<char>(0x84));
    command.append(static_cast<char>(channel));
    command.append(static_cast<char>(target & 0x7F));
    command.append(static_cast<char>((target >> 7) & 0x7F));
    return command;
}
```

This function constructs the specific binary protocol required by the Maestro controller, converting PWM microsecond values (typically 1000-2000μs) to the controller's internal format.


Automated Test Sequence
The application includes a sophisticated test sequence capability that:

Turns on the flow bench motor
Sets the servo to an initial position and holds for stabilization
Incrementally steps through servo positions
Records data at each step
Turns off the motor and resets when complete

```
void MainWindow::onSequenceTimerTick()
{
    const int totalPwmSteps = ((2000 - 1000) / 10) + 1;
    if (currentSequenceStep == 0) {
        // Step 0: Set register 40006 to 1.
        serialPort->write(createModbusRequest(0x06, 40006, 1, 1));
        // ...
    } else if (currentSequenceStep == 1) {
        // Step 1: Set PWM to 1000 and hold 15 seconds.
        // ...
    } 
    // ...
}
```

This state machine implementation handles the entire test sequence, ensuring proper timing and synchronization between servo movements and data capture.


Real-time Data Visualization
The application includes a dedicated dialog for monitoring flow data:

```
void ChannelsDialog::initializeChannels() {
    // Fill channel information. Adjust as necessary.
    m_channels.append({0,  "Flow Pressure",         40008});
    m_channels.append({1,  "Test Pressure",         40009});
    // ...
}
```

This code maps physical channels to Modbus registers and provides user-friendly names for data visualization in a table format, updating at 100Hz.
Technical Architecture
The application is built with Qt 6 and follows a clean object-oriented architecture:

Main Components:

MainWindow: Core UI and control logic
ChannelsDialog: Real-time data visualization
ModbusRegisters: Static registry of available Modbus registers and metadata


Tools & Technologies:

Qt 6 framework (Core, GUI, Widgets, SerialPort)
C++17
CMake build system
Modbus RTU protocol
Pololu Maestro servo control protocol



Use Cases
This application is particularly suited for:

Flow Bench Testing: Measuring and recording flow rates through components at varying aperture settings
Calibration: Establishing baselines and calibration curves for flow equipment
Research & Development: Collecting precise flow data for analysis and product development

Hardware Requirements

Flow bench with Modbus RTU interface (slave ID 0x1C)
Pololu Maestro servo controller
Two available serial ports (or USB-to-serial adapters)

Getting Started

Select appropriate serial ports for both Modbus and servo connections
Connect to both devices
Use manual controls or automatic sequence for testing
Data is logged to CSV for further analysis
