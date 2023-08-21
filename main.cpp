#include <audioeq/audioeq.h>
#include <audioeq/filters/low_pass.h>

#include <iostream>

constexpr float cutoff_freq = 2000;
constexpr int sample_rate = 44100;
constexpr int nr_channels = 2;


int main(int argc, char *argv[])
{
	aeq::Core core {argc, argv};

	sleep(1);

	core.lock_loop();

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

	std::unique_ptr<aeq::filters::LowPassFilter> low_pass_filter =
		std::make_unique<aeq::filters::LowPassFilter>(
				std::move(*core.create_filter("audio-dsp").release()), cutoff_freq, sample_rate, nr_channels);

	core.unlock_loop();
	while (true) {
	}

	return 0;
}
