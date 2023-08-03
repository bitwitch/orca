/************************************************************//**
*
*	@file: main.c
*	@author: Martin Fouilleul
*	@date: 11/04/2023
*
*****************************************************************/
#include<stdio.h>
#include<errno.h>
#include<math.h>

#define MG_INCLUDE_GL_API
#include"milepost.h"
#include"graphics_common.h"

#include"orca_app.h"

#include"memory_impl.c"
#include"io_impl.c"


#define LOG_SUBSYSTEM "Orca"

int orca_assert(const char* file, const char* function, int line, const char* src, const char* note)
{
	mem_arena* scratch = mem_scratch();
	str8 msg = str8_pushf(scratch,
	                      "Assertion failed in function %s() in file \"%s\", line %i:\n%s\nNote: %s\n",
	                      function,
	                      file,
	                      line,
	                      src,
	                      note);

	const char* msgCStr = str8_to_cstring(scratch, msg);
	log_error(msgCStr);

	const char* options[] = {"OK"};
	mp_alert_popup("Assertion Failed", msgCStr, 1, options);

	//TODO: should terminate more gracefully...
	exit(-1);
}

mg_font orca_font_create(const char* resourcePath)
{
	//NOTE(martin): create default font
	str8 fontPath = path_executable_relative(mem_scratch(), STR8(resourcePath));

	FILE* fontFile = fopen(fontPath.ptr, "r");
	if(!fontFile)
	{
		log_error("Could not load font file '%s': %s\n", fontPath.ptr, strerror(errno));
		return(mg_font_nil());
	}
	unsigned char* fontData = 0;
	fseek(fontFile, 0, SEEK_END);
	u32 fontDataSize = ftell(fontFile);
	rewind(fontFile);
	fontData = (unsigned char*)malloc(fontDataSize);
	fread(fontData, 1, fontDataSize, fontFile);
	fclose(fontFile);

	unicode_range ranges[5] = {UNICODE_RANGE_BASIC_LATIN,
	                           UNICODE_RANGE_C1_CONTROLS_AND_LATIN_1_SUPPLEMENT,
	                           UNICODE_RANGE_LATIN_EXTENDED_A,
	                           UNICODE_RANGE_LATIN_EXTENDED_B,
	                           UNICODE_RANGE_SPECIALS};

	mg_font font = mg_font_create_from_memory(fontDataSize, fontData, 5, ranges);

	free(fontData);

	return(font);
}

mg_font mg_font_create_default()
{
	return(orca_font_create("../resources/OpenSansLatinSubset.ttf"));
}


orca_app __orcaApp = {0};

orca_app* orca_app_get()
{
	return(&__orcaApp);
}

orca_runtime* orca_runtime_get()
{
	return(&__orcaApp.runtime);
}

void orca_log(log_level level,
              int fileLen,
              char* file,
              int functionLen,
              char* function,
              int line,
              int msgLen,
              char* msg)
{
	orca_debug_overlay* debug = &__orcaApp.debugOverlay;

	//NOTE: recycle first entry if we exceeded the max entry count
	debug->entryCount++;
	if(debug->entryCount > debug->maxEntries)
	{
		log_entry* e = list_pop_entry(&debug->logEntries, log_entry, listElt);
		if(e)
		{
			list_push(&debug->logFreeList, &e->listElt);
			debug->entryCount--;
		}
	}

	u64 cap = sizeof(log_entry)+fileLen+functionLen+msgLen;

	//NOTE: allocate a new entry
	//TODO: should probably use a buddy allocator over the arena or something
	log_entry* entry = 0;
	for_list(&debug->logFreeList, elt, log_entry, listElt)
	{
		if(elt->cap >= cap)
		{
			list_remove(&debug->logFreeList, &elt->listElt);
			entry = elt;
			break;
		}
	}

	if(!entry)
	{
		char* mem = mem_arena_alloc(&debug->logArena, cap);
		entry = (log_entry*)mem;
		entry->cap = cap;
	}
	char* payload = (char*)entry + sizeof(log_entry);

	entry->file.len = fileLen;
	entry->file.ptr = payload;
	payload += entry->file.len;

	entry->function.len = functionLen;
	entry->function.ptr = payload;
	payload += entry->function.len;

	entry->msg.len = msgLen;
	entry->msg.ptr = payload;
	payload += entry->msg.len;

	memcpy(entry->file.ptr, file, fileLen);
	memcpy(entry->function.ptr, function, functionLen);
	memcpy(entry->msg.ptr, msg, msgLen);

	entry->level = level;
	entry->line = line;
	entry->recordIndex = debug->logEntryTotalCount;
	debug->logEntryTotalCount++;

	list_push_back(&debug->logEntries, &entry->listElt);

	log_push(level,
	         str8_from_buffer(fileLen, file),
	         str8_from_buffer(functionLen, function),
	         line,
	         "%.*s\n",
	         msgLen,
	         msg);
}

