#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace aeq {

class Object {
	friend class Core;
public:
	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;

protected:
	explicit Object(uint32_t id) : id(id) {}

	Object(Object&&) = default;
	Object& operator=(Object&&) = default;

	uint32_t id;
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
public:
	Port& get_linked_port(size_t index) const;
	size_t get_nr_linked_ports(size_t index) const;
private:
	Port(Object&& object, Node& owner, PortDirection direction)
		: Object(std::move(object)), owner(owner), direction(direction) {}

	void link_to(Port& other);
	void unlink_from(Port& other);

	Node& owner;
	PortDirection direction;
	std::vector<Port *> linked_ports;
};

}
