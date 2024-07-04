# Release 1.0.0

**Version Changes:**
* `vk_rate_*`: 1.2.0 -> `vk_rate_*` 1.4.0 (improvements, bugfixes)
* `vk_latency`: 1.1.1 -> `vk_latency_scalar` and `vk_latency_vector` 1.2.0 (improvements, renamed)

**Known Issues:**
* `vk_rate_int16_*` sometimes produce incorrect results (observed on AMD RDNA2 and NVIDIA Turing/Ampere)
* `vk_rate_fp16_*` produces incorrect results on Linux with some AMD Radeon GPUs (seems to be RADV specific)
* `vk_bandwidth` does not appear to be very accurate near cache boundaries on Linux
* `vk_uplink_mapped_read` and `vk_uplink_mapped_write` produce impossible results on Linux (due to CPU-side caching differences from Windows)
* `vk_uplink_latency` and `vk_uplink_latency_long` produce impossible results on Linux (due to CPU-side caching differences from Windows)

**New Features:**
* Floating point reciprocal throughput tests (`vk_rate_fp16_rcp`, `vk_rate_fp32_rcp`, `vk_rate_fp64_rcp`)
* Separate scalar and vector latency tests (`vk_latency_scalar`, `vk_latency_vector`)
* Proper DPI scaling in the GUI.
* Ability to abort the running test.
* Ability to remove tests that have been queued to run.
* About, Changelog and Build Info menus in the GUI.
* Linux support (highly experimental, test results may not be accurate!)

**Improvements:**
* Accuracy improvements to `vk_rate_fp*_mul`, `vk_rate_fp*_sub` and `vk_rate_fp*_add`.
* Vastly improved the GUI, both visually and code-wise.
* GUI takes much less processing power, mainly noticeable on integrated graphics.
* Shader files are now packed into the binary, not requiring the clunky shaders folder anymore.

**Bug Fixes:**
* Fixed `vk_rate_fp16_sub` and `vk_rate_fp16_mul` producing garbage results on AMD RDNA GPUs.
* Fixed `vk_rate_fp*_add` compiling to incorrect instructions.
* Fixed potential crashes on some very low end GPUs in the bandwidth and latency GUI.

**Deprecated or Removed:**
* None

# Prerelease 0.9.0

**Version Changes:**
* `vk_uplink_read`: 1.0.0 -> `vk_uplink_copy_read` 1.1.0 (improvements, renamed)
* `vk_uplink_write`: 1.0.0 -> `vk_uplink_copy_write` 1.1.0 (improvements, renamed)
* `vk_uplink_latency`: 1.0.0 -> 1.1.0 (improvements)
* `vk_uplink_latency_long`: 1.0.0 -> 1.1.0 (improvements)
* `vk_uplink_compute_read`: 1.1.0 (new)
* `vk_uplink_compute_write`: 1.1.0 (new)
* `vk_uplink_mapped_read`: 1.1.0 (new)
* `vk_uplink_mapped_write`: 1.1.0 (new)

**New Features:**
* GPU uplink (PCIe & similar) read and write bandwidth test using compute shaders (`vk_uplink_compute_read`, `vk_uplink_compute_write`)
* GPU uplink (PCIe & similar) read and write bandwidth test using CPU memory access through mapped buffers (`vk_uplink_mapped_read`, `vk_uplink_mapped_write`)

**Improvements:**
* Improved accuracy of `vk_uplink_copy_read` and `vk_uplink_copy_write`.
* Code cleanup, removed some compilation warnings.
* Renamed project/solution.

**Bug Fixes:**
* `vk_uplink_*` tests can now run on Intel iGPUs.

**Deprecated or Removed:**
* Removed unused library files, exes, docs, etc.
* Removed `vk_uplink_read` and `vk_uplink_write` tests (renamed to `vk_uplink_copy_read` and `vk_uplink_copy_write`)

# Prerelease 0.8.0

