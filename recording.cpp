

/**
 * (c) 2024, Micro:bit Educational Foundation and contributors
 *
 * SPDX-License-Identifier: MIT
 */
/*
    Based on
    https://github.com/microsoft/pxt-microbit/blob/master/libs/audio-recording/recording.cpp

    Added:
    - sendToSerial()
*/
/*
    The MIT License (MIT)

    Copyright (c) 2022 Lancaster University

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "pxt.h"
#include "MicroBit.h"

#if MICROBIT_CODAL
#include "StreamRecording.h"
#include "SerialStreamer.h"
#endif

using namespace pxt;

namespace record {

#if MICROBIT_CODAL
static const uint8_t RADIO_PKT_START = 0xA0;
static const uint8_t RADIO_PKT_DATA  = 0xA1;
static const uint8_t RADIO_PKT_END   = 0xA2;

static const int RADIO_AUDIO_SAMPLES_PER_PACKET = 24;
static int radioGroup = 23;
#endif

#if MICROBIT_CODAL
// Based on
// https://github.com/lancaster-university/codal-core/blob/master/source/streams/SerialStreamer.cpp
// Copyright (c) 2016 Lancaster University.
class SerialSink : public DataSink
{
    DataSource      &upstream; 
 
public:
    SerialSink( DataSource &source) : upstream( source) {
        source.connect( *this);
        source.dataWanted( DATASTREAM_WANTED);
    }

    int pullRequest() {
        static volatile int pr = 0;
        if ( !pr) {
            pr++;
            while ( pr) {
                send( upstream.pull());
                pr--;
            }
        } else {
            pr++;
        }
        return DEVICE_OK;
    }

    void send( ManagedBuffer buffer) {
        if ( buffer.length() <= 0) return;
        int format = upstream.getFormat();
        int skip = DATASTREAM_FORMAT_BYTES_PER_SAMPLE( format);
        uint8_t *ptr  = &buffer[0]; 
        uint8_t *next = ptr + buffer.length();
        while ( ptr < next) {
            int32_t v = StreamNormalizer::readSample[ format]( ptr);
            Serial::defaultSerial->send( ManagedString( (int) v) + "\n");
            ptr += skip;
        }
    }
};
#endif //MICROBIT_CODAL


#if MICROBIT_CODAL
class RadioSink : public DataSink
{
    DataSource &upstream;
    uint8_t seq;
    bool started;

public:
    RadioSink(DataSource &source) : upstream(source), seq(0), started(false) {
        source.connect(*this);
        source.dataWanted(DATASTREAM_WANTED);
    }

    void begin(int sampleRate) {
        seq = 0;
        started = true;

        PacketBuffer pkt(4);
        pkt[0] = RADIO_PKT_START;
        pkt[1] = (sampleRate >> 8) & 0xFF;
        pkt[2] = sampleRate & 0xFF;
        pkt[3] = 0;
        uBit.radio.datagram.send(pkt);
    }

    void end() {
        if (!started) return;

        PacketBuffer pkt(2);
        pkt[0] = RADIO_PKT_END;
        pkt[1] = seq;
        uBit.radio.datagram.send(pkt);

        started = false;
    }

    int pullRequest() {
        static volatile int pr = 0;
        if (!pr) {
            pr++;
            while (pr) {
                send(upstream.pull());
                pr--;
            }
        } else {
            pr++;
        }
        return DEVICE_OK;
    }

    void send(ManagedBuffer buffer) {
        if (buffer.length() <= 0) return;

        int format = upstream.getFormat();
        int skip = DATASTREAM_FORMAT_BYTES_PER_SAMPLE(format);
        uint8_t *ptr = &buffer[0];
        uint8_t *next = ptr + buffer.length();

        while (ptr < next) {
            int remaining = (next - ptr) / skip;
            int n = remaining > RADIO_AUDIO_SAMPLES_PER_PACKET ? RADIO_AUDIO_SAMPLES_PER_PACKET : remaining;

            PacketBuffer pkt(2 + n);
            pkt[0] = RADIO_PKT_DATA;
            pkt[1] = seq++;

            for (int i = 0; i < n; i++) {
                int32_t v = StreamNormalizer::readSample[format](ptr);

                if (v < -32768) v = -32768;
                if (v > 32767)  v = 32767;
                uint8_t pcm8 = (uint8_t)((v + 32768) >> 8);

                pkt[2 + i] = pcm8;
                ptr += skip;
            }

            uBit.radio.datagram.send(pkt);
            fiber_sleep(3);
        }
    }
};
#endif


#if MICROBIT_CODAL
static StreamRecording *recording = NULL;
static SplitterChannel *splitterChannel = NULL;
static MixerChannel *channel = NULL;
static SerialSink *serialSink = NULL;
static RadioSink *radioSink = NULL;
#endif


void checkEnv() {
#if MICROBIT_CODAL
    if (recording == NULL) {
        int defaultSampleRate = 11000;
        MicroBitAudio::requestActivation();

        splitterChannel = uBit.audio.splitter->createChannel();
        uBit.audio.mic->setSampleRate(defaultSampleRate);

        recording = new StreamRecording(*splitterChannel);

        channel = uBit.audio.mixer.addChannel(*recording, defaultSampleRate);
        channel->setVolume(75.0);

        serialSink = new SerialSink(*recording);
        radioSink = new RadioSink(*recording);

        uBit.radio.enable();
        uBit.radio.setGroup(radioGroup);
    }
#endif
}

/**
 * Record an audio clip
 */
