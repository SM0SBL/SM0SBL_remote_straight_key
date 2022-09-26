// COPYRIGHT AND PERMISSION NOTICE

// Copyright (c) 2020 - 2021, Bjorn Langels, <sm0sbl@langelspost.se>
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

#include <QDateTime>
#include <QTimer>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QtMath>
#include <QBuffer>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QSettings>
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <algorithm>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // Setup a program Icon
    setWindowIcon(QIcon(":/icon/Images/MorseKeyIcon_3.png"));

    // Setup Key TCP connection
    keyPortStatus = false;
    tcpKeySocket = new QTcpSocket(this);
    tcpKeySocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(tcpKeySocket,
            SIGNAL(readyRead()),
            this,
            SLOT(readyReadKeyTcp()));

    // Setup serial port for key up/down detection
    keySerialPort = new QSerialPort(this);
    connect(keySerialPort,
            SIGNAL(readyRead()),
            this,
            SLOT(readyReadKeySerial()));

    // Setupt a millisecond timer to poll key serial port regularely
    QTimer* timer = new QTimer(this);
    timer->connect(timer,
                   SIGNAL(timeout()),
                   this,
                   SLOT(msEvent()));
    timer->start(1); //, Qt::PreciseTimer, timeout());

    // Setup audio to be used as side tone. This is a workaround
    // Recommended to find out how to generate the tone as a stream
    // instead of creating a short 'file' repeated over and over again.
    // My implementaiton gives pops and hickups in audio at other frequencies
    // than 700Hz
    bytebuf = new QByteArray();
    bytebuf->resize(SAMPLE_TIME * SAMPLE_RATE * (SAMPLE_BITS/8));
    quint16 T;
    for (int i=0; i<(SAMPLE_TIME * SAMPLE_RATE * (SAMPLE_BITS/8)); i++) {
        qreal t;// = (qreal)(TONE_FREQ * i);
        t = (qreal)((qSin(i*FREQ_CONST))*(TG_MAX_VAL));
        // Normalize t and convert to signed qint32
        T = (qint16)t;
        if ( i < (SAMPLE_RATE/TONE_FREQ)*2 )
            ////qDebug() << "t:" << t << "T:" << T;
        (*bytebuf)[i*(SAMPLE_BITS/8)] = (((quint8)T)&0xFF);
        (*bytebuf)[(i*2)+1] = (((quint8)(T>>8))&0xFF);
    }
    input = new QBuffer(bytebuf);
    input->open(QIODevice::ReadOnly);

    // Create an output with our premade QAudioFormat (See example in QAudioOutput)
    QAudioFormat format;
       // Set up the format, eg.
       format.setSampleRate(SAMPLE_RATE);
       format.setChannelCount(1);
       format.setSampleSize(SAMPLE_BITS);
       format.setCodec("audio/pcm");
       format.setByteOrder(QAudioFormat::LittleEndian);
       format.setSampleType(QAudioFormat::SignedInt);
    audio = new QAudioOutput(format, this);
    audio->setBufferSize(BUFFER_SIZE);
    //////qDebug() << "NotifyInterval initially:" << audio->notifyInterval();
    audio->setNotifyInterval(1000);
    //qDebug() << "Starting audio";
    audio->start(input);
    //qDebug() << "Started audio";
    audio->suspend();
    //audio->setVolume(0.5);
    SideToneEnabled = false;

    // Restart audio at regular intervals to 'never run out of sound'
    audioTimer = new QTimer(this);
    timer->connect(audioTimer,
                   SIGNAL(timeout()),
                   this,
                   SLOT(audioTimerEvent()));
    audioTimer->start((SAMPLE_TIME/2) * 1000); //, Qt::PreciseTimer, timeout());
    //audioTimer->stop();

    // default initial delay to be used if "Set Delay Time" has not been activated
    packetDelay = 300;
    ui->keyDelay->setValue(int(packetDelay));

    // initialize the list of available COM-ports
    updateComPortList();

    // Setup colors to key connect buttons
    ui->ConnectToKeyNetwork->setStyleSheet("background-color: red;");
    ui->ConnectToKeyPort->setStyleSheet("background-color: red;");

    // Load settings from file system from last open session
    loadSettings();
}

MainWindow::~MainWindow()
{
    // Save all settings from this session before closing
    saveSettings();
    tcpKeySocket->close();
    keySerialPort->close();
    delete ui;
}

