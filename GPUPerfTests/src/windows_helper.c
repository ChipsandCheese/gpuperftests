// Copyright (c) 2023, Nemes <nemes@nemez.net>
// SPDX-License-Identifier: MIT
// 
// MIT License
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software andassociated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, andto permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice andthis permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifdef _WIN32

#include <Windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <winternl.h>
#include "main.h"
#include "logger.h"
#include "helper.h"
#include "windows_helper.h"

#pragma comment(lib, "Setupapi.lib")

typedef ULONG	MYNTSTATUS;

typedef struct _D3DKMT_OPENADAPTERFROMDEVICENAME {
    PCWSTR							pDeviceName;
    D3DKMT_HANDLE					hAdapter;
    LUID							AdapterLuid;
} D3DKMT_OPENADAPTERFROMDEVICENAME;

typedef struct _D3DKMT_CLOSEADAPTER {
    D3DKMT_HANDLE					hAdapter;
} D3DKMT_CLOSEADAPTER;

typedef MYNTSTATUS(APIENTRY *D3DKMTOPENADAPTERFROMDEVICENAME)(D3DKMT_OPENADAPTERFROMDEVICENAME *);
typedef MYNTSTATUS(APIENTRY *D3DKMTCLOSEADAPTER)(D3DKMT_CLOSEADAPTER *);

static bool d3dkmt_initialize();
static void d3dkmt_uninitialize();
static MYNTSTATUS d3dkmt_OpenAdapterFromDeviceName(D3DKMT_OPENADAPTERFROMDEVICENAME *pData);
static MYNTSTATUS d3dkmt_CloseAdapter(D3DKMT_CLOSEADAPTER *pData);

static HINSTANCE d3dkmt_gdi32_dll_handle;
static D3DKMTOPENADAPTERFROMDEVICENAME d3dkmt_ptr_OpenAdapterFromDeviceName;
static D3DKMTCLOSEADAPTER d3dkmt_ptr_CloseAdapter;

static bool d3dkmt_initialize() {
    d3dkmt_uninitialize();

    d3dkmt_gdi32_dll_handle = LoadLibrary(L"GDI32.DLL");
    if (d3dkmt_gdi32_dll_handle) {
        d3dkmt_ptr_OpenAdapterFromDeviceName = (D3DKMTOPENADAPTERFROMDEVICENAME)GetProcAddress(d3dkmt_gdi32_dll_handle, "D3DKMTOpenAdapterFromDeviceName");
        d3dkmt_ptr_CloseAdapter = (D3DKMTCLOSEADAPTER)GetProcAddress(d3dkmt_gdi32_dll_handle, "D3DKMTCloseAdapter");
    }

    if (d3dkmt_ptr_OpenAdapterFromDeviceName && d3dkmt_ptr_CloseAdapter) {
        return true;
    } else {
        return false;
    }
}

static void d3dkmt_uninitialize() {
    if (d3dkmt_gdi32_dll_handle) {
        FreeLibrary(d3dkmt_gdi32_dll_handle);
    }
    d3dkmt_gdi32_dll_handle = 0;
    d3dkmt_ptr_OpenAdapterFromDeviceName = NULL;
    d3dkmt_ptr_CloseAdapter = NULL;
}

static MYNTSTATUS d3dkmt_OpenAdapterFromDeviceName(D3DKMT_OPENADAPTERFROMDEVICENAME* pData) {
    if (d3dkmt_ptr_OpenAdapterFromDeviceName) {
        return d3dkmt_ptr_OpenAdapterFromDeviceName(pData);
    }
    return WIN_D3DKMT_OPENADAPTERFROMDEVICENAME_FAIL;
}

static MYNTSTATUS d3dkmt_CloseAdapter(D3DKMT_CLOSEADAPTER* pData) {
    if (d3dkmt_ptr_CloseAdapter) {
        return d3dkmt_ptr_CloseAdapter(pData);
    }
    return WIN_D3DKMT_CLOSEADAPTER_FAIL;
}

