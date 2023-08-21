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

	lock_loop();
	setup_registry_events();
	unlock_loop();
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


void Core::lock_loop() const
{
	pw_thread_loop_lock(loop.get());
}


void Core::unlock_loop() const
{
	pw_thread_loop_unlock(loop.get());
}


void Core::wait_loop() const
{
	pw_thread_loop_wait(loop.get());
}


void Core::list_nodes(std::vector<Node *>& nodes) const
{
	std::transform(registry_objects.nodes.begin(),
			registry_objects.nodes.end(),
			std::back_inserter(nodes),
			[](const auto& id_node) { return id_node.second.get(); });
}


uint32_t Core::link_ports(Port& o_port, Port& i_port)
{
	if (o_port.get_direction() != PortDirection::Output || i_port.get_direction() != PortDirection::Input)
		throw CoreErr(AudioEqErr("Error: wrong port given."));

	pw_properties *props = pw_properties_new(nullptr, nullptr);
	pw_properties_setf(props, PW_KEY_LINK_OUTPUT_PORT, "%d", o_port.get_id());
	pw_properties_setf(props, PW_KEY_LINK_INPUT_PORT, "%d", i_port.get_id());
	pw_proxy *link = static_cast<pw_proxy *>(
			pw_core_create_object(core.get(),
				"link-factory",
				PW_TYPE_INTERFACE_Link,
				PW_VERSION_LINK,
				&props->dict, 0));
	pw_properties_free(props);
	return pw_proxy_get_id(link);
}


void Core::unlink_ports(uint32_t link_id)
{
	pw_proxy *link = static_cast<pw_proxy *>(pw_core_find_proxy(core.get(), link_id));
	pw_proxy_destroy(link);
}


std::unique_ptr<Filter> Core::create_filter(const char *name)
{
	pw_properties *props = pw_properties_new(
		PW_KEY_MEDIA_TYPE, "Audio",
		PW_KEY_MEDIA_CATEGORY, "Filter",
		PW_KEY_MEDIA_ROLE, "DSP",
		NULL);
	pw_filter *filter = pw_filter_new(core.get(), name, props);
	return std::unique_ptr<Filter>(new Filter(filter));
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
	if (reud.self->try_unwrap_node(id))
		return;
	if (reud.self->try_unwrap_port(id))
		return;
	if (reud.self->try_unwrap_link(id))
		return;
}


void Core::wrap_node(uint32_t id, const spa_dict *props)
{
	// get node name
	const char *name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
	if (name == nullptr)
		name = "";
	// get node description
	const char *description = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
	if (description == nullptr)
		description = "";

	// create the node wrapper and store it in registry_objects
	Node *node = new Node(Object(id), name, description);
	registry_objects.nodes[id] = std::unique_ptr<Node>(node);

	// find ports that belong to this node but were found and wrapped before this node was found and wrapped
	auto port_it = registry_objects.nodeless_ports.find(id);
	if (port_it == registry_objects.nodeless_ports.end())
		return;

	// set this wrapper node as an owner of these ports
	std::vector<Port *>& ports = port_it->second;
	for (auto port : ports) {
		port->set_owner(*node);
		node->add_port(*port);
	}

	// remove refs to these ports in nodeless_ports
	registry_objects.nodeless_ports.erase(port_it);
}


void Core::wrap_port(uint32_t id, const spa_dict *props)
{
	// get port name
	const char *name = spa_dict_lookup(props, PW_KEY_PORT_NAME);
	if (name == nullptr)
		name = "";
	// get port direction
	const char *str_direction = spa_dict_lookup(props, PW_KEY_PORT_DIRECTION);
	if (str_direction == nullptr)
		str_direction = "out";
	PortDirection direction = std::string(str_direction) == "in" ?
						PortDirection::Input : PortDirection::Output;

	// create the port wrapper and store it in registry_objects
	Port *port = new Port(Object(id), name, direction);
	registry_objects.ports[id] = std::unique_ptr<Port>(port);

	// get owner node id
	const char *str_node_id = spa_dict_lookup(props, PW_KEY_NODE_ID);
	uint32_t node_id = str_node_id ? ::atoi(str_node_id) : 0;

	// find the node in the registry_objects
	auto node_it = registry_objects.nodes.find(node_id);
	if (node_it != registry_objects.nodes.end()) {
		// if the node found, assign the node as owner of the port and add port to the node
		Node *node = node_it->second.get();
		port->set_owner(*node);
		node->add_port(*port);
	} else {
		// if the node ain't found, assign only owner id to the port, node wrapper will be assigned later when found
		port->set_owner_id(node_id);
		// add port to the map of nodeless ports under the node id, so when the node appears it could find the ports it owns
		registry_objects.nodeless_ports[node_id].push_back(port);
	}

	// find links that were missing this port to be 'complete' and were found and wrapped before this port was found and wrapped
	auto portless_link_it = registry_objects.portless_links.find(id);
	if (portless_link_it == registry_objects.portless_links.end())
		return;

	// resolve links
	std::vector<LinkInfo>& portless_links = portless_link_it->second;
	for (auto&& link : portless_links) {
		if (direction == PortDirection::Input) {
			// if this port is an input port, the other one is output port
			if (link.o_port.ptr == nullptr)
				// if the other port is missing like this one did, just link this port to an id without wrapper
				port->link_to_id(link.o_port.id);
			else
				// otherwise the link will be complete now
				port->link_to(*link.o_port.ptr);
		} else {
			// if this port is an output port, the other one is input port (same goes here as above)
			if (link.i_port.ptr == nullptr)
				port->link_to_id(link.i_port.id);
			else
				port->link_to(*link.i_port.ptr);
		}
	}
	// this links aren't missing this port anymore, so remove them
	registry_objects.portless_links.erase(portless_link_it);
}