mg_surface orca_surface_main(void)
{
	return(__orcaApp.surface);
}

void orca_surface_render_commands(mg_surface surface,
                                  mg_color clearColor,
                                  u32 primitiveCount,
                                  mg_primitive* primitives,
                                  u32 eltCount,
                                  mg_path_elt* elements)
{
	orca_app* app = &__orcaApp;

	char* memBase = app->runtime.wasmMemory.ptr;
	u32 memSize = app->runtime.wasmMemory.committed;
	if( ((char*)primitives > memBase)
	  &&((char*)primitives + primitiveCount*sizeof(mg_primitive) - memBase <= memSize)
	  &&((char*)elements > memBase)
	  &&((char*)elements + eltCount*sizeof(mg_path_elt) - memBase <= memSize))
	{
		mg_surface_render_commands(surface,
		                           clearColor,
		                           primitiveCount,
		                           primitives,
		                           eltCount,
		                           elements);
	}
}

void debug_overlay_toggle(orca_debug_overlay* overlay)
{
	overlay->show = !overlay->show;
	mg_surface_set_hidden(overlay->surface, !overlay->show);

	if(overlay->show)
	{
		overlay->logScrollToLast = true;
	}
}


void log_entry_ui(orca_debug_overlay* overlay, log_entry* entry)
{
	static const char* levelNames[] = {"Error: ", "Warning: ", "Info: "};
	static const mg_color levelColors[] = {{0.8, 0, 0, 1},
	                                       {1, 0.5, 0, 1},
	                                       {0, 0.8, 0, 1}};

	static const mg_color bgColors[3][2] = {//errors
	                                        {{0.6, 0, 0, 0.5}, {0.8, 0, 0, 0.5}},
	                                        //warning
	                                        {{0.4, 0.4, 0.4, 0.5}, {0.5, 0.5, 0.5, 0.5}},
	                                        //info
	                                        {{0.4, 0.4, 0.4, 0.5}, {0.5, 0.5, 0.5, 0.5}}};

	ui_style_next(&(ui_style){.size.width = {UI_SIZE_PARENT, 1},
	                          .size.height = {UI_SIZE_CHILDREN},
	                          .layout.axis = UI_AXIS_Y,
	                          .layout.margin.x = 10,
	                          .layout.margin.y = 5,
	                          .bgColor = bgColors[entry->level][entry->recordIndex & 1]},
		             UI_STYLE_SIZE
		             |UI_STYLE_LAYOUT_AXIS
		             |UI_STYLE_LAYOUT_MARGINS
		             |UI_STYLE_BG_COLOR);

	str8 key = str8_pushf(mem_scratch(), "%ull", entry->recordIndex);

	ui_container_str8(key, UI_FLAG_DRAW_BACKGROUND)
	{
		ui_style_next(&(ui_style){.size.width = {UI_SIZE_PARENT, 1},
		                          .size.height = {UI_SIZE_CHILDREN},
		                          .layout.axis = UI_AXIS_X},
		              UI_STYLE_SIZE
		              |UI_STYLE_LAYOUT_AXIS);

		ui_container("header", 0)
		{
			ui_style_next(&(ui_style){.color = levelColors[entry->level],
			                          .font = overlay->fontBold},
			              UI_STYLE_COLOR
			              |UI_STYLE_FONT);
			ui_label(levelNames[entry->level]);

			str8 loc = str8_pushf(mem_scratch(),
			                      "%.*s() in %.*s:%i:",
			                      str8_ip(entry->file),
			                      str8_ip(entry->function),
			                      entry->line);
			ui_label_str8(loc);
		}
		ui_label_str8(entry->msg);
	}
}


