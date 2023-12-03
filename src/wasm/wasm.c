/*************************************************************************
*
*  Orca
*  Copyright 2023 Martin Fouilleul and the Orca project contributors
*  See LICENSE.txt for licensing information
*
**************************************************************************/

#include "wasm.h"

oc_wasm* oc_wasm_backend_wasm3_create();

oc_wasm* oc_wasm_create(oc_wasm_backend_type type)
{
    return oc_wasm_backend_wasm3_create();
}
