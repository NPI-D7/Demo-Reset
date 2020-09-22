#include <cstring>
#include <mpg123.h>
#include <string>
#include "m3d/audio/sound.hpp"
#include "m3d/private/ndsp.hpp"

namespace m3d {
    Sound::Sound(const std::string& t_filename) :
            m_position(0),
            m_volumeLeft(1.f),
            m_volumeRight(1.f),
            m_started(false),
            m_waitForChannel(false),
            m_ending(false),
            m_reader(nullptr) {
        setFile(t_filename);

        m_thread.initialize(std::bind(&m3d::Sound::playLoop, this, std::placeholders::_1), nullptr);
        m_thread.start();
    }

    Sound::~Sound() {
        if (!m_started) {
            m_thread.initialize([](m3d::Parameter){}, nullptr);
            m_thread.start();
        } else {
            m_ending = true;
            m_thread.join();
        }

        delete m_reader;
    }

    void Sound::setFile(const std::string& t_filename) {
        m_file = t_filename;
        m_filetype = getFileType(m_file);

        switch(m_filetype) {
            case m3d::Sound::FileType::Mp3:
                delete m_reader;
                m_reader = new m3d::Playable::MP3Reader;
                m_reader->set(m_decoder);
                break;

            case m3d::Sound::FileType::Wav:
                delete m_reader;
                m_reader = new m3d::Playable::WAVReader;
                m_reader->set(m_decoder);
                break;

            default:
                delete m_reader;
                m_reader = nullptr;
        }
    }

    const std::string& Sound::getFile() {
        return m_file;
    }

    void Sound::play(bool t_waitForChannel) {
        if (m_filetype != m3d::Playable::FileType::Error) {
            if (m_playing) {
                m_playing = false;
                m_thread.join();
            }

            m_started = true;
            m_waitForChannel = t_waitForChannel;
            m_thread.initialize(std::bind(&m3d::Sound::playLoop, this, std::placeholders::_1), nullptr);
            m_thread.start();

            m_playing = true;

            for (const auto& callback: m_playCallbacks) {
                callback();
            }
        }
    }

    void Sound::setVolume(float t_volume, m3d::Playable::Side t_side) {
        switch (t_side) {
            case m3d::Playable::Side::Left:
                if (t_volume < 0) {
                    m_volumeLeft = 0.f;
                } else {
                    m_volumeLeft = t_volume;
                }
                break;
            case m3d::Playable::Side::Right:
                if (t_volume < 0) {
                    m_volumeRight = 0.f;
                } else {
                    m_volumeRight = t_volume;
                }
                break;
            case m3d::Playable::Side::Both:
                if (t_volume < 0) {
                    m_volumeLeft = 0.f;
                    m_volumeRight = 0.f;
                } else {
                    m_volumeLeft = t_volume;
                    m_volumeRight = t_volume;
                }
        }
    }

    float Sound::getVolume(m3d::Playable::Side t_side) {
        switch (t_side) {
            case m3d::Playable::Side::Left:
                return m_volumeLeft;
            case m3d::Playable::Side::Right:
                return m_volumeRight;
            default:
                return (m_volumeLeft + m_volumeRight) / 2;
        }
    }

