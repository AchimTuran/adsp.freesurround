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

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "p8-platform/util/StdString.h"

#include "addon.h"
#include "DSPProcessFreeSurround.h"
#include "FreeSurroundSettings.h"

// our default internal block size, in floats
static const unsigned default_block_size = SURROUND_BUFSIZE;

CDSPProcess_FreeSurround::CDSPProcess_FreeSurround( const AUDIODSP_ADDON_AUDIO_FORMAT& inputFormat,
                                                    const AUDIODSP_ADDON_AUDIO_FORMAT& outputFormat,
                                                    const AUDIODSP_ADDON_STREAM_PROPERTIES& streamProperties) :
  m_StreamID(streamProperties.uiStreamID),
  m_inputFormat(inputFormat),
  m_outputFormat(outputFormat),
  m_Decoder(NULL),
  m_ProcessBufferPos(0),
  m_ProcessedSize(0),
  m_LatencyFrames(0)
{
  m_InbufArray[0] = (float*)calloc(SURROUND_BUFSIZE, sizeof(float));
  m_InbufArray[1] = (float*)calloc(SURROUND_BUFSIZE, sizeof(float));
}

CDSPProcess_FreeSurround::~CDSPProcess_FreeSurround()
{
  free(m_InbufArray[0]);
  free(m_InbufArray[1]);
  Deinitialize();
}

AUDIODSP_ADDON_ERROR CDSPProcess_FreeSurround::Create()
{
  if (m_streamProperties.eStreamType == AUDIODSP_ADDON_ASTREAM_INVALID)
    return AUDIODSP_ADDON_ERROR_IGNORE_ME;
  if (m_inputFormat.channelLayout.channelCount > 2)
    return AUDIODSP_ADDON_ERROR_IGNORE_ME;
  if (m_outputFormat.channelLayout.channelCount <= 3)
    return AUDIODSP_ADDON_ERROR_IGNORE_ME;
  //! @todo check if this is really a bottleneck
  //if (m_inputFormat.sampleRate > 96000)
  //  return AUDIODSP_ADDON_ERROR_IGNORE_ME;

  m_inputFormat.dataFormat = AUDIODSP_ADDON_FMT_FLOATP;
  m_outputFormat.dataFormat = AUDIODSP_ADDON_FMT_FLOATP;

  m_LFEPresent = false;
  for (unsigned int ch = 0; ch < m_outputFormat.channelLayout.channelCount; ch++)
  {
    if (m_outputFormat.channelLayout.channels[ch] == AUDIODSP_ADDON_CH_LFE)
    {
      m_LFEPresent = true;
      break;
    }
  }
  
  m_DecoderChannelSetup;// = (channel_setup)(settings->lOutChannelPresentFlags | AUDIODSP_ADDON_PRSNT_CH_LFE);
  
  if (m_Decoder)
    delete m_Decoder;
  m_Decoder = new CFreeSurroundDecoder(m_DecoderChannelSetup, default_block_size, m_inputFormat.sampleRate);
  
  m_Decoder->Flush();
  ResetSettings();
  
  m_ProcessBufferPos = 0;
  m_LatencyFrames = 0;
  
  return AUDIODSP_ADDON_ERROR_NO_ERROR;
}

AUDIODSP_ADDON_ERROR CDSPProcess_FreeSurround::Destroy()
{
  return AUDIODSP_ADDON_ERROR_NO_ERROR;
}

void CDSPProcess_FreeSurround::Deinitialize()
{
  if (m_Decoder)
  {
    delete m_Decoder;
    m_Decoder = NULL;
  }
}

void CDSPProcess_FreeSurround::ResetSettings()
{
  CDSPSettings settings;
  settings.LoadSettingsData();

  m_Params.fInputGain       = settings.m_Settings.fInputGain;
  m_Params.fCircularWrap    = settings.m_Settings.fCircularWrap;
  m_Params.fShift           = settings.m_Settings.fShift;
  m_Params.fDepth           = settings.m_Settings.fDepth;
  m_Params.fFocus           = settings.m_Settings.fFocus;
  m_Params.fCenterImage     = settings.m_Settings.fCenterImage;
  m_Params.fFrontSeparation = settings.m_Settings.fFrontSeparation;
  m_Params.fRearSeparation  = settings.m_Settings.fRearSeparation;
  m_Params.bLFE             = settings.m_Settings.bLFE;
  m_Params.fLowCutoff       = settings.m_Settings.fLowCutoff;
  m_Params.fHighCutoff      = settings.m_Settings.fHighCutoff;

  SetParams();
}

