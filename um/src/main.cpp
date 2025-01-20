#include "cheat/offsets/offsets.h"
#include <iostream>
#include <thread>
#include <chrono>
#include "cheat/entity.hpp"
#include "driver.h"
#include <Windows.h>
#include <TlHelp32.h>
#include "cheat.h"
#include <d3d11.h>
#include <dwmapi.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"



static bool mouseHidden = false;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
	if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param)) {
		return 0L;
	}


	if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0L;
	}


	if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN) {
		if (show_menu) {
			return 0L;
		}
	}


	return DefWindowProc(window, message, w_param, l_param);
}



const HANDLE kdriver = CreateFile(L"\\\\.\\SexyDriver", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

static DWORD get_process_id(const wchar_t* process_name) {
	DWORD process_id = 0;

	HANDLE snap_shot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (snap_shot == INVALID_HANDLE_VALUE)
		return process_id;

	PROCESSENTRY32W entry = {};
	entry.dwSize = sizeof(decltype(entry));

	if (Process32FirstW(snap_shot, &entry) == TRUE) {
		if (_wcsicmp(process_name, entry.szExeFile) == 0)
			process_id = entry.th32ProcessID;
		else {
			while (Process32NextW(snap_shot, &entry) == TRUE) {
				if (_wcsicmp(process_name, entry.szExeFile) == 0) {
					process_id = entry.th32ProcessID;
					break;
				}
			}
		}
	}

	CloseHandle(snap_shot);

	return process_id;
}

uintptr_t GetModuleBaseAddress(uintptr_t procID, const wchar_t* module)
{
	HANDLE handle;
	handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procID);

	MODULEENTRY32 mod;
	mod.dwSize = sizeof(MODULEENTRY32);
	Module32First(handle, &mod);

	do
	{
		if (!wcscmp(module, mod.szModule))
		{
			CloseHandle(handle);
			return (uintptr_t)mod.modBaseAddr;
		}
	} while (Module32Next(handle, &mod));

	CloseHandle(handle);
	return 0;
}


namespace driver {
	namespace codes {
		constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
		constexpr ULONG read = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
		constexpr ULONG write = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	}

	struct Request {
		HANDLE process_id;

		PVOID target;
		PVOID buffer;

		SIZE_T size;
		SIZE_T return_size;
	};

	bool attach_to_process(HANDLE driver_handle, const DWORD pid) {
		Request r;
		r.process_id = reinterpret_cast<HANDLE>(pid);

		return DeviceIoControl(driver_handle, codes::attach, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);
	}

	template <class T>
	T read_memory(HANDLE driver_handle, const std::uintptr_t addr) {
		T temp = {};

		Request r;
		r.target = reinterpret_cast<PVOID>(addr);
		r.buffer = &temp;
		r.size = sizeof(T);

		DeviceIoControl(driver_handle, codes::read, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);

		return temp;
	}

	template <class T>
	void write_memory(HANDLE driver_handle, const std::uintptr_t addr, const T& value) {
		Request r;
		r.target = reinterpret_cast<PVOID>(addr);
		r.buffer = (PVOID)&value;
		r.size = sizeof(T);

		DeviceIoControl(driver_handle, codes::write, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);


	}
}

void Reader::ThreadLoop()
{

	while (!client) {
		std::this_thread::sleep_for(std::chrono::milliseconds(15));


		client = GetModuleBaseAddress(reader.pid, L"client.dll");
		//std::cout << "[/] Client: " << reader.client << "\n";
		//std::cout << "[/] Client found: 0x" << std::hex << reader.client << "\n";
	}

	while (true)
	{

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		FilterPlayers();
	}
}

