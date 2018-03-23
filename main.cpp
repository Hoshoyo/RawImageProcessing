#include "libraw/libraw.h"
#include "win32_platform.h"
#include <stdio.h>
#include <GL/GL.h>
#include <assert.h>
#include <math.h>

internal Application_State app;

#include "renderer.cpp"

LRESULT CALLBACK window_callback(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg)
	{
	case WM_INITMENUPOPUP: {
		// reload plugins
		if(lparam == 3)
			printf("Plugins menu open\n");
	}break;
	case WM_COMMAND: {
		switch (wparam) {
		case F_COMMAND_OPEN: {
			wchar_t buffer[1024] = { 0 };

			OPENFILENAME fn = { 0 };
			fn.lStructSize = sizeof(OPENFILENAME);
			fn.lpstrFile = buffer;
			fn.nMaxFile = sizeof(buffer);

			GetOpenFileName(&fn);
			wprintf(L"Tried to open file %s\n", buffer);
		}break;
		case F_COMMAND_SAVE:
		case F_COMMAND_SAVEAS: {
			wchar_t buffer[1024] = { 0 };

			OPENFILENAME fn = { 0 };
			fn.lStructSize = sizeof(OPENFILENAME);
			fn.lpstrFile = buffer;
			fn.nMaxFile = sizeof(buffer);

			GetSaveFileName(&fn);
			wprintf(L"Tried to save file %s\n", buffer);
		}break;
		case F_COMMAND_EXIT: {
			app.running = false;
		}break;
		default: break;
		}
	}break;
	case WM_KILLFOCUS: {
	}break;
	case WM_CREATE: {
		app.file_menu = CreateMenu();
		AppendMenu(app.file_menu, MF_STRING, F_COMMAND_NEW, L"&New... \tCtrl+N");
		AppendMenu(app.file_menu, MF_STRING, F_COMMAND_OPEN, L"&Open...\tCtrl+O");
		AppendMenu(app.file_menu, MF_STRING, F_COMMAND_SAVE, L"&Save\tCtrl+S");
		AppendMenu(app.file_menu, MF_STRING, F_COMMAND_SAVEAS, L"Save &As...");
		AppendMenu(app.file_menu, MF_STRING, F_COMMAND_CLOSE, L"&Close\tCtrl+W");
		AppendMenu(app.file_menu, MF_MENUBREAK, 0, 0);
		AppendMenu(app.file_menu, MF_STRING, F_COMMAND_EXIT, L"E&xit");

		app.edit_menu = CreateMenu();
		AppendMenu(app.edit_menu, MF_STRING | MF_GRAYED, F_COMMAND_UNDO, L"&Undo\tCtrl+Z");
		AppendMenu(app.edit_menu, MF_STRING | MF_GRAYED, F_COMMAND_REDO, L"&Redo\tCtrl+Y");
		AppendMenu(app.edit_menu, MF_MENUBREAK, 0, 0);
		AppendMenu(app.edit_menu, MF_STRING | MF_GRAYED, F_COMMAND_CUT, L"Cu&t\tCtrl+X");
		AppendMenu(app.edit_menu, MF_STRING | MF_GRAYED, F_COMMAND_COPY, L"&Copy\tCtrl+C");
		AppendMenu(app.edit_menu, MF_STRING | MF_GRAYED, F_COMMAND_PASTE, L"&Paste\tCtrl+V");
		AppendMenu(app.edit_menu, MF_STRING, F_COMMAND_SEARCH, L"&Search\tCtrl+F");

		app.view_menu = CreateMenu();
		AppendMenu(app.view_menu, MF_STRING | MF_UNCHECKED, F_COMMAND_VIEW_HISTORY, L"&History");
		AppendMenu(app.view_menu, MF_STRING | MF_UNCHECKED, F_COMMAND_VIEW_FILTERS, L"&Filters");

		app.plugins_menu = CreateMenu();

		app.main_menu = CreateMenu();
		AppendMenu(app.main_menu, MF_POPUP, (UINT_PTR)app.file_menu, L"&File");
		AppendMenu(app.main_menu, MF_POPUP, (UINT_PTR)app.edit_menu, L"&Edit");
		AppendMenu(app.main_menu, MF_POPUP, (UINT_PTR)app.view_menu, L"&View");
		AppendMenu(app.main_menu, MF_POPUP, (UINT_PTR)app.plugins_menu, L"&Plugins");
		SetMenu(window, app.main_menu);


		app.subwindow = CreateWindow(L"static", L"Fotos_Subwindow_Opengl", WS_VISIBLE | WS_CHILD | WS_BORDER, 0, 0, 500, 500, window, 0, 0, 0);
		app.window_info.window_handle = app.subwindow;
		app.window_info.width = 500;
		app.window_info.height = 500;
		hogl_init_opengl(&app.window_info, 3, 3);
		hogl_init_gl_extensions();

		glClearColor(0.44f, 0.6f, 0.66f, 1.0f);
	} break;
	case WM_CLOSE:
		app.running = false;
		break;
	case WM_CHAR:
		break;
	case WM_SIZE: {
		RECT client_rect;
		GetClientRect(window, &client_rect);
		int width = client_rect.right - client_rect.left;
		int height = client_rect.bottom - client_rect.top;
		app.window_info.width = width;
		app.window_info.height = height;
		if (app.subwindow) {
			BOOL res = SetWindowPos(app.subwindow, HWND_TOP, 0, 0, width, height, 0);
			if (!res) {
				OutputDebugStringA("Error resizing subwindow\n");
			} else {
				glViewport(0, 0, width, height);
			}
		}

	} break;
	case WM_DROPFILES: {
	}break;
	default:
		return DefWindowProc(window, msg, wparam, lparam);
	}
	return 0;
}