    // private methods
    void Sound::playLoop(m3d::Parameter) {
        // ndsp wasn't initialized or there was an error
        if (!m3d::priv::ndsp::initialized) {
            m_playing = false;
            return;
        }

        int channel = occupyChannel(m_waitForChannel);

        while (!m_ending && m_playing) {
            if (m_playing) {

                int16_t *buffer1, *buffer2;
                ndspWaveBuf waveBuf[2];
                bool lastbuffer = false;
                std::string file;

                {
                    m3d::Lock lock(m_mutex);
                    file = m_file;
                }

                if(m_decoder.init(file.c_str()) != 0) {
                    return;
                }

                if(m_decoder.getChannels() > 2 || m_decoder.getChannels() < 1) {
                    return;
                }

                buffer1 = static_cast<int16_t*>(linearAlloc(m_decoder.m_buffSize * sizeof(int16_t)));
                buffer2 = static_cast<int16_t*>(linearAlloc(m_decoder.m_buffSize * sizeof(int16_t)));

                ndspChnReset(channel);
                ndspChnWaveBufClear(channel);
                ndspSetOutputMode(NDSP_OUTPUT_STEREO);
                ndspChnSetInterp(channel, NDSP_INTERP_POLYPHASE);
                ndspChnSetRate(channel, m_decoder.getRate());
                ndspChnSetFormat(channel,
                        m_decoder.getChannels() == 2 ? NDSP_FORMAT_STEREO_PCM16 :
                        NDSP_FORMAT_MONO_PCM16);

                float volume[] = {
                    m_volumeLeft,  // front left
                    m_volumeRight, // front right
                    m_volumeLeft,  // back left
                    m_volumeRight, // back right
                    m_volumeLeft,  // aux 0 front left
                    m_volumeRight, // aux 0 front right
                    m_volumeLeft,  // aux 0 back left
                    m_volumeRight, // aux 0 back right
                    m_volumeLeft,  // aux 1 front left
                    m_volumeRight, // aux 1 front right
                    m_volumeLeft,  // aux 1 back left
                    m_volumeRight  // aux 1 back right
                };

                ndspChnSetMix(channel, volume);

                memset(waveBuf, 0, sizeof(waveBuf));
                waveBuf[0].nsamples = m_decoder.decode(&buffer1[0]) / m_decoder.getChannels();
                waveBuf[0].data_vaddr = &buffer1[0];
                ndspChnWaveBufAdd(channel, &waveBuf[0]);

                waveBuf[1].nsamples = m_decoder.decode(&buffer2[0]) / m_decoder.getChannels();
                waveBuf[1].data_vaddr = &buffer2[0];
                ndspChnWaveBufAdd(channel, &waveBuf[1]);

                // wait for music to start
                while (ndspChnIsPlaying(channel) == false);

                while (m_playing) {
                    svcSleepThread(100 * 1000);

                    // break after the last buffer has finished
                    if(lastbuffer == true && waveBuf[0].status == NDSP_WBUF_DONE &&
                            waveBuf[1].status == NDSP_WBUF_DONE) {
                        m_playing = false;
                        continue;
                    }

                    if(ndspChnIsPaused(channel) == true || lastbuffer == true) continue;

                    if(waveBuf[0].status == NDSP_WBUF_DONE) {
                        size_t read = m_decoder.decode(&buffer1[0]);

                        if(read <= 0) {
                            lastbuffer = true;
                            continue;
                        } else if(read < m_decoder.m_buffSize) {
                            waveBuf[0].nsamples = read / m_decoder.getChannels();
                        }

                        ndspChnWaveBufAdd(channel, &waveBuf[0]);
                    }

                    if(waveBuf[1].status == NDSP_WBUF_DONE) {
                        size_t read = m_decoder.decode(&buffer2[0]);

                        if(read <= 0) {
                            lastbuffer = true;
                            continue;
                        } else if(read < m_decoder.m_buffSize) {
                            waveBuf[1].nsamples = read / m_decoder.getChannels();
                        }

                        ndspChnWaveBufAdd(channel, &waveBuf[1]);
                    }

                    DSP_FlushDataCache(buffer1, m_decoder.m_buffSize * sizeof(int16_t));
                    DSP_FlushDataCache(buffer2, m_decoder.m_buffSize * sizeof(int16_t));
                }

                m_decoder.exit();

                ndspChnWaveBufClear(channel);
                linearFree(buffer1);
                linearFree(buffer2);
                m3d::priv::ndsp::freeChannel(channel);

                m_playing = false;

                for (const auto& callback: m_finishCallbacks) {
                    callback();
                }
            }
        }
    }
}; /* m3d */