void Reader::FilterPlayers()
{

	playerList.clear();



	auto entityList = driver::read_memory<uintptr_t>(kdriver, reader.client + offset::dwEntityList);


	if (!entityList) {
		std::cout << "[-] Entity list was empty\n";
		return;
	}





	auto localPawn = driver::read_memory<uintptr_t>(kdriver, client + offset::dwLocalPlayerPawn);

	if (!localPawn)
		std::cout << "[-] Local player pawn was empty";

	for (int i = 0; i <= 64; ++i)
	{
		uintptr_t list_entry1 = driver::read_memory<uintptr_t>(kdriver, entityList + (8 * (i & 0x7FFF) >> 9) + 16);

		uintptr_t playerController = driver::read_memory<uintptr_t>(kdriver, list_entry1 + 120 * (i & 0x1FF));

		uint32_t playerPawn = driver::read_memory<uint32_t>(kdriver, playerController + offset::m_hPlayerPawn);

		uintptr_t list_entry2 = driver::read_memory<uintptr_t>(kdriver, entityList + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);

		uintptr_t pCSPlayerPawnPtr = driver::read_memory<uintptr_t>(kdriver, list_entry2 + 120 * (playerPawn & 0x1FF));


		int health = driver::read_memory<int>(kdriver, pCSPlayerPawnPtr + offset::m_iHealth);

		if (health <= 0 || health > 100)
			continue;

		int team = driver::read_memory<int>(kdriver, pCSPlayerPawnPtr + offset::m_iTeamNum);

		if (team == driver::read_memory<int>(kdriver, localPawn + offset::m_iTeamNum))
			continue;


		CCSPlayerPawn.pCSPlayerPawn = pCSPlayerPawnPtr;


		playerList.push_back(CCSPlayerPawn);
	}
}


Vector findClosest(const std::vector<Vector> playerPositions)
{



	Vector center_of_screen{ (float)GetSystemMetrics(0) / 2, (float)GetSystemMetrics(1) / 2, 0.0f };


	float lowestDistance = 10000;

	int index = -1;

	for (int i = 0; i < playerPositions.size(); ++i)
	{

		float distance(std::pow(playerPositions[i].x - center_of_screen.x, 2) + std::pow(playerPositions[i].y - center_of_screen.y, 2));


		if (distance < lowestDistance) {
			lowestDistance = distance;
			index = i;
		}
	}


	if (index == -1) {
		return { 0.0f, 0.0f, 0.0f };
	}


	return { playerPositions[index].x, playerPositions[index].y, 0.0f };
}



void MoveMouseToPlayer(Vector position)
{

	if (position.IsZero())
		return;


	Vector center_of_screen{ (float)GetSystemMetrics(0) / 2, (float)GetSystemMetrics(1) / 2, 0.0f };


	auto new_x = position.x - center_of_screen.x;
	auto new_y = position.y - center_of_screen.y;


	mouse_event(MOUSEEVENTF_MOVE, new_x, new_y, 0, 0);
}

void DoAimbot()
{

	auto view_matrix = driver::read_memory<view_matrix_t>(kdriver, reader.client + offset::dwViewMatrix);



	std::vector<Vector> playerPositions;


	playerPositions.clear();


	for (const auto& player : reader.playerList)
	{

		Vector playerPosition = driver::read_memory<Vector>(kdriver, player.pCSPlayerPawn + offset::m_vOldOrigin);


		Vector headPos = { playerPosition.x += 0.0, playerPosition.y += 0.0, playerPosition.z += 57.0f };


		Vector f, h;

		if (Vector::world_to_screen(view_matrix, playerPosition, f) &&
			Vector::world_to_screen(view_matrix, headPos, h))
		{
			playerPositions.push_back(h);
		}
	}
	if (GetAsyncKeyState(VK_LMENU))
	{
		auto closest_player = findClosest(playerPositions);

		MoveMouseToPlayer(closest_player);
	}
}