// char m3_type_to_tag(M3ValueType type)
// {
// 	switch(type)
// 	{
// 		case c_m3Type_none:
// 			return('v');
// 		case c_m3Type_i32:
// 			return('i');
// 		case c_m3Type_i64:
// 			return('I');
// 		case c_m3Type_f32:
// 			return('f');
// 		case c_m3Type_f64:
// 			return('d');

// 		case c_m3Type_unknown:
// 		default:
// 			return('!');
// 	}
// }

bool tag_to_valtype(char tag, bb_valtype* out)
{
	switch (tag)
	{
	case 'i': *out = BB_VALTYPE_I32; return true;
	case 'I': *out = BB_VALTYPE_I64; return true;
	case 'f': *out = BB_VALTYPE_F32; return true;
	case 'F': *out = BB_VALTYPE_F64; return true;
	case 'd': *out = BB_VALTYPE_F64; return true;
	}

	return false;
}

void orca_runtime_init(orca_runtime* runtime)
{
	memset(runtime, 0, sizeof(orca_runtime));
	mem_base_allocator* allocator = mem_base_allocator_default();
	runtime->wasmMemory.committed = 0;
	runtime->wasmMemory.reserved = 4ULL<<30;
	runtime->wasmMemory.ptr = mem_base_reserve(allocator, runtime->wasmMemory.reserved);
}

#include"core_api_bind_gen.c"
#include"canvas_api_bind.c"
#include"clock_api_bind_gen.c"
#include"io_api_bind_gen.c"

#include"gles_api_bind_gen.c"
#include"manual_gles_api.c"

i32 quit_on_wasm_init_failure(const char* stage, bb_error err)
{
	const char* errStr = bb_error_str(err);
	log_error("wasm error at init stage '%s': %s\n", stage, errStr);

	str8 msg = str8_pushf(mem_scratch(), "The application couldn't load: encountered fatal error at stage '%s': %s", stage, errStr);

	const char* options[] = {"OK"};
	mp_alert_popup("Error",
	               msg.ptr,
	               1,
	               options);

	mp_request_quit();
	return(-1);
}

