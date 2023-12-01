#include "gui.h"
#include "macro.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include <thread>
#include <iostream>

#include <fstream>
using namespace no_recoil;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

// Send inputs to programs

void move(float SMOOTHING_FACTOR, int DRAG_RATE_X, int DRAG_RATE_Y) {
	// Set local scope smoothing
	float smoothingX = SMOOTHING_FACTOR;
	float smoothingY = SMOOTHING_FACTOR;
	// If the drag rate is 0 then the smoothing should also be zero 
	if (DRAG_RATE_X == 0) {
		smoothingX = 0;
	}
	if (DRAG_RATE_Y == 0) {
		smoothingY = 0;
	}
	// If the LMB is pressed, we get the current position of the cursor and then add to it depending on the smoothing, and drag rate
	if (GetAsyncKeyState(VK_LBUTTON)) {
		POINT currentPos;
		GetCursorPos(&currentPos);
		// Seeing if we have a smoothing factor; if theres an error, it wont move
		if (SMOOTHING_FACTOR >= 0) {
			// If we have a delay, wait for up to one second
			if (pull_delay > 0) {
				Sleep(pull_delay);
			}
			// Higher level/eaiser way to set cursor position
			if (no_recoil::within_program == false) {
				// Taking the drag rate and then multiplying it by a smootinh factor, then multiplying it by a user selected multiplier
				SetCursorPos((currentPos.x + ((int)std::ceil(((DRAG_RATE_X * smoothingX) * no_recoil::multiplier)))), (currentPos.y + ((int)std::ceil((DRAG_RATE_Y * smoothingY) * no_recoil::multiplier))));
			}
			else if (no_recoil::within_program == true) {
				//int newX = currentPos.x + static_cast<int>(std::ceil(DRAG_RATE_X * smoothingX));
				// newY = currentPos.y + static_cast<int>(std::ceil(DRAG_RATE_Y * smoothingY));
				// The mouse_event function is absolute position based so instead of using our relative position, we just directly send commands to mouse the mouse down at a pace of deltaX and deltaY
				int deltaX = static_cast<int>(std::ceil((DRAG_RATE_X * smoothingX) * no_recoil::multiplier));
				int deltaY = static_cast<int>(std::ceil((DRAG_RATE_Y * smoothingY) * no_recoil::multiplier));
				mouse_event(MOUSEEVENTF_MOVE, deltaX, deltaY, NULL, NULL);
			}
			Sleep(20);
		}
	}

} 

// Gets the center of the screen for the "smart recoil feature"
POINT GetScreenCenter() {
	RECT screenRect;
	GetWindowRect(GetDesktopWindow(), &screenRect);

	POINT center;
	center.x = (screenRect.left + screenRect.right) / 2;
	center.y = (screenRect.top + screenRect.bottom) / 2;

	return center;
}


POINT lastCursorPosition;
POINT originalCursorPosition;

void smartRecoil() {
	if (no_recoil::within_program == false) {
		GetCursorPos(&lastCursorPosition);

		// Save the original cursor position
		originalCursorPosition = lastCursorPosition;

		Sleep(200);
		POINT currentCursorPosition;
		GetCursorPos(&currentCursorPosition);
		if (GetAsyncKeyState(VK_LBUTTON)) {
			// Restore the cursor to its original position
			int differenceX = originalCursorPosition.x - currentCursorPosition.x;
			int differenceY = originalCursorPosition.y - currentCursorPosition.y;
			SetCursorPos(currentCursorPosition.x + differenceX, currentCursorPosition.y + differenceY);
		}
	}
	else if (no_recoil::within_program == true) {
		if (GetAsyncKeyState(VK_LBUTTON)) {
			POINT screenCenter = GetScreenCenter();
			POINT currentPos;
			GetCursorPos(&currentPos);
			int deltaX = static_cast<int>(std::ceil((screenCenter.x - currentPos.x) * no_recoil::multiplier));
			int deltaY = static_cast<int>(std::ceil((screenCenter.y - currentPos.y) * no_recoil::multiplier));

			// Call mouse_event to move the mouse
			if (abs(deltaX) > 1 || abs(deltaY) > 1) {
				// Move the mouse back to the center to prevent continuous rotation
				mouse_event(MOUSEEVENTF_MOVE, screenCenter.x - currentPos.x, screenCenter.y - currentPos.y, 0, 0);
			}

		}
	}
}

