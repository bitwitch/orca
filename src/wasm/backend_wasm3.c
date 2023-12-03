/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include "wasm.h"

// #include "m3_compile.h"
// #include "m3_env.h"
// #include "wasm3.h"

u64 oc_wasm_mem_size_wasm3(oc_wasm* wasm)
{
    oc_wasm_backend_wasm3* backend = wasm->backend;
    return m3_GetMemorySize(backend->m3Runtime);
}

oc_wasm* oc_wasm_backend_wasm3_create()
{
    oc_wasm_backend_wasm3* backend = (oc_wasm_backend_wasm3*)malloc(sizeof(oc_wasm_backend_wasm3));
    {
        u32 stackSize = 65536;
        backend->m3Env = m3_NewEnvironment();
        backend->m3Runtime = m3_NewRuntime(backend->m3Env, stackSize, NULL);
    }

    oc_wasm* wasm = (oc_wasm*)malloc(sizeof(oc_wasm));
    wasm->backend = backend;
    wasm->memSize = oc_wasm_mem_size_wasm3;

    return wasm;
}