//% promise
void record() {
#if MICROBIT_CODAL
    checkEnv();
    recording->recordAsync();
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}

/**
 * Play the audio clip that is saved in the buffer
 */
//%
void play() {
#if MICROBIT_CODAL
    checkEnv();
    if ( recording->isPlaying() && recording->downStream != channel) {
        recording->stop();
    }
    recording->connect( *channel);
    recording->playAsync();
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}

/**
 * Stop recording
 */
//%
void stop() {
#if MICROBIT_CODAL
    checkEnv();
    recording->stop();
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}

/**
 * Clear the buffer
 */
//%
void erase() {
#if MICROBIT_CODAL
    checkEnv();
    recording->erase();
#endif
}

/**
 * Set sensitity of the microphone input
 */
//%
void setMicrophoneGain(float gain) {
#if MICROBIT_CODAL
    uBit.audio.processor->setGain(gain);
#endif
}

/**
 * Get how long the recorded audio clip is
 */
//%
int audioDuration(int sampleRate) {
#if MICROBIT_CODAL
    return recording->duration(sampleRate);
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
    return MICROBIT_NOT_SUPPORTED;
#endif
}

/**
 * Get whether the playback is active
 */
//%
bool audioIsPlaying() {
#if MICROBIT_CODAL
    return recording->isPlaying();
#else
    return false;
#endif
}

/**
 * Get whether the microphone is listening
 */
//%
bool audioIsRecording() {
#if MICROBIT_CODAL
    return recording->isRecording();
#else
    return false;
#endif
}

/**
 * Get whether the board is recording or playing back
 */
//%
bool audioIsStopped() {
#if MICROBIT_CODAL
    return recording->isStopped();
#else
    return false;
#endif
}

/**
 * Change the sample rate of the splitter channel (audio input)
 */
//%
void setInputSampleRate(int sampleRate) {
#if MICROBIT_CODAL
    checkEnv();
    uBit.audio.mic->setSampleRate(sampleRate);
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}


/**
 * Change the sample rate of the mixer channel (audio output)
 */
//%
void setOutputSampleRate(int sampleRate) {
#if MICROBIT_CODAL
    checkEnv();
    channel->setSampleRate(sampleRate);
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}

/**
 * Set the sample rate for both input and output
*/
//%
void setBothSamples(int sampleRate) {
#if MICROBIT_CODAL
    setOutputSampleRate(sampleRate);
    uBit.audio.mic->setSampleRate(sampleRate);
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}


//%
void setPlaybackVolume(int volume) {
#if MICROBIT_CODAL
    checkEnv();
    channel->setVolume(volume);
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}

//%
void send() {
#if MICROBIT_CODAL
    checkEnv();
    if ( recording->isPlaying() && recording->downStream != serialSink) {
        recording->stop();
    }
    recording->connect( *serialSink);
    recording->playAsync();
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}


//%
bool sendingToSerial() {
#if MICROBIT_CODAL
    checkEnv();
    return recording->isPlaying() && recording->downStream == serialSink;
#else
    return false;
#endif
}

//%
void setRadioGroup(int group) {
#if MICROBIT_CODAL
    checkEnv();
    radioGroup = group;
    uBit.radio.setGroup(group);
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}

//%
void sendToRadio() {
#if MICROBIT_CODAL
    checkEnv();

    if (recording->isPlaying() && recording->downStream != radioSink) {
        recording->stop();
    }

    int sr = channel->getSampleRate();

    radioSink->begin(sr);
    recording->connect(*radioSink);
    recording->playAsync();

    int ms = recording->duration(sr);
    fiber_sleep(ms + 50);
    radioSink->end();
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}

//%
bool sendingToRadio() {
#if MICROBIT_CODAL
    checkEnv();
    return recording->isPlaying() && recording->downStream == radioSink;
#else
    return false;
#endif
}

//%
void setRadioGroup(int group) {
#if MICROBIT_CODAL
    checkEnv();
    radioGroup = group;
    uBit.radio.setGroup(group);
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}

//%
void sendToRadio() {
#if MICROBIT_CODAL
    checkEnv();

    if (recording->isPlaying() && recording->downStream != radioSink) {
        recording->stop();
    }

    int sr = channel->getSampleRate();

    radioSink->begin(sr);
    recording->connect(*radioSink);
    recording->playAsync();

    int ms = recording->duration(sr);
    fiber_sleep(ms + 50);
    radioSink->end();
#else
    target_panic(PANIC_VARIANT_NOT_SUPPORTED);
#endif
}

//%
bool sendingToRadio() {
#if MICROBIT_CODAL
    checkEnv();
    return recording->isPlaying() && recording->downStream == radioSink;
#else
    return false;
#endif
}

} // namespace record