void gui::Render() noexcept
{
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT });
	ImGui::Begin(
		"Black Dice Macros",
		&isRunning
	);

	ImGuiStyle* style = &ImGui::GetStyle();

	style->WindowPadding = ImVec2(15, 15);
	style->WindowRounding = 5.0f;
	style->FramePadding = ImVec2(5, 5);
	style->FrameRounding = 4.0f;
	style->ItemSpacing = ImVec2(12, 8);
	style->ItemInnerSpacing = ImVec2(8, 6);
	style->IndentSpacing = 25.0f;
	style->ScrollbarSize = 15.0f;
	style->ScrollbarRounding = 9.0f;
	style->GrabMinSize = 5.0f;
	style->GrabRounding = 3.0f;

	style->Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
	style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.f);
	style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
	style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
	style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
	style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
	style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
	style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
	style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
	style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
	style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
	style->Colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
	style->Colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
	style->Colors[ImGuiCol_TabActive] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);

	if (macros::bhop && macros::isActive) {
		if (GetAsyncKeyState(bhop::HotKey)) {
			INPUT input;
			Sleep(5000);
			input.type = INPUT_KEYBOARD;
			input.ki.wScan = 0;
			input.ki.wVk = VK_SPACE; 
			input.ki.dwFlags = 0;   

			SendInput(1, &input, sizeof(INPUT));

		
			Sleep(50);

			input.ki.dwFlags = KEYEVENTF_KEYUP; 
			SendInput(1, &input, sizeof(INPUT));

		}
	}
	if (macros::turbo_crouch && macros::isActive) {
		if (GetAsyncKeyState(crouch::HotKey)) {
			INPUT input;
			input.type = INPUT_KEYBOARD;
			input.ki.wVk = 'C'; 
			input.ki.dwFlags = 0;   

			SendInput(1, &input, sizeof(INPUT));

			
			Sleep(1000);

			input.ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(1, &input, sizeof(INPUT));
		}
	}
	if (macros::rapid_fire && macros::isActive) {
		if (GetAsyncKeyState(VK_LBUTTON)) {
			INPUT input;
			input.type = INPUT_MOUSE;
			input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; 

			SendInput(1, &input, sizeof(INPUT));

			Sleep(50);

			input.mi.dwFlags = MOUSEEVENTF_LEFTUP; 
			SendInput(1, &input, sizeof(INPUT));
		}
	}

	if (GetAsyncKeyState(VK_F2)) {
		no_recoil::active = false;
	}
	else if (GetAsyncKeyState(VK_F1)) {
		no_recoil::active = true;
	}
	else if (GetAsyncKeyState(VK_F4)) {
		no_recoil::strengthY += 1;
		Sleep(100);
	}
	else if (GetAsyncKeyState(VK_F5)) {
		no_recoil::strengthY -= 1;
		Sleep(100);
	}


	
		
		if (smart_recoil && no_recoil::active) {
			smartRecoil();
		}
		else if (no_recoil::active) {
			move(no_recoil::smoothing, no_recoil::strengthX, no_recoil::strengthY);
		}
		else {
			//no_recoil::smart_recoil = false;
		}
	
	

	if (GetAsyncKeyState(VK_F3)) {
		gui::isRunning = false;
	}

	if (smart_recoil) {
		no_recoil::strengthX = 0;
		no_recoil::strengthY = 0;
		no_recoil::smoothing = 1.f;
		no_recoil::pull_delay = 0.f;
	}

	if (preset::CurrentPreset == PISTOL) {
		no_recoil::strengthX = 0;
		no_recoil::strengthY = 6;
		no_recoil::smoothing = 1.f;
		no_recoil::pull_delay = 0.f;
		no_recoil::multiplier = 1.f;
	}
	else if (preset::CurrentPreset == DMR) {
		no_recoil::strengthX = -1;
		no_recoil::strengthY = 16;
		no_recoil::smoothing = 1.f;
		no_recoil::pull_delay = 0.f;
		no_recoil::multiplier = 1.f;
	}
	else if (preset::CurrentPreset == RIFLE) {
		no_recoil::strengthX = -1;
		no_recoil::strengthY = 11;
		no_recoil::smoothing = 1.f;
		no_recoil::pull_delay = 0.f;
		no_recoil::multiplier = 1.f;
	}

	ImGui::Text("Version:");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0, 1, 0, 1), "1.1.2");

	if (ImGui::BeginTabBar("Cheats")) {
		if (ImGui::BeginTabItem("Anti-Recoil")) {
			int min_valueY = 0;
			int min_valueX = -20;
			if (strengthY > 25) {
				strengthY = 25;
			}
			else if (strengthY < 0) {
				strengthY = 0;
			}
			if (strengthX < -20) {
				strengthX = -20;
			}
			else if (strengthX > 20) {
				strengthX = 20;
			};
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "F1");
			ImGui::SameLine();
			ImGui::Text("to enable the macro");
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "F2");
			ImGui::SameLine();
			ImGui::Text("to disable the macro");
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "F3");
			ImGui::SameLine();
			ImGui::Text("to close the window");
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "F4");
			ImGui::SameLine();
			ImGui::Text("to increase the vertical strength");
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "F5");
			ImGui::SameLine();
			ImGui::Text("to decrease the vertical strength");
			ImGui::SliderInt("Vertical Strength", &no_recoil::strengthY, min_valueY, 25);
			ImGui::InputInt("Vertical Manual", &no_recoil::strengthY);
			ImGui::Spacing();
			ImGui::SliderInt("Horizontal Strength", &no_recoil::strengthX, min_valueX, 10);
			ImGui::InputInt("Horizontal Manual", &no_recoil::strengthX);
			ImGui::Spacing();
			ImGui::SliderFloat("Smoothing", &no_recoil::smoothing, 0.f, 1.f);
			ImGui::Spacing(); 
			ImGui::SliderInt("Delay (milliseconds)", &no_recoil::pull_delay, 0.f, 100.f);
			ImGui::Spacing();
			ImGui::SliderFloat("Multiplier", &no_recoil::multiplier, 1.f, 2.0f, "%0.1f", ImGuiSliderFlags_NoInput);
			ImGui::Spacing();
			ImGui::NextColumn();
			if (ImGui::Combo("Preset", &preset::CurrentPreset, "DEFAULT\0PISTOL\0DMR\0RIFLE")) {
				preset::SetPreset(preset::CurrentPreset);
			}
			ImGui::Text("Current Preset:");
			ImGui::SameLine();
			if (preset::CurrentPreset == DEFAULT) {
				ImGui::TextColored(ImVec4(0, 0, 1, 1), "DEFAULT");
			}
			else if (preset::CurrentPreset == PISTOL) {
				ImGui::TextColored(ImVec4(0, 0, 1, 1), "PISTOL");
			}
			else if (preset::CurrentPreset == DMR) {
				ImGui::TextColored(ImVec4(0, 0, 1, 1), "DMR");
			}
			else if (preset::CurrentPreset == RIFLE) {
				ImGui::TextColored(ImVec4(0, 0, 1, 1), "RIFLE");
			}

			ImGui::Separator();
			ImGui::Checkbox("Active", &no_recoil::active);
			ImGui::Checkbox("Smart Anti Recoil (unusable)", &smart_recoil);
			ImGui::Checkbox("In Game", &within_program);
			ImGui::Spacing();
			if (ImGui::Button("Reset")) {
				no_recoil::active = false;
				no_recoil::strengthX = 0;
				no_recoil::strengthY = 0;
				no_recoil::smoothing = 1.f;
				no_recoil::pull_delay = 0.f;
				no_recoil::multiplier = 1.f;
			}
			ImGui::Spacing();
			if (no_recoil::active) {
				ImGui::Text("Anti-Recoil Status:");
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0, 1, 0, 1), "Active");
			}
			else {
				ImGui::Text("Anti-Recoil Status:");
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Not Active");
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Macros")) {
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "Macros are game dependent");
			ImGui::Checkbox("Bhop", &macros::bhop);
			if (ImGui::Combo("Bhop Key", &bhop::HotKey, "LALT\0LBUTTON\0RBUTTON\0XBUTTON1\0XBUTTON2\0CAPITAL\0SHIFT\0CONTROL")) {
				bhop::SetHotKey(bhop::HotKey);
			}
			ImGui::Spacing();
			ImGui::Checkbox("Turbo Crouch", &macros::turbo_crouch);
			if (ImGui::Combo("Crouch Key", &crouch::HotKey, "LALT\0LBUTTON\0RBUTTON\0XBUTTON1\0XBUTTON2\0CAPITAL\0SHIFT\0CONTROL")) {
				crouch::SetHotKey(crouch::HotKey);
			}
			ImGui::Spacing();
			ImGui::Checkbox("Rapid Fire (happens on fire)", &macros::rapid_fire);
			ImGui::Spacing();
			ImGui::EndTabItem();
			ImGui::Separator();
			ImGui::Checkbox("Master Switch", &macros::isActive);
			ImGui::Spacing();
			if (ImGui::Button("Reset")) {
				macros::bhop = false;
				macros::turbo_crouch = false;
				macros::rapid_fire = false;
			}
			ImGui::Spacing();
			if (macros::isActive) {
				ImGui::Text("Macro Status:");
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0, 1, 0, 1), "Active");
			}
			else {
				ImGui::Text("Macro Status:");
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "Not Active");
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}