i32 orca_runloop(void* user)
{
	orca_app* app = &__orcaApp;

	orca_runtime_init(&app->runtime);

	//NOTE: loads wasm module
	const char* bundleNameCString = "module";
	str8 modulePath = path_executable_relative(mem_scratch(), STR8("../app/wasm/module.wasm"));

	FILE* file = fopen(modulePath.ptr, "rb");
	if(!file)
	{
		log_error("Couldn't load wasm module at %s\n", modulePath.ptr);

		const char* options[] = {"OK"};
		mp_alert_popup("Error",
		               "The application couldn't load: web assembly module not found",
		               1,
		               options);

		mp_request_quit();
		return(-1);
	}

	fseek(file, 0, SEEK_END);
	u64 wasmSize = ftell(file);
	rewind(file);

	app->runtime.wasmBytecode.len = wasmSize;
	app->runtime.wasmBytecode.ptr = malloc_array(char, wasmSize);
	fread(app->runtime.wasmBytecode.ptr, 1, app->runtime.wasmBytecode.len, file);
	fclose(file);

	bb_error wasm_init_err;
	bb_module_definition_init_opts module_def_init_opts = { .debug_name = bundleNameCString };
	app->runtime.bbModuleDef = bb_module_definition_init(module_def_init_opts);
	wasm_init_err = bb_module_definition_decode(app->runtime.bbModuleDef, app->runtime.wasmBytecode.ptr, app->runtime.wasmBytecode.len);
	if (wasm_init_err != BB_ERROR_OK)
	{
		return quit_on_wasm_init_failure("wasm decode", wasm_init_err);
	}

	// TODO [ReubenD] - allow specifying stack size
	// TODO [ReubenD] - allow overriding wasm memory alloc/resize/free

	// u32 stackSize = 65536;
	// app->runtime.m3Env = m3_NewEnvironment();

	// app->runtime.m3Runtime = m3_NewRuntime(app->runtime.m3Env, stackSize, NULL);
	// m3_RuntimeSetMemoryCallbacks(app->runtime.m3Runtime, wasm_memory_resize_callback, wasm_memory_free_callback, &app->runtime.wasmMemory);
	//NOTE: host memory will be freed when runtime is freed.

	//TODO check errors
	// m3_ParseModule(app->runtime.m3Env, &app->runtime.m3Module, (u8*)app->runtime.wasmBytecode.ptr, app->runtime.wasmBytecode.len);
	// m3_LoadModule(app->runtime.m3Runtime, app->runtime.m3Module);
	// m3_SetModuleName(app->runtime.m3Module, bundleNameCString);

	mem_arena_clear(mem_scratch());

	//NOTE: bind orca APIs
	bb_import_package* module_imports = bb_import_package_init("*");
	bindgen_link_core_api(module_imports);
	bindgen_link_canvas_api(module_imports);
	bindgen_link_clock_api(module_imports);
	bindgen_link_io_api(module_imports);
	bindgen_link_gles_api(module_imports);
	manual_link_gles_api(module_imports);

	//NOTE: compile
	// M3Result res = m3_CompileModule(app->runtime.m3Module);
	// if(res)
	// {
	// 	M3ErrorInfo errInfo = {0};
	// 	m3_GetErrorInfo(app->runtime.m3Runtime, &errInfo);

	// 	log_error("wasm error: %s\n", errInfo.message);

	// 	const char* options[] = {"OK"};
	// 	mp_alert_popup("Error",
	// 	               "The application couldn't load: can't compile web assembly module",
	// 	               1,
	// 	               options);
	// 	mp_request_quit();
	// 	return(-1);
	// }

	// bb_set_debug_trace_mode(BB_DEBUG_TRACE_FUNCTION);

	app->runtime.bbModuleInst = bb_module_instance_init(app->runtime.bbModuleDef);
	bb_module_instance_instantiate_opts module_inst_instantiate_opts = { 
		.packages = &module_imports, 
		.num_packages = 1, 
		.enable_debug = false,
		.wasm_memory_config = {
			.resize_callback = wasm_memory_resize_callback,
			.free_callback = wasm_memory_free_callback,
			.userdata = &app->runtime.wasmMemory,
		},
	};
	wasm_init_err = bb_module_instance_instantiate(app->runtime.bbModuleInst, module_inst_instantiate_opts);

	if (wasm_init_err != BB_ERROR_OK)
	{
		return quit_on_wasm_init_failure("wasm instantiate", wasm_init_err);
	}

	bb_import_package_deinit(module_imports); // safe to deinit this now that the module has been instantiated

	// TODO up next: expose a cached handle to functions to invoke instead of a string search

	//NOTE: Find and type check event handlers.
	for(int i=0; i<G_EXPORT_COUNT; i++)
	{
		const g_export_desc* desc = &G_EXPORT_DESC[i];

		bb_func_handle handler;
		const bb_error find_func_err = bb_module_instance_find_func(app->runtime.bbModuleInst, desc->name.ptr, &handler);
		// IM3Function handler = 0;
		// m3_FindFunction(&handler, app->runtime.m3Runtime, desc->name.ptr);

		if(find_func_err == BB_ERROR_OK)
		{
			//NOTE: check function signature
			bb_func_info func_info = bb_module_instance_func_info(app->runtime.bbModuleInst, handler);

			bool checked = func_info.num_returns == desc->retTags.len && func_info.num_params == desc->argTags.len;

			for(int retIndex = 0; checked && retIndex < func_info.num_returns; retIndex++)
			{
				bb_valtype valtype;
				if(tag_to_valtype(desc->retTags.ptr[retIndex], &valtype) && valtype != func_info.returns[retIndex])
				{
					checked = false;
				}
			}

			for(int argIndex = 0; checked && argIndex < func_info.num_params; argIndex++)
			{
				bb_valtype valtype;
				if(tag_to_valtype(desc->argTags.ptr[argIndex], &valtype) && valtype != func_info.params[argIndex])
				{
					checked = false;
				}
			}

			// int retCount = m3_GetRetCount(handler);
			// int argCount = m3_GetArgCount(handler);
			// if(retCount == desc->retTags.len && argCount == desc->argTags.len)
			// {
			// 	checked = true;
			// 	for(int retIndex = 0; retIndex < retCount; retIndex++)
			// 	{
			// 		M3ValueType m3Type = m3_GetRetType(handler, retIndex);
			// 		char tag = m3_type_to_tag(m3Type);

			// 		if(tag != desc->retTags.ptr[retIndex])
			// 		{
			// 			checked = false;
			// 			break;
			// 		}
			// 	}
			// 	if(checked)
			// 	{
			// 		for(int argIndex = 0; argIndex < argCount; argIndex++)
			// 		{
			// 			M3ValueType m3Type = m3_GetArgType(handler, argIndex);
			// 			char tag = m3_type_to_tag(m3Type);

			// 			if(tag != desc->argTags.ptr[argIndex])
			// 			{
			// 				checked = false;
			// 				break;
			// 			}
			// 		}
			// 	}
			// }

			if(checked)
			{
				app->runtime.exports[i] = handler;
			}
			else
			{
				log_error("type mismatch for event handler %.*s\n", (int)desc->name.len, desc->name.ptr);
			}
		}
	}

	//NOTE: get location of the raw event slot
	// IM3Global rawEventGlobal = m3_FindGlobal(app->runtime.m3Module, "_OrcaRawEvent");
	// app->runtime.rawEventOffset = (u32)rawEventGlobal->intValue;
	bb_global rawEventGlobal = bb_module_instance_find_global(app->runtime.bbModuleInst, "_OrcaRawEvent");
	app->runtime.rawEventOffset = (u32)rawEventGlobal.value->i32_val;

	//NOTE: preopen the app local root dir
	{
		str8 localRootPath = path_executable_relative(mem_scratch(), STR8("../app/data"));

		io_req req = {.op = IO_OP_OPEN_AT,
		              .open.rights = FILE_ACCESS_READ|FILE_ACCESS_WRITE,
		              .size = localRootPath.len,
		              .buffer = localRootPath.ptr};
		io_cmp cmp = io_wait_single_req_with_table(&req, &app->fileTable);
		app->rootDir = cmp.handle;
	}

	//NOTE: prepare GL surface
	mg_surface_prepare(app->surface);

	// IM3Function* exports = app->runtime.exports;
	bb_func_handle* exports = app->runtime.exports;

	//NOTE: call init handler
	if(bb_func_handle_isvalid(exports[G_EXPORT_ON_INIT]))
	{
		bb_error err = bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_ON_INIT], 0, 0, 0, 0, (bb_module_instance_invoke_opts){0});

		// M3Result err = m3_Call(exports[G_EXPORT_ON_INIT], 0, 0);
		// if(err != NULL)
		if(err != BB_ERROR_OK)
		{
			const char* errStr = bb_error_str(err);
			log_error("runtime error: %s\n", errStr);

			str8 msg = str8_pushf(mem_scratch(), "Runtime error: %s\n", errStr);
			const char* options[] = {"OK"};
			mp_alert_popup("Error",
		               	msg.ptr,
		               	1,
		               	options);

			mp_request_quit();
			return(-1);
		}
	}

	if(bb_func_handle_isvalid(exports[G_EXPORT_FRAME_RESIZE]))
	{
		mp_rect content = mp_window_get_content_rect(app->window);
		const bb_val args[2] = {
			{ .i32_val = (i32)content.w },
			{ .i32_val = (i32)content.h },
		};
		bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_FRAME_RESIZE], args, 2, 0, 0, (bb_module_instance_invoke_opts){0});
	}

	ui_set_context(&app->debugOverlay.ui);

	while(!mp_should_quit())
	{
		mp_event* event = 0;
		while((event = mp_next_event(mem_scratch())) != 0)
		{

			if(app->debugOverlay.show)
			{
				ui_process_event(event);
			}

			if(bb_func_handle_isvalid(exports[G_EXPORT_RAW_EVENT]))
			{
				#ifndef M3_BIG_ENDIAN
				mp_event* eventPtr = (mp_event*)wasm_memory_offset_to_ptr(&app->runtime.wasmMemory, app->runtime.rawEventOffset);
				memcpy(eventPtr, event, sizeof(*event));
				bb_val args[1];
				args[0].i32_val = app->runtime.rawEventOffset;
				bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_RAW_EVENT], args, 1, 0, 0, (bb_module_instance_invoke_opts){0});
				#else
				log_error("OnRawEvent() is not supported on big endian platforms");
				#endif
			}

			switch(event->type)
			{
				case MP_EVENT_WINDOW_CLOSE:
				{
					mp_request_quit();
				} break;

				case MP_EVENT_WINDOW_RESIZE:
				{
					mp_rect frame = {0, 0, event->move.frame.w, event->move.frame.h};

					if(bb_func_handle_isvalid(exports[G_EXPORT_FRAME_RESIZE]))
					{
						const bb_val args[2] = {
							{ .i32_val = (i32)(u32)event->move.content.w },
							{ .i32_val = (i32)(u32)event->move.content.h },
						};
						bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_FRAME_RESIZE], args, 2, 0, 0, (bb_module_instance_invoke_opts){0});

						// u32 width = (u32)event->move.content.w;
						// u32 height = (u32)event->move.content.h;
						// const void* args[2] = {&width, &height};
						// m3_Call(exports[G_EXPORT_FRAME_RESIZE], 2, args);
					}
				} break;

				case MP_EVENT_MOUSE_BUTTON:
				{
					if(event->key.action == MP_KEY_PRESS)
					{
						if(bb_func_handle_isvalid(exports[G_EXPORT_MOUSE_DOWN]))
						{
							const bb_val key = {.i32_val = event->key.code };
							// int key = event->key.code;
							// const void* args[1] = {&key};
							// m3_Call(exports[G_EXPORT_MOUSE_DOWN], 1, args);
							bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_MOUSE_DOWN], &key, 1, 0, 0, (bb_module_instance_invoke_opts){0});
						}
					}
					else
					{
						if(bb_func_handle_isvalid(exports[G_EXPORT_MOUSE_UP]))
						{
							const bb_val key = {.i32_val = event->key.code };
							// int key = event->key.code;
							// const void* args[1] = {&key};
							// m3_Call(exports[G_EXPORT_MOUSE_UP], 1, args);
							bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_MOUSE_UP], &key, 1, 0, 0, (bb_module_instance_invoke_opts){0});
						}
					}
				} break;

				case MP_EVENT_MOUSE_MOVE:
				{
					if(bb_func_handle_isvalid(exports[G_EXPORT_MOUSE_MOVE]))
					{
						const bb_val args[4] = {
							{ .f32_val = event->mouse.x },
							{ .f32_val = event->mouse.y },
							{ .f32_val = event->mouse.deltaX },
							{ .f32_val = event->mouse.deltaY },
						};
						// const void* args[4] = {&event->mouse.x, &event->mouse.y, &event->mouse.deltaX, &event->mouse.deltaY};
						// m3_Call(exports[G_EXPORT_MOUSE_MOVE], 4, args);
						bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_MOUSE_MOVE], args, 4, 0, 0, (bb_module_instance_invoke_opts){0});
					}
				} break;

				case MP_EVENT_KEYBOARD_KEY:
				{
					if(event->key.action == MP_KEY_PRESS)
					{
						if(event->key.code == MP_KEY_D && (event->key.mods & (MP_KEYMOD_SHIFT | MP_KEYMOD_CMD)))
						{
						#if 1 // EPILEPSY WARNING! on windows this has a bug which causes a pretty strong stroboscopic effect
							debug_overlay_toggle(&app->debugOverlay);
						#endif
						}

						if(bb_func_handle_isvalid(exports[G_EXPORT_KEY_DOWN]))
						{
							const bb_val key = {.i32_val = event->key.code };
							// const void* args[1] = {&event->key.code};
							// m3_Call(exports[G_EXPORT_KEY_DOWN], 1, args);
							bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_KEY_DOWN], &key, 1, 0, 0, (bb_module_instance_invoke_opts){0});
						}
					}
					else if(event->key.action == MP_KEY_RELEASE)
					{
						if(bb_func_handle_isvalid(exports[G_EXPORT_KEY_UP]))
						{
							const bb_val key = {.i32_val = event->key.code };
							// const void* args[1] = {&event->key.code};
							// m3_Call(exports[G_EXPORT_KEY_UP], 1, args);
							bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_KEY_UP], &key, 1, 0, 0, (bb_module_instance_invoke_opts){0});
						}
					}
				} break;

				default:
					break;
			}
		}

		if(app->debugOverlay.show)
		{
			ui_style debugUIDefaultStyle = {.bgColor = {0},
			                                .color = {1, 1, 1, 1},
			                                .font = app->debugOverlay.fontReg,
			                                .fontSize = 16,
			                                .borderColor = {1, 0, 0, 1},
			                                .borderSize = 2};

			ui_style_mask debugUIDefaultMask = UI_STYLE_BG_COLOR
			                                 | UI_STYLE_COLOR
			                                 | UI_STYLE_BORDER_COLOR
			                                 | UI_STYLE_BORDER_SIZE
			                                 | UI_STYLE_FONT
			                                 | UI_STYLE_FONT_SIZE;

			vec2 frameSize = mg_surface_get_size(app->debugOverlay.surface);

			ui_frame(frameSize, &debugUIDefaultStyle, debugUIDefaultMask)
			{
				ui_style_next(&(ui_style){.size.width = {UI_SIZE_PARENT, 1},
				                          .size.height = {UI_SIZE_PARENT, 1, 1}},
				               UI_STYLE_SIZE);

				ui_container("overlay area", 0)
				{
					//...
				}

				ui_style_next(&(ui_style){.size.width = {UI_SIZE_PARENT, 1},
				                          .size.height = {UI_SIZE_PARENT, 0.4},
				                          .layout.axis = UI_AXIS_Y,
				                          .bgColor = {0, 0, 0, 0.5}},
				               UI_STYLE_SIZE
				              |UI_STYLE_LAYOUT_AXIS
				              |UI_STYLE_BG_COLOR);

				ui_container("log console", UI_FLAG_DRAW_BACKGROUND)
				{
					ui_style_next(&(ui_style){.size.width = {UI_SIZE_PARENT, 1},
					                          .size.height = {UI_SIZE_CHILDREN},
					                          .layout.axis = UI_AXIS_X,
					                          .layout.spacing = 10,
					                          .layout.margin.x = 10,
					                          .layout.margin.y = 10},
					               UI_STYLE_SIZE
					              |UI_STYLE_LAYOUT);

					ui_container("log toolbar", 0)
					{
						ui_style buttonStyle = {.layout.margin.x = 4,
						                        .layout.margin.y = 4,
						                        .roundness = 2,
						                        .bgColor = {0, 0, 0, 0.5},
						                        .color = {1, 1, 1, 1}};

						ui_style_mask buttonStyleMask = UI_STYLE_LAYOUT_MARGINS
						                              | UI_STYLE_ROUNDNESS
						                              | UI_STYLE_BG_COLOR
						                              | UI_STYLE_COLOR;

						ui_style_match_after(ui_pattern_all(), &buttonStyle, buttonStyleMask);
						if(ui_button("Clear").clicked)
						{
							for_list_safe(&app->debugOverlay.logEntries, entry, log_entry, listElt)
							{
								list_remove(&app->debugOverlay.logEntries, &entry->listElt);
								list_push(&app->debugOverlay.logFreeList, &entry->listElt);
								app->debugOverlay.entryCount--;
							}
						}
					}

					ui_style_next(&(ui_style){.size.width = {UI_SIZE_PARENT, 1},
					                          .size.height = {UI_SIZE_PARENT, 1, 1}},
					              UI_STYLE_SIZE);

					//TODO: this is annoying to have to do that. Basically there's another 'contents' box inside ui_panel,
					//      and we need to change that to size according to its parent (whereas the default is sizing according
					//      to its children)
					ui_pattern pattern = {0};
					ui_pattern_push(mem_scratch(), &pattern, (ui_selector){.kind = UI_SEL_OWNER});
					ui_pattern_push(mem_scratch(), &pattern, (ui_selector){.kind = UI_SEL_TEXT, .text = STR8("contents")});
					ui_style_match_after(pattern, &(ui_style){.size.width = {UI_SIZE_PARENT, 1}}, UI_STYLE_SIZE_WIDTH);

					ui_box* panel = ui_box_lookup("log view");
					f32 scrollY = 0;
					if(panel)
					{
						scrollY = panel->scroll.y;
					}

					ui_panel("log view", UI_FLAG_SCROLL_WHEEL_Y)
					{
						panel = ui_box_top();

						ui_style_next(&(ui_style){.size.width = {UI_SIZE_PARENT, 1},
						                          .size.height = {UI_SIZE_CHILDREN},
						                          .layout.axis = UI_AXIS_Y,
						                          .layout.margin.y = 5},
						              UI_STYLE_SIZE
						             |UI_STYLE_LAYOUT_AXIS);

						ui_container("contents", 0)
						{
							for_list(&app->debugOverlay.logEntries, entry, log_entry, listElt)
							{
								log_entry_ui(&app->debugOverlay, entry);
							}
						}
					}
					if(app->debugOverlay.logScrollToLast)
					{
						if(panel->scroll.y >= scrollY)
						{
							panel->scroll.y = ClampLowBound(panel->childrenSum[1] - panel->rect.h, 0);
						}
						else
						{
							app->debugOverlay.logScrollToLast = false;
						}
					}
					else if(panel->scroll.y >= (panel->childrenSum[1] - panel->rect.h) - 1)
					{
						app->debugOverlay.logScrollToLast = true;
					}
				}
			}

			mg_surface_prepare(app->debugOverlay.surface);
			mg_canvas_set_current(app->debugOverlay.canvas);
				ui_draw();

			mg_render(app->debugOverlay.surface, app->debugOverlay.canvas);
		}

		if(bb_func_handle_isvalid(exports[G_EXPORT_FRAME_REFRESH]))
		{
			mg_surface_prepare(app->surface);
			// m3_Call(exports[G_EXPORT_FRAME_REFRESH], 0, 0);
			bb_module_instance_invoke(app->runtime.bbModuleInst, exports[G_EXPORT_FRAME_REFRESH], 0, 0, 0, 0, (bb_module_instance_invoke_opts){0});
		}

		if(app->debugOverlay.show)
		{
			mg_surface_prepare(app->debugOverlay.surface);
			mg_surface_present(app->debugOverlay.surface);
		}

		mem_arena_clear(mem_scratch());
	}

	return(0);
}

