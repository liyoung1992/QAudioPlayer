#include "player.h"
#include <QBuffer>
#include <QAudioOutput>
#include <QIODevice>
#include <QDebug>
#include "audiobuffer.h"
#include "audiofilereader.h"

Player::Player(void)
{
    volume = 1;
    notifyInterval = -1;
    autoRestart = false;
    outputAudioDeviceInfo = QAudioDeviceInfo::defaultOutputDevice();
}

Player::~Player(void)
{
    close();
}

QString Player::getFileName(void)
{
    return ( originalAudioBuffer.isNull() ) ? QString() :  originalAudioBuffer.data()->objectName();
}


void Player::setVolume(int v)
{
    if( v >= 0 )
        volume = 0.01*v;
    if( ! audioOutputDevice.isNull() )
        audioOutputDevice->setVolume(volume);
}

bool Player::open(const QString & name)
{
    if( fileLoader.isNull())
    {
        fileLoader.reset(new AudioFileLoader());
        connect(fileLoader.data(),SIGNAL(ready(AudioBuffer*)),this,SLOT(bufferReadySlot(AudioBuffer*)));
    }
    return fileLoader->Load(name);
}


void Player::close(void)
{
    if( ! audioOutputDevice.isNull() )
    {
        if( audioOutputDevice->state() != QAudio::StoppedState )
            audioOutputDevice->stop();
        audioOutputDevice.reset();
    }
    audioBufferDevice.reset();
    originalAudioBuffer.reset();
    transformedAudioBuffer.reset();
    fileLoader.reset();
    emit currentTimeChanged(0);
    emit playerReady(false);
}

bool Player::start(qint32 from)
{
    if( audioOutputDevice.isNull() || audioBufferDevice.isNull() )
        return false;
    if( from != 0 )
        seek(from);
    audioOutputDevice->start(audioBufferDevice.data());
    return true;
}

void Player::notifySlot(void)
{
    emit currentTimeChanged(currentTime());
}

void Player::stop(void)
{
    if( audioOutputDevice.isNull() )
        return;
    if( audioOutputDevice->state() == QAudio::ActiveState )
        audioOutputDevice->stop();
}

qint32 Player::currentTime(void)
{
    return ( audioBufferDevice.isNull() || audioOutputDevice.isNull() ) ? -1 : audioOutputDevice->format().durationForBytes(audioBufferDevice->pos())/1000;
}

qint32 Player::getDuration(void)
{
    return ( originalAudioBuffer.isNull() ) ? -1 : originalAudioBuffer->duration()/1000;
}

void Player::setNotifyInterval(int ms)
{
    notifyInterval = ms;
    if( ! audioOutputDevice.isNull() )
        audioOutputDevice->setNotifyInterval(notifyInterval);
}


void Player::stateChangedSlot(QAudio::State state)
{
    switch (state)
    {
    case QAudio::ActiveState:
        emit started();
        break;
    case QAudio::IdleState:
    {
        if( autoRestart )
        {
            seek(0);
            audioOutputDevice->start(audioBufferDevice.data());
        }
        else
            audioOutputDevice->stop();
        break;
    }
    case QAudio::StoppedState:
    {
        if (audioOutputDevice->error() != QAudio::NoError)
            emit errorSignal();
        emit stoped();
        break;
    }
    default:
        break;
    }
}

void Player::bufferReadySlot(AudioBuffer * a)
{
    close(); // clear all
    if( a == 0 )
        return;
    audioOutputDevice.reset( new QAudioOutput(outputAudioDeviceInfo,*a) ) ;
    if( audioOutputDevice.isNull() )
    {
        delete a;
        return;
    }
    audioBufferDevice.reset(new QBuffer(a,this));
    if( ! audioBufferDevice->open(QIODevice::ReadOnly) )
    {
        delete a;
        audioBufferDevice.reset();
        audioOutputDevice.reset();
        return;
    }
    setNotifyInterval(notifyInterval);
    setVolume();
    connect(audioOutputDevice.data(),SIGNAL(stateChanged(QAudio::State)),this,SIGNAL(stateChanged(QAudio::State)));
    connect(audioOutputDevice.data(),SIGNAL(stateChanged(QAudio::State)),this,SLOT(stateChangedSlot(QAudio::State)));
    connect(audioOutputDevice.data(),SIGNAL(notify()),this,SLOT(notifySlot()));
    originalAudioBuffer.reset(a);
    seek(0);
    emit playerReady(true);
}

bool Player::seek(qint32 to)
{
    if( originalAudioBuffer.isNull() || audioBufferDevice.isNull() )
        return false;
    return audioBufferDevice->seek(qMin(originalAudioBuffer->bytesForDuration(to*1000),originalAudioBuffer->size()));
}

bool Player::setAudioDevice(const QAudioDeviceInfo & info)
{
    if( info.isNull() )
        return false;
    outputAudioDeviceInfo = info;
    return true;
}
