#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

namespace aeq {

/* Base wrapper class for pipewire objects. */
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


/* A struct that can store both the ID and the pointer of an object or
 * store only its ID if the pointer is not present yet. */
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

/* Node wrapper. Stores its input and output ports. */
class Node : public Object {
	friend class Core;
public:
	/* Get input port at given index. */
	Port& get_i_port(int index) const;
	/* Get number of input ports. */
	size_t get_nr_i_ports() const;

	/* Get output port at given index. */
	Port& get_o_port(int index) const;
	/* Get number of output ports. */
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

/* Port wrapper. Stores its direction, owner and the linked ports. */
class Port : public Object {
	friend class Core;
	using LinkedPortIt = std::vector<ID_Ptr<Port>>::iterator;
public:
	/* Get ID of a linked port at given index. */
	uint32_t get_linked_port_id(size_t index) const;
	/* Get pointer to a linked port at given index. */
	Port *get_linked_port(size_t index) const;
	/* Get number of linked ports. */
	size_t get_nr_linked_ports() const;

	const std::string& get_name() const;
	PortDirection get_direction() const;

	/* Get ID of its Node owner. */
	uint32_t get_owner_id() const;
	/* Get pointer to its Node owner. */
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