void exec(const char* cmd) {
	// CreatePipe function investigate
	char buffer[2048] = {};
	FILE* pipe = _popen(cmd, "r");

	int size = 0;
	while (size == 0) {
		fseek(pipe, 0, SEEK_END);
		size = ftell(pipe);
		fseek(pipe, 0, SEEK_SET);
	}

	fread(buffer, size, 1, pipe);
	_pclose(pipe);
}

int get_index(int x, int y, int width, int height, int channel) {
	if (x < 0) x += 2;
	if (x >= width) x -= 2;
	if (y < 0) y += 2;
	if (y >= height) y -= 2;
	return (y * 4 * width + x * 4 + channel);
}

static u32 tex_id;
static s32 img_width;
static s32 img_height;

int demosaic(LibRaw& img) {
	int width = img.imgdata.sizes.iwidth;
	int height = img.imgdata.sizes.iheight;
	ushort top_margin = img.imgdata.sizes.top_margin;
	ushort max = 4036;
	void* data = calloc(4, width * height * sizeof(float));

	short* ptr = (short*)img.imgdata.image;
	for (int i = 0; i < width * height; ++i) {
		s32 x = i % width;
		s32 y = i / width;

		ushort sr = ptr[get_index(x, y, width, height, 0)];
		ushort sg = ptr[get_index(x, y, width, height, 1)];
		ushort sb = ptr[get_index(x, y, width, height, 2)];
		ushort sa = ptr[get_index(x, y, width, height, 3)];

		r32 r = (r32)sr / (r32)max;
		r32 g = (r32)sg / (r32)max;
		r32 b = (r32)sb / (r32)max;
		r32 a = (r32)sa / (r32)max;

		bool pair_line = (y % 2 == 0);
		bool pair_column = (x % 2 == 0);

		if (!pair_column && !pair_line) {
			// 4 cantos
			r32 r_tl = ptr[get_index(x - 1, y - 1, width, height, 0)] / (r32)max;
			r32 r_tr = ptr[get_index(x + 1, y - 1, width, height, 0)] / (r32)max;
			r32 r_bl = ptr[get_index(x - 1, y + 1, width, height, 0)] / (r32)max;
			r32 r_br = ptr[get_index(x + 1, y + 1, width, height, 0)] / (r32)max;
			r = (((r_tl + r_tr) / 2.0f) + ((r_bl + r_br) / 2.0f)) / 2.0f;
			
			r32 g_l = (ptr[get_index(x - 1, y, width, height, 1)] + ptr[get_index(x - 1, y, width, height, 3)]) / (r32)max;
			r32 g_r = (ptr[get_index(x + 1, y, width, height, 1)] + ptr[get_index(x + 1, y, width, height, 3)]) / (r32)max;
			r32 g_t = (ptr[get_index(x, y - 1, width, height, 1)] + ptr[get_index(x, y - 1, width, height, 3)]) / (r32)max;
			r32 g_b = (ptr[get_index(x, y + 1, width, height, 1)] + ptr[get_index(x, y + 1, width, height, 3)]) / (r32)max;
			g = (((g_l + g_r) / 2.0f) + ((g_t + g_b) / 2.0f)) / 2.0f;
		}
		if (!pair_column && pair_line) {
			r32 r_l  = ptr[get_index(x - 1, y, width, height, 0)] / (r32)max;
			r32 r_r  = ptr[get_index(x + 1, y, width, height, 0)] / (r32)max;
			r = (r_l + r_r) / 2.0f;

			r32 b_t = ptr[get_index(x, y - 1, width, height, 2)] / (r32)max;
			r32 b_b = ptr[get_index(x, y + 1, width, height, 2)] / (r32)max;
			b = (b_t + b_b) / 2.0f;
		}
		if (pair_column && !pair_line) {
			r32 r_t  = ptr[get_index(x, y - 1, width, height, 0)] / (r32)max;
			r32 r_b  = ptr[get_index(x, y + 1, width, height, 0)] / (r32)max;
			r = (r_t + r_b) / 2.0f;

			r32 b_l = ptr[get_index(x - 1, y, width, height, 2)] / (r32)max;
			r32 b_r = ptr[get_index(x + 1, y, width, height, 2)] / (r32)max;
			b = (b_l + b_r) / 2.0f;
		}
		if (pair_column && pair_line) {
			r32 g_l = (ptr[get_index(x - 1, y, width, height, 1)] + ptr[get_index(x - 1, y, width, height, 3)]) / (r32)max;
			r32 g_r = (ptr[get_index(x + 1, y, width, height, 1)] + ptr[get_index(x + 1, y, width, height, 3)]) / (r32)max;
			r32 g_t = (ptr[get_index(x, y - 1, width, height, 1)] + ptr[get_index(x, y - 1, width, height, 3)]) / (r32)max;
			r32 g_b = (ptr[get_index(x, y + 1, width, height, 1)] + ptr[get_index(x, y + 1, width, height, 3)]) / (r32)max;
			g = (((g_l + g_r) / 2.0f) + ((g_t + g_b) / 2.0f)) / 2.0f;

			r32 b_tl = ptr[get_index(x - 1, y - 1, width, height, 2)] / (r32)max;
			r32 b_tr = ptr[get_index(x + 1, y - 1, width, height, 2)] / (r32)max;
			r32 b_bl = ptr[get_index(x - 1, y + 1, width, height, 2)] / (r32)max;
			r32 b_br = ptr[get_index(x + 1, y + 1, width, height, 2)] / (r32)max;
			b = (((b_tl + b_tr) / 2.0f) + ((b_bl + b_br) / 2.0f)) / 2.0f;
		}

		((r32*)data)[i * 4 + 0] = r;
		((r32*)data)[i * 4 + 1] = g + a;
		((r32*)data)[i * 4 + 2] = b;
		((r32*)data)[i * 4 + 3] = 1.0f;
	}

	// 1082 1662
	r32 wr = 1.0f / ((r32*)data)[get_index(1082, 1662, width, height, 0)];
	r32 wg = 1.0f / ((r32*)data)[get_index(1082, 1662, width, height, 1)];
	r32 wb = 1.0f / ((r32*)data)[get_index(1082, 1662, width, height, 2)];

	r32 wb_max = 0.0f;

	for (int i = 0; i < width * height; ++i) {
		s32 x = i % width;
		s32 y = i / width;

		r32 r = ((r32*)data)[get_index(x, y, width, height, 0)];
		r32 g = ((r32*)data)[get_index(x, y, width, height, 1)];
		r32 b = ((r32*)data)[get_index(x, y, width, height, 2)];

		r *= wr;
		g *= wg;
		b *= wb;

		//r = powf(r, 1.0f / 2.2f);
		//g = powf(g, 1.0f / 2.2f);
		//b = powf(b, 1.0f / 2.2f);

		if (r > wb_max) wb_max = r;
		if (g > wb_max) wb_max = g;
		if (b > wb_max) wb_max = b;

		((r32*)data)[get_index(x, y, width, height, 0)] = r;
		((r32*)data)[get_index(x, y, width, height, 1)] = g;
		((r32*)data)[get_index(x, y, width, height, 2)] = b;
	}

	
	for (int i = 0; i < width * height; ++i) {
		s32 x = i % width;
		s32 y = i / width;

		r32 r = ((r32*)data)[get_index(x, y, width, height, 0)];
		r32 g = ((r32*)data)[get_index(x, y, width, height, 1)];
		r32 b = ((r32*)data)[get_index(x, y, width, height, 2)];

		((r32*)data)[get_index(x, y, width, height, 0)] = r / wb_max;
		((r32*)data)[get_index(x, y, width, height, 1)] = g / wb_max;
		((r32*)data)[get_index(x, y, width, height, 2)] = b / wb_max;

		assert(r / wb_max <= 1.0f);
		assert(g / wb_max <= 1.0f);
		assert(b / wb_max <= 1.0f);
	}


	char* png = (char*)calloc(4, width * height);
	for (int i = 0; i < width * height * 4; i += 4) {
		png[i + 0] = (char)(255.0f * ((r32*)data)[i + 0]);
		png[i + 1] = (char)(255.0f * ((r32*)data)[i + 1]);
		png[i + 2] = (char)(255.0f * ((r32*)data)[i + 2]);
		png[i + 3] = (char)(255.0f * ((r32*)data)[i + 3]);
		int x = 0;
	}
	tex_id = create_texture(width, height, png);
	return tex_id;
}