int main(int argc, char** argv)
{
	log_set_level(LOG_LEVEL_INFO);

	mp_init();
	mp_clock_init();

	orca_app* app = &__orcaApp;

	//NOTE: create window and surfaces
	mp_rect windowRect = {.x = 100, .y = 100, .w = 810, .h = 610};
	app->window = mp_window_create(windowRect, "orca", 0);

	app->surface = mg_surface_create_for_window(app->window, MG_CANVAS);
	app->canvas = mg_canvas_create();
	mg_surface_swap_interval(app->surface, 1);

	app->debugOverlay.show = false;
	app->debugOverlay.surface = mg_surface_create_for_window(app->window, MG_CANVAS);
	app->debugOverlay.canvas = mg_canvas_create();
	app->debugOverlay.fontReg = orca_font_create("../resources/Menlo.ttf");
	app->debugOverlay.fontBold = orca_font_create("../resources/Menlo Bold.ttf");
	app->debugOverlay.maxEntries = 200;
	mem_arena_init(&app->debugOverlay.logArena);

	mg_surface_swap_interval(app->debugOverlay.surface, 0);

	mg_surface_set_hidden(app->debugOverlay.surface, true);

	mg_surface_deselect();

	//WARN: this is a workaround to avoid stalling the first few times we acquire drawables from
	//      the surfaces... This should probably be fixed in the implementation of mtl_surface!

	for(int i=0; i<3; i++)
	{
		mg_surface_prepare(app->surface);
		mg_canvas_set_current(app->canvas);
		mg_render(app->surface, app->canvas);
		mg_surface_present(app->surface);

		mg_surface_prepare(app->debugOverlay.surface);
		mg_canvas_set_current(app->debugOverlay.canvas);
		mg_render(app->debugOverlay.surface, app->debugOverlay.canvas);
		mg_surface_present(app->debugOverlay.surface);
	}

	ui_init(&app->debugOverlay.ui);

	//NOTE: show window and start runloop
	mp_window_bring_to_front(app->window);
	mp_window_focus(app->window);
	mp_window_center(app->window);

	mp_thread* runloopThread = mp_thread_create(orca_runloop, 0);

	while(!mp_should_quit())
	{
		mp_pump_events(-1);
		//TODO: what to do with mem scratch here?
	}

	mp_thread_join(runloopThread, NULL);

	mg_canvas_destroy(app->canvas);
	mg_surface_destroy(app->surface);

	mg_canvas_destroy(app->debugOverlay.canvas);
	mg_surface_destroy(app->debugOverlay.surface);

	mp_window_destroy(app->window);

	mp_terminate();
	return(0);
}

#undef LOG_SUBSYSTEM
