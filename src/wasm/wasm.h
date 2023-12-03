/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#ifndef __WASM_H_
#define __WASM_H_

// struct oc_wasm_error
// {
//     const char* msg;
// };

// enum oc_wasm_value_type
// {
//     OC_WASM_VALUE_TYPE_VOID,
//     OC_WASM_VALUE_TYPE_I32,
//     OC_WASM_VALUE_TYPE_I64,
//     OC_WASM_VALUE_TYPE_F32,
//     OC_WASM_VALUE_TYPE_F64,
// };
// typedef enum oc_wasm_value_type oc_wasm_value_type;

// struct oc_wasm_value
// {
//     oc_wasm_value_type type;

//     union
//     {
//         i32 vi32;
//         i64 vi64;
//         f32 vf32;
//         f64 vf64;
//     };
// };

typedef u64 oc_wasm_backend_mem_size(void* backend);

// typedef oc_wasm_error oc_wasm_backend_mem_resize(void* backend, u64 total_size);

enum oc_wasm_backend_type
{
    OC_WASM_BACKEND_TYPE_WASM3,
};
typedef enum oc_wasm_backend_type oc_wasm_backend_type;

struct oc_wasm
{
    void* backend;
    oc_wasm_backend_mem_size* memSize;
    // oc_wasm_backend_mem_resize* memResize;
};
typedef struct oc_wasm oc_wasm;

ORCA_API oc_wasm* oc_wasm_create(oc_wasm_backend_type type);

ORCA_API u64 oc_wasm_mem_size(oc_wasm* wasm);

// oc_wasm_error oc_wasm_mem_resize(oc_wasm* wasm);

//////////////////////////////////////////////////////////////////
// Inline implementation

inline u64 oc_wasm_mem_size(oc_wasm* wasm)
{
    return wasm->memSize(wasm->backend);
}

// inline oc_wasm_error oc_wasm_mem_resize(oc_wasm* wasm)
// {

// }

//////////////////////////////////////////////////////////
// TEMPORARILY LOCATED HERE, SHOULD MOVE TO wasm.c
#include "m3_compile.h"
#include "m3_env.h"
#include "wasm3.h"

struct oc_wasm_backend_wasm3
{
    IM3Environment m3Env;
    IM3Runtime m3Runtime;
    IM3Module m3Module;
};
typedef struct oc_wasm_backend_wasm3 oc_wasm_backend_wasm3;

inline IM3Environment oc_wasm_m3env_temp(oc_wasm* wasm)
{
    oc_wasm_backend_wasm3* backend = wasm->backend;
    return backend->m3Env;
}

inline IM3Runtime oc_wasm_m3runtime_temp(oc_wasm* wasm)
{
    oc_wasm_backend_wasm3* backend = wasm->backend;
    return backend->m3Runtime;
}

inline IM3Module oc_wasm_m3module_temp(oc_wasm* wasm)
{
    oc_wasm_backend_wasm3* backend = wasm->backend;
    return backend->m3Module;
}

//
//////////////////////////////////////////////////////////

#endif // __WASM_H_