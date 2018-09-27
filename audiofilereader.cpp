#include "audiofilereader.h"

bool AudioFileReader::Read(const QString & fName)
{
    if( buffer->isValid() || ! buffer->isEmpty() )
        return false;
    fileName = fName;
    if( ! Decode() )
    {
        release();
        return false;
    }
    if( buffer->isEmpty() )
        return false;
    buffer->setObjectName(fName);
    return true;
}


bool AudioFileReader::Decode()
{
    if( avformat_open_input(&formatContext, fileName.toLocal8Bit(), nullptr, nullptr) != 0)
        return false;
    if (avformat_find_stream_info(formatContext, nullptr) < 0)
        return false;
    AVCodec* cdc = nullptr;
    int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &cdc, 0);
    if( streamIndex < 0 )
        return false;
    AVStream *audioStream = formatContext->streams[streamIndex];
    codecContext = avcodec_alloc_context3(nullptr);
    avcodec_parameters_to_context(codecContext, audioStream->codecpar);
    //
    if( avcodec_open2(codecContext, cdc, nullptr) != 0)
        return false;
    if( codecContext->channel_layout == 0 )
    {
        if( codecContext->channels == 1 )
            codecContext->channel_layout = AV_CH_LAYOUT_MONO;
        else
            codecContext->channel_layout = AV_CH_LAYOUT_STEREO;
    }
    //
    //  Init QAudioFormat format
    //
    if( ! buffer->isValid() )
    {
        buffer->setByteOrder(QAudioFormat::LittleEndian);
        buffer->setSampleRate(codecContext->sample_rate);
        buffer->setSampleSize(8*av_get_bytes_per_sample(codecContext->sample_fmt));
        buffer->setSampleType(getSampleType(codecContext->sample_fmt));
        buffer->setChannelCount((codecContext->channel_layout == AV_CH_LAYOUT_MONO) ? 1 : 2);
    }
    //
    AVPacket readingPacket;
    av_init_packet(&readingPacket);
    bool rv = true;
    while( rv && (av_read_frame(formatContext, &readingPacket) == 0) )
    {
        if(readingPacket.stream_index == audioStream->index)
            rv = DecodePacket(readingPacket);
        av_packet_unref(&readingPacket);
    }
    //
    if (cdc->capabilities & AV_CODEC_CAP_DELAY)
    {
        av_init_packet(&readingPacket);
        rv = DecodePacket(readingPacket);
    }
    release();
    return rv ;
}

bool AudioFileReader::DecodePacket(AVPacket & packet)
{
    if( iframe == nullptr )
    {
        iframe = av_frame_alloc();
        if( ! iframe )
            return false;
    }
    AVPacket decodingPacket = packet;
    while(decodingPacket.size > 0)
    {
        int gotFrame = 0;
        int result = avcodec_decode_audio4(codecContext, iframe, &gotFrame, &decodingPacket);
        if( (result >= 0) && gotFrame)
        {
            const AVFrame *f = ConvertFrame(iframe);
            if( f != nullptr )
            {
                int data_size = av_samples_get_buffer_size(nullptr, codecContext->channels,
                                                           f->nb_samples,codecContext->sample_fmt,1);
                buffer->append(reinterpret_cast<const char*>(f->data[0]),data_size);
            }
            else
                return false;
            decodingPacket.size -= result;
            decodingPacket.data += result;
        }
        else
        {
            decodingPacket.size = 0;
            decodingPacket.data = nullptr;
        }
    }
    return true;
}

const AVFrame * AudioFileReader::ConvertFrame(const AVFrame* frame)
{
    if( (getAVSampleFormat(buffer) == codecContext->sample_fmt) &&
            (buffer->channelCount() == codecContext->channels) &&
            (buffer->sampleRate() == codecContext->sample_rate ) )
        return frame;
    int nb_samples = av_rescale_rnd(frame->nb_samples,buffer->sampleRate(),codecContext->sample_rate,AV_ROUND_UP);
    if( oframe != nullptr )
    {
        if( oframe->nb_samples < nb_samples )
            av_frame_free(&oframe);
    }
    if( oframe == nullptr )
    {
        oframe = alloc_audio_frame(getAVSampleFormat(buffer),getAVChannelLayout(buffer),buffer->sampleRate(),nb_samples);
        if( oframe == nullptr )
            return nullptr;
    }
    memset(oframe->data[0],0,oframe->linesize[0]);
    if( swr_ctx == nullptr )
    {
        swr_ctx = swr_alloc_set_opts(nullptr,
                                     getAVChannelLayout(buffer), getAVSampleFormat(buffer),buffer->sampleRate(),
                                     codecContext->channel_layout,codecContext->sample_fmt,codecContext->sample_rate,
                                     0, nullptr);
        if( ! swr_ctx )
            return nullptr;
        if( swr_init(swr_ctx) < 0)
        {
            swr_free(&swr_ctx);
            return nullptr;
        }
    }
    //
    int got_samples = swr_convert(swr_ctx,oframe->data,nb_samples,(const uint8_t **)iframe->data,iframe->nb_samples);
    if( got_samples < 0 )
        return nullptr;
    //
    while( got_samples > 0 )
        got_samples = swr_convert(swr_ctx,oframe->data,nb_samples,nullptr,0);
    return oframe;
}

