/*
This project, FPGA Crypto Service Server, is licensed as below

***************************************************************************

Copyright 2020-2021 Intel Corporation. All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***************************************************************************
*/

#include "FcsCommunication.h"
#include "Logger.h"
#include "utils.h"

#include "intel_fcs-ioctl.h"
#include "intel_fcs_structs.h"

#include <fcntl.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

#define FCS_DEVICE_PATH "/dev/fcs"

bool FcsCommunication::sendIoctl(
    intel_fcs_dev_ioctl *data,
    unsigned long commandCode)
{
    int deviceFileDescriptor = open(FCS_DEVICE_PATH, O_RDWR);
    if (deviceFileDescriptor < 0)
    {
        Logger::logWithReturnCode("Opening device failed.", errno, Error);
        return false;
    }
    if (ioctl(deviceFileDescriptor, commandCode, data) < 0)
    {
        close(deviceFileDescriptor);
        Logger::logWithReturnCode("Ioctl failed.", errno, Error);
        return false;
    }
    close(deviceFileDescriptor);
    Logger::logWithReturnCode("Ioctl success.", data->status, Debug);
    return true;
}

int32_t FcsCommunication::getChipId(
    size_t &outSize, unsigned char **outBuffer)
{
    Logger::log("Calling getChipId");
    intel_fcs_dev_ioctl data = {};
    if (!sendIoctl(&data, INTEL_FCS_DEV_CHIP_ID))
    {
        return kIoctlFailedError;
    }
    outSize = sizeof(data.com_paras.c_id);
    *outBuffer = new unsigned char[outSize];

    //TODO: find out why low is first and high is second
    Utils::encodeToLittleEndianBuffer(
        data.com_paras.c_id.chip_id_low,
        *outBuffer);
    Utils::encodeToLittleEndianBuffer(
        data.com_paras.c_id.chip_id_high,
        *outBuffer + sizeof(data.com_paras.c_id.chip_id_low));
    return data.status;
}

int32_t FcsCommunication::sigmaTeardown(uint32_t sessionId)
{
    Logger::log("Calling sigmaTeardown with session ID: "
        + std::to_string(static_cast<int32_t>(sessionId)));
    intel_fcs_dev_ioctl data = {};
    data.com_paras.tdown.teardown = true;
    data.com_paras.tdown.sid = sessionId;

    if (!sendIoctl(&data, INTEL_FCS_DEV_PSGSIGMA_TEARDOWN))
    {
        return kIoctlFailedError;
    }
    return data.status;
}

int32_t FcsCommunication::createAttestationSubkey(
    unsigned char *inBuffer, size_t inSize,
    unsigned char **outBuffer, size_t &outSize)
{
    Logger::log("Calling createAttestationSubkey");
    *outBuffer = new unsigned char[ATTESTATION_SUBKEY_RSP_MAX_SZ];
    outSize = 0;

    intel_fcs_dev_ioctl data = {};
    data.com_paras.subkey.resv.resv_word = 0;
    data.com_paras.subkey.cmd_data = (char*)inBuffer;
    data.com_paras.subkey.cmd_data_sz = inSize;
    data.com_paras.subkey.rsp_data = (char*)*outBuffer;
    data.com_paras.subkey.rsp_data_sz = ATTESTATION_SUBKEY_RSP_MAX_SZ;

    if (!sendIoctl(&data, INTEL_FCS_DEV_ATTESTATION_SUBKEY)
        || data.com_paras.subkey.rsp_data_sz > ATTESTATION_SUBKEY_RSP_MAX_SZ)
    {
        delete[] *outBuffer;
        return kIoctlFailedError;
    }

    outSize = data.com_paras.subkey.rsp_data_sz;
    return data.status;
}

int32_t FcsCommunication::getMeasurement(
    unsigned char *inBuffer, size_t inSize,
    unsigned char **outBuffer, size_t &outSize)
{
    Logger::log("Calling getMeasurement");
    *outBuffer = new unsigned char[ATTESTATION_MEASUREMENT_RSP_MAX_SZ];
    outSize = 0;

    intel_fcs_dev_ioctl data = {};
    data.com_paras.measurement.resv.resv_word = 0;
    data.com_paras.measurement.cmd_data = (char*)inBuffer;
    data.com_paras.measurement.cmd_data_sz = inSize;
    data.com_paras.measurement.rsp_data = (char*)*outBuffer;
    data.com_paras.measurement.rsp_data_sz = ATTESTATION_MEASUREMENT_RSP_MAX_SZ;

    if (!sendIoctl(&data, INTEL_FCS_DEV_ATTESTATION_MEASUREMENT)
        || data.com_paras.measurement.rsp_data_sz > ATTESTATION_MEASUREMENT_RSP_MAX_SZ)
    {
        delete[] *outBuffer;
        return kIoctlFailedError;
    }

    outSize = data.com_paras.subkey.rsp_data_sz;
    return data.status;
}