**Version Changes:**
* `vk_bandwidth`: 1.3.0 -> 1.3.1 (bug fixes)
* `vk_latency`: 1.1.0 -> 1.1.1 (bug fixes)
* `vk_uplink_read`: 1.0.0 (new)
* `vk_uplink_write`: 1.0.0 (new)
* `vk_uplink_latency`: 1.0.0 (new)
* `vk_uplink_latency_long`: 1.0.0 (new)

**New Features:**
* GPU uplink (PCIe & similar) read and write bandwidth test (`vk_uplink_read`, `vk_uplink_write`)
* GPU uplink (PCIe & similar) latency test (`vk_uplink_latency`, `vk_uplink_latency_long`)

**Improvements:**
* General code improvements, got rid of most compiler warnings.

**Bug Fixes:**
* Fixed incorrect device name formatting in `vk_bandwidth` CSV output results.
* Fixed incorrect device name formatting in `vk_latency` CSV output results.

**Deprecated or Removed:**
* None

# Prerelease 0.7.0

**Improvements:**
* Made GUI buttons and text fields more visible.
* Changed the layout of result tables.
* Made default GUI window size more suitable for the new layout.
* Massively improved the accuracy of `vk_rate_*` tests.

**Bug Fixes:**
* Fixed GUI not showing GPUs if ran from a folder that contains a space.
* Fixed spurious GUI crash when starting a test.

# Prerelease 0.6.0

**New Features:**
* Vulkan instruction rate test for FP16, FP32 and FP64 inverse square root (`vk_rate_fp16_isqrt`, `vk_rate_fp32_isqrt`, `vk_rate_fp64_isqrt`)
* GUI

# Prerelease 0.5.0

**New Features:**
* Vulkan instruction rate test for FP16 add, subtract, multiply, multiply-add, divide, remainder (`vk_rate_fp16_add`, `vk_rate_fp16_sub`, `vk_rate_fp16_mul`, `vk_rate_fp16_mac`, `vk_rate_fp16_div`, `vk_rate_fp16_rem`)
* Vulkan instruction rate test for FP32 add, subtract, multiply, multiply-add, divide, remainder (`vk_rate_fp32_add`, `vk_rate_fp32_sub`, `vk_rate_fp32_mul`, `vk_rate_fp32_mac`, `vk_rate_fp32_div`, `vk_rate_fp32_rem`)
* Vulkan instruction rate test for FP64 add, subtract, multiply, multiply-add, divide, remainder (`vk_rate_fp64_add`, `vk_rate_fp64_sub`, `vk_rate_fp64_mul`, `vk_rate_fp64_mac`, `vk_rate_fp64_div`, `vk_rate_fp64_rem`)
* Vulkan instruction rate test for INT8 add, subtract, multiply, multiply-add, divide, remainder (`vk_rate_int8_add`, `vk_rate_int8_sub`, `vk_rate_int8_mul`, `vk_rate_int8_mac`, `vk_rate_int8_div`, `vk_rate_int8_rem`)
* Vulkan instruction rate test for INT16 add, subtract, multiply, multiply-add, divide, remainder (`vk_rate_int16_add`, `vk_rate_int16_sub`, `vk_rate_int16_mul`, `vk_rate_int16_mac`, `vk_rate_int16_div`, `vk_rate_int16_rem`)
* Vulkan instruction rate test for INT32 add, subtract, multiply, multiply-add, divide, remainder (`vk_rate_int32_add`, `vk_rate_int32_sub`, `vk_rate_int32_mul`, `vk_rate_int32_mac`, `vk_rate_int32_div`, `vk_rate_int32_rem`)
* Vulkan instruction rate test for INT64 add, subtract, multiply, multiply-add, divide, remainder (`vk_rate_int64_add`, `vk_rate_int64_sub`, `vk_rate_int64_mul`, `vk_rate_int64_mac`, `vk_rate_int64_div`, `vk_rate_int64_rem`)

