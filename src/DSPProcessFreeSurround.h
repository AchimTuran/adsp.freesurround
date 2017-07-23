#pragma once
/*
 *      Copyright (C) 2014-2015 Team KODI
 *      http://kodi.tv
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include <string>
#include <vector>
#include <map>

#include "FreeSurroundDecoder.h"
#include "FreeSurroundSettings.h"

#include <kodi/addon-instance/AudioDSP.h>

#define SURROUND_BUFSIZE 8192

class CDSPProcess_FreeSurround
{
private:
  const unsigned int          m_StreamID;           /*!< @brief unique id of the audio stream packets */
  AUDIODSP_ADDON_AUDIO_FORMAT m_inputFormat;        /*!< @brief the active XBMC audio settings */
  AUDIODSP_ADDON_AUDIO_FORMAT m_outputFormat;       /*!< @brief the active XBMC audio settings */
  AUDIODSP_ADDON_STREAM_PROPERTIES m_streamProperties;       /*!< @brief the active XBMC audio settings */
  sSettings                   m_Params;             /*!< @brief The actual settings from settings file */
  class CFreeSurroundDecoder *m_Decoder;            /*!< @brief the surround decoder */
  float                      *m_InbufArray[2];      /*!< @brief Input buffer for free surround decoder */
  unsigned int                m_ProcessBufferPos;   /*!< @brief amount in lt,rt */
  int                         m_ProcessedSize;      /*!< @brief amount processed */
  int                         m_LatencyFrames;      /*!< @brief number of frames of incurred latency */
  channel_setup               m_DecoderChannelSetup;/*!< @brief the free surround channel setup */
  bool                        m_LFEPresent;         /*!< @brief if true LFE signal generation is done inside decoder */
  int                         m_channelMapping[AUDIODSP_ADDON_CH_MAX];

  void SetParams();
  void Deinitialize();
  channel_setup GetChannelSetup();

public:
  CDSPProcess_FreeSurround( const AUDIODSP_ADDON_AUDIO_FORMAT& inputFormat,
                            const AUDIODSP_ADDON_AUDIO_FORMAT& outputFormat,
                            const AUDIODSP_ADDON_STREAM_PROPERTIES& streamProperties);
  virtual ~CDSPProcess_FreeSurround();

  AUDIODSP_ADDON_ERROR Create();
  AUDIODSP_ADDON_ERROR Destroy();

  unsigned int Process(const float **array_in, float **array_out);
  float        StreamGetDelay();

  AUDIODSP_ADDON_AUDIO_FORMAT GetInputFormat();
  AUDIODSP_ADDON_AUDIO_FORMAT GetOutputFormat();

  void SetCircularWrap(float degree);
  void SetShift(float value);
  void SetDepth(float depth);
  void SetFocus(float focus);
  void SetCenterImage(float value);
  void SetFrontSeparation(float separation);
  void SetRearSeparation(float separation);
  void SetBassRedirection(bool onOff);
  void SetLowCutoff(float value);
  void SetHighCutoff(float value);

  void ResetSettings();
};
