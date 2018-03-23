#pragma once
#define UNICODE
#include <windows.h>
#include "common.h"
#include "ho_gl.h"

enum MenuCommands {
	F_COMMAND_NONE = 0,
	F_COMMAND_OPEN,
	F_COMMAND_CLOSE,
	F_COMMAND_NEW,
	F_COMMAND_SAVE,
	F_COMMAND_SAVEAS,
	F_COMMAND_EXIT,

	F_COMMAND_UNDO,
	F_COMMAND_REDO,
	F_COMMAND_CUT,
	F_COMMAND_PASTE,
	F_COMMAND_COPY,
	F_COMMAND_SEARCH,

	F_COMMAND_VIEW_HISTORY,
	F_COMMAND_VIEW_FILTERS,

	F_COMMAND_NUMBER
};
/*
struct Window_Info
{
	s32 width;
	s32 height;

	HWND handle;
	HINSTANCE instance;
	HDC device_context;
	HGLRC rendering_context;
	LRESULT(CALLBACK* window_proc)(HWND, UINT, WPARAM, LPARAM);

	WINDOWPLACEMENT placement;
	bool core_context;
};
*/
struct Application_State {
	bool running;

	Window_Info window_info;

	HMENU main_menu;
	HMENU file_menu;
	HMENU edit_menu;
	HMENU view_menu;
	HMENU plugins_menu;


	HWND  subwindow;
};