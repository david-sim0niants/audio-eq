#include <audioeq/audioeq.h>

#include <iostream>


int main(int argc, char *argv[])
{
	aeq::Core core {argc, argv};

	while (true) {
	}

	std::vector<aeq::Node *> nodes;
	core.list_nodes(nodes);

	for (auto node : nodes) {
		std::cout << node->get_id() << ": " << node->get_name() << ": "
			  << node->get_descripiton() << std::endl;

		std::cout << "Input ports:" << std::endl;
		size_t nr_ports = node->get_nr_i_ports();
		for (size_t i = 0; i < nr_ports; ++i) {
			auto& port = node->get_i_port(i);
			std::cout << "\t" << port.get_id() << ": " << port.get_name() << std::endl;
		}

		std::cout << "Output ports:" << std::endl;
		nr_ports = node->get_nr_i_ports();
		for (size_t i = 0; i < nr_ports; ++i) {
			auto& port = node->get_o_port(i);
			std::cout << "\t" << port.get_id() << ": " << port.get_name() << std::endl;
		}
	}

	return 0;
}
