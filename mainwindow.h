#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QTimer>
#include <QPushButton>
#include <QComboBox>
#include <QFile>
#include <QTextStream>
#include <QMap>
#include <QList>
#include <QDialog>
#include <QTableWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

//----------------------
// Helper structure for channels
struct ChannelInfo {
    int channel;
    QString name;
    int registerNumber; // The Modbus register corresponding to this channel.
};

//----------------------
// Live Channels Dialog
class ChannelsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChannelsDialog(QMap<int,int>* modbusData, QWidget *parent = nullptr);
private slots:
    void updateTable();
private:
    QTableWidget *m_table;
    QTimer *m_timer;
    QMap<int,int>* m_modbusData;
    QList<ChannelInfo> m_channels;
    void initializeChannels();
};

//----------------------
// MainWindow Declaration
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Modbus slots:
    void onConnectButtonClicked();
    void onSerialDataReceived();
    void onUpdateTimer();
    void writeRegister();
    void readRegisters();

    // Autosequence control:
    void runAutoSequence();
    void stopAutoSequence();
    void onSequenceTimerTick(); // Autosequence steps timer
    void captureAndDisablePolling(); // Called at the end of each hold period

    // Maestro servo command slots (renamed for auto-connection):
    void on_pwm1000Button_clicked();
    void on_pwm1500Button_clicked();
    void on_pwm2000Button_clicked();
    void on_incrementButton_clicked();

    // Servo connection slot:
    void onServoConnectButtonClicked();

    // CSV logging – called at each autosequence hold capture.
    void onDataLogTimerTick();

    // New: View Channels slot.
    void onViewChannelsButtonClicked();

private:
    Ui::MainWindow *ui;
    // Modbus-related members:
    QSerialPort *serialPort;
    QTimer *updateTimer;      // Used for polling Modbus registers
    QTimer *sequenceTimer;    // Used for the autosequence steps
    bool connected;
    bool sequenceRunning;
    int currentSequenceStep;  // Tracks which step of the autosequence we're in

    // For sequentially polling all registers:
    QList<int> allRegisters;  // List of register numbers from ModbusRegisters
    int currentPollIndex;     // Index into allRegisters
    int currentPollRegister;  // The register number currently being polled

    // Buffer for accumulating incoming Modbus data.
    QByteArray modbusBuffer;

    // Servo-related members:
    QSerialPort *servoPort;
    QComboBox *servoPortCombo;
    QPushButton *servoConnectButton;
    int currentServoPWM;  // in microseconds

    // Maestro servo command UI elements:
    QPushButton *pwm1000Button;
    QPushButton *pwm1500Button;
    QPushButton *pwm2000Button;
    QPushButton *incrementButton;

    // CSV logging members:
    QFile *dataLogFile;
    QTextStream *dataLogStream;

    // For storing the latest Modbus register values:
    QMap<int,int> modbusData;  // key: register number, value: last read value

    void setupUi();
    void scanPorts();
    QByteArray createModbusRequest(uint8_t function, uint16_t registerAddr,
                                   uint16_t numRegisters = 1, uint16_t value = 0);
    uint16_t calculateCRC(const QByteArray &data);
    void processModbusResponse(const QByteArray &response);

    // Maestro command creation:
    QByteArray createMaestroCommand(int channel, int pwmValue);
};

#endif // MAINWINDOW_H
