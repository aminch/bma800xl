//
// network.cpp
//
// Based on BMC64's network start up (viceapp.cpp, vice_network.cpp).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "network.h"

#include "network_time_sync.h"
#include "options.h"
#include "webui/webui.h"

#include <circle/logger.h>
#include <circle/machineinfo.h>
#include <circle/net/netsubsystem.h>
#include <circle/string.h>
#include <fatfs/ff.h>
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>

#include <stdio.h>
#include <string.h>

static const char FromNetwork[] = "network";
#define HOSTNAME "bma800xl"
#define FIRMWARE_PATH "SD:/firmware/"
#define WPA_CONFIG "SD:/wpa_supplicant.conf"

static CNetSubSystem *s_net = 0;
static CBcm4343Device *s_wlan = 0;
static CWPASupplicant *s_wpa = 0;
static int s_device = 0;  // 0 off, 1 Ethernet, 2 Wi-Fi
static int s_status = NETWORK_DISABLED;

static bool HasOnboardWLAN(TMachineModel model) {
  switch (model) {
  case MachineModelZeroW:
  case MachineModelZero2W:
  case MachineModel3B:
  case MachineModel3APlus:
  case MachineModel3BPlus:
    return true;
  default:
    return false;
  }
}

static bool HasOnboardEthernet(TMachineModel model) {
  switch (model) {
  case MachineModel2B:
  case MachineModel3B:
  case MachineModel3BPlus:
    return true;
  default:
    return false;
  }
}

static bool FileExists(const char *path) {
  FILINFO info;
  return f_stat(path, &info) == FR_OK && !(info.fattrib & AM_DIR) &&
         info.fsize > 0;
}

static bool HasWifiFirmware(void) {
  static const char *const files[] = {
      "brcmfmac43430-sdio.bin",  "brcmfmac43430-sdio.txt",
      "brcmfmac43430-sdio.clm_blob", "brcmfmac43436-sdio.bin",
      "brcmfmac43436-sdio.txt",  "brcmfmac43436-sdio.clm_blob",
      "brcmfmac43436s-sdio.bin", "brcmfmac43436s-sdio.txt",
      "brcmfmac43455-sdio.bin",  "brcmfmac43455-sdio.clm_blob",
      "brcmfmac43455-sdio.txt",
  };
  for (unsigned i = 0; i < sizeof files / sizeof files[0]; i++) {
    char path[64];
    snprintf(path, sizeof path, "%s%s", FIRMWARE_PATH, files[i]);
    if (!FileExists(path)) {
      return false;
    }
  }
  return true;
}

static void StartServices(const Options *options) {
  StartNetworkTimeSync(s_net);
  if (options->GetBool("webui", true)) {
    WebUiStart(s_net, options->Get("webui_pin", ""));
  }
}

void NetworkInitialize(const Options *options) {
  CLogger *log = CLogger::Get();
  const char *device = options->Get("network", "off");
  if (strcmp(device, "ethernet") == 0) {
    s_device = 1;
  } else if (strcmp(device, "wifi") == 0) {
    s_device = 2;
  } else {
    s_device = 0;
    s_status = NETWORK_DISABLED;
    log->Write(FromNetwork, LogNotice, "Networking not enabled");
    return;
  }

  // Minutes east of UTC, for file times from the network clock.
  if (!ConfigureSystemTimeZone(options->GetInt("timezone", 0))) {
    log->Write(FromNetwork, LogWarning, "Cannot configure timezone");
  }

  TMachineModel model = CMachineInfo::Get()->GetMachineModel();

  if (s_device == 1) {
    if (!HasOnboardEthernet(model)) {
      s_status = NETWORK_ETHERNET_UNAVAILABLE;
      log->Write(FromNetwork, LogError,
                 "Ethernet selected, but this Raspberry Pi has none");
      return;
    }
    s_status = NETWORK_ETHERNET_INITIALIZING;
    s_net = new CNetSubSystem(0, 0, 0, 0, HOSTNAME, NetDeviceTypeEthernet);
    if (!s_net->Initialize(FALSE)) {
      s_status = NETWORK_ETHERNET_INIT_FAILED;
      log->Write(FromNetwork, LogError, "Cannot initialize Ethernet");
      delete s_net;
      s_net = 0;
      return;
    }
    s_status = NETWORK_ETHERNET_WAITING_FOR_DHCP;
    log->Write(FromNetwork, LogNotice, "Ethernet initialized");
    StartServices(options);
    return;
  }

  if (!HasOnboardWLAN(model)) {
    s_status = NETWORK_WIFI_UNAVAILABLE;
    log->Write(FromNetwork, LogError,
               "Wi-Fi selected, but this Raspberry Pi has none");
    return;
  }
  if (!HasWifiFirmware()) {
    s_status = NETWORK_WIFI_FIRMWARE_MISSING;
    log->Write(FromNetwork, LogError, "Wi-Fi firmware is missing from %s",
               FIRMWARE_PATH);
    return;
  }
  if (!FileExists(WPA_CONFIG)) {
    s_status = NETWORK_WIFI_CONFIG_MISSING;
    log->Write(FromNetwork, LogError, "Wi-Fi config is missing: %s",
               WPA_CONFIG);
    return;
  }

  s_status = NETWORK_WIFI_DEVICE_INITIALIZING;
  s_wlan = new CBcm4343Device(FIRMWARE_PATH);
  if (!s_wlan->Initialize()) {
    s_status = NETWORK_WIFI_DEVICE_INIT_FAILED;
    log->Write(FromNetwork, LogError, "Cannot initialize WLAN");
    delete s_wlan;
    s_wlan = 0;
    return;
  }

  s_net = new CNetSubSystem(0, 0, 0, 0, HOSTNAME, NetDeviceTypeWLAN);
  if (!s_net->Initialize(FALSE)) {
    s_status = NETWORK_WIFI_NETWORK_INIT_FAILED;
    log->Write(FromNetwork, LogError, "Cannot initialize WLAN network stack");
    delete s_net;
    s_net = 0;
    delete s_wlan;
    s_wlan = 0;
    return;
  }
  StartServices(options);

  s_status = NETWORK_WIFI_WPA_INITIALIZING;
  s_wpa = new CWPASupplicant(WPA_CONFIG);
  if (!s_wpa->Initialize()) {
    s_status = NETWORK_WIFI_WPA_INIT_FAILED;
    log->Write(FromNetwork, LogError, "Cannot initialize WPA supplicant");
    delete s_wpa;
    s_wpa = 0;
    return;
  }
  s_status = NETWORK_WIFI_CONNECTING;
  log->Write(FromNetwork, LogNotice, "Wi-Fi initialized");
}

//
// C interface used by the web UI and the emulator.
//

extern "C" int circle_get_network_ip_address(char *address,
                                             unsigned int address_size) {
  if (address == 0 || address_size == 0 || s_net == 0 || !s_net->IsRunning()) {
    return 0;
  }
  CString formatted;
  s_net->GetConfig()->GetIPAddress()->Format(&formatted);
  if (strcmp((const char *)formatted, "0.0.0.0") == 0) {
    return 0;
  }
  strncpy(address, (const char *)formatted, address_size - 1);
  address[address_size - 1] = '\0';
  return 1;
}

extern "C" int circle_get_network_status(void) {
  char ip[32];
  if (circle_get_network_ip_address(ip, sizeof ip)) {
    return s_device == 1 ? NETWORK_ETHERNET_CONNECTED : NETWORK_WIFI_CONNECTED;
  }
  return s_status;
}

extern "C" const char *circle_get_disk_volume(void) { return "SD"; }
