#include <Windows.h>
#include <functional>
#include <list>
#include <thread>
#include <mutex>
#include "shutdown.h"
#include <string>
#include <map>

std::map<HWND, shutdown_notifications*> instances;
std::mutex m_instances;

shutdown_notifications::shutdown_notifications()
{
	set_shutdown_msg(L"Cleaning up...");
}

shutdown_notifications::~shutdown_notifications()
{
	this->quit();
	if (thread.joinable())
		thread.join();
}

void shutdown_notifications::register_shutdown_notification(const std::function<void()>& func)
{
	std::lock_guard<std::mutex> lock(m);
	callbacks.push_back(func);
}

void shutdown_notifications::register_pre_shutdown_blocking_cleanup_func(const std::function<void()>& func)
{
	std::lock_guard<std::mutex> lock(m);
	preshutdown.push_back(func);
}

void shutdown_notifications::register_sleep_notification(const std::function<void(bool)>& func)
{
	std::lock_guard<std::mutex> lock(m);
	onsleep.push_back(func);
}

void shutdown_notifications::start()
{
	thread = std::thread(&shutdown_notifications::WndMsgThrd, this);
}

void shutdown_notifications::quit()
{
	bQuit = true;
}

LRESULT CALLBACK shutdown_notifications::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	shutdown_notifications* instance = nullptr;
	{
		std::lock_guard<std::mutex> lock(m_instances);
		if (instances.find(hwnd) != instances.end())
			instance = instances[hwnd];
	}
	if (instance)
		instance->process_msg(uMsg, wParam, lParam);
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void shutdown_notifications::WndMsgThrd()
{
	WNDCLASSEX wcex = {};
	wcex.cbSize = sizeof(wcex);
	wcex.hInstance = GetModuleHandle(NULL);
	wcex.lpszClassName = L" ";
	wcex.lpfnWndProc = &shutdown_notifications::WindowProc;
	if (!RegisterClassEx(&wcex))
		return;
	hwnd = CreateWindowEx(0, wcex.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, NULL, wcex.hInstance, NULL);
	if (hwnd == NULL)
		return;
	{
		std::lock_guard<std::mutex> lock(m_instances);
		instances[hwnd] = this;
	}

	MSG msg;
	while (!bQuit) {
		if (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		Sleep(5);
	}

	{
		std::lock_guard<std::mutex> lock(m_instances);
		instances.erase(hwnd);
	}
}


void shutdown_notifications::process_msg(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//https://www.apriorit.com/dev-blog/413-win-api-shutdown-events
	switch (uMsg) {
	case WM_POWERBROADCAST:
	{
		/*
		if (wParam == PBT_APMSUSPEND)
			debug("computer is suspending..."); //aka "sleeping"
		else
			debug("computer is resuming..."); //warning: seems to be called twice.
		*/
		bool suspending = (wParam == PBT_APMSUSPEND);
		std::lock_guard<std::mutex> lock(m);
		for (auto& func : onsleep)
			func(suspending);
	}
	break;
	case WM_QUERYENDSESSION:
	{
		/*
		if (lParam == 0)
			if ((lParam & ENDSESSION_LOGOFF) == ENDSESSION_LOGOFF)
				debug("computer is logging off...");
			else
				debug("computer is shutting down = " + std::to_string(lParam));
		*/

		/*
        ShutdownBlockReasonCreate(hwnd, _T("hello world"));
		*/
		std::lock_guard<std::mutex> lock(m);
		if (preshutdown.size()) {
			ShutdownBlockReasonCreate(hwnd, reason.c_str());
			for (auto& func : preshutdown)
				func();
			ShutdownBlockReasonDestroy(hwnd);
		}

	}
	break;
	case WM_ENDSESSION:
	{
		std::lock_guard<std::mutex> lock(m);
		for (auto& func : callbacks)
			func();
	}
	break;
	}
}