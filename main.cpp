#include "libraw/libraw.h"
#include "win32_platform.h"
#include <stdio.h>
#include <GL/GL.h>
#define HOGL_IMPLEMENT
#include "ho_gl.h"
#include "homath.h"
#include <assert.h>
#include <math.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

internal Application_State app;

static LibRaw lr;

struct RawImage {
	char* filename;
	LibRaw* raw;
	s32 width;
	s32 height;
	u32 texture_id;
	bool loaded;

	r32* demosaic_data;

	hm::vec2 position;
};
static RawImage loaded_image;

struct LDRImage {
	char* filename;
	u8* data;
	s32 width;
	s32 height;
	s32 channels;
	u32 texture_id;
	bool loaded;
	hm::vec2 position;
	r32 zoom;
	r32 exposure_time;
};
static LDRImage ldr_image[16];
static s32 ldr_index;
static s32 selected_ldr;
static u32 hdr_texture_id;
static bool display_hdr;

int process_image(RawImage* loaded_image, char *file);
int load_ldr_image(LDRImage* image, r32 exp_time, char* file);
int save_as_png(RawImage* image, char* out_filename);

#include "renderer.cpp"
#include "raw.cpp"
#include "ldr_to_hdr.cpp"

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
			char buffer[1024] = { 0 };

			OPENFILENAMEA fn = { 0 };
			fn.lStructSize = sizeof(OPENFILENAMEA);
			fn.lpstrFile = buffer;
			fn.nMaxFile = sizeof(buffer);

			int r = GetOpenFileNameA(&fn);
			if (r != 0) {
#if ASSIGNMENT_1
				int err = process_image(&loaded_image, buffer);//"scene_raw.CR2");
#else
				int err = load_ldr_image(&ldr_image[ldr_index++], 1.0f, buffer);	// TODO(psv): read exposure time
#endif
				if (err == 0) {
					ModifyMenu(app.file_menu, F_COMMAND_SAVE, MF_STRING, F_COMMAND_SAVE, L"&Save\tCtrl+S");
					ModifyMenu(app.file_menu, F_COMMAND_SAVEAS, MF_STRING, F_COMMAND_SAVEAS, L"Save &As...");
					ModifyMenu(app.file_menu, F_COMMAND_CLOSE, MF_STRING, F_COMMAND_CLOSE, L"&Close\tCtrl+W");
				}
			}
		}break;
		case F_COMMAND_SAVE:
		case F_COMMAND_SAVEAS: {
			char buffer[1024] = { 0 };

			OPENFILENAMEA fn = { 0 };
			fn.lStructSize = sizeof(OPENFILENAMEA);
			fn.lpstrFile = buffer;
			fn.nMaxFile = sizeof(buffer);

			int r = GetSaveFileNameA(&fn);
			if (r != 0) {
				int err = save_as_png(&loaded_image, buffer);
				if (err != 0) {
					char error_buffer[512] = { 0 };
					sprintf(error_buffer, "Could not save file %s\n", buffer);
					fprintf(stderr, "Could not save file %s\n", buffer);
					MessageBoxA(0, error_buffer, "Error", MB_ICONERROR);
				}
			}
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
		AppendMenu(app.file_menu, MF_STRING | MF_GRAYED, F_COMMAND_SAVE, L"&Save\tCtrl+S");
		AppendMenu(app.file_menu, MF_STRING | MF_GRAYED, F_COMMAND_SAVEAS, L"Save &As...");
		AppendMenu(app.file_menu, MF_STRING | MF_GRAYED, F_COMMAND_CLOSE, L"&Close\tCtrl+W");
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

int save_as_png(RawImage* image, char* out_filename) {
	char* png = (char*)calloc(4, image->width * image->height);
	glGetTextureImage(image->texture_id, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->width * image->height * 4, png);
	GLenum glerror = glGetError();
	if (glerror != 0)
		return -1;
	int err = stbi_write_png(out_filename, image->width, image->height, 4, png, image->width * 4);
	free(png);
	if (err != 0)
		return 0;
	else
		return -1;
}

int process_image(RawImage* loaded_image, char *file)
{
	// Open the file and read the metadata
	int err = lr.open_file(file);
	if (err != 0) {
		char buffer[512] = {0};
		sprintf(buffer, "Could not open file %s\n", file);
		fprintf(stderr, "Could not open file %s\n", file);
		MessageBoxA(0, buffer, "Error", MB_ICONERROR);
		return -1;
	}
	
	// Let us unpack the image
	err = lr.unpack();
	if (err != 0) {
		char buffer[512] = { 0 };
		sprintf(buffer, "Could not unpack the data in the file %s\n", file);
		fprintf(stderr, "Could not unpack the data in the file %s\n", file);
		MessageBoxA(0, buffer, "Error", MB_ICONERROR);
		return -1;
	}
	
	// Convert from imgdata.rawdata to imgdata.image:
	err = lr.raw2image();
	if (err != 0) {
		char buffer[512] = { 0 };
		sprintf(buffer, "Could not get raw data from file %s\n", file);
		fprintf(stderr, "Could not get raw data from file %s\n", file);
		MessageBoxA(0, buffer, "Error", MB_ICONERROR);
		return -1;
	}
	
	loaded_image->raw = &lr;
	loaded_image->width = lr.imgdata.sizes.width;
	loaded_image->height = lr.imgdata.sizes.height;
	loaded_image->texture_id = demosaic(loaded_image);
	//loaded_image->texture_id = load_raw(loaded_image);
	loaded_image->loaded = true;
	loaded_image->position = hm::vec2(0, 0);
	
	// Free data
	lr.recycle();
	return 0;
}

int load_ldr_image(LDRImage* image, r32 exp_time, char* file) {
	image->data = stbi_load(file, &image->width, &image->height, &image->channels, 4);
	if (!image->data) {
		char buffer[512] = { 0 };
		sprintf(buffer, "Could not load file %s", file);
		fprintf(stderr, "Could not load file %s\n", file);
		MessageBoxA(0, buffer, "Error", MB_ICONERROR);
		return -1;
	}
	image->filename = file;
	image->texture_id = create_texture(image->width, image->height, image->data);
	image->loaded = true;
	image->position = hm::vec2(0, 0);
	image->zoom = 1.0f;
	image->exposure_time = exp_time;
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
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 720, 0, 0, instance, 0);

	if (!window) {
		MessageBox(0, L"Error creating window", L"Fatal error", MB_ICONERROR);
		ExitProcess(-1);
	}

	init_immediate_quad_mode();

	app.running = true;
	MSG msg;
	while (GetMessage(&msg, window, 0, 0) && app.running) {
		switch (msg.message) {
			case WM_KEYDOWN: {
				bool ctrl_was_pressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
				switch (msg.wParam) {
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':{
						selected_ldr = msg.wParam - '0';
					} break;
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
						} else {
							if (!ldr_image[0].loaded) {
								load_ldr_image(&ldr_image[ldr_index++], 1.0f/30.0f, "C:\\dev\\\CompPhotography\\\LDR_image_sequence\\office_1.jpg");
								load_ldr_image(&ldr_image[ldr_index++], 1.0f/10.0f,"C:\\dev\\\CompPhotography\\\LDR_image_sequence\\office_2.jpg");
								load_ldr_image(&ldr_image[ldr_index++], 1.0f/3.0f,"C:\\dev\\\CompPhotography\\\LDR_image_sequence\\office_3.jpg");
								load_ldr_image(&ldr_image[ldr_index++], 1.0f/2.0f,"C:\\dev\\\CompPhotography\\\LDR_image_sequence\\office_4.jpg");
								load_ldr_image(&ldr_image[ldr_index++], 1.3f,"C:\\dev\\\CompPhotography\\\LDR_image_sequence\\office_5.jpg");
								load_ldr_image(&ldr_image[ldr_index++], 4.0f,"C:\\dev\\\CompPhotography\\\LDR_image_sequence\\office_6.jpg");
							}
						}
					}break;
					case VK_UP:    loaded_image.position.y -= 30.0f; ldr_image[selected_ldr].position.y += 30.0f; break;
					case VK_DOWN:  loaded_image.position.y += 30.0f; ldr_image[selected_ldr].position.y -= 30.0f; break;
					case VK_LEFT:  loaded_image.position.x += 30.0f; ldr_image[selected_ldr].position.x -= 30.0f; break;
					case VK_RIGHT: loaded_image.position.x -= 30.0f; ldr_image[selected_ldr].position.x += 30.0f; break;
					case VK_OEM_PLUS:  ldr_image[selected_ldr].zoom /= 1.3f; break;
					case VK_OEM_MINUS: ldr_image[selected_ldr].zoom *= 1.3f; break;
					case 'R': ldr_image[selected_ldr].zoom = 1.0f; ldr_image[selected_ldr].position = hm::vec2(0, 0);  break;
					case 'T': hdr_texture_id = ldr_to_hdr(ldr_image, ldr_index); break;
					case 'H': display_hdr = !display_hdr; break;
					default: {
						fprintf(stdout, "Key: %llu\n", msg.wParam);
					}break;
				}
			}break;
			case WM_KEYUP: {
			} break;
			default: {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}break;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (loaded_image.loaded) {
			immediate_quad(loaded_image.texture_id, 0, loaded_image.width / 4, loaded_image.height / 4, 0, hm::vec4(1, 1, 1, 1), loaded_image.position, app.window_info.width, app.window_info.height);
		}

		if (display_hdr) {
			immediate_quad(hdr_texture_id, 0, ldr_image[0].width, ldr_image[0].height, 0, hm::vec4(1, 1, 1, 1), ldr_image[0].position, app.window_info.width, app.window_info.height);
		} else {
			if (ldr_image[selected_ldr].loaded) {
				immediate_quad(ldr_image[selected_ldr].texture_id, 0,
					ldr_image[selected_ldr].width / ldr_image[selected_ldr].zoom,
					ldr_image[selected_ldr].height / ldr_image[selected_ldr].zoom,
					0,
					hm::vec4(1, 1, 1, 1), 
					ldr_image[selected_ldr].position,
					app.window_info.width, 
					app.window_info.height);
			}
		}

		SwapBuffers(app.window_info.device_context);
	}

	FreeConsole();
	ExitProcess(0);
}