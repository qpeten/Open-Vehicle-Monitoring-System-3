/*
; Project:        Open Vehicle Monitor System
; Subproject:     Integration of support for the VW ID.3
;
; (c) Quentin Peten
;
; Based on the OBDII integration
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"
static const char *TAG = "v-vwid3";

#include <stdio.h>
#include "vehicle_vwid3.h"

static const OvmsPoller::poll_pid_t obdii_polls[]
  =
  {
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x05, {  0, 30, 30 }, 0, ISOTP_STD }, // Engine coolant temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0c, { 10, 10, 10 }, 0, ISOTP_STD }, // Engine RPM
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0d, {  0, 10, 10 }, 0, ISOTP_STD }, // Speed
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0f, {  0, 30, 30 }, 0, ISOTP_STD }, // Engine air intake temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x2f, {  0, 30, 30 }, 0, ISOTP_STD }, // Fuel level
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x46, {  0, 30, 30 }, 0, ISOTP_STD }, // Ambiant temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x5c, {  0, 30, 30 }, 0, ISOTP_STD }, // Engine oil temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIIVEHICLE, 0x02, {999,999,999 }, 0, ISOTP_STD }, // VIN
    POLL_LIST_END
  };

OvmsVehicleVWID3::OvmsVehicleVWID3()
  {
  ESP_LOGI(TAG, "VW ID3 vehicle module");

  memset(m_vin,0,sizeof(m_vin));

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  PollSetPidList(m_can1,obdii_polls);
  PollSetState(0);
  }

OvmsVehicleVWID3::~OvmsVehicleVWID3()
  {
  ESP_LOGI(TAG, "Shutdown VW ID3 vehicle module");
  }

void OvmsVehicleVWID3::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
  {
  int value1 = (int)data[0];
  int value2 = ((int)data[0] << 8) + (int)data[1];

  switch (job.pid)
    {
    case 0x02:  // VIN (multi-line response)
      // Data in the first frame starts with 0x01 for some (all?) vehicles
      if (length > 1 && data[0] == 0x01)
        {
        ++data;
        --length;
        }
      strncat(m_vin,(char*)data,length);
      if (job.mlremain==0)
        {
        StandardMetrics.ms_v_vin->SetValue(m_vin);
        m_vin[0] = 0;
        }
      break;
    case 0x05:  // Engine coolant temperature
      StandardMetrics.ms_v_bat_temp->SetValue(value1 - 0x28);
      break;
    case 0x0f:  // Engine intake air temperature
      StandardMetrics.ms_v_inv_temp->SetValue(value1 - 0x28);
      break;
    case 0x5c:  // Engine oil temperature
      StandardMetrics.ms_v_mot_temp->SetValue(value1 - 0x28);
      break;
    case 0x46:  // Ambient temperature
      StandardMetrics.ms_v_env_temp->SetValue(value1 - 0x28);
      break;
    case 0x0d:  // Speed
      StandardMetrics.ms_v_pos_speed->SetValue(value1);
      break;
    case 0x2f:  // Fuel Level
      StandardMetrics.ms_v_bat_soc->SetValue((value1 * 100) >> 8);
      break;
    case 0x0c:  // Engine RPM
      if (value2 == 0)
        { // Car engine is OFF
        PollSetState(0);
        StandardMetrics.ms_v_env_handbrake->SetValue(true);
        StandardMetrics.ms_v_env_on->SetValue(false);
        StandardMetrics.ms_v_pos_speed->SetValue(0);
        StandardMetrics.ms_v_env_charging12v->SetValue(false);
        }
      else
        { // Car engine is ON
        PollSetState(1);
        StandardMetrics.ms_v_env_handbrake->SetValue(false);
        StandardMetrics.ms_v_env_on->SetValue(true);
        StandardMetrics.ms_v_env_charging12v->SetValue(true);
        }
      break;
    }
  }

class OvmsVehicleVWID3Init
  {
  public: OvmsVehicleVWID3Init();
} MyOvmsVehicleVWID3Init  __attribute__ ((init_priority (9001)));

OvmsVehicleVWID3Init::OvmsVehicleVWID3Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: VWID3 (9001)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleVWID3>("MEB","VWID3");
  }