void RenderEsp()
{
	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	ImVec2 screenCenter(screenWidth / 2.0f, screenHeight / 2.0f);
	ImVec2 screenTop(screenCenter.x, 0.0f);


	auto view_matrix = driver::read_memory<view_matrix_t>(kdriver, reader.client + offset::dwViewMatrix);







	for (size_t i = 0; i < reader.playerList.size(); i++)
	{
		const auto& player = reader.playerList[i];

		if (player.pCSPlayerPawn == 0) {
			continue;
		}

		Vector playerPosition = driver::read_memory<Vector>(kdriver, player.pCSPlayerPawn + offset::m_vOldOrigin);



		if (playerPosition == Vector(0, 0, 0)) {
			std::cout << "Invalid";
			continue;
		}

		if (!player.pCSPlayerPawn) {
			std::cout << "Invalid player.pCSPlayerPawn pointer!" << std::endl;
		}



		Vector headPos = { playerPosition.x, playerPosition.y, playerPosition.z + 72.0f };
		Vector feetPos = { playerPosition.x, playerPosition.y, playerPosition.z };

		Vector headScreenPos, feetScreenPos;
		if (Vector::world_to_screen(view_matrix, headPos, headScreenPos) &&
			Vector::world_to_screen(view_matrix, feetPos, feetScreenPos))
		{
			float boxHeight = fabs(feetScreenPos.y - headScreenPos.y);
			float boxWidth = boxHeight / 2.0f;

			ImDrawList* drawList = ImGui::GetBackgroundDrawList();
			if (drawList)
			{

				ImVec2 topLeft(headScreenPos.x - boxWidth / 2.0f, headScreenPos.y);
				ImVec2 bottomRight(headScreenPos.x + boxWidth / 2.0f, feetScreenPos.y);
				if (toggle_box_esp)
				{
					ImU32 boxColor = IM_COL32((int)(box_esp_color[0] * 255), (int)(box_esp_color[1] * 255), (int)(box_esp_color[2] * 255), (int)(box_esp_color[3] * 255));






					if (esp_BoxType == 0)
					{
						drawList->AddRect(topLeft, bottomRight, boxColor, 0.0f, ImDrawFlags_None, 0.3f);
					}
					else if (esp_BoxType == 1)
					{
						float cornerSize = 6.0f; // Size of the corner lines

						// Top-left corner
						drawList->AddLine(topLeft, ImVec2(topLeft.x + cornerSize, topLeft.y), boxColor, 2.0f); // Horizontal
						drawList->AddLine(topLeft, ImVec2(topLeft.x, topLeft.y + cornerSize), boxColor, 2.0f); // Vertical

						// Top-right corner
						drawList->AddLine(ImVec2(bottomRight.x, topLeft.y), ImVec2(bottomRight.x - cornerSize, topLeft.y), boxColor, 2.0f); // Horizontal
						drawList->AddLine(ImVec2(bottomRight.x, topLeft.y), ImVec2(bottomRight.x, topLeft.y + cornerSize), boxColor, 2.0f); // Vertical

						// Bottom-left corner
						drawList->AddLine(ImVec2(topLeft.x, bottomRight.y), ImVec2(topLeft.x + cornerSize, bottomRight.y), boxColor, 2.0f); // Horizontal
						drawList->AddLine(ImVec2(topLeft.x, bottomRight.y), ImVec2(topLeft.x, bottomRight.y - cornerSize), boxColor, 2.0f); // Vertical

						// Bottom-right corner
						drawList->AddLine(ImVec2(bottomRight.x, bottomRight.y), ImVec2(bottomRight.x - cornerSize, bottomRight.y), boxColor, 2.0f); // Horizontal
						drawList->AddLine(ImVec2(bottomRight.x, bottomRight.y), ImVec2(bottomRight.x, bottomRight.y - cornerSize), boxColor, 2.0f); // Vertical

					}
					else if (esp_BoxType == 2)
					{

						ImU32 transparentColor = IM_COL32(255, 255, 255, 128);


						ImU32 outlineColor = IM_COL32(
							(int)(box_esp_color[0] * 255 * 0.8f),
							(int)(box_esp_color[1] * 255 * 0.8f),
							(int)(box_esp_color[2] * 255 * 0.8f),
							255
						);

						drawList->AddRectFilled(topLeft, bottomRight, transparentColor);
						drawList->AddRect(topLeft, bottomRight, outlineColor, 0.0f, ImDrawFlags_None, 2.0f); // Outline with darker color

					}


				}



				int health = driver::read_memory<int>(kdriver, player.pCSPlayerPawn + offset::m_iHealth);
				int maxHealth = driver::read_memory<int>(kdriver, player.pCSPlayerPawn + offset::m_iMaxHealth);



				if (toggle_healthbar_esp)
				{

					float healthBarWidth = 4.0f;
					float healthBarHeight = boxHeight;
					float healthBarX = headScreenPos.x - boxWidth / 2.0f - 6.0f;
					float healthBarY = headScreenPos.y;

					float healthBarFillHeight = (health / float(maxHealth)) * healthBarHeight;

					ImU32 barBackgroundColor = IM_COL32(255, 0, 0, 255);


					ImU32 barColor = IM_COL32(0, 255, 0, 255);



					drawList->AddRectFilled(ImVec2(healthBarX, healthBarY), ImVec2(healthBarX + healthBarWidth, healthBarY + healthBarHeight), barBackgroundColor);
					drawList->AddRectFilled(ImVec2(healthBarX, healthBarY + healthBarHeight - healthBarFillHeight),
						ImVec2(healthBarX + healthBarWidth, healthBarY + healthBarHeight), barColor);
				}


				if (toggle_healthtext_esp)
				{
					ImVec2 healthTextPos(headScreenPos.x + boxWidth / 2.0f + 10.0f, headScreenPos.y);
					ImGui::GetBackgroundDrawList()->AddText(healthTextPos, IM_COL32(255, 255, 255, 255), std::to_string(health).c_str());
				}








				if (toggle_snaplines_esp)
				{
					ImU32 snaplinesColor = IM_COL32((int)(snaplines_esp_color[0] * 255), (int)(snaplines_esp_color[1] * 255), (int)(snaplines_esp_color[2] * 255), (int)(snaplines_esp_color[3] * 255));
					drawList->AddLine(screenTop, ImVec2(headScreenPos.x, headScreenPos.y), snaplinesColor, 0.8f);
				}

			}
		}
	}
}




INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show) {
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = instance;
	wc.lpszClassName = L"External Overlay Class";
	if (!RegisterClassExW(&wc)) {
		MessageBoxW(nullptr, L"Window Registration Failed!", L"Error", MB_ICONERROR);
		return -1;
	}


	const HWND window = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_NOACTIVATE,
		wc.lpszClassName,
		L"External Overlay",
		WS_POPUP,
		0, 0, 1920, 1080,
		nullptr, nullptr,
		wc.hInstance,
		nullptr
	);

	if (!window) {
		MessageBoxW(nullptr, L"Window Creation Failed!", L"Error", MB_ICONERROR);
		return -1;
	}

	SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_COLORKEY);


	RECT client_area{};
	GetClientRect(window, &client_area);

	RECT window_area{};
	GetWindowRect(window, &window_area);

	POINT diff{};
	ClientToScreen(window, &diff);

	const MARGINS margins{
		window_area.left + (diff.x - window_area.left),
		window_area.top + (diff.y - window_area.top),
		client_area.right,
		client_area.bottom
	};
	DwmExtendFrameIntoClientArea(window, &margins);

	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.RefreshRate.Numerator = 60U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = window;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	constexpr D3D_FEATURE_LEVEL levels[2]{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};

	ID3D11Device* device{ nullptr };
	ID3D11DeviceContext* device_context{ nullptr };
	IDXGISwapChain* swap_chain{ nullptr };
	ID3D11RenderTargetView* render_target_view{ nullptr };
	D3D_FEATURE_LEVEL level{};

	if (FAILED(D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0U, levels, 2U, D3D11_SDK_VERSION,
		&sd, &swap_chain, &device, &level, &device_context
	))) {
		MessageBoxW(nullptr, L"DirectX Initialization Failed!", L"Error", MB_ICONERROR);
		return -1;
	}

	ID3D11Texture2D* back_buffer{ nullptr };
	if (FAILED(swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer)))) {
		MessageBoxW(nullptr, L"Failed to Get Back Buffer!", L"Error", MB_ICONERROR);
		return -1;
	}

	if (FAILED(device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view))) {
		MessageBoxW(nullptr, L"Failed to Create Render Target View!", L"Error", MB_ICONERROR);
		return -1;
	}
	back_buffer->Release();

	ShowWindow(window, cmd_show);
	UpdateWindow(window);


	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(device, device_context);



	bool running = true;






	while (running) {
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) {
				running = false;
			}
		}

		if (!running) {
			break;
		}



		if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
			show_menu = !show_menu;
			Sleep(200);
		}


		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize({ 550, 440 });
		if (show_menu)
		{
			static int tabb = 0;
			ImGui::Begin("NAMELESS CS2 EXTERNAL", &show_menu, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
			{
				if (ImGui::BeginChild(1, ImVec2(160, 340), true)) {
					ImGui::SeparatorText("Visual");
					if (ImGui::Button("Players", ImVec2(130, 25))) {
						tabb = 0;
					}
					if (ImGui::Button("Objects", ImVec2(130, 25))) {
						tabb = 1;
					}
					ImGui::SeparatorText("Combat");
					if (ImGui::Button("Aimbot", ImVec2(130, 25))) {
						tabb = 2;
					}
					if (ImGui::Button("Triggerbot", ImVec2(130, 25))) {
						tabb = 3;
					}
					ImGui::SeparatorText("Misc");
					if (ImGui::Button("Extra", ImVec2(130, 25))) {
						tabb = 4;
					}
					ImGui::SeparatorText("Account");
					//ImGui::Text("Username: %s", keyauth_username.c_str());
					//ImGui::Text("Expiry: %d days", keyauth_expiry_days);
				}ImGui::EndChild();
				ImGui::SameLine();

				if (ImGui::BeginChild(2, ImVec2(320, 160), true)) {
					if (tabb == 0) {
						ImGui::SeparatorText("Player ESP");
						ImGui::Checkbox("Toggle", &toggle_esp);
						ImGui::Checkbox("Box", &toggle_box_esp);
						ImGui::SameLine();
						ImGui::ColorEdit4("", box_esp_color, ImGuiColorEditFlags_NoInputs);
						ImGui::Combo("Box Type", &esp_BoxType, BoxTypesNames, IM_ARRAYSIZE(BoxTypesNames));

						ImGui::Checkbox("Toggle Healtht ESP", &toggle_healthtext_esp);
						ImGui::Checkbox("Toggle Healthbar ESP", &toggle_healthbar_esp);
						ImGui::Checkbox("Snaplines", &toggle_snaplines_esp);
						ImGui::SameLine();
						ImGui::ColorEdit4("##snaplines", snaplines_esp_color, ImGuiColorEditFlags_NoInputs);
					}
					if (tabb == 1) {
						ImGui::Text("Soon");
					}
					if (tabb == 2) {
						ImGui::SeparatorText("Aimbot");
						ImGui::Checkbox("Toggle aimbot", &toggle_aimbot);
					}
					if (tabb == 3) {
						ImGui::Text("soon");
					}
					if (tabb == 4) {
						ImGui::Text("soon");
						if (ImGui::Button("Close program"))
						{
							running = false;
						}
					}

				} ImGui::EndChild();

			}ImGui::End();
		}

		if (toggle_aimbot)
			DoAimbot();

		if (toggle_esp)
			RenderEsp();




		if (!show_menu && !mouseHidden) {
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
			ShowCursor(FALSE);
			mouseHidden = true;
		}
		else if (show_menu && mouseHidden) {
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
			ShowCursor(TRUE);
			mouseHidden = false;
		}



		device_context->OMSetRenderTargets(1, &render_target_view, nullptr);
		const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };
		device_context->ClearRenderTargetView(render_target_view, clear_color);
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		swap_chain->Present(1, 0);
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (swap_chain) swap_chain->Release();
	if (device_context) device_context->Release();
	if (device) device->Release();
	if (render_target_view) render_target_view->Release();

	DestroyWindow(window);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}


int main() {
	const DWORD pid = get_process_id(L"cs2.exe");


	reader.pid = get_process_id(L"cs2.exe");


	if (reader.pid == 0) {
		std::cout << "Failed to find notepad\n";
		std::cin.get();
		return 1;


	}




	if (kdriver == INVALID_HANDLE_VALUE) {
		std::cout << "Failed to create our driver handle.\n";
		std::cin.get();
		return 1;
	}

	std::cout << "Attachment successful. 111\n";

	std::thread ReadThread(&Reader::ThreadLoop, &reader);
	ReadThread.detach();

	Sleep(100);
	std::cout << "Attachment successful. 333\n";



	if (driver::attach_to_process(kdriver, reader.pid) == true) {
		std::cout << "Attachment successful.\n";

		std::cout << "Client value: " << reader.client << "\n";

		if (reader.client != 0) {



			return WinMain(GetModuleHandle(nullptr), nullptr, nullptr, SW_SHOW);
		}
	}


	CloseHandle(kdriver);

	std::cin.get();

	return 0;
}