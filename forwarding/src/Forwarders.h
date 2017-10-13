#pragma once 
#include <client.h>
namespace forwarding {
	using SafeAutoResetEvent = std::shared_ptr<void>;
	inline SafeAutoResetEvent MakeAutoResetEvent()
	{
		return std::shared_ptr<void>(CreateEvent(nullptr, FALSE, FALSE, nullptr), [](void* ev) {
			if (ev) {
				CloseHandle(ev);
			}
		});
	}
}
