// COPYRIGHT AND PERMISSION NOTICE

// Copyright (c) 2020 - 2021, Björn Langels, <sm0sbl@langelspost.se>
// All rights reserved.

// Permission to use, copy, modify, and distribute this software for any purpose
// with or without fee is hereby granted, provided that the above copyright
// notice and this permission notice appear in all copies.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.

// Except as contained in this notice, the name of a copyright holder shall not
// be used in advertising or otherwise to promote the sale, use or other dealings
// in this Software without prior written authorization of the copyright holder.

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
//#include <QUdpSocket>
#include <QTcpSocket>
#include <QSerialPort>
#include <QBuffer>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QStandardPaths>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

#define TONE_FREQ 700
#define SAMPLE_RATE 44100
#define FREQ_CONST ((2.0 * M_PI) / ((SAMPLE_RATE*1.0)/(TONE_FREQ*1.0)))
#define SAMPLE_BITS 16
#define TG_MAX_VAL ((pow(2, SAMPLE_BITS)/2)-1)
#define SAMPLE_TIME 10
#define BUFFER_SIZE 8192
//2048

#define MIN(A,B) ((A)<(B)?(A):(B))
#define MAX(A,B) ((A)>(B)?(A):(B))

#define QT_NO_DEBUG_OUTPUT

#define CW_MAX_DELAY 300
#define CW_MIN_DELAY 25

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    // Buttons and fields
    void on_keyButton_released();
    void on_keyButton_pressed();
    void on_toneButton_released();
    void on_toneButton_pressed();
    void on_SetKeyDelay_clicked();
    void on_KeyPortName_textChanged(const QString &arg1);
    void on_sideTone_stateChanged(int arg1);
    void on_keyPortInvert_stateChanged(int arg1);
    void on_verticalSlider_valueChanged(int value);
    void on_toneFreqBox_valueChanged(int arg1);
    void on_ShowComPortList_clicked();
    void on_audioDevice_currentIndexChanged(int index);
    void on_keyPortDevice_currentIndexChanged(const QString &arg1);
    void on_ConnectToKeyNetwork_clicked();
    void on_ConnectToKeyPort_clicked();

    // Functions
    void on_AudioNotify();
    void audioTimerEvent();
    void msEvent();
    void readyReadKeyTcp();
    void readyReadKeySerial();
    void KeyUp();
    void KeyDown();
    void RadioPing();

    void on_keyDelay_valueChanged(int arg1);

private:
    Ui::MainWindow *ui;
    void loadSettings();
    void saveSettings();
    void sort(QList<QSerialPortInfo> list, int column, Qt::SortOrder order = Qt::AscendingOrder);
    void updateComPortList();

    quint32 KeyDeBounceCnt = 0;
    QString SettingsPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString SettingsFile = "remotecwclient.ini";
    QTcpSocket *tcpKeySocket;
    QSerialPort *keySerialPort;
    QByteArray keyPort;
    quint32 hostPort;
    QByteArray hostAddress;
    bool KeyIsDownLast;
    bool keyPortStatus;
    QByteArray* bytebuf;
    QAudioOutput* audio;
    QBuffer* input;
    bool SideToneEnabled;
    quint32 packetDelay = 0;
    quint32 SetKeyDelayCnt = 0;
    unsigned long remdiff = 0;
    QTimer* audioTimer;
    qint32 audioTimerCnt = 0;
    bool TimeToRestartAudio = false;
    bool keyPortInverted = false;
};
#endif // MAINWINDOW_H
