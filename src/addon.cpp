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

#include <vector>
#include <string>
#include "addon.h"
#include <kodi/Filesystem.h>
#include "p8-platform/util/util.h"
#include "p8-platform/util/StdString.h"
#include "GUIDialogFreeSurround.h"
#include "DSPProcessFreeSurround.h"

using namespace std;

#define ID_MASTER_PROCESS_FREE_SURROUND   1300

#if defined(TARGET_WINDOWS)
#undef CreateDirectory
#endif

class CFreeSurroundAddon
  : public kodi::addon::CAddonBase,
    public kodi::addon::CInstanceAudioDSP
{
public:
  CFreeSurroundAddon();
  virtual ~CFreeSurroundAddon();

  virtual std::string GetDSPName() override { return "Free Surround Processor"; }
  virtual std::string GetDSPVersion() override { return FREESURROUND_VERSION; };
  virtual AUDIODSP_ADDON_ERROR MenuHook(const AUDIODSP_MENU_HOOK& menuhook) override;
  virtual AUDIODSP_ADDON_ERROR CreateModeHandle(const AUDIODSP_ADDON_AUDIO_FORMAT *inputFormat,
                                                  const AUDIODSP_ADDON_AUDIO_FORMAT* outputFormat,
                                                  const AUDIODSP_ADDON_STREAM_PROPERTIES* streamProperties,
                                                  const uint64_t ID,
                                                  ADDON_HANDLE modeHandle) override;
    
    virtual AUDIODSP_ADDON_ERROR CreateMode(const ADDON_HANDLE modeHandle) override;

    virtual AUDIODSP_ADDON_ERROR DestroyMode(const ADDON_HANDLE modeHandle) override;

    virtual AUDIODSP_ADDON_AUDIO_FORMAT GetModeInputFormat(const ADDON_HANDLE modeHandle) override;

    virtual AUDIODSP_ADDON_AUDIO_FORMAT GetModeOutFormat(const ADDON_HANDLE modeHandle) override;

    virtual unsigned int NeededModeFrameSize(const ADDON_HANDLE modeHandle) { return 0; }

    virtual int ProcessMode(const ADDON_HANDLE modeHandle, const uint8_t** array_in, uint8_t** array_out) override;

    virtual AUDIODSP_ADDON_ERROR DestroyModeHandle(ADDON_HANDLE modeHandle) override;

private:
  AUDIODSP_MENU_HOOK m_MenuHook;
  AUDIODSP_ADDON_MODE_DATA m_ModeInfoStruct;
};

CFreeSurroundAddon::CFreeSurroundAddon()
{
  // create addon user path
  if (!kodi::vfs::DirectoryExists(kodi::GetBaseUserPath()))
  {
    kodi::vfs::CreateDirectory(kodi::GetBaseUserPath());
  }

  CDSPSettings settings;
  settings.LoadSettingsData(true);

  std::string imagePath;

  m_ModeInfoStruct.uiUniqueDBModeId       = 0;         // set by RegisterMode, @todo needs to be removed so that the developer doesn't has to care about it
  m_ModeInfoStruct.uiModeNumber            = ID_MASTER_PROCESS_FREE_SURROUND;
  m_ModeInfoStruct.bHasSettingsDialog     = true;
  m_ModeInfoStruct.iModeDescription       = 30002;
  m_ModeInfoStruct.iModeHelp              = 30003;
  m_ModeInfoStruct.iModeName              = 30000;
  m_ModeInfoStruct.iModeSetupName         = 30001;
  //m_ModeInfoStruct.iModeSupportTypeFlags  = AE_DSP_PRSNT_ASTREAM_BASIC | AE_DSP_PRSNT_ASTREAM_MUSIC | AE_DSP_PRSNT_ASTREAM_MOVIE;
  m_ModeInfoStruct.bIsDisabled            = false;

  strncpy(m_ModeInfoStruct.strModeName, "Free Surround", sizeof(m_ModeInfoStruct.strModeName) - 1);
  imagePath = kodi::GetAddonPath();
  imagePath += "/resources/media/adsp-freesurround.png";
  strncpy(m_ModeInfoStruct.strOwnModeImage, imagePath.c_str(), sizeof(m_ModeInfoStruct.strOwnModeImage) - 1);
  memset(m_ModeInfoStruct.strOverrideModeImage, 0, sizeof(m_ModeInfoStruct.strOwnModeImage)); // unused
  RegisterMode(&m_ModeInfoStruct);

  m_MenuHook.iHookId                      = ID_MASTER_PROCESS_FREE_SURROUND;
  m_MenuHook.category                     = AUDIODSP_ADDON_MENUHOOK_MODE;
  m_MenuHook.iLocalizedStringId           = 30001;
  m_MenuHook.iRelevantModeId              = ID_MASTER_PROCESS_FREE_SURROUND;
  AddMenuHook(&m_MenuHook);
}

