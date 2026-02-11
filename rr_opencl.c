#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

const char* kernel_source = 
"#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))\n"
"\n"
"__constant uint K[64] = {\n"
"    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,\n"
"    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,\n"
"    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,\n"
"    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,\n"
"    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,\n"
"    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,\n"
"    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,\n"
"    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2\n"
"};\n"
"\n"

"uint swap32(uint x) {\n"
"    return as_uint(as_uchar4(x).s3210);\n"
"}\n"
"\n"
"__kernel void find_nonce_kernel(\n"
"    __global const uint* input_state, \n"
"    __global uint* result_buffer, \n"
"    __global uint* found_count,\n"
"    uint start_nonce,\n"
"    uint max_results\n"
") {\n"
"    uint gid = get_global_id(0);\n"
"    uint nonce = start_nonce + gid;\n"
"    \n"
"    uint diff1 = input_state[11];\n"
"    uint diff2 = input_state[12];\n"
"\n"
"    uint w[64];\n"
"    w[0] = input_state[8];\n"
"    w[1] = input_state[9];\n"
"    w[2] = input_state[10];\n"
"    w[3] = nonce;\n"
"    w[4] = 0x80000000;\n"
"    \n"
"    #pragma unroll\n"
"    for(int i=5; i<15; i++) w[i] = 0;\n"
"    w[15] = 0x280; // Length of header in bits (640)\n"
"\n"
"    #pragma unroll\n"
"    for (int i = 16; i < 64; i++) {\n"
"        uint s0 = ROTR(w[i-15], 7) ^ ROTR(w[i-15], 18) ^ (w[i-15] >> 3);\n"
"        uint s1 = ROTR(w[i-2], 17) ^ ROTR(w[i-2], 19) ^ (w[i-2] >> 10);\n"
"        w[i] = w[i-16] + s0 + w[i-7] + s1;\n"
"    }\n"
"\n"
"    uint a = input_state[0]; \n"
"    uint b = input_state[1]; \n"
"    uint c = input_state[2]; \n"
"    uint d = input_state[3]; \n"
"    uint e = input_state[4]; \n"
"    uint f = input_state[5]; \n"
"    uint g = input_state[6]; \n"
"    uint h = input_state[7];\n"
"\n"
"    #pragma unroll\n"
"    for (int i = 0; i < 64; i++) {\n"
"        uint S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);\n"
"        uint ch = (e & f) ^ ((~e) & g);\n"
"        uint temp1 = h + S1 + ch + K[i] + w[i];\n"
"        uint S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);\n"
"        uint maj = (a & b) ^ (a & c) ^ (b & c);\n"
"        uint temp2 = S0 + maj;\n"
"\n"
"        h = g; g = f; f = e; e = d + temp1;\n"
"        d = c; c = b; b = a; a = temp1 + temp2;\n"
"    }\n"
"\n"
"    // Prepare second round (Double SHA)\n"
"    w[0] = input_state[0] + a;\n"
"    w[1] = input_state[1] + b;\n"
"    w[2] = input_state[2] + c;\n"
"    w[3] = input_state[3] + d;\n"
"    w[4] = input_state[4] + e;\n"
"    w[5] = input_state[5] + f;\n"
"    w[6] = input_state[6] + g;\n"
"    w[7] = input_state[7] + h;\n"
"\n"
"    w[8] = 0x80000000;\n"
"    #pragma unroll\n"
"    for(int i=9; i<15; i++) w[i] = 0;\n"
"    w[15] = 0x100; // Length of hash in bits (256)\n"
"\n"
"    #pragma unroll\n"
"    for (int i = 16; i < 64; i++) {\n"
"        uint s0 = ROTR(w[i-15], 7) ^ ROTR(w[i-15], 18) ^ (w[i-15] >> 3);\n"
"        uint s1 = ROTR(w[i-2], 17) ^ ROTR(w[i-2], 19) ^ (w[i-2] >> 10);\n"
"        w[i] = w[i-16] + s0 + w[i-7] + s1;\n"
"    }\n"
"\n"
"    a = 0x6a09e667; b = 0xbb67ae85; c = 0x3c6ef372; d = 0xa54ff53a;\n"
"    e = 0x510e527f; f = 0x9b05688c; g = 0x1f83d9ab; h = 0x5be0cd19;\n"
"\n"
"    #pragma unroll\n"
"    for (int i = 0; i < 64; i++) {\n"
"        uint S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);\n"
"        uint ch = (e & f) ^ ((~e) & g);\n"
"        uint temp1 = h + S1 + ch + K[i] + w[i];\n"
"        uint S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);\n"
"        uint maj = (a & b) ^ (a & c) ^ (b & c);\n"
"        uint temp2 = S0 + maj;\n"
"\n"
"        h = g; g = f; f = e; e = d + temp1;\n"
"        d = c; c = b; b = a; a = temp1 + temp2;\n"
"    }\n"
"    uint res_h37 = h + 0x5be0cd19; \n"
"\n"
"    // Check difficulty\n"
"    if (res_h37 == 0) {\n"
"        uint res_h36 = g + 0x1f83d9ab;\n"
"        uint res_h36_le = swap32(res_h36);\n"
"        if (res_h36_le <= diff1) {\n"
"            uint res_h35 = f + 0x9b05688c;\n"
"            uint res_h35_le = swap32(res_h35);\n"
"            if (res_h35_le <= diff2) {\n"
"                uint old = atomic_inc(found_count);\n"
"                if (old < max_results) {\n"
"                    result_buffer[old] = nonce;\n"
"                }\n"
"            }\n"
"        }\n"
"    }\n"
"}\n";


