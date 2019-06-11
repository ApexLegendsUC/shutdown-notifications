#pragma once

class shutdown_notifications {
public:
	shutdown_notifications();
	~shutdown_notifications();

	void register_shutdown_notification(const std::function<void()>& func);
	void register_pre_shutdown_blocking_cleanup_func(const std::function<void()>& func);
	void register_sleep_notification(const std::function<void(bool)>& func);

	void set_shutdown_msg(const std::wstring& msg) {
		this->reason = msg;
	};

	void start();
	void quit();
private:
	std::wstring reason;
	LRESULT static CALLBACK WindowProc(
		_In_ HWND   hwnd,
		_In_ UINT   uMsg,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam);
	void WndMsgThrd();
	void process_msg(UINT uMsg, WPARAM wParam, LPARAM lParam);
	bool bQuit = false;
	HWND hwnd = NULL;
	std::thread thread;
	std::mutex m;
	std::list < std::function<void()> > callbacks, preshutdown;
	std::list < std::function<void(bool)> > onsleep;

};