void MainWindow::updateComPortList()
{
    QList<QSerialPortInfo> comDevices = QSerialPortInfo::availablePorts();
    //qDebug() << "on_keyPortDevice_highlighted";
    ui->keyPortDevice->clear();
    this->ui->keyPortDevice->addItem("Select Key port");
    foreach (QSerialPortInfo i, comDevices) {
        this->ui->keyPortDevice->addItem(i.portName()+" "+i.description());
        //qDebug() << "comDevice:" << i.portName();
    }
}

void MainWindow::loadSettings() {

    QSettings settings(SettingsPath + "/" + SettingsFile, QSettings::IniFormat);
    settings.beginGroup("MAIN");
    quint32 val = settings.value("keyNetPort", "").toUInt();
    ui->keyNetPort->setValue(val);
    QString str = settings.value("keyIP", "").toString();
    ui->keyIP->setText(str);
    str = settings.value("KeyPort", "").toString();
    ui->KeyPortName->setText(str);
    str = settings.value("KeyInput", "").toString();
    if ( !QString::compare(str, "CTS") ) {
        ui->KeyOnCTS->setChecked(true);
        ui->KeyOnDSR->setChecked(false);
    } else if ( !QString::compare(str, "DSR") ) {
        ui->KeyOnCTS->setChecked(false);
        ui->KeyOnDSR->setChecked(true);
    }
    str = settings.value("KeyInvert", "").toString();
    if ( !QString::compare(str, "Inverted") ) {
        ui->keyPortInvert->setChecked(true);
    } else {
        ui->keyPortInvert->setChecked(false);
    }
    str = settings.value("UseSideTone", "").toString();
    if (!QString::compare(str, "true")) {
        ui->sideTone->setChecked(true);
    } else {
        ui->sideTone->setChecked(false);
    }
    qint32 ival = settings.value("SideToneVolume", "").toInt();
    ui->verticalSlider->setValue(ival);
    val = settings.value("SideToneFrequency", "").toInt();
    ui->toneFreqBox->setValue(val);
    settings.endGroup();
}

void MainWindow::saveSettings()
{
    QSettings settings(SettingsPath + "/" + SettingsFile, QSettings::IniFormat);
    settings.beginGroup("MAIN");
    settings.setValue("keyNetPort",  ui->keyNetPort->value());
    settings.setValue("keyIP",  ui->keyIP->text());
    settings.setValue("KeyPort",  ui->KeyPortName->text().toLatin1());
    if ( ui->KeyOnCTS->isChecked() ) {
        settings.setValue("KeyInput",  "CTS");
    } else {
        settings.setValue("KeyInput",  "DSR");
    }
    if ( ui->keyPortInvert->isChecked() ) {
        settings.setValue("KeyInvert",  "Inverted");
    } else {
        settings.setValue("KeyInvert",  "NotInverted");
    }
    if ( ui->sideTone->isChecked() ) {
        settings.setValue("UseSideTone", "true");
    } else {
        settings.setValue("UseSideTone", "false");
    }
    settings.setValue("SideToneVolume", ui->verticalSlider->value());
    settings.setValue("SideToneFrequency", ui->toneFreqBox->value());
    settings.endGroup();
}

void MainWindow::audioTimerEvent()
{
    input->seek(0);
    ////qDebug() << "ReloadAudio";
}

void MainWindow::on_AudioNotify()
{
    //qDebug() << "on_AudioNotify, state:" << audio->state();
}

void MainWindow::msEvent()
{
    static uint pingTimer = 0;
    if ( keyPortStatus ) {
        bool KeyIsDown;
        if ( KeyDeBounceCnt == 0 ) {
            QSerialPort::PinoutSignals pinoutSignals = keySerialPort->pinoutSignals();
            if ( ui->KeyOnDSR->isChecked() ) {
               KeyIsDown = (pinoutSignals & QSerialPort::DataSetReadySignal ? true : false);
             }
            if ( ui->KeyOnCTS->isChecked() ) {
               KeyIsDown = (pinoutSignals & QSerialPort::ClearToSendSignal ? true : false);
            }
            if ( keyPortInverted )
                KeyIsDown = !KeyIsDown;
            if ( KeyIsDown != KeyIsDownLast ) {
                KeyDeBounceCnt = 45;
                if ( KeyIsDown ) {
                    qDebug() << "KeyIsDown";
                    KeyDown();
                } else {
                    KeyUp();
                    qDebug() << "KeyIsUp";
                }
                KeyIsDownLast = KeyIsDown;
            }
        } else {
            KeyDeBounceCnt--;
        }
    }
    if ( !((pingTimer++) % 5000) ) {
        RadioPing();
    }
}