static cl_context context = NULL;
static cl_command_queue queue = NULL;
static cl_program program = NULL;
static cl_kernel kernel = NULL;
static cl_device_id device = NULL;
static int cl_initialized = 0;

int init_opencl() {
    if (cl_initialized) return 0;

    cl_platform_id platform;
    cl_int err;

    err = clGetPlatformIDs(1, &platform, NULL);
    if (err != CL_SUCCESS) {
        printf("OpenCL Error: Failed to get platform.\n");
        return -1;
    }

    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err != CL_SUCCESS) {
        printf("OpenCL Warning: No GPU found. Trying CPU...\n");
        err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &device, NULL);
        if (err != CL_SUCCESS) {
            printf("OpenCL Error: No suitable device found.\n");
            return -2;
        }
    }

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        printf("OpenCL Error: Failed to create context.\n");
        return -3;
    }

    queue = clCreateCommandQueue(context, device, 0, &err);
    if (err != CL_SUCCESS) {
        printf("OpenCL Error: Failed to create command queue.\n");
        return -4;
    }

    program = clCreateProgramWithSource(context, 1, &kernel_source, NULL, &err);
    if (err != CL_SUCCESS) {
        printf("OpenCL Error: Failed to create program.\n");
        return -5;
    }

    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        char build_log[4096];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(build_log), build_log, NULL);
        printf("OpenCL Build Error:\n%s\n", build_log);
        return -6;
    }

    kernel = clCreateKernel(program, "find_nonce_kernel", &err);
    if (err != CL_SUCCESS) {
        printf("OpenCL Error: Failed to create kernel.\n");
        return -7;
    }

    cl_initialized = 1;
    return 0;
}

int run_gpu_miner(uint32_t* input_data, uint32_t start_nonce, uint32_t* output_buffer, int max_results) {
    if (!cl_initialized) {
        if (init_opencl() != 0) {
            return -1;
        }
    }

    cl_int err;
    size_t global_work_size = 256 * 5120; 
    uint32_t h_found_count = 0;

    cl_mem d_input = clCreateBuffer(context, CL_MEM_READ_ONLY, 13 * sizeof(uint32_t), NULL, &err);
    cl_mem d_result = clCreateBuffer(context, CL_MEM_WRITE_ONLY, max_results * sizeof(uint32_t), NULL, &err);
    cl_mem d_count = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(uint32_t), NULL, &err);

    if (!d_input || !d_result || !d_count) {
        if(d_input) clReleaseMemObject(d_input);
        if(d_result) clReleaseMemObject(d_result);
        if(d_count) clReleaseMemObject(d_count);
        return -1;
    }

    clEnqueueWriteBuffer(queue, d_input, CL_TRUE, 0, 13 * sizeof(uint32_t), input_data, 0, NULL, NULL);
    
    clEnqueueWriteBuffer(queue, d_count, CL_TRUE, 0, sizeof(uint32_t), &h_found_count, 0, NULL, NULL);

    clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_input);
    clSetKernelArg(kernel, 1, sizeof(cl_mem), &d_result);
    clSetKernelArg(kernel, 2, sizeof(cl_mem), &d_count);
    clSetKernelArg(kernel, 3, sizeof(uint32_t), &start_nonce);
    clSetKernelArg(kernel, 4, sizeof(uint32_t), &max_results);

    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        // printf("OpenCL Kernel Launch Error: %d\n", err);
        clReleaseMemObject(d_input);
        clReleaseMemObject(d_result);
        clReleaseMemObject(d_count);
        return 0;
    }

    clEnqueueReadBuffer(queue, d_count, CL_TRUE, 0, sizeof(uint32_t), &h_found_count, 0, NULL, NULL);

    if (h_found_count > 0) {
        uint32_t count_to_copy = (h_found_count > (uint32_t)max_results) ? (uint32_t)max_results : h_found_count;
        clEnqueueReadBuffer(queue, d_result, CL_TRUE, 0, count_to_copy * sizeof(uint32_t), output_buffer, 0, NULL, NULL);
    }

    clReleaseMemObject(d_input);
    clReleaseMemObject(d_result);
    clReleaseMemObject(d_count);

    return (int)h_found_count;
}
