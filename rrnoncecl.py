import ctypes
import os
import time
import hashlib
import random
# --- Konfigurasi Library ---
lib_name = "./rr_opencl.so" if os.name == 'posix' else "./rr_opencl.dll"
cuda_miner = ctypes.CDLL(os.path.abspath(lib_name))

# Konstanta
MAX_RESULTS = 10

cuda_miner.run_gpu_miner.argtypes = [
    ctypes.POINTER(ctypes.c_uint32), 
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_uint32),
    ctypes.c_int
]
cuda_miner.run_gpu_miner.restype = ctypes.c_int

def double_sha256_hex(hexdata):
    b = bytes.fromhex(hexdata)
    return hashlib.sha256(hashlib.sha256(b).digest()).hexdigest()

def rev_hex(hexstr):
    return "".join([hexstr[i:i+2] for i in range(0, len(hexstr), 2)][::-1])

def lev(nonce):
    byte0 = (nonce & 0xFF000000) >> 24
    byte1 = (nonce & 0x00FF0000) >> 16
    byte2 = (nonce & 0x0000FF00) >> 8
    byte3 = (nonce & 0x000000FF)
    little_endian_value = (byte3 << 24) | (byte2 << 16) | (byte1 << 8) | byte0
    return little_endian_value

def get_target_params(target_miner):

    if target_miner < 1:
        diff_target1 = 0xffff0000
        diff_target2 = 0xffffffff
    else:
        if target_miner < 4294967296:
            diff_target1 = int(0xffff0000 / target_miner)
            diff_target2 = 0xffffffff
        else:
            if target_miner < 18446744073709551616:
                diff_target1 = 0
                diff_target2 = int(0xffff000000000000 / target_miner)
            else:
                diff_target1 = 0
                diff_target2 = 0
    return diff_target1, diff_target2

def mining_nonce(header_hex, target_miner,batch_number):
    START_NONCES = []
    random_space = 0xffffffff // batch_number
    for i in range(batch_number):
        xx = i * random_space
        yy = xx + random.randint(1,random_space - (256*5120))
        START_NONCES.append(yy)

    pw = []
    for i in range(0, len(header_hex), 8):
        chunk = header_hex[i:i+8]
        value = int(chunk, 16)
        pw.append(value)

    from midstate import fchunk
    h10, h11, h12, h13, h14, h15, h16, h17 = fchunk(pw[0], pw[1], pw[2], pw[3], pw[4], pw[5], pw[6], pw[7], pw[8], pw[9], pw[10], pw[11], pw[12], pw[13], pw[14], pw[15])
    
    dt1, dt2 = get_target_params(target_miner)
    
    final_results = []

    for s_nonce in START_NONCES:
        input_data = (ctypes.c_uint32 * 13)(
            h10, h11, h12, h13, h14, h15, h16, h17,
            pw[16], pw[17], pw[18], dt1, dt2
        )
        output_buffer = (ctypes.c_uint32 * MAX_RESULTS)()

        count = cuda_miner.run_gpu_miner(
            input_data, 
            ctypes.c_uint32(s_nonce), 
            output_buffer, 
            MAX_RESULTS
        )

        if count > 0:
            for i in range(count):
                found_nonce = output_buffer[i]
                full_header = header_hex + f"{found_nonce:08x}"
                final_hash = double_sha256_hex(full_header)
                hash_result = rev_hex(final_hash)
                
                final_results.append((lev(found_nonce), hash_result))
    
    return final_results
