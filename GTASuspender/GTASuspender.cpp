#include <iostream>
#include <mutex>
#include <string>
#include <chrono>
#include <future>
#include <thread>
#include <assert.h>

#include <boost/dll/import.hpp>
#include <boost/dll/shared_library.hpp>

#include <wil/result.h>

#include <windows.h>
#include <tlhelp32.h>

#include "console.h"

using namespace std::literals::chrono_literals;

typedef LONG(NTAPI NtSuspendProcess_t)(HANDLE);
typedef LONG(NTAPI NtResumeProcess_t)(HANDLE);

const auto NtSuspendProcess = boost::dll::import<NtSuspendProcess_t>("ntdll.dll", "NtSuspendProcess",
	boost::dll::load_mode::search_system_folders);
const auto NtResumeProcess = boost::dll::import<NtResumeProcess_t>("ntdll.dll", "NtResumeProcess",
	boost::dll::load_mode::search_system_folders);

std::mutex processMutex;
std::mutex usageMutex;
std::mutex suspendLoopMutex;

#define HOTKEY_OFFSCREEN_ID 1
#define HOTKEY_SUSPEND_ID 2
#define HOTKEY_SUSPENDLOOP_ID 3

#define NT_SUCCESS(Status) (((int32_t)(Status)) >= 0)
#define NAME_OF(s) #s
#define NAME_OFW(s) L#s

void toggle_offscreen() {
	const wil::unique_hwnd hwnd(FindWindow(L"grcWindow", L"Grand Theft Auto V"));

	if (hwnd == nullptr) {
		console::print_error("GTA5 window not found (FindWindow returned 0x%p)", hwnd.get());
		return;
	}

	console::print_info("Found GTA5 window (handle: 0x%p)", hwnd.get());

	const int width = GetSystemMetrics(SM_CXSCREEN);
	const int height = GetSystemMetrics(SM_CYSCREEN);
	RECT rect;

	bool ret = GetWindowRect(hwnd.get(), &rect);

	if (!ret) {
		console::print_error("Failed to get window rectangle (GetWindowRect returned %s)", ret ? "true" : "false");
		return;
	}

	const int winWidth = rect.right - rect.left;
	const int winHeight = rect.bottom - rect.top;
	const int midLeft = (width - winWidth) / 2;
	const int midTop = (height - winHeight) / 2;
	const bool mid = rect.left == midLeft && rect.top == midTop;
	const int x = mid ? width : midLeft;
	const int y = mid ? height : midTop;

	console::print_debug("[DEBUG] Window width: %i", winWidth);
	console::print_debug("[DEBUG] Window height: %i", winHeight);
	console::print_debug("[DEBUG] Centered screen position left: %i", midLeft);
	console::print_debug("[DEBUG] Centered screen position top: %i", midTop);
	console::print_debug("[DEBUG] Is currently in the middle of the screen?: %s", mid ? "true" : "false");
	console::print_debug("[DEBUG] X: %i", x);
	console::print_debug("[DEBUG] Y: %i", y);

	ret = MoveWindow(hwnd.get(), x, y, winWidth, winHeight, true);

	if (!ret) {
		console::print_error("Failed to move window (MoveWindow returned %s)", ret ? "true" : "false");
	}

	console::print_info("New window position x: %i, y: %i, w: %i, h: %i", x, y, winWidth, winHeight);
}

bool is_gta_running() {
	const wil::unique_handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL));

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot.get(), &entry)) {
		while (Process32Next(snapshot.get(), &entry)) {
			if (!wcscmp(entry.szExeFile, L"GTA5.exe")) {
				return true;
			}
		}
	}

	return false;
}

bool set_gta_state(bool suspend) {
	const wil::unique_handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL));

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot.get(), &entry)) {
		while (Process32Next(snapshot.get(), &entry)) {
			if (!wcscmp(entry.szExeFile, L"GTA5.exe")) {
				const wil::unique_process_handle process(OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID));

				console::print_info("Found %ls (pid: %li, flags: %li, handle: 0x%p)\n", entry.szExeFile,
					entry.th32ProcessID, entry.dwFlags, process.get());

				if (process.get() == nullptr) {
					break;
				}

				if (suspend) {
					console::print_info("Attempting to suspend GTA5");

					const LONG ret = NtSuspendProcess(process.get());
					if (!NT_SUCCESS(ret)) {
						console::print_error("Failed to suspend GTA5 process (NtSuspendProcess returned 0x%lX)", ret);
						return false;
					}
				}
				else {
					console::print_info("Attempting to resume GTA5");

					const LONG ret = NtResumeProcess(process.get());
					if (!NT_SUCCESS(ret)) {
						console::print_error("Failed to resume GTA5 process (NtResumeProcess returned 0x%lX)", ret);
						return false;
					}
				}

				return true;
			}
		}
	}

	return false;
}

inline bool suspend_gta() {
	return set_gta_state(true);
}

inline bool resume_gta() {
	return set_gta_state(false);
}

void suspend_gta_for(std::chrono::milliseconds duration) {
	if (suspend_gta()) {
		console::print_info("Suspending GTA5 for %llu seconds",
			std::chrono::duration_cast<std::chrono::seconds>(duration).count());
		Sleep(duration.count());
		resume_gta();
	}
}