void Core::wrap_link(uint32_t id, const spa_dict *props)
{
	const char *str_port_id;
	// get output port id
	str_port_id = spa_dict_lookup(props, PW_KEY_LINK_OUTPUT_PORT);
	uint32_t o_port_id = str_port_id ? ::atoi(str_port_id) : 0;
	// get input port id
	str_port_id = spa_dict_lookup(props, PW_KEY_LINK_INPUT_PORT);
	uint32_t i_port_id = str_port_id ? ::atoi(str_port_id) : 0;

	// find the output port if present
	Port *o_port;
	auto o_port_it = registry_objects.ports.find(o_port_id);
	if (o_port_it == registry_objects.ports.end())
		o_port = nullptr;
	else
		o_port = o_port_it->second.get();

	// find the input port if present
	Port *i_port;
	auto i_port_it = registry_objects.ports.find(i_port_id);
	if (i_port_it == registry_objects.ports.end())
		i_port = nullptr;
	else
		i_port = i_port_it->second.get();

	// store the link info in the registry_objects
	registry_objects.links[id] = {ID_Ptr(i_port_id, i_port), ID_Ptr(o_port_id, o_port)};

	if (o_port && i_port) {
		// if both ports present, fully link them
		o_port->link_to(*i_port);
		return;
	}

	if (o_port) {
		// if input port is missing but output port isn't, link output port to an input port id
		o_port->link_to_id(i_port_id);
		// and store it in the portless_links map under the input port id
		registry_objects.portless_links[i_port_id].push_back(
				{ID_Ptr<Port>(i_port_id), ID_Ptr(o_port)});
	}

	if (i_port) {
		// if output port is missing but input port isn't, link input port to an output port id
		i_port->link_to_id(o_port_id);
		// and store it in the portless_links map under the output port id
		registry_objects.portless_links[o_port_id].push_back(
				{ID_Ptr(i_port), ID_Ptr<Port>(o_port_id)});
	}

	if (i_port != o_port)
		return;

	// if both ports are missing, store the link info under both input and output port IDs
	registry_objects.portless_links[i_port_id].push_back(
			{ID_Ptr<Port>(i_port_id), ID_Ptr<Port>(o_port_id)});
	registry_objects.portless_links[o_port_id].push_back(
			{ID_Ptr<Port>(i_port_id), ID_Ptr<Port>(o_port_id)});
}


bool Core::try_unwrap_node(uint32_t id)
{
	// find the node
	auto node_it = registry_objects.nodes.find(id);
	if (node_it == registry_objects.nodes.end())
		return false;

	Node *node = node_it->second.get();

	// unown all its ports
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

	// remove it from registry_objects
	registry_objects.nodes.erase(node_it);
	return true;
}


bool Core::try_unwrap_port(uint32_t id)
{
	// find the port
	auto port_it = registry_objects.ports.find(id);
	if (port_it == registry_objects.ports.end())
		return false;

	Port *port = port_it->second.get();

	// unlink from all its linked ports
	size_t num_links = port->get_nr_linked_ports();
	while (num_links > 0) {
		auto linked_port = port->get_linked_port(--num_links);
		if (linked_port) [[likely]]
			linked_port->unlink_from(*port);
	}

	// remove it from registry_objects
	registry_objects.ports.erase(port_it);
	return true;
}


bool Core::try_unwrap_link(uint32_t id)
{
	// find the link
	auto link_it = registry_objects.links.find(id);
	if (link_it == registry_objects.links.end())
		return false;

	LinkInfo& link = link_it->second;

	// unlink its ports one from each other
	if (link.i_port.ptr)
		link.i_port.ptr->unlink_from_id(link.o_port.id);
	else if (link.o_port.ptr)
		link.o_port.ptr->unlink_from_id(link.i_port.id);

	// remove it from registry_objects
	registry_objects.links.erase(link_it);
	return true;
}


pw_registry_events Core::registry_events = {
	.version = PW_VERSION_REGISTRY,
	.global = on_global,
	.global_remove = on_global_remove,
};

}
