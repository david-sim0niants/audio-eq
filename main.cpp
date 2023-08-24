#include "audioeq/audioeq.h"
#include "audioeq/filters/low_pass.h"

#include <iostream>
#include <sstream>
#include <functional>
#include <unordered_map>

constexpr float cutoff_freq = 2000;
constexpr int sample_rate = 44100;
constexpr int nr_channels = 2;


class BoringCLI {
public:
	struct CommandContext {
		aeq::Core& core;
		aeq::filters::LowPassFilter& low_pass_filter;
	};

	using CommandFunc = std::function<void (std::stringstream& cmdline_ss, CommandContext& context)>;
	using CommandsMap = std::unordered_map<std::string, CommandFunc>;

	explicit BoringCLI(CommandContext context) : context(context) {}

	void run();
private:
	CommandContext context;

	static void do_link(std::stringstream& cmdline_ss, CommandContext& context);
	static void do_unlink(std::stringstream& cmdline_ss, CommandContext& context);
	static void do_list(std::stringstream& cmdline_ss, CommandContext& context);
	static void do_freq(std::stringstream& cmdline_ss, CommandContext& context);

	static CommandsMap commands;
};


int main(int argc, char *argv[])
{
	aeq::Core core {argc, argv};

	aeq::filters::LowPassFilter low_pass_filter {cutoff_freq, sample_rate, nr_channels};
	core.init_filter(low_pass_filter, "AudioEQ: low pass filter");

	low_pass_filter.start();

	BoringCLI boring_cli {{.core = core, .low_pass_filter = low_pass_filter}};
	boring_cli.run();

	return 0;
}


void BoringCLI::run()
{
	std::string command_line;

	while (true) {
		std::cout << "(aeq) ";
		if (!std::getline(std::cin, command_line))
			break;
		if (std::all_of(command_line.begin(), command_line.end(), isspace))
			continue;

		std::stringstream command_line_ss {command_line};

		std::string command;
		command_line_ss >> command;

		if (command == "exit")
			break;

		auto found_command_it = commands.find(command);
		if (found_command_it == commands.end()) {
			std::cerr << "Error: no such command - '" << command << "'." << std::endl;
			continue;
		}
		found_command_it->second(command_line_ss, context);
	}
}


void BoringCLI::do_link(std::stringstream& cmdline_ss, CommandContext& context)
{
	context.core.lock_loop();
	aeq::utils::Defer defer{[&core = context.core](){core.unlock_loop();}};

	uint32_t port_id_a, port_id_b;
	cmdline_ss >> port_id_a >> port_id_b;

	aeq::Port *port_a = context.core.find_port(port_id_a);
	if (port_a == nullptr) {
		std::cerr << "Error: no such port with id '" << port_id_a << "'." << std::endl;
		return;
	}

	aeq::Port *port_b = context.core.find_port(port_id_b);
	if (port_b == nullptr) {
		std::cerr << "Error: no such port with id '" << port_id_b << "'." << std::endl;
		return;
	}

	if (port_a->get_direction() == port_b->get_direction()) {
		std::cerr << "Error: both ports have the same direction." << std::endl;
		return;
	}

	aeq::Port *i_port, *o_port;
	if (port_a->get_direction() == aeq::PortDirection::Input) {
		i_port = port_a;
		o_port = port_b;
	} else {
		i_port = port_b;
		o_port = port_a;
	}

	uint32_t link_id = context.core.link_ports(*o_port, *i_port);
	if (link_id == 0)
		std::cerr << "Failed to link ports." << std::endl;
	else
		std::cout << "Link ID: " << link_id << std::endl;
}


void BoringCLI::do_unlink(std::stringstream& cmdline_ss, CommandContext& context)
{
	context.core.lock_loop();
	aeq::utils::Defer defer{[&core = context.core](){core.unlock_loop();}};

	uint32_t link_id;
	cmdline_ss >> link_id;
	context.core.unlink_ports(link_id);
}


void BoringCLI::do_list(std::stringstream& cmdline_ss, CommandContext& context)
{
	aeq::Core& core = context.core;

	core.lock_loop();
	aeq::utils::Defer defer{[&core = core](){core.unlock_loop();}};

	std::vector<aeq::Node *> nodes;
	core.list_nodes(nodes);

	for (auto node : nodes) {
		std::cout << node->get_id() << ": " << node->get_name() << ": "
			  << node->get_descripiton() << std::endl;

		std::cout << "Input ports:" << std::endl;
		size_t nr_ports = node->get_nr_i_ports();
		for (size_t i = 0; i < nr_ports; ++i) {
			auto& port = node->get_i_port(i);
			std::cout << '\t' << port.get_id() << ": " << port.get_name() << std::endl;
		}

		std::cout << "Output ports:" << std::endl;
		nr_ports = node->get_nr_o_ports();
		for (size_t i = 0; i < nr_ports; ++i) {
			auto& port = node->get_o_port(i);
			std::cout << '\t' << port.get_id() << ": " << port.get_name() << std::endl;
		}
	}
}


void BoringCLI::do_freq(std::stringstream& cmdline_ss, CommandContext& context)
{
	float cutoff_freq;
	cmdline_ss >> cutoff_freq;

	if (cutoff_freq < 16.F) {
		std::cerr << "Error: too low frequency for low pass filter." << std::endl;
		return;
	}

	if (cutoff_freq > 20000.F) {
		std::cerr << "Error: too high frequency for low pass filter." << std::endl;
		return;
	}

	context.low_pass_filter.set_cutoff_freq(cutoff_freq);
}


std::unordered_map<std::string, BoringCLI::CommandFunc> BoringCLI::commands = {
	{"link", 	do_link},
	{"unlink", 	do_unlink},
	{"list", 	do_list},
	{"freq", 	do_freq},
};
