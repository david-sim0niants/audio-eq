#pragma once

#include <pipewire/pipewire.h>

#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "objects.h"
#include "err.h"
#include "utils/defer.h"

namespace aeq {

class Core {
	struct RegistryWrapper {
		mutable std::mutex mutex;
		std::unordered_map<uint32_t, std::unique_ptr<Node>> nodes;
		std::unordered_map<uint32_t, std::unique_ptr<Port>> ports;
	};

	template<typename T> struct T_deleter {
		void operator()(T *);
	};
	template<typename T>
	using PW_UniquePtr = std::unique_ptr<T, T_deleter<T>>;

	struct RegistryEventUserData {
		Core *self;
	};

public:
	Core(int &argc, char **&argv);
	~Core();

	Core(const Core&) = delete;
	Core& operator=(const Core&) = delete;
	Core(Core&&) = delete;
	Core& operator=(Core&&) = delete;

	void list_nodes(std::vector<Node *>& nodes) const;
private:
	void setup_registry_events() noexcept;

	utils::Defer<void (*)()> deferred_deinit;
	PW_UniquePtr<pw_thread_loop> loop;
	PW_UniquePtr<pw_context> context;
	PW_UniquePtr<pw_core> core;
	PW_UniquePtr<pw_registry> registry;

	RegistryEventUserData reud;
	spa_hook registry_listener;

	RegistryWrapper registry_objects;

	static void on_global(void *data, uint32_t id,
			uint32_t permissions,
			const char *type,
			uint32_t version,
			const spa_dict *props);
	static void on_global_remove(void *data, uint32_t id);

	static pw_registry_events registry_events;
};

struct CoreErr : AudioEqErr {
	CoreErr(AudioEqErr&& base) : AudioEqErr(std::move(base)) {}
};

}