test_status WindowsHelperInitializeGPUs(void *win_devices, uint32_t* device_count) {
    if (!d3dkmt_initialize()) {
        FATAL("Failed to initialize D3DKMT subsystem\n");
        return WIN_D3DKMT_FAIL_INIT;
    }
    test_status status = HelperArrayListInitialize(win_devices, sizeof(windows_helper_gpu));
    TEST_RETFAIL(status);
    GUID DISPLAY_DEVICE_ARRIVAL_GUID = { 0x1CA05180, 0xA699, 0x450A, 0x9A, 0x0C, 0xDE, 0x4F, 0xBE, 0x3D, 0xDD, 0x89 };
    HDEVINFO hdev_info = SetupDiGetClassDevs(&DISPLAY_DEVICE_ARRIVAL_GUID, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdev_info == INVALID_HANDLE_VALUE) {
        FATAL("SetupDiGetClassDevs failed\n");
        return WIN_SETUPDIGETCLASSDEV_FAIL;
    }
    SP_DEVICE_INTERFACE_DATA device_interface_data;
    device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    for (DWORD device_interface = 0; SetupDiEnumDeviceInterfaces(hdev_info, NULL, &DISPLAY_DEVICE_ARRIVAL_GUID, device_interface, &device_interface_data); device_interface++) {
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetail(hdev_info, &device_interface_data, NULL, 0, &required_size, NULL);
        if (required_size) {
            PSP_DEVICE_INTERFACE_DETAIL_DATA device_interface_detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(required_size);
            if (device_interface_detail_data == NULL) {
                SetupDiDestroyDeviceInfoList(hdev_info);
                return TEST_OUT_OF_MEMORY;
            }
            device_interface_detail_data->cbSize = sizeof(PSP_DEVICE_INTERFACE_DETAIL_DATA);

            if (SetupDiGetDeviceInterfaceDetail(hdev_info, &device_interface_data, device_interface_detail_data, required_size, NULL, NULL)) {
                char device_name[WIN_MAX_STRING_LENGTH];
                WCHAR device_path[MAX_PATH];
                memcpy(device_path, device_interface_detail_data->DevicePath, sizeof(device_path));
                if (wcsstr(device_path, L"root#basicrender") == 0) {
                    DWORD property_reg_datatype;
                    DWORD property_reg_required_size;
                    SP_DEVINFO_DATA devinfo_data;
                    devinfo_data.cbSize = sizeof(SP_DEVINFO_DATA);
                    if (SetupDiEnumDeviceInfo(hdev_info, device_interface, &devinfo_data)) {
                        SetupDiGetDeviceRegistryPropertyA(hdev_info, &devinfo_data, SPDRP_DEVICEDESC, &property_reg_datatype, NULL, 0, &property_reg_required_size);
                        if (property_reg_required_size) {
                            char* temp_name = (char*)malloc(property_reg_required_size);
                            memset(temp_name, 0, property_reg_required_size);
                            if (!SetupDiGetDeviceRegistryPropertyA(hdev_info, &devinfo_data, SPDRP_DEVICEDESC, &property_reg_datatype, (BYTE*)temp_name, property_reg_required_size, NULL)) {
                                free(temp_name);
                                temp_name = (char*)malloc(sizeof("Unknown Graphics Adapter"));
                                strcpy_s(temp_name, strlen("Unknown Graphics Adapter"), "Unknown Graphics Adapter");
                            }
                            strncpy(device_name, temp_name, sizeof(device_name));
                            free(temp_name);
                        }
                    }
                    windows_helper_gpu win_gpu;
                    strncpy(win_gpu.name, device_name, WIN_MAX_STRING_LENGTH);
                    memcpy(win_gpu.path, device_path, MAX_PATH * sizeof(WCHAR));
                    if (&win_gpu != NULL) {
                        D3DKMT_OPENADAPTERFROMDEVICENAME openAdapter;
                        ZeroMemory(&openAdapter, sizeof(D3DKMT_OPENADAPTERFROMDEVICENAME));
                        openAdapter.pDeviceName = win_gpu.path;
                        if (SUCCEEDED(d3dkmt_OpenAdapterFromDeviceName(&openAdapter))) {
                            memcpy(&(win_gpu.device_luid), &openAdapter.AdapterLuid, sizeof(LUID));
                            memcpy(&(win_gpu.adapter_handle), &openAdapter.hAdapter, sizeof(D3DKMT_HANDLE));
                            (*device_count)++;
                            D3DKMT_CLOSEADAPTER closeAdapter;
                            ZeroMemory(&closeAdapter, sizeof(D3DKMT_CLOSEADAPTER));
                            closeAdapter.hAdapter = win_gpu.adapter_handle;
                            d3dkmt_CloseAdapter(&closeAdapter);
                            status = HelperArrayListAdd(win_devices, &win_gpu, sizeof(windows_helper_gpu), &win_gpu.gpu_index);
                            if (!TEST_SUCCESS(status)) {
                                HelperArrayListClean(win_devices);
                                return status;
                            }
                        }
                    }
                }
            }
            free(device_interface_detail_data);
        }
    }
    SetupDiDestroyDeviceInfoList(hdev_info);
    return TEST_OK;
}

test_status WindowsHelperCleanGPUs(void* win_devices) {
    test_status status = HelperArrayListClean(win_devices);
    TEST_RETFAIL(status);
    d3dkmt_uninitialize();
    return TEST_OK;
}

uint64_t WindowsHelperGPUGetLUID(windows_helper_gpu *device) {
    uint64_t luid = (uint64_t)device->device_luid.LowPart | ((uint64_t)device->device_luid.HighPart << 32);
    return luid;
}
#endif