**Improvements:**
* Added texture bandwidth mode to vk_bandwidth, not actually usable for anything of value
* Finished `vk_latency`

**Removed:**
* OpenCL infrastructure
* OpenCL information listing test (`cl_info`)
* OpenCL memory bandwidth test (`cl_bandwidth`)
* OpenCL FP16 FADD, FSUB, FMUL, FDIV and FMAC rate tests (`cl_fp16_fadd`, `cl_fp16_fsub`, `cl_fp16_fmul`, `cl_fp16_fdiv`, `cl_fp16_fmac`)
* OpenCL FP32 FADD, FSUB, FMUL, FDIV and FMAC rate tests (`cl_fp32_fadd`, `cl_fp32_fsub`, `cl_fp32_fmul`, `cl_fp32_fdiv`, `cl_fp32_fmac`)
* OpenCL FP64 FADD, FSUB, FMUL, FDIV and FMAC rate tests (`cl_fp64_fadd`, `cl_fp64_fsub`, `cl_fp64_fmul`, `cl_fp64_fdiv`, `cl_fp64_fmac`)
* OpenCL INT8 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int8_add`, `cl_int8_sub`, `cl_int8_mul`, `cl_int8_div`, `cl_int8_mac`)
* OpenCL INT16 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int16_add`, `cl_int16_sub`, `cl_int16_mul`, `cl_int16_div`, `cl_int16_mac`)
* OpenCL INT32 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int32_add`, `cl_int32_sub`, `cl_int32_mul`, `cl_int32_div`, `cl_int32_mac`)
* OpenCL INT64 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int64_add`, `cl_int64_sub`, `cl_int64_mul`, `cl_int64_div`, `cl_int64_mac`)
* OpenCL UINT8 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint8_add`, `cl_uint8_sub`, `cl_uint8_mul`, `cl_uint8_div`, `cl_uint8_mac`)
* OpenCL UINT16 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint16_add`, `cl_uint16_sub`, `cl_uint16_mul`, `cl_uint16_div`, `cl_uint16_mac`)
* OpenCL UINT32 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint32_add`, `cl_uint32_sub`, `cl_uint32_mul`, `cl_uint32_div`, `cl_uint32_mac`)
* OpenCL UINT64 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint64_add`, `cl_uint64_sub`, `cl_uint64_mul`, `cl_uint64_div`, `cl_uint64_mac`)

# Prerelease 0.4.1

**Improvements:**
* Improved `vk_latency` test accuracy (Still not final!)
* Improved `vk_latency` compatibility with Intel Iris Xe Graphics
* Improved `vk_bandwidth` compatibility with Intel Iris Xe Graphics

# Prerelease 0.4.0

**New Features:**
* Vulkan memory latency test (`vk_latency`) (Not final!)

**Improvements:**
* Improved `vk_bandwidth` test accuracy

# Prerelease 0.3.5

**Improvements:**
* Improved `vk_bandwidth` test accuracy

# Prerelease 0.3.1

**Improvements:**
* Improved `vk_bandwidth` test accuracy
* Fixed some crash issues

# Prerelease 0.3.0

**New Features:**
* Vulkan infrastructure
* Vulkan information listing test (`vk_info`)
* Vulkan memory bandwidth test (`vk_bandwidth`) (Not final!)

