#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "modbusregisters.h"

#include <QSerialPortInfo>
#include <QMessageBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QDateTime>
#include <QTimer>

static const uint16_t MODBUS_BASE = 40001;

// Implementation of ChannelsDialog
ChannelsDialog::ChannelsDialog(QMap<int,int>* modbusData, QWidget *parent)
    : QDialog(parent), m_modbusData(modbusData)
{
    setWindowTitle("Live Channel Data");
    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels(QStringList() << "Channel" << "Name" << "Value");
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_table);
    setLayout(layout);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ChannelsDialog::updateTable);
    m_timer->start(10); // 100 Hz update
    initializeChannels();
}

void ChannelsDialog::initializeChannels() {
    // Fill channel information. Adjust as necessary.
    m_channels.append({0,  "Flow Pressure",         40008});
    m_channels.append({1,  "Test Pressure",         40009});
    m_channels.append({2,  "Velocity Pressure",     40010});
    m_channels.append({3,  "Barometric Pressure",   40011});
    m_channels.append({4,  "Temperature #1",        40012});
    m_channels.append({5,  "Temperature #2",        40013});
    m_channels.append({6,  "Aux Input",             40014});
    m_channels.append({7,  "Temperature #3",        40015});
    m_channels.append({8,  "Flow Rate",             40016});
    m_channels.append({9,  "Velocity",              40017});
    m_channels.append({10, "Delta Temperature",     40018});
    m_channels.append({11, "Percent Flow",          40019});
    m_channels.append({12, "Swirl",                 40020});
    m_channels.append({13, "Frequency",             40021});
    m_table->setRowCount(m_channels.size());
}

void ChannelsDialog::updateTable() {
    for (int i = 0; i < m_channels.size(); i++) {
        const ChannelInfo &info = m_channels[i];
        QTableWidgetItem *itemChannel = new QTableWidgetItem(QString::number(info.channel));
        QTableWidgetItem *itemName = new QTableWidgetItem(info.name);
        int value = m_modbusData->value(info.registerNumber, -1);
        QTableWidgetItem *itemValue = new QTableWidgetItem(QString::number(value));
        m_table->setItem(i, 0, itemChannel);
        m_table->setItem(i, 1, itemName);
        m_table->setItem(i, 2, itemValue);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , serialPort(new QSerialPort(this))
    , servoPort(new QSerialPort(this))
    , updateTimer(new QTimer(this))
    , sequenceTimer(new QTimer(this))
    , connected(false)
    , sequenceRunning(false)
    , currentSequenceStep(0)
    , currentServoPWM(1000)
    , dataLogFile(nullptr)
    , dataLogStream(nullptr)
    , currentPollIndex(0)
    , currentPollRegister(-1)
{
    ui->setupUi(this);
    setupUi();

    // Setup servo port combo
    ui->servoPortCombo->clear();
    const auto servoPorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : servoPorts) {
        ui->servoPortCombo->addItem(info.portName());
    }
    int index = ui->servoPortCombo->findText("COM4");
    if (index != -1)
        ui->servoPortCombo->setCurrentIndex(index);
    connect(ui->servoConnectButton, &QPushButton::clicked,
            this, &MainWindow::onServoConnectButtonClicked);

    // Modbus Connections
    connect(serialPort, &QSerialPort::readyRead, this, &MainWindow::onSerialDataReceived);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
    connect(ui->writeButton, &QPushButton::clicked, this, &MainWindow::writeRegister);
    connect(ui->readButton, &QPushButton::clicked, this, &MainWindow::readRegisters);

    // Autosequence Controls
    connect(ui->startSequenceButton, &QPushButton::clicked, this, &MainWindow::runAutoSequence);
    connect(ui->stopSequenceButton, &QPushButton::clicked, this, &MainWindow::stopAutoSequence);
    connect(sequenceTimer, &QTimer::timeout, this, &MainWindow::onSequenceTimerTick);

    updateTimer->setInterval(1000);

    // Initialize polling of all registers
    allRegisters = ModbusRegisters::getRegisters().keys();
    currentPollIndex = 0;
    currentPollRegister = allRegisters.isEmpty() ? -1 : allRegisters.first();

    // Note: Duplicate signal connections have been removed.
}

MainWindow::~MainWindow()
{
    if (serialPort->isOpen())
        serialPort->close();
    if (servoPort->isOpen())
        servoPort->close();

    if (dataLogStream) {
        dataLogStream->flush();
        delete dataLogStream;
    }
    if (dataLogFile) {
        dataLogFile->close();
        delete dataLogFile;
    }
    delete ui;
}

void MainWindow::setupUi()
{
    ui->baudRateCombo->addItem("9600");
    ui->baudRateCombo->addItem("19200");

    auto registers = ModbusRegisters::getRegisters();
    for (auto it = registers.begin(); it != registers.end(); ++it) {
        ui->registerCombo->addItem(QString("%1 - %2").arg(it.key()).arg(it.value().name), it.key());
    }
    scanPorts();
}

void MainWindow::scanPorts()
{
    ui->portCombo->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ui->portCombo->addItem(info.portName());
    }
}

