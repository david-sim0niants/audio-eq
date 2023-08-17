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
	printf("%s %u\n", type, id);
	RegistryEventUserData& reud = *static_cast<RegistryEventUserData *>(data);
	std::string str_type {type};

	if (str_type == PW_TYPE_INTERFACE_Node)
		reud.self->wrap_node(id, props);
	else if (str_type == PW_TYPE_INTERFACE_Port)
		reud.self->wrap_port(id, props);
	else if (str_type == PW_TYPE_INTERFACE_Link)
		reud.self->wrap_link(id, props);
}


void Core::on_global_remove(void *data, uint32_t id)
{
	RegistryEventUserData& reud = *static_cast<RegistryEventUserData *>(data);

	auto& registry_objects = reud.self->registry_objects;

}


void Core::wrap_node(uint32_t id, const spa_dict *props)
{
	const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
	if (name == nullptr)
		name = "";
	const char *description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
	if (description == nullptr)
		description = "";
	Node *node = new Node(Object(id), name, description);
	registry_objects.nodes[id] = std::unique_ptr<Node>(node);

	auto port_it = registry_objects.nodeless_ports.find(id);
	if (port_it == registry_objects.nodeless_ports.end())
		return;

	std::list<Port *>& ports = port_it->second;
	for (auto port : ports) {
		port->set_owner(*node);
		node->add_port(*port);
	}
	registry_objects.nodeless_ports.erase(port_it);
}


void Core::wrap_port(uint32_t id, const spa_dict *props)
{
	const char *name = spa_dict_lookup(props, PW_KEY_PORT_NAME);
	if (name == nullptr)
		name = "";
	const char *str_direction = spa_dict_lookup(props, PW_KEY_PORT_DIRECTION);
	if (str_direction == nullptr)
		str_direction = "out";
	PortDirection direction = std::string(str_direction) == "in" ?
						PortDirection::Input : PortDirection::Output;

	Port *port = new Port(Object(id), name, direction);
	registry_objects.ports[id] = std::unique_ptr<Port>(port);

	const char *str_node_id = spa_dict_lookup(props, PW_KEY_NODE_ID);
	uint32_t node_id = str_node_id ? ::atoi(str_node_id) : 0;
	auto node_it = registry_objects.nodes.find(node_id);

	if (node_it != registry_objects.nodes.end()) {
		Node *node = node_it->second.get();
		port->set_owner(*node);
		node->add_port(*port);
	} else {
		port->set_owner_id(node_id);
		registry_objects.nodeless_ports[node_id].push_back(port);
	}

	auto link_it = registry_objects.portless_links.find(id);
	if (link_it == registry_objects.portless_links.end())
		return;

	std::list<LinkInfo>& links = link_it->second;
	for (auto&& link : links) {
		if (direction == PortDirection::Input) {
			if (link.o_port.ptr == nullptr)
				port->link_to_id(link.o_port.id);
			else
				port->link_to(*link.o_port.ptr);
		} else {
			if (link.i_port.ptr == nullptr)
				port->link_to_id(link.i_port.id);
			else
				port->link_to(*link.i_port.ptr);
		}
	}
	registry_objects.portless_links.erase(link_it);
}


void Core::wrap_link(uint32_t id, const spa_dict *props)
{
	const char *str_port_id;

	str_port_id = spa_dict_lookup(props, PW_KEY_LINK_OUTPUT_PORT);
	uint32_t o_port_id = str_port_id ? ::atoi(str_port_id) : 0;

	str_port_id = spa_dict_lookup(props, PW_KEY_LINK_INPUT_PORT);
	uint32_t i_port_id = str_port_id ? ::atoi(str_port_id) : 0;

	Port *o_port;
	auto o_port_it = registry_objects.ports.find(o_port_id);
	if (o_port_it == registry_objects.ports.end())
		o_port = nullptr;
	else
		o_port = o_port_it->second.get();

	Port *i_port;
	auto i_port_it = registry_objects.ports.find(i_port_id);
	if (i_port_it == registry_objects.ports.end())
		i_port = nullptr;
	else
		i_port = i_port_it->second.get();

	registry_objects.links[id] = {ID_Ptr(i_port_id, i_port), ID_Ptr(o_port_id, o_port)};

	if (o_port && i_port) {
		o_port->link_to(*i_port);
		return;
	}

	if (o_port) {
		o_port->link_to_id(i_port_id);
		registry_objects.portless_links[i_port_id].push_back(
				{ID_Ptr<Port>(i_port_id), ID_Ptr(o_port)});
	}

	if (i_port) {
		i_port->link_to_id(o_port_id);
		registry_objects.portless_links[o_port_id].push_back(
				{ID_Ptr(i_port), ID_Ptr<Port>(o_port_id)});
	}

	if (i_port != o_port)
		return;

	registry_objects.portless_links[i_port_id].push_back(
			{ID_Ptr<Port>(i_port_id), ID_Ptr<Port>(o_port_id)});
	registry_objects.portless_links[o_port_id].push_back(
			{ID_Ptr<Port>(i_port_id), ID_Ptr<Port>(o_port_id)});
}


bool Core::try_unwrap_node(uint32_t id)
{
	auto node_it = registry_objects.nodes.find(id);
	if (node_it == registry_objects.nodes.end())
		return false;

	Node *node = node_it->second.get();

	size_t num_ports = node->get_nr_i_ports();
	for (size_t i = 0; i < num_ports; ++i) {
		Port& port = node->get_i_port(i);
		port.unown();
	}

	num_ports = node->get_nr_o_ports();
	for (size_t i = 0; i < num_ports; ++i) {
		Port& port = node->get_o_port(i);
		port.unown();
	}

	registry_objects.nodes.erase(node_it);
	return true;
}


bool Core::try_unwrap_port(uint32_t id)
{
	auto port_it = registry_objects.ports.find(id);
	if (port_it == registry_objects.ports.end())
		return false;

	Port *port = port_it->second.get();

	size_t num_links = port->get_nr_linked_ports();
	while (num_links > 0) {
		auto linked_port = port->get_linked_port(--num_links);
		if (linked_port) [[likely]]
			linked_port->unlink_from(*port);
	}

	registry_objects.ports.erase(port_it);
	return true;
}


bool Core::try_unwrap_link(uint32_t id)
{
	auto link_it = registry_objects.links.find(id);
	if (link_it == registry_objects.links.end())
		return false;

	LinkInfo& link = link_it->second;
	if (link.i_port.ptr)
		link.i_port.ptr->unlink_from_id(link.o_port.id);
	else if (link.o_port.ptr)
		link.o_port.ptr->unlink_from_id(link.i_port.id);

	registry_objects.links.erase(link_it);
	return true;
}


pw_registry_events Core::registry_events = {
	.version = PW_VERSION_REGISTRY,
	.global = on_global,
	.global_remove = on_global_remove,
};

}
