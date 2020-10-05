/*
 * Copyright 2020 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef SMU_11_5_0_PPSMC_H
#define SMU_11_5_0_PPSMC_H

// SMU Response Codes:
#define PPSMC_Result_OK 0x1
#define PPSMC_Result_Failed 0xFF
#define PPSMC_Result_UnknownCmd 0xFE
#define PPSMC_Result_CmdRejectedPrereq 0xFD
#define PPSMC_Result_CmdRejectedBusy 0xFC

// Message Definitions:
#define PPSMC_MSG_TestMessage 0x1
#define PPSMC_MSG_GetSmuVersion 0x2
#define PPSMC_MSG_GetDriverIfVersion 0x3
#define PPSMC_MSG_EnableGfxOff 0x4
#define PPSMC_MSG_DisableGfxOff 0x5
#define PPSMC_MSG_PowerDownIspByTile 0x6 // ISP is power gated by default
#define PPSMC_MSG_PowerUpIspByTile 0x7
#define PPSMC_MSG_PowerDownVcn 0x8 // VCN is power gated by default
#define PPSMC_MSG_PowerUpVcn 0x9
#define PPSMC_MSG_spare 0xA
#define PPSMC_MSG_SetHardMinVcn 0xB // For wireless display
#define PPSMC_MSG_SetMinVideoGfxclkFreq	0xC //Sets SoftMin for GFXCLK. Arg is in MHz
#define PPSMC_MSG_ActiveProcessNotify 0xD
#define PPSMC_MSG_SetHardMinIspiclkByFreq 0xE
#define PPSMC_MSG_SetHardMinIspxclkByFreq 0xF
#define PPSMC_MSG_SetDriverDramAddrHigh 0x10
#define PPSMC_MSG_SetDriverDramAddrLow 0x11
#define PPSMC_MSG_TransferTableSmu2Dram 0x12
#define PPSMC_MSG_TransferTableDram2Smu 0x13
#define PPSMC_MSG_GfxDeviceDriverReset 0x14 //mode 2 reset during TDR
#define PPSMC_MSG_GetEnabledSmuFeatures 0x15
#define PPSMC_MSG_spare1 0x16
#define PPSMC_MSG_SetHardMinSocclkByFreq 0x17
#define PPSMC_MSG_SetMinVideoFclkFreq 0x18
#define PPSMC_MSG_SetSoftMinVcn 0x19
#define PPSMC_MSG_EnablePostCode 0x1A
#define PPSMC_MSG_GetGfxclkFrequency 0x1B
#define PPSMC_MSG_GetFclkFrequency 0x1C
#define PPSMC_MSG_AllowGfxOff 0x1D
#define PPSMC_MSG_DisallowGfxOff 0x1E
#define PPSMC_MSG_SetSoftMaxGfxClk 0x1F
#define PPSMC_MSG_SetHardMinGfxClk 0x20
#define PPSMC_MSG_SetSoftMaxSocclkByFreq 0x21
#define PPSMC_MSG_SetSoftMaxFclkByFreq 0x22
#define PPSMC_MSG_SetSoftMaxVcn 0x23
#define PPSMC_MSG_GpuChangeState 0x24 //FIXME AHOLLA - check how to do for VGM
#define PPSMC_MSG_SetPowerLimitPercentage 0x25
#define PPSMC_MSG_PowerDownJpeg 0x26
#define PPSMC_MSG_PowerUpJpeg 0x27
#define PPSMC_MSG_SetHardMinFclkByFreq 0x28
#define PPSMC_MSG_SetSoftMinSocclkByFreq 0x29
#define PPSMC_MSG_PowerUpCvip 0x2A
#define PPSMC_MSG_PowerDownCvip 0x2B
#define PPSMC_Message_Count 0x2C

//Argument for  PPSMC_MSG_GpuChangeState
enum {
  GpuChangeState_D0Entry = 1,
  GpuChangeState_D3Entry,
};

#endif