void MainWindow::on_keyButton_pressed()
{
    QStringList s;
        MainWindow::KeyDown();
}

void MainWindow::on_keyButton_released()
{
    MainWindow::KeyUp();
}


void MainWindow::on_toneButton_pressed()
{
    audio->resume();
}

void MainWindow::on_toneButton_released()
{
    audio->suspend();
}


void MainWindow::KeyUp()
{
    unsigned long keytime;
    quint32 ms = (QDateTime::currentMSecsSinceEpoch() % 4294967295);
    QByteArray Data;
    Data.append("KU ");
    Data.append(QString::number(ms));
    Data.append(" ");
    keytime = remdiff + (ms&0xFFFFFFFF) + packetDelay;
    Data.append(QString::number(keytime));
    tcpKeySocket->write(Data.data());
    tcpKeySocket->waitForBytesWritten(1);
    if ( SideToneEnabled ) {
        audio->suspend();
    }
}

void MainWindow::RadioPing()
{
    quint32 ms = (QDateTime::currentMSecsSinceEpoch() % 4294967295);
    QByteArray Data;
    Data.append("P ");
    Data.append(QString::number(ms));
    tcpKeySocket->write(Data.data());
    tcpKeySocket->waitForBytesWritten(1);
}

void MainWindow::KeyDown()
{
    unsigned long keytime;
    if ( SideToneEnabled ) {
        audio->resume();
    }
    quint32 ms = (QDateTime::currentMSecsSinceEpoch() % 4294967295);
    QByteArray Data;
    Data.append("KD ");
    Data.append(QString::number(ms));
    Data.append(" ");
    keytime = remdiff+(ms&0xFFFFFFFF) + packetDelay;
    Data.append(QString::number(keytime));
    tcpKeySocket->write(Data.data());
    tcpKeySocket->waitForBytesWritten(1);
}

void MainWindow::readyReadKeySerial()
{
    //qDebug() << "readyReadKeySerial() called";
}

    void MainWindow::readyReadKeyTcp()
{
    static quint32 delay_acc;
    // when data comes in
    quint32 diff, sms = 0, rms, remTime;
    static quint32 min = 99999, max = 0;
    QByteArray buffer;
//    QByteArray dbgbuf;
    char str[100];
    rms = (QDateTime::currentMSecsSinceEpoch() % 4294967295);

    buffer.clear();
    while ( tcpKeySocket->bytesAvailable() ) {
        tcpKeySocket->read(str, 1);
        buffer.append(str, 1);

    }
    ////qDebug() << "DBG:" << buffer.data();
    sscanf(buffer.data(), "%s %lu %lu", str, &sms, &remTime);
    diff = (rms-sms)/2;
    if ( diff > max ) max = diff;
    if ( diff < min ) min = diff;
    ui->packetLatency->display(int(diff));
    ui->packetLatencyMin->display(int(min));
    ui->packetLatencyMax->display(int(max));
    if ( !strcmp(str, "PP") ) {
        remdiff = remTime-(sms&0xFFFFFFFF);
    }
    if ( !strcmp(str, "PP") ) {
        //qDebug() << buffer.data()<<", SetKeyDelayCnt:"<<SetKeyDelayCnt<<"delay_acc:"<<delay_acc<<"diff:"<<diff;
        if ( SetKeyDelayCnt == 10 ) {
            delay_acc = diff;
            on_SetKeyDelay_clicked();
        } else if ( SetKeyDelayCnt == 1 ) {
            delay_acc = (delay_acc + diff)/10;
            packetDelay = MAX(CW_MIN_DELAY, MIN(CW_MAX_DELAY, delay_acc*5)) + delay_acc;
            //qDebug() << "New packetDelay:"<<packetDelay;
            ui->keyDelay->setValue(int(packetDelay));
        } else if ( SetKeyDelayCnt != 0 ){
            delay_acc += diff;
            on_SetKeyDelay_clicked();
        }
        if ( SetKeyDelayCnt != 0 )
            SetKeyDelayCnt--;
    } else {
        qDebug() << "Unknown data received:"<<buffer.data();
    }
}


void MainWindow::on_SetKeyDelay_clicked()
{
    if ( SetKeyDelayCnt == 0 )
        SetKeyDelayCnt = 10;
    int i;
    quint32 ms = (QDateTime::currentMSecsSinceEpoch() % 4294967295);
    QByteArray Data;
    Data.append("P  ");
    Data.append(QString::number(ms));
    tcpKeySocket->write(Data.data());
    tcpKeySocket->waitForBytesWritten(1);
}