void CDSPProcess_FreeSurround::SetParams()
{
  if (!m_Decoder)
    return;

  m_Decoder->SetCircularWrap(m_Params.fCircularWrap);
  m_Decoder->SetShift(m_Params.fShift);
  m_Decoder->SetDepth(m_Params.fDepth);
  m_Decoder->SetFocus(m_Params.fFocus);
  m_Decoder->SetCenterImage(m_Params.fCenterImage);
  m_Decoder->SetFrontSeparation(m_Params.fFrontSeparation);
  m_Decoder->SetRearSeparation(m_Params.fRearSeparation);
  m_Decoder->SetBassRedirection(m_Params.bLFE);
  m_Decoder->SetLowCutoff(m_Params.fLowCutoff);
  m_Decoder->SetHighCutoff(m_Params.fHighCutoff);
}

float CDSPProcess_FreeSurround::StreamGetDelay()
{
  float delay = 0.0;

  if (m_LatencyFrames != 0)
    delay = ((float)m_LatencyFrames)/(2*m_inputFormat.sampleRate);

  return delay;
}

unsigned int CDSPProcess_FreeSurround::Process(const float **array_in, float **array_out)
{
  if (!m_Decoder)
    return 0;

  float **outputs = m_Decoder->getOutputBuffers();

  for (unsigned int pos = 0; pos < m_inputFormat.frameSize; ++pos)
  {
    m_InbufArray[0][m_ProcessBufferPos] = array_in[AUDIODSP_ADDON_CH_FL][pos];// * m_Params.input_gain;
    m_InbufArray[1][m_ProcessBufferPos] = array_in[AUDIODSP_ADDON_CH_FR][pos];// * m_Params.input_gain;

    switch (m_DecoderChannelSetup)
    {
      case cs_stereo:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[2][m_ProcessBufferPos];
        break;
      }
      case cs_3stereo:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[3][m_ProcessBufferPos];
        break;
      }
      case cs_5stereo:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FLOC][pos]  = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FROC][pos]  = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[5][m_ProcessBufferPos];
        break;
      }
      case cs_4point1_Side:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[4][m_ProcessBufferPos];
        break;
      }
      case cs_4point1_Back:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BL][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BR][pos]    = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[4][m_ProcessBufferPos];
        break;
      }
      case cs_5point1_Side:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[5][m_ProcessBufferPos];
        break;
      }
      case cs_5point1_Back:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BL][pos]    = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[5][m_ProcessBufferPos];
        break;
      }
      case cs_6point1:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BC][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[6][m_ProcessBufferPos];
        break;
      }
      case cs_7point1:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BL][pos]    = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BR][pos]    = outputs[6][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[7][m_ProcessBufferPos];
        break;
      }
      case cs_7point1_panorama:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FLOC][pos]  = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FROC][pos]  = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[6][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[7][m_ProcessBufferPos];
        break;
      }
      case cs_7point1_tricenter:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FLOC][pos]  = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FROC][pos]  = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BL][pos]    = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BR][pos]    = outputs[6][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[7][m_ProcessBufferPos];
        break;
      }
      case cs_8point1:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BL][pos]    = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BC][pos]    = outputs[6][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BR][pos]    = outputs[7][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[8][m_ProcessBufferPos];
        break;
      }
      case cs_9point1_densepanorama:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FLOC][pos]  = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FROC][pos]  = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TFL][pos]   = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TFR][pos]   = outputs[6][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[7][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[8][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[9][m_ProcessBufferPos];
        break;
      }
      case cs_9point1_wrap:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FLOC][pos]  = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FROC][pos]  = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[6][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BL][pos]    = outputs[7][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BR][pos]    = outputs[8][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[9][m_ProcessBufferPos];
        break;
      }
      case cs_11point1_densewrap:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FLOC][pos]  = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FROC][pos]  = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TFL][pos]   = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TFR][pos]   = outputs[6][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[7][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[8][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BL][pos]    = outputs[9][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BR][pos]    = outputs[10][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[11][m_ProcessBufferPos];
        break;
      }
      case cs_13point1_totalwrap:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FLOC][pos]  = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FROC][pos]  = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TFL][pos]   = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TFR][pos]   = outputs[6][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[7][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[8][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TBL][pos]   = outputs[9][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TBR][pos]   = outputs[10][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BL][pos]    = outputs[11][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BR][pos]    = outputs[12][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[13][m_ProcessBufferPos];
        break;
      }
      case cs_16point1:
      {
        array_out[AUDIODSP_ADDON_CH_FL][pos]    = outputs[0][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FLOC][pos]  = outputs[1][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FC][pos]    = outputs[2][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FROC][pos]  = outputs[3][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_FR][pos]    = outputs[4][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TFL][pos]   = outputs[5][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TFR][pos]   = outputs[6][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SL][pos]    = outputs[7][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_SR][pos]    = outputs[8][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TBL][pos]   = outputs[9][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_TBR][pos]   = outputs[10][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BL][pos]    = outputs[11][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BLOC][pos]  = outputs[12][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BC][pos]    = outputs[14][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BROC][pos]  = outputs[15][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_BR][pos]    = outputs[15][m_ProcessBufferPos];
        array_out[AUDIODSP_ADDON_CH_LFE][pos]   = outputs[16][m_ProcessBufferPos];
        break;
      }
      case cs_legacy:
      default:
      {
        if (array_out[AUDIODSP_ADDON_CH_FL])
          array_out[AUDIODSP_ADDON_CH_FL][pos]  = outputs[0][m_ProcessBufferPos];
        if (array_out[AUDIODSP_ADDON_CH_FC])
          array_out[AUDIODSP_ADDON_CH_FC][pos]  = outputs[1][m_ProcessBufferPos];
        if (array_out[AUDIODSP_ADDON_CH_FR])
          array_out[AUDIODSP_ADDON_CH_FR][pos]  = outputs[2][m_ProcessBufferPos];
        if (array_out[AUDIODSP_ADDON_CH_BL])
          array_out[AUDIODSP_ADDON_CH_BL][pos]  = outputs[3][m_ProcessBufferPos];
        if (array_out[AUDIODSP_ADDON_CH_BR])
          array_out[AUDIODSP_ADDON_CH_BR][pos]  = outputs[4][m_ProcessBufferPos];
        if (array_out[AUDIODSP_ADDON_CH_LFE])
          array_out[AUDIODSP_ADDON_CH_LFE][pos] = outputs[5][m_ProcessBufferPos];
        break;
      }
    }

    ++m_ProcessBufferPos;

    // If the FIFO is full
    if (m_ProcessBufferPos >= default_block_size)
    {
      m_Decoder->Decode(m_InbufArray);

      m_ProcessBufferPos = 0;
      m_LatencyFrames = default_block_size;
    }
  }

  return m_inputFormat.frameSize;
}