CFreeSurroundAddon::~CFreeSurroundAddon()
{
}

AUDIODSP_ADDON_ERROR CFreeSurroundAddon::MenuHook(const AUDIODSP_MENU_HOOK &menuhook)
{
  if (menuhook.iHookId == ID_MASTER_PROCESS_FREE_SURROUND)
  {
    CGUIDialogFreeSurround settings(0/*item.data.iStreamId*/);
    settings.DoModal();
    return AUDIODSP_ADDON_ERROR_NO_ERROR;
  }
  return AUDIODSP_ADDON_ERROR_UNKNOWN;
}

AUDIODSP_ADDON_ERROR CFreeSurroundAddon::CreateModeHandle(const AUDIODSP_ADDON_AUDIO_FORMAT* inputFormat,
                                                          const AUDIODSP_ADDON_AUDIO_FORMAT* outputFormat,
                                                          const AUDIODSP_ADDON_STREAM_PROPERTIES* streamProperties,
                                                          const uint64_t ID,
                                                          ADDON_HANDLE modeHandle)
{
  //! @todo check ID
  CDSPProcess_FreeSurround *proc = new CDSPProcess_FreeSurround(*inputFormat, *outputFormat, *streamProperties);

  if (!proc)
  {
    return AUDIODSP_ADDON_ERROR_FAILED;
  }

  modeHandle->callerAddress = proc;
  modeHandle->dataIdentifier = streamProperties->uiStreamID;

  return AUDIODSP_ADDON_ERROR_NO_ERROR;
}

AUDIODSP_ADDON_ERROR CFreeSurroundAddon::CreateMode(const ADDON_HANDLE modeHandle)
{
  return ((CDSPProcess_FreeSurround*)modeHandle->callerAddress)->Create();
}

AUDIODSP_ADDON_ERROR CFreeSurroundAddon::DestroyMode(const ADDON_HANDLE modeHandle)
{
  return ((CDSPProcess_FreeSurround*)modeHandle->callerAddress)->Destroy();
}

AUDIODSP_ADDON_AUDIO_FORMAT CFreeSurroundAddon::GetModeInputFormat(const ADDON_HANDLE modeHandle)
{
  return ((CDSPProcess_FreeSurround*)modeHandle->callerAddress)->GetInputFormat();
}

AUDIODSP_ADDON_AUDIO_FORMAT CFreeSurroundAddon::GetModeOutFormat(const ADDON_HANDLE modeHandle)
{
  return ((CDSPProcess_FreeSurround*)modeHandle->callerAddress)->GetOutputFormat();
}

int CFreeSurroundAddon::ProcessMode(const ADDON_HANDLE modeHandle, const uint8_t **array_in, uint8_t **array_out)
{
  return ((CDSPProcess_FreeSurround*)modeHandle->callerAddress)->Process(reinterpret_cast<const float**>(array_in), reinterpret_cast<float**>(array_out));
}

AUDIODSP_ADDON_ERROR CFreeSurroundAddon::DestroyModeHandle(ADDON_HANDLE modeHandle)
{
  CDSPProcess_FreeSurround *mode = ((CDSPProcess_FreeSurround*)modeHandle->callerAddress);
  if (mode)
  {
    delete mode;
  }

  modeHandle->callerAddress = nullptr;
  modeHandle->dataAddress = nullptr;
  modeHandle->dataIdentifier = -1;

  return AUDIODSP_ADDON_ERROR_NO_ERROR;
}

ADDONCREATOR(CFreeSurroundAddon);