void MainWindow::on_KeyPortName_textChanged(const QString &arg1)
{
    ui->ConnectToKeyPort->setChecked(false);
    keyPort = ui->KeyPortName->text().toLatin1();
}

void MainWindow::on_ShowComPortList_clicked()
{
    updateComPortList();
}


void MainWindow::on_sideTone_stateChanged(int arg1)
{
    SideToneEnabled = (arg1 != 0);
}

void MainWindow::on_keyPortInvert_stateChanged(int arg1)
{
    keyPortInverted = (arg1 != 0);
}

void MainWindow::on_verticalSlider_valueChanged(int value)
{
    //qDebug() << "Volume:" << value;
    qreal linearVolume = QAudio::convertVolume(value / qreal(100.0), QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
    //qDebug() << "LogVol:" << linearVolume;
    audio->setVolume(linearVolume);
}

void MainWindow::on_toneFreqBox_valueChanged(int arg1)
{
    //qDebug() << "Spinbox done editing";
    quint16 T;
    qreal freq_const ((2.0 * M_PI) / ((SAMPLE_RATE*1.0)/(arg1 * 1.0)));
    for (int i=0; i<(SAMPLE_TIME * SAMPLE_RATE * (SAMPLE_BITS/8)); i++) {
        qreal t;
        t = (qreal)((qSin(i*freq_const))*(TG_MAX_VAL));
        // Normalize t and convert to signed qint32
        T = (qint16)t;
        (*bytebuf)[i*(SAMPLE_BITS/8)] = (((quint8)T)&0xFF);
        (*bytebuf)[(i*2)+1] = (((quint8)(T>>8))&0xFF);
    }

}


void MainWindow::on_audioDevice_currentIndexChanged(int index)
{
    //qDebug() << "Audio device selected: [" << index << "]" << ui->audioDevice->currentText();
}


void MainWindow::on_keyPortDevice_currentIndexChanged(const QString &arg1)
{
    QString qstr;
    QStringList list = arg1.split(" ");
    qstr = list[0];
    //qDebug() << "on_keyPortDevice_currentIndexChanged to:" << arg1;
    //qDebug() << qstr;

    ui->KeyPortName->setText(qstr);

}



void MainWindow::on_ConnectToKeyNetwork_clicked()
{
    //qDebug() << "Initial key openMode()=="<<tcpKeySocket->openMode();
    if ( tcpKeySocket->openMode() != 0 ) {
        //qDebug()<<"KeyNet open, closing";
        tcpKeySocket->close();
    } else {
        //qDebug()<<"KeyNet not open, opening";
        tcpKeySocket->connectToHost(ui->keyIP->text(), ui->keyNetPort->value());
    }
    if ( tcpKeySocket->openMode() != 0 ) {
        //qDebug()<<"KeyNet open, turning green";
        ui->ConnectToKeyNetwork->setStyleSheet("background-color: green;");
    } else {
        ui->ConnectToKeyNetwork->setStyleSheet("background-color: red;");
    }
    //qDebug() << "Final key openMode()=="<<tcpKeySocket->openMode();
}

void MainWindow::on_ConnectToKeyPort_clicked()
{
    if ( keyPortStatus ) {
        keySerialPort->close();
        keyPortStatus = false;
        ui->ConnectToKeyPort->setStyleSheet("background-color: red;");
    } else {
        keySerialPort->setPortName(ui->KeyPortName->text());
        keySerialPort->setBaudRate(QSerialPort::Baud115200);
        keySerialPort->setParity(QSerialPort::Parity::NoParity);
        keySerialPort->setDataBits(QSerialPort::DataBits::Data8);
        keySerialPort->setStopBits(QSerialPort::StopBits::OneStop);
        keySerialPort->setFlowControl(QSerialPort::FlowControl::UnknownFlowControl);
        keyPortStatus = keySerialPort->open(QSerialPort::OpenModeFlag::ReadWrite);
        //qDebug() << "on_ConnectTokeyPort_stateChanged: keySerialPort->isOpen():"<<keySerialPort->isOpen()<<"keyPortStatus:"<<keyPortStatus;
        if ( keyPortStatus ) {
            keySerialPort->write("The serial port is open!");
            ui->ConnectToKeyPort->setStyleSheet("background-color: green;");
        }
    }

}



void MainWindow::on_keyDelay_valueChanged(int arg1)
{
    packetDelay = arg1;
}