int process_image(char *file)
{
	// Let us create an image processor
	LibRaw iProcessor;

	// Open the file and read the metadata
	iProcessor.open_file(file);

	// Let us unpack the image
	iProcessor.unpack();

	// Convert from imgdata.rawdata to imgdata.image:
	iProcessor.raw2image();

	img_width = iProcessor.imgdata.sizes.width;
	img_height = iProcessor.imgdata.sizes.height;
	tex_id = demosaic(iProcessor);

	// Finally, let us free the image processor for work with the next image
	iProcessor.recycle();
	return 0;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int cmd_show) {

	AllocConsole();
	FILE* pCout;
	freopen_s(&pCout, "CONOUT$", "w", stdout);

	WNDCLASSEX window_class = {};
	window_class.cbSize = sizeof(WNDCLASSEX);
	window_class.style = 0;
	window_class.lpfnWndProc = window_callback;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = instance;
	window_class.hIcon = 0;
	window_class.hCursor = LoadCursor(0, IDC_ARROW);
	window_class.hbrBackground = (HBRUSH)COLOR_WINDOW;
	window_class.lpszMenuName = 0;
	window_class.lpszClassName = L"World";
	window_class.hIconSm = 0;

	if (!RegisterClassEx(&window_class)) {
		MessageBox(0, L"Error registering window class.", L"Fatal error", MB_ICONERROR);
		ExitProcess(-1);
	}

	HWND window = CreateWindowEx(
		WS_EX_ACCEPTFILES | WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
		window_class.lpszClassName, L"Fotos",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, 0, 0, instance, 0);

	if (!window) {
		MessageBox(0, L"Error creating window", L"Fatal error", MB_ICONERROR);
		ExitProcess(-1);
	}

	init_immediate_quad_mode();
	process_image("scene_raw.CR2");
	//hm::vec2 pos(30.0f, -2580.0f);
	hm::vec2 pos(0.0f, 0.0f);
	bool keys[] = {
		false, false, false, false
	};

	app.running = true;
	MSG msg;
	while (GetMessage(&msg, window, 0, 0) && app.running) {
		switch (msg.message) {
			case WM_KEYDOWN: {
				bool ctrl_was_pressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
				switch (msg.wParam) {
					case 'O': {
						if (ctrl_was_pressed)
							SendMessage(window, WM_COMMAND, F_COMMAND_OPEN, 0);
					}break;
					case 'N': {
						if (ctrl_was_pressed)
							SendMessage(window, WM_COMMAND, F_COMMAND_NEW, 0);
					}break;
					case 'S': {
						if (ctrl_was_pressed)
							SendMessage(window, WM_COMMAND, F_COMMAND_SAVE, 0);
					}break;
					case 'A': {
						if (ctrl_was_pressed)
							SendMessage(window, WM_COMMAND, F_COMMAND_SAVEAS, 0);
					}break;
					case 'Z': {
						if (ctrl_was_pressed)
							SendMessage(window, WM_COMMAND, F_COMMAND_UNDO, 0);
					}break;
					case 'Y': {
						if (ctrl_was_pressed)
							SendMessage(window, WM_COMMAND, F_COMMAND_REDO, 0);
					}break;
					case 'X': {
						if (ctrl_was_pressed)
							SendMessage(window, WM_COMMAND, F_COMMAND_CUT, 0);
					}break;
					case 'C': {
						if (ctrl_was_pressed)
							SendMessage(window, WM_COMMAND, F_COMMAND_COPY, 0);
					}break;
					case 'V': {
						if (ctrl_was_pressed)
							SendMessage(window, WM_COMMAND, F_COMMAND_PASTE, 0);
					}break;
					case 'L': {
						if (ctrl_was_pressed) {
							//ModifyMenu(app.file_menu, F_COMMAND_NEW, MF_STRING, 0, L"&Novo...\tCtrl+N");
						}
					}break;
					case VK_UP:    keys[0] = true; break;
					case VK_DOWN:  keys[1] = true; break;
					case VK_LEFT:  keys[2] = true; break;
					case VK_RIGHT: keys[3] = true; break;
				}
			}break;
			case WM_KEYUP: {
				switch (msg.wParam) {
					case VK_UP:    keys[0] = false; break;
					case VK_DOWN:  keys[1] = false; break;
					case VK_LEFT:  keys[2] = false; break;
					case VK_RIGHT: keys[3] = false; break;
				}
			} break;
			default: {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}break;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (keys[0])
			pos.y -= 30.0f;
		if (keys[1])
			pos.y += 30.0f;
		if (keys[2])
			pos.x += 30.0f;
		if (keys[3])
			pos.x -= 30.0f;

		immediate_quad(tex_id, 0, img_width / 2, img_height / 2, 0, hm::vec4(1, 1, 1, 1), pos, app.window_info.width, app.window_info.height);

		SwapBuffers(app.window_info.device_context);
	}

	ExitProcess(0);
}