AUDIODSP_ADDON_AUDIO_FORMAT CDSPProcess_FreeSurround::GetInputFormat()
{
  return m_inputFormat;
}

AUDIODSP_ADDON_AUDIO_FORMAT CDSPProcess_FreeSurround::GetOutputFormat()
{
  return m_outputFormat;
}

void CDSPProcess_FreeSurround::SetCircularWrap(float degree)
{
  m_Params.fCircularWrap = degree;
  if (m_Decoder)
    m_Decoder->SetCircularWrap(m_Params.fCircularWrap);
}

void CDSPProcess_FreeSurround::SetShift(float value)
{
  m_Params.fShift = value;
  if (m_Decoder)
    m_Decoder->SetShift(m_Params.fShift);
}

void CDSPProcess_FreeSurround::SetDepth(float depth)
{
  m_Params.fDepth = depth;
  if (m_Decoder)
    m_Decoder->SetDepth(m_Params.fDepth);
}

void CDSPProcess_FreeSurround::SetFocus(float focus)
{
  m_Params.fFocus = focus;
  if (m_Decoder)
    m_Decoder->SetFocus(m_Params.fFocus);
}

void CDSPProcess_FreeSurround::SetCenterImage(float value)
{
  m_Params.fCenterImage = value;
  if (m_Decoder)
    m_Decoder->SetCenterImage(m_Params.fCenterImage);
}

void CDSPProcess_FreeSurround::SetFrontSeparation(float separation)
{
  m_Params.fFrontSeparation = separation;
  if (m_Decoder)
    m_Decoder->SetFrontSeparation(m_Params.fFrontSeparation);
}

void CDSPProcess_FreeSurround::SetRearSeparation(float separation)
{
  m_Params.fRearSeparation = separation;
  if (m_Decoder)
    m_Decoder->SetRearSeparation(m_Params.fRearSeparation);
}

void CDSPProcess_FreeSurround::SetBassRedirection(bool onOff)
{
  m_Params.bLFE = onOff;
  if (m_Decoder)
    m_Decoder->SetBassRedirection(m_Params.bLFE);
}

void CDSPProcess_FreeSurround::SetLowCutoff(float value)
{
  m_Params.fLowCutoff = value;
  if (m_Decoder)
    m_Decoder->SetLowCutoff(m_Params.fLowCutoff/m_inputFormat.sampleRate);
}

void CDSPProcess_FreeSurround::SetHighCutoff(float value)
{
  m_Params.fHighCutoff = value;
  if (m_Decoder)
    m_Decoder->SetHighCutoff(m_Params.fHighCutoff/ m_inputFormat.sampleRate);
}
