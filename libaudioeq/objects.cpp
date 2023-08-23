#include "audioeq/objects.h"

namespace aeq {

void Node::add_port(Port& port)
{
	if (port.get_direction() == PortDirection::Input)
		i_ports.push_back(&port);
	else
		o_ports.push_back(&port);
}

void Node::rem_port(Port& port)
{
	auto& ports = port.get_direction() == PortDirection::Input ? i_ports : o_ports;
	auto found_port_it = std::find_if(ports.begin(), ports.end(),
			[&port](Port *curr_port) { return curr_port->get_id() == port.get_id(); });
	if (found_port_it == ports.end())
		return;
	ports.erase(found_port_it);
}


void Port::link_to(Port& other)
{
	auto found_port_it = find_linked_port(other.id);
	if (found_port_it == linked_ports.end())
		linked_ports.emplace_back(other.id, &other);
	else
		*found_port_it = {other.id, &other};
	other.linked_ports.emplace_back(id, this);
}

void Port::link_to_id(uint32_t other_id)
{
	auto found_port_it = find_linked_port(other_id);
	if (found_port_it == linked_ports.end())
		linked_ports.emplace_back(other_id);
	else
		*found_port_it = ID_Ptr<Port>(other_id);
}

void Port::unlink_from_id(uint32_t other_id)
{
	linked_ports.erase(std::remove_if(linked_ports.begin(),
				linked_ports.end(),
				[other_id](auto& port) {return port.id == other_id;}),
			linked_ports.end());
}

void Port::unlink_from(Port& other)
{
	unlink_from_id(other.id);
}

Port::LinkedPortIt Port::find_linked_port(uint32_t id)
{
	return std::find_if(linked_ports.begin(), linked_ports.end(),
			[id](const ID_Ptr<Port>& port){ return port.id == id; });

}

}
