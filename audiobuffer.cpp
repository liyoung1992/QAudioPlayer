#include "audiobuffer.h"
#include <algorithm>
using namespace std;

AudioBuffer::AudioBuffer(const QAudioFormat & format, QObject *parent):QObject(parent),QAudioFormat(format)
{
}

AudioBuffer::~AudioBuffer()
{
}