std::string vk_to_string(UCHAR virtualKey) {
	UINT scanCode = MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);
	CHAR szName[128];
	int result;

	switch (virtualKey) {
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN:
	case VK_RCONTROL:
	case VK_RMENU:
	case VK_LWIN:
	case VK_RWIN:
	case VK_APPS:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_END:
	case VK_HOME:
	case VK_INSERT:
	case VK_DELETE:
	case VK_DIVIDE:
	case VK_NUMLOCK:
		scanCode |= KF_EXTENDED;
	default:
		result = GetKeyNameTextA(scanCode << 16, szName, 128);
	}

	if (result == 0) {
		throw std::system_error(std::error_code(GetLastError(), std::system_category()), "WinAPI Error occured.");
	}

	return szName;
}

std::string get_hotkey_string(const UINT modifiers, const UINT vk) {
	std::string str("");

	if (modifiers & MOD_ALT) {
		str += vk_to_string(VK_MENU);
	}

	if (modifiers & MOD_CONTROL) {
		if (modifiers & MOD_ALT) {
			str += " + ";
		}
		str += vk_to_string(VK_CONTROL);
	}

	if (modifiers & MOD_SHIFT) {
		if (modifiers & (MOD_ALT | MOD_CONTROL)) {
			str += " + ";
		}
		str += vk_to_string(VK_SHIFT);
	}

	if (modifiers & (MOD_ALT | MOD_CONTROL | MOD_SHIFT)) {
		str += " + ";
	}

	str += vk_to_string(vk);

	return str;
}

void register_and_check_hotkey(const UINT id, const std::string& name, const UINT modifiers, const UINT vk) {
	assert(id >= 1);
	assert(modifiers);
	assert(vk);

	const std::string combinationString = get_hotkey_string(modifiers, vk);
	const bool result = RegisterHotKey(nullptr, id, modifiers, vk);

	if (result) {
		console::print_info("Registered hotkey \"%s\" (%s)", name, combinationString);
		return;
	}

	console::print_error("Failed to register hotkey id %d (%s)", id, combinationString);

	console::print_info("Press any key to exit");

	getc(stdin);

	exit(0);
}

inline std::chrono::time_point<std::chrono::steady_clock> get_finish_time() {
	return std::chrono::high_resolution_clock::now() + 10s;
}

void suspendloop_threadfunc(const std::future<void> futureObj) {
	std::lock_guard<std::mutex> usageLock(usageMutex);
	auto finishTime = get_finish_time();
	volatile bool suspended = false;

	while (futureObj.wait_for(1s) == std::future_status::timeout) {
		if (suspended && std::chrono::high_resolution_clock::now() < finishTime) {
			continue;
		}

		if (suspended && !resume_gta()) {
			throw std::exception("Failed to resume GTA5");
		}

		suspended = false;

		if (futureObj.wait_for(10s) == std::future_status::ready) {
			return;
		}

		finishTime = get_finish_time();

		if (!suspended && suspend_gta()) {
			suspended = true;
		}
		else {
			throw std::exception("Failed to suspend GTA5");
		}
	}
}

int __cdecl wmain() {
	/* F1-F8 is used by GTA */

	const UINT modifiers = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;

	register_and_check_hotkey(HOTKEY_OFFSCREEN_ID, "Toggle off screen", modifiers, VK_F9);
	register_and_check_hotkey(HOTKEY_SUSPEND_ID, "Suspend once", modifiers, VK_F10);
	register_and_check_hotkey(HOTKEY_SUSPENDLOOP_ID, "Suspend loop", modifiers, VK_F11);

	std::thread* thread = nullptr;
	std::promise<void> stopSignal;
	std::future<void> futureObj = stopSignal.get_future();
	std::mutex secondPassMutex;

	MSG msg;

	while (GetMessage(&msg, nullptr, NULL, NULL)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (msg.message == WM_HOTKEY) {
			if (msg.wParam == HOTKEY_OFFSCREEN_ID) {
				toggle_offscreen();
			}
			else if (msg.wParam == HOTKEY_SUSPEND_ID || msg.wParam == HOTKEY_SUSPENDLOOP_ID) {
				if (!is_gta_running()) {
					console::print_error("GTA5 isn't running");
					continue;
				}

				if (thread != nullptr) {
					std::lock_guard<std::mutex> lock(secondPassMutex);
					stopSignal.set_value();
					thread->join();
					delete thread;
					thread = nullptr;
					continue;
				}

				std::lock_guard<std::mutex> usageLock(usageMutex);

				if (msg.wParam == HOTKEY_SUSPEND_ID) {
					suspend_gta_for(8000ms);
				}
				else {
					stopSignal = std::promise<void>();
					futureObj = stopSignal.get_future();
					thread = new std::thread(&suspendloop_threadfunc, std::move(futureObj));
				}
			}
		}
	}

	UnregisterHotKey(nullptr, HOTKEY_OFFSCREEN_ID);
	UnregisterHotKey(nullptr, HOTKEY_SUSPEND_ID);
	UnregisterHotKey(nullptr, HOTKEY_SUSPENDLOOP_ID);
	return 0;
}
