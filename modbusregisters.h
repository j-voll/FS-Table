#ifndef MODBUSREGISTERS_H
#define MODBUSREGISTERS_H

#include <QString>
#include <QMap>

struct ModbusRegister {
    QString name;
    QString units;
    QString description;
    int multiplier;
    bool readOnly;
};

class ModbusRegisters {
public:
    static QMap<int, ModbusRegister> getRegisters() {
        QMap<int, ModbusRegister> registers;

        // Example definitions (adjust as needed):
        registers[40002] = {"Servo Mode", "", "0=Test pressure mode, 1=Flow mode", 1, false};
        registers[40003] = {"Intake/Exhaust", "", "0=Intake, 1=Exhaust", 1, false};
        registers[40004] = {"AutoZero Now", "", "1=Autozero all channels now", 1, false};
        registers[40005] = {"Pause", "", "0=Unpause, 1=Pause", 1, false};
        registers[40006] = {"Motor On/Off", "", "0=Off, 1=On", 1, false};
        registers[40007] = {"FlowBench ID", "", "Flow bench model", 1, true};
        registers[40008] = {"Flow Pressure", "Current Units", "Flow pressure * 10", 1, true};
        registers[40009] = {"Test Pressure", "Current Units", "Test pressure * 10", 1, true};
        registers[40010] = {"Velocity Pressure", "Current Units", "Velocity pressure * 10", 1, true};
        registers[40011] = {"Barometric Pressure", "Current Units", "Baro * 100", 1, true};
        registers[40012] = {"Temperature #1", "Current Units", "Temp * 10", 1, true};
        registers[40013] = {"Temperature #2", "Current Units", "Temp * 10", 1, true};
        registers[40014] = {"Aux Input", "Current Units", "Aux Input * 10", 1, true};
        registers[40015] = {"Temperature #3", "Current Units", "Temp * 10", 1, true};
        registers[40016] = {"Flow Rate", "Current Units", "Flow rate * 10", 1, true};
        registers[40017] = {"Velocity", "Current Units", "Velocity * 10", 1, true};
        registers[40018] = {"Delta Temperature", "Current Units", "∆T * 10", 1, true};
        registers[40019] = {"Percent Flow", "None", "%Flow * 10", 1, true};
        registers[40020] = {"Swirl", "Current Units", "Swirl * 10", 1, true};
        registers[40021] = {"Frequency", "Hz", "Freq * 10", 1, true};
        registers[40022] = {"Full-scale Flow", "Current Units", "Flow * 10", 1, true};
        registers[40023] = {"Range Setting", "", "1 to MaxRange", 1, false};
        registers[40024] = {"Test Pressure Setting", "Current Units", "Pressure * 100", 1, false};
        registers[40025] = {"Flow Rate Setting", "Current Units", "Flow rate * 10", 1, false};
        registers[40026] = {"Leakage", "Current Units", "Leakage flow rate * 10", 1, false};
        // ... add other registers as needed...

        return registers;
    }
};

#endif // MODBUSREGISTERS_H
