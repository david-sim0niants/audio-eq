#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace aeq {

class Object {
	friend class Core;
public:
	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;

	uint32_t get_id() const;

protected:
	explicit Object(uint32_t id) : id(id) {}

	Object(Object&&) = default;
	Object& operator=(Object&&) = default;

	uint32_t id;
};


template<typename O, typename ID=uint32_t>
struct ID_Ptr {
	ID id = 0;
	O *ptr = nullptr;

	ID_Ptr() = default;
	explicit ID_Ptr(ID id) : id(id), ptr(nullptr) {}
	explicit ID_Ptr(O *ptr) : id(ptr->get_id()), ptr(ptr) {}
	ID_Ptr(ID id, O *ptr) : id(id), ptr(ptr) {}
};


class Port;

class Node : public Object {
	friend class Core;
public:
	Port& get_i_port(int index) const;
	size_t get_nr_i_ports() const;

	Port& get_o_port(int index) const;
	size_t get_nr_o_ports() const;

	const std::string& get_name() const;
	const std::string& get_descripiton() const;

protected:
	Node(Object&& object, std::string_view name, std::string_view description = "")
		: Object(std::move(object)), name(name), description(description) {}

	void add_port(Port& port);
	void rem_port(Port& port);

	std::vector<Port *> i_ports;
	std::vector<Port *> o_ports;
private:
	std::string name;
	std::string description;
};

enum class PortDirection { Input, Output };

class Port : public Object {
	friend class Core;
	using LinkedPortIt = std::vector<ID_Ptr<Port>>::iterator;
public:
	uint32_t get_linked_port_id(size_t index) const;
	Port *get_linked_port(size_t index) const;
	size_t get_nr_linked_ports() const;

	const std::string& get_name() const;
	PortDirection get_direction() const;

	uint32_t get_owner_id() const;
	Node *get_owner() const;
private:
	Port(Object&& object, std::string_view name, PortDirection direction)
		: Object(std::move(object)), name(name), direction(direction) {}

	void link_to(Port& other);
	void link_to_id(uint32_t other_id);

	void unlink_from_id(uint32_t other_id);
	void unlink_from(Port& other);

	LinkedPortIt find_linked_port(uint32_t id);

	void set_owner_id(uint32_t owner_id);
	void set_owner(Node& owner);
	void unown();

	std::string name;
	PortDirection direction;

	ID_Ptr<Node> owner;
	std::vector<ID_Ptr<Port>> linked_ports;
};


inline uint32_t Object::get_id() const
{
	return id;
}


inline Port& Node::get_i_port(int index) const
{
	return *i_ports[index];
}

inline size_t Node::get_nr_i_ports() const
{
	return i_ports.size();
}

inline Port& Node::get_o_port(int index) const
{
	return *o_ports[index];
}

inline size_t Node::get_nr_o_ports() const
{
	return o_ports.size();
}

inline const std::string& Node::get_name() const
{
	return name;
}

inline const std::string& Node::get_descripiton() const
{
	return description;
}

inline void Node::add_port(Port& port)
{
	if (port.get_direction() == PortDirection::Input)
		i_ports.push_back(&port);
	else
		o_ports.push_back(&port);
}

inline void Node::rem_port(Port& port)
{
	auto& ports = port.get_direction() == PortDirection::Input ? i_ports : o_ports;
	auto found_port_it = std::find_if(ports.begin(), ports.end(),
			[&port](Port *curr_port) { return curr_port->get_id() == port.get_id(); });
	if (found_port_it == ports.end())
		return;
	ports.erase(found_port_it);
}


inline uint32_t Port::get_linked_port_id(size_t index) const
{
	return linked_ports[index].id;
}

inline Port *Port::get_linked_port(size_t index) const
{
	return linked_ports[index].ptr;
}

inline size_t Port::get_nr_linked_ports() const
{
	return linked_ports.size();
}

inline const std::string& Port::get_name() const
{
	return name;
}

inline PortDirection Port::get_direction() const
{
	return direction;
}

inline uint32_t Port::get_owner_id() const
{
	return owner.id;
}

inline Node *Port::get_owner() const
{
	return owner.ptr;
}

inline void Port::link_to(Port& other)
{
	auto found_port_it = find_linked_port(other.id);
	if (found_port_it == linked_ports.end())
		linked_ports.emplace_back(other.id, &other);
	else
		*found_port_it = {other.id, &other};
	other.linked_ports.emplace_back(id, this);
}

inline void Port::link_to_id(uint32_t other_id)
{
	auto found_port_it = find_linked_port(other_id);
	if (found_port_it == linked_ports.end())
		linked_ports.emplace_back(other_id);
	else
		*found_port_it = ID_Ptr<Port>(other_id);
}

inline void Port::unlink_from_id(uint32_t other_id)
{
	std::erase_if(linked_ports, [other_id](auto& port) {return port.id == other_id;});
}

inline void Port::unlink_from(Port& other)
{
	unlink_from_id(other.id);
}

inline Port::LinkedPortIt Port::find_linked_port(uint32_t id)
{
	return std::find_if(linked_ports.begin(), linked_ports.end(),
			[id](const ID_Ptr<Port>& port){ return port.id == id; });

}

inline void Port::set_owner_id(uint32_t owner_id)
{
	owner = ID_Ptr<Node>(owner_id);
}

inline void Port::set_owner(Node& owner)
{
	this->owner = ID_Ptr(owner.get_id(), &owner);
}

inline void Port::unown()
{
	set_owner_id(0);
}

}
