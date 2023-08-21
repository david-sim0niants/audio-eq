#pragma once

#include <pipewire/pipewire.h>

#include <vector>
#include <memory>
#include <unordered_map>

#include "objects.h"
#include "filter.h"
#include "err.h"
#include "utils/defer.h"

namespace aeq {

/* Abstraction over pipewire core, context and thread loop objects.
 * Deals with collecting and providing necessary pipewire objects,
 * as well as creating and deleting new ones.
 * This class is not intended to be movable/copiable. */
class Core {
	struct LinkInfo {
		ID_Ptr<Port> i_port;
		ID_Ptr<Port> o_port;
	};

	struct RegistryObjects {
		std::unordered_map<uint32_t, std::unique_ptr<Node>> nodes;
		std::unordered_map<uint32_t, std::unique_ptr<Port>> ports;

		std::unordered_map<uint32_t, LinkInfo> links;

		std::unordered_map<uint32_t, std::vector<Port *>>   nodeless_ports;
		std::unordered_map<uint32_t, std::vector<LinkInfo>> portless_links;
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

	/* Lock the thread loop. */
	void lock_loop() const;
	/* Unlock the thread loop. */
	void unlock_loop() const;
	/* Wait the thread loop. */
	void wait_loop() const;

	/* List all currently available nodes. */
	void list_nodes(std::vector<Node *>& nodes) const;

	/* Link two ports. Returns a link id. */
	uint32_t link_ports(Port& o_port, Port& i_port);
	/* Unlink two ports given the link id. */
	void unlink_ports(uint32_t link_id);

	void init_filter(Filter& filter, const char *name);
private:
	void setup_registry_events() noexcept;

	void wrap_node(uint32_t id, const spa_dict *props);
	void wrap_port(uint32_t id, const spa_dict *props);
	void wrap_link(uint32_t id, const spa_dict *props);

	bool try_unwrap_node(uint32_t id);
	bool try_unwrap_port(uint32_t id);
	bool try_unwrap_link(uint32_t id);

	void do_roundtrip();

	utils::Defer<void (*)()> deferred_deinit;
	PW_UniquePtr<pw_thread_loop> loop;
	PW_UniquePtr<pw_context> context;
	PW_UniquePtr<pw_core> core;
	PW_UniquePtr<pw_registry> registry;
	PW_UniquePtr<pw_main_loop> main_loop;

	RegistryEventUserData reud;
	spa_hook registry_listener;

	RegistryObjects registry_objects;

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