void MainWindow::onConnectButtonClicked()
{
    if (!connected) {
        serialPort->setPortName(ui->portCombo->currentText());
        serialPort->setBaudRate(ui->baudRateCombo->currentText().toInt());
        serialPort->setDataBits(QSerialPort::Data8);
        serialPort->setParity(QSerialPort::NoParity);
        serialPort->setStopBits(QSerialPort::OneStop);

        if (serialPort->open(QIODevice::ReadWrite)) {
            connected = true;
            ui->connectButton->setText("Disconnect");
            updateTimer->start();
        } else {
            QMessageBox::critical(this, "Error", "Failed to open serial port");
        }
    } else {
        updateTimer->stop();
        serialPort->close();
        connected = false;
        ui->connectButton->setText("Connect");
    }
}

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

    if (function == 0x06) {
        request.append(static_cast<char>((value >> 8) & 0xFF));
        request.append(static_cast<char>(value & 0xFF));
    } else {
        request.append(static_cast<char>((numRegisters >> 8) & 0xFF));
        request.append(static_cast<char>(numRegisters & 0xFF));
    }

    uint16_t crc = calculateCRC(request);
    request.append(static_cast<char>(crc & 0xFF));
    request.append(static_cast<char>((crc >> 8) & 0xFF));
    return request;
}

