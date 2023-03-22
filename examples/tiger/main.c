/************************************************************//**
*
*	@file: main.cpp
*	@author: Martin Fouilleul
*	@date: 30/07/2022
*	@revision:
*
*****************************************************************/
#include<stdlib.h>
#include<string.h>
#include<errno.h>

#define _USE_MATH_DEFINES //NOTE: necessary for MSVC
#include<math.h>

#include"milepost.h"

#define LOG_SUBSYSTEM "Main"

#include"tiger.c"

mg_font create_font()
{
	//NOTE(martin): create font
	str8 fontPath = mp_app_get_resource_path(mem_scratch(), "../resources/OpenSansLatinSubset.ttf");
	char* fontPathCString = str8_to_cstring(mem_scratch(), fontPath);

	FILE* fontFile = fopen(fontPathCString, "r");
	if(!fontFile)
	{
		LOG_ERROR("Could not load font file '%s': %s\n", fontPathCString, strerror(errno));
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

int main()
{
	LogLevel(LOG_LEVEL_WARNING);

	mp_init();
	mp_clock_init(); //TODO put that in mp_init()?

	mp_rect windowRect = {.x = 100, .y = 100, .w = 810, .h = 610};
	mp_window window = mp_window_create(windowRect, "test", 0);

	mp_rect contentRect = mp_window_get_content_rect(window);

	//NOTE: create surface
	mg_surface surface = mg_surface_create_for_window(window, MG_BACKEND_DEFAULT);
	mg_surface_swap_interval(surface, 0);

	//TODO: create canvas
	mg_canvas canvas = mg_canvas_create(surface);

	if(mg_canvas_is_nil(canvas))
	{
		printf("Error: couldn't create canvas\n");
		return(-1);
	}

	mg_font font = create_font();

	// start app
	mp_window_bring_to_front(window);
	mp_window_focus(window);

	bool tracked = false;
	vec2 trackPoint = {0};
	f32 zoom = 1;
	f32 startX = 300, startY = 200;

	f64 frameTime = 0;

	while(!mp_should_quit())
	{
		f64 startTime = mp_get_time(MP_CLOCK_MONOTONIC);

		mp_pump_events(0);
		mp_event event = {0};
		while(mp_next_event(&event))
		{
			switch(event.type)
			{
				case MP_EVENT_WINDOW_CLOSE:
				{
					mp_request_quit();
				} break;

				case MP_EVENT_MOUSE_BUTTON:
				{
					if(event.key.code == MP_MOUSE_LEFT)
					{
						if(event.key.action == MP_KEY_PRESS)
						{
							tracked = true;
							vec2 mousePos = mp_mouse_position();
							trackPoint.x = (mousePos.x - startX)/zoom;
							trackPoint.y = (mousePos.y - startY)/zoom;
						}
						else
						{
							tracked = false;
						}
					}
				} break;

				case MP_EVENT_MOUSE_WHEEL:
				{
					vec2 mousePos = mp_mouse_position();
					f32 pinX = (mousePos.x - startX)/zoom;
					f32 pinY = (mousePos.y - startY)/zoom;

					zoom *= 1 + event.move.deltaY * 0.01;
					zoom = Clamp(zoom, 0.5, 5);

					startX = mousePos.x - pinX*zoom;
					startY = mousePos.y - pinY*zoom;
				} break;

				default:
					break;
			}
		}

		if(tracked)
		{
			vec2 mousePos = mp_mouse_position();
			startX = mousePos.x - trackPoint.x*zoom;
			startY = mousePos.y - trackPoint.y*zoom;
		}

		mg_surface_prepare(surface);

		mg_set_color_rgba(1, 0, 1, 1);
		mg_clear();

		mg_matrix_push((mg_mat2x3){zoom, 0, startX,
		                           0, zoom, startY});

		draw_tiger();

		mg_matrix_pop();
/*
			// text
			mg_set_color_rgba(0, 0, 1, 1);
			mg_set_font(font);
			mg_set_font_size(12);
			mg_move_to(50, 600-50);

			str8 text = str8_pushf(mem_scratch(),
			                      "Milepost vector graphics test program (frame time = %fs, fps = %f)...",
			                      frameTime,
			                      1./frameTime);
			mg_text_outlines(text);
			mg_fill();
*/
			printf("Milepost vector graphics test program (frame time = %fs, fps = %f)...\n",
			                      frameTime,
			                      1./frameTime);

			mg_flush();
		mg_surface_present(surface);

		mem_arena_clear(mem_scratch());
		frameTime = mp_get_time(MP_CLOCK_MONOTONIC) - startTime;
	}

	mg_font_destroy(font);
	mg_canvas_destroy(canvas);
	mg_surface_destroy(surface);
	mp_window_destroy(window);

	mp_terminate();

	return(0);
}