**Deprecated:**
* OpenCL infrastructure
* OpenCL information listing test (`cl_info`)
* OpenCL cache and memory bandwidth test (`cl_bandwidth`)
* FP16 FADD, FSUB, FMUL, FDIV and FMAC rate tests (`cl_fp16_fadd`, `cl_fp16_fsub`, `cl_fp16_fmul`, `cl_fp16_fdiv`, `cl_fp16_fmac`)
* FP32 FADD, FSUB, FMUL, FDIV and FMAC rate tests (`cl_fp32_fadd`, `cl_fp32_fsub`, `cl_fp32_fmul`, `cl_fp32_fdiv`, `cl_fp32_fmac`)
* FP64 FADD, FSUB, FMUL, FDIV and FMAC rate tests (`cl_fp64_fadd`, `cl_fp64_fsub`, `cl_fp64_fmul`, `cl_fp64_fdiv`, `cl_fp64_fmac`)
* INT8 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int8_add`, `cl_int8_sub`, `cl_int8_mul`, `cl_int8_div`, `cl_int8_mac`)
* INT16 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int16_add`, `cl_int16_sub`, `cl_int16_mul`, `cl_int16_div`, `cl_int16_mac`)
* INT32 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int32_add`, `cl_int32_sub`, `cl_int32_mul`, `cl_int32_div`, `cl_int32_mac`)
* INT64 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int64_add`, `cl_int64_sub`, `cl_int64_mul`, `cl_int64_div`, `cl_int64_mac`)
* UINT8 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint8_add`, `cl_uint8_sub`, `cl_uint8_mul`, `cl_uint8_div`, `cl_uint8_mac`)
* UINT16 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint16_add`, `cl_uint16_sub`, `cl_uint16_mul`, `cl_uint16_div`, `cl_uint16_mac`)
* UINT32 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint32_add`, `cl_uint32_sub`, `cl_uint32_mul`, `cl_uint32_div`, `cl_uint32_mac`)
* UINT64 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint64_add`, `cl_uint64_sub`, `cl_uint64_mul`, `cl_uint64_div`, `cl_uint64_mac`)

# Prerelease 0.2.0

**New Features:**
* OpenCL cache and memory bandwidth test (`cl_bandwidth`)

**Improvements:**
* Improved device detection (only GPUs are now detected)

# Prerelease 0.1.0

Initial release.

**New Features:**
* OpenCL information listing test (`cl_info`)
* FP16 FADD, FSUB, FMUL, FDIV and FMAC rate tests (`cl_fp16_fadd`, `cl_fp16_fsub`, `cl_fp16_fmul`, `cl_fp16_fdiv`, `cl_fp16_fmac`)
* FP32 FADD, FSUB, FMUL, FDIV and FMAC rate tests (`cl_fp32_fadd`, `cl_fp32_fsub`, `cl_fp32_fmul`, `cl_fp32_fdiv`, `cl_fp32_fmac`)
* FP64 FADD, FSUB, FMUL, FDIV and FMAC rate tests (`cl_fp64_fadd`, `cl_fp64_fsub`, `cl_fp64_fmul`, `cl_fp64_fdiv`, `cl_fp64_fmac`)
* INT8 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int8_add`, `cl_int8_sub`, `cl_int8_mul`, `cl_int8_div`, `cl_int8_mac`)
* INT16 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int16_add`, `cl_int16_sub`, `cl_int16_mul`, `cl_int16_div`, `cl_int16_mac`)
* INT32 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int32_add`, `cl_int32_sub`, `cl_int32_mul`, `cl_int32_div`, `cl_int32_mac`)
* INT64 ADD, SUB, MUL, DIV and MAC rate tests (`cl_int64_add`, `cl_int64_sub`, `cl_int64_mul`, `cl_int64_div`, `cl_int64_mac`)
* UINT8 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint8_add`, `cl_uint8_sub`, `cl_uint8_mul`, `cl_uint8_div`, `cl_uint8_mac`)
* UINT16 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint16_add`, `cl_uint16_sub`, `cl_uint16_mul`, `cl_uint16_div`, `cl_uint16_mac`)
* UINT32 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint32_add`, `cl_uint32_sub`, `cl_uint32_mul`, `cl_uint32_div`, `cl_uint32_mac`)
* UINT64 ADD, SUB, MUL, DIV and MAC rate tests (`cl_uint64_add`, `cl_uint64_sub`, `cl_uint64_mul`, `cl_uint64_div`, `cl_uint64_mac`)