uint16_t MainWindow::calculateCRC(const QByteArray &data)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < data.size(); pos++) {
        crc ^= static_cast<uint8_t>(data.at(pos));
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void MainWindow::writeRegister()
{
    if (!connected)
        return;
    int reg = ui->registerCombo->currentData().toInt();
    int value = ui->valueSpinBox->value();
    QByteArray request = createModbusRequest(0x06, reg, 1, value);
    serialPort->write(request);
}

void MainWindow::readRegisters()
{
    if (!connected)
        return;
    int reg = ui->registerCombo->currentData().toInt();
    QByteArray request = createModbusRequest(0x03, reg, 1);
    serialPort->write(request);
}

void MainWindow::onSerialDataReceived()
{
    modbusBuffer.append(serialPort->readAll());
    while (true) {
        if (modbusBuffer.size() < 7)
            break;
        if (static_cast<uint8_t>(modbusBuffer.at(0)) != 0x1C) {
            modbusBuffer.remove(0, 1);
            continue;
        }
        uint8_t function = static_cast<uint8_t>(modbusBuffer.at(1));
        int expectedLength = 0;
        if (function == 0x03) {
            if (modbusBuffer.size() < 3)
                break;
            uint8_t byteCount = static_cast<uint8_t>(modbusBuffer.at(2));
            expectedLength = 3 + byteCount + 2;
        } else if (function == 0x06) {
            expectedLength = 8;
        } else {
            modbusBuffer.remove(0, 1);
            continue;
        }
        if (modbusBuffer.size() < expectedLength)
            break;
        QByteArray completeMsg = modbusBuffer.left(expectedLength);
        modbusBuffer.remove(0, expectedLength);
        processModbusResponse(completeMsg);
    }
}

void MainWindow::processModbusResponse(const QByteArray &response)
{
    if (response.size() < 7)
        return;
    uint16_t receivedCRC = (static_cast<uint8_t>(response.at(response.size()-1)) << 8) |
                           static_cast<uint8_t>(response.at(response.size()-2));
    QByteArray data = response.left(response.size()-2);
    uint16_t calculatedCRC = calculateCRC(data);
    if (receivedCRC != calculatedCRC) {
        qDebug() << "CRC error. Raw response:" << response.toHex();
        ui->statusBar->showMessage("CRC Error", 2000);
        return;
    }
    uint8_t function = static_cast<uint8_t>(response.at(1));
    if (function & 0x80) {
        qDebug() << "Exception response:" << response.toHex();
        ui->statusBar->showMessage("Modbus Exception", 2000);
        return;
    }
    if (function == 0x03) {
        uint16_t value = (static_cast<uint8_t>(response.at(3)) << 8) |
                         static_cast<uint8_t>(response.at(4));
        if (currentPollRegister == 40016)
            qDebug() << "Polled 40016. Raw response:" << response.toHex() << "Calculated value:" << value;
        modbusData[currentPollRegister] = value;
        int selectedReg = ui->registerCombo->currentData().toInt();
        if (selectedReg == currentPollRegister)
            ui->valueLabel->setText(QString::number(value));
    }
}

void MainWindow::onUpdateTimer()
{
    if (!allRegisters.isEmpty()) {
        currentPollRegister = allRegisters[currentPollIndex];
        QByteArray request = createModbusRequest(0x03, currentPollRegister, 1);
        serialPort->write(request);
        currentPollIndex = (currentPollIndex + 1) % allRegisters.size();
    }
}

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

// --- Servo Control Slots (renamed for auto-connection) ---

void MainWindow::on_pwm1000Button_clicked()
{
    currentServoPWM = 1000;
    QByteArray cmd = createMaestroCommand(0, currentServoPWM);
    if (servoPort->isOpen()) {
        servoPort->write(cmd);
        ui->binaryDisplay->append(QString("Sent PWM 1000: %1").arg(cmd.toHex(' ')));
    } else {
        QMessageBox::warning(this, "Not Connected", "Servo port not connected");
    }
}

void MainWindow::on_pwm1500Button_clicked()
{
    currentServoPWM = 1500;
    QByteArray cmd = createMaestroCommand(0, currentServoPWM);
    if (servoPort->isOpen()) {
        servoPort->write(cmd);
        ui->binaryDisplay->append(QString("Sent PWM 1500: %1").arg(cmd.toHex(' ')));
    } else {
        QMessageBox::warning(this, "Not Connected", "Servo port not connected");
    }
}

void MainWindow::on_pwm2000Button_clicked()
{
    currentServoPWM = 2000;
    QByteArray cmd = createMaestroCommand(0, currentServoPWM);
    if (servoPort->isOpen()) {
        servoPort->write(cmd);
        ui->binaryDisplay->append(QString("Sent PWM 2000: %1").arg(cmd.toHex(' ')));
    } else {
        QMessageBox::warning(this, "Not Connected", "Servo port not connected");
    }
}

void MainWindow::on_incrementButton_clicked()
{
    currentServoPWM++;
    QByteArray cmd = createMaestroCommand(0, currentServoPWM);
    if (servoPort->isOpen()) {
        servoPort->write(cmd);
        ui->binaryDisplay->append(QString("Sent PWM %1: %2")
                                      .arg(currentServoPWM)
                                      .arg(cmd.toHex(' ')));
    } else {
        QMessageBox::warning(this, "Not Connected", "Servo port not connected");
    }
}

void MainWindow::onServoConnectButtonClicked()
{
    if (!servoPort->isOpen()) {
        // Use the QComboBox "servoPortCombo" from your UI for the servo port.
        servoPort->setPortName(ui->servoPortCombo->currentText());
        servoPort->setBaudRate(9600);  // Adjust the baud rate as needed.
        servoPort->setDataBits(QSerialPort::Data8);
        servoPort->setParity(QSerialPort::NoParity);
        servoPort->setStopBits(QSerialPort::OneStop);
        if (servoPort->open(QIODevice::ReadWrite)) {
            ui->servoConnectButton->setText("Disconnect Servo");
        } else {
            QMessageBox::critical(this, "Error", "Failed to open servo serial port");
        }
    } else {
        servoPort->close();
        ui->servoConnectButton->setText("Connect Servo");
    }
}


void MainWindow::onSequenceTimerTick()
{
    const int totalPwmSteps = ((2000 - 1000) / 10) + 1;
    if (currentSequenceStep == 0) {
        // Step 0: Set register 40006 to 1.
        serialPort->write(createModbusRequest(0x06, 40006, 1, 1));
        ui->binaryDisplay->append("Autosequence Step 0: Set register 40006 to 1");
        currentSequenceStep = 1;
        // Wait 1 second before moving to the next step.
        QTimer::singleShot(1000, this, SLOT(onSequenceTimerTick()));
    } else if (currentSequenceStep == 1) {
        // Step 1: Set PWM to 1000 and hold 15 seconds.
        currentServoPWM = 1000;
        servoPort->write(createMaestroCommand(0, currentServoPWM));
        ui->binaryDisplay->append("Autosequence Step 1: Set PWM to 1000");
        currentSequenceStep = 2;
        updateTimer->setInterval(50);
        updateTimer->start();
        // Wait 15 seconds before capturing data.
        QTimer::singleShot(15000, this, SLOT(captureAndDisablePolling()));
    } else if (currentSequenceStep < totalPwmSteps + 1) {
        // Steps 2 to totalPwmSteps: increment PWM by 10, hold 5 seconds.
        currentServoPWM = 1000 + (currentSequenceStep - 1) * 10;
        servoPort->write(createMaestroCommand(0, currentServoPWM));
        ui->binaryDisplay->append(QString("Autosequence Step %1: Set PWM to %2")
                                      .arg(currentSequenceStep)
                                      .arg(currentServoPWM));
        currentSequenceStep++;
        updateTimer->setInterval(50);
        updateTimer->start();
        // Wait 5 seconds before capturing data.
        QTimer::singleShot(5000, this, SLOT(captureAndDisablePolling()));
    } else if (currentSequenceStep == totalPwmSteps + 1) {
        // Final Step: Capture data, then set register 40006 to 0 and reset PWM.
        captureAndDisablePolling();
        serialPort->write(createModbusRequest(0x06, 40006, 1, 0));
        ui->binaryDisplay->append("Autosequence Final Step: Set register 40006 to 0");
        currentServoPWM = 1000;
        servoPort->write(createMaestroCommand(0, currentServoPWM));
        ui->binaryDisplay->append("Autosequence Final Step: Reset PWM to 1000");
        stopAutoSequence();
    }
}

void MainWindow::captureAndDisablePolling()
{
    onDataLogTimerTick();
    updateTimer->stop();
    // Delay a short moment (100ms) then trigger the next sequence step.
    QTimer::singleShot(100, this, SLOT(onSequenceTimerTick()));
}

void MainWindow::onDataLogTimerTick()
{
    QString timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    QStringList fields;
    fields << QString::number(currentServoPWM) << timestamp;

    auto regs = ModbusRegisters::getRegisters();
    for (auto it = regs.begin(); it != regs.end(); ++it) {
        int regNumber = it.key();
        int regValue = modbusData.value(regNumber, -1);
        if (regNumber == 40016 && regValue != -1) {
            double adjustedValue = regValue / 10.0;
            fields << QString::number(adjustedValue);
        } else {
            fields << QString::number(regValue);
        }
    }
    if (dataLogStream) {
        *dataLogStream << fields.join(",") << "\n";
        dataLogStream->flush();
    }
}

void MainWindow::runAutoSequence()
{
    if (!connected)
        return;
    sequenceRunning = true;
    currentSequenceStep = 0;

    // Retrieve metadata from UI fields:
    QString csvFileName = ui->fileNameLineEdit->text();
    if(csvFileName.isEmpty()) {
        csvFileName = "autosequence_log.csv";
    }
    QString serialNumber = ui->serialNumberLineEdit->text();
    QString csvType = ui->csvTypeComboBox->currentText();

    // Open the CSV file using the specified (or default) file name.
    dataLogFile = new QFile(csvFileName);
    if (dataLogFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        dataLogStream = new QTextStream(dataLogFile);

        // Write metadata as header comments (optional):
        *dataLogStream << "# Serial Number: " << serialNumber << "\n";
        *dataLogStream << "# CSV Type: " << csvType << "\n";

        // Write the column header row.
        auto regs = ModbusRegisters::getRegisters();
        QStringList header;
        header << "PWM" << "Timestamp";
        // Use the register name instead of the register number.
        for (auto it = regs.begin(); it != regs.end(); ++it) {
            header << it.value().name;
        }
        *dataLogStream << header.join(",") << "\n";
        dataLogStream->flush();
    } else {
        QMessageBox::warning(this, "Log Error", "Unable to open CSV log file for writing.");
    }
    // Kick off the autosequence immediately.
    QTimer::singleShot(0, this, SLOT(onSequenceTimerTick()));
}


void MainWindow::stopAutoSequence()
{
    sequenceRunning = false;
    sequenceTimer->stop();
    if (dataLogStream) {
        dataLogStream->flush();
        delete dataLogStream;
        dataLogStream = nullptr;
    }
    if (dataLogFile) {
        dataLogFile->close();
        delete dataLogFile;
        dataLogFile = nullptr;
    }
}

void MainWindow::onViewChannelsButtonClicked()
{
    ChannelsDialog *dialog = new ChannelsDialog(&modbusData, this);
    dialog->exec();
}
