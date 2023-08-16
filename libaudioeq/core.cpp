#include <audioeq/core.h>

#include <algorithm>


namespace aeq {


Core::Core(int &argc, char **&argv)
{
	pw_init(&argc, &argv);
	utils::Defer deferred_deinit {+pw_deinit};

	PW_UniquePtr<pw_thread_loop> loop {
		pw_thread_loop_new("core-loop", nullptr) };
	if (loop == nullptr)
		throw CoreErr(AudioEqErr("Error: failed to create new thread loop.", errno));

	PW_UniquePtr<pw_context> context {
		pw_context_new(pw_thread_loop_get_loop(loop.get()), nullptr, 0) };

	PW_UniquePtr<pw_core> core { pw_context_connect(context.get(), nullptr, 0) };
	if (core == nullptr)
		throw CoreErr(AudioEqErr("Error: failed connecting to pipewire core", errno));

	PW_UniquePtr<pw_registry> registry {
		pw_core_get_registry(core.get(), PW_VERSION_REGISTRY, 0) };

	int ret = pw_thread_loop_start(loop.get());
	if (ret)
		throw CoreErr(AudioEqErr("Error: failed to start a loop.", errno));

	this->deferred_deinit = std::move(deferred_deinit);
	this->loop = std::move(loop);
	this->context = std::move(context);
	this->core = std::move(core);
	this->registry = std::move(registry);

	setup_registry_events();
	deferred_deinit.cancel();
}


template<>
void Core::T_deleter<pw_registry>::operator()(pw_registry *registry)
{
	pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry));
}

template<>
void Core::T_deleter<pw_core>::operator()(pw_core *core)
{
	pw_core_disconnect(core);
}

template<>
void Core::T_deleter<pw_context>::operator()(pw_context *context)
{
	pw_context_destroy(context);
}

template<>
void Core::T_deleter<pw_thread_loop>::operator()(pw_thread_loop *loop)
{
	pw_thread_loop_destroy(loop);
}

Core::~Core() = default;


void Core::list_nodes(std::vector<Node *>& nodes) const
{
	std::transform(registry_objects.nodes.begin(),
			registry_objects.nodes.end(),
			std::back_inserter(nodes),
			[](const auto& id_node) { return id_node.second.get(); });
}


void Core::setup_registry_events() noexcept
{
	reud.self = this;
	spa_zero(registry_listener);
	pw_registry_add_listener(registry.get(), &registry_listener,
			&registry_events, &reud);
}


void Core::on_global(void *data, uint32_t id,
		uint32_t permissions,
		const char *type,
		uint32_t version,
		const spa_dict *props)
{
	RegistryEventUserData& udata = *static_cast<RegistryEventUserData *>(data);

	pw_proxy *object = pw_core_find_proxy(udata.self->core.get(), id);
	std::string str_type {type};

	if (str_type == PW_TYPE_INTERFACE_Node) {
		const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
		if (name == nullptr)
			name = "";
		const char *description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
		if (description == nullptr)
			description = "";
		udata.self->registry_objects.nodes[id] =
			std::unique_ptr<Node>(new Node(Object(id), name, description));
	} else if (str_type == PW_TYPE_INTERFACE_Port) {
		const char *name = spa_dict_lookup(props, PW_KEY_PORT_NAME);
		if (name == nullptr)
			name = "";
		const char *direction = spa_dict_lookup(props, PW_KEY_PORT_DIRECTION);
		printf("%s\n", direction);
		if (direction == nullptr)
			direction = "out";
		std::string str_direction = direction;
		PortDirection port_dir = str_direction == "in" ?
					  PortDirection::Input : PortDirection::Output;
		udata.self->registry_objects.ports[id] =
			std::unique_ptr<Port>(new Port(Object(id), , port_dir));
	}
}


void Core::on_global_remove(void *data, uint32_t id)
{
}


pw_registry_events Core::registry_events = {
	.version = PW_VERSION_REGISTRY,
	.global = on_global,
	.global_remove = on_global_remove,
};

}
