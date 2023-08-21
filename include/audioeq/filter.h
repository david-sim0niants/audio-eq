#pragma once

#include "objects.h"

#include <pipewire/pipewire.h>

namespace aeq {

class AudioPort;

class Filter {
	friend class Core;

	struct FilterEventsUserData {
		Filter *self;
	};
public:
	Filter() = default;
	~Filter();

	Filter(Filter &&) = delete;
	Filter& operator=(Filter &&) = delete;
	Filter(const Filter&) = delete;
	Filter& operator=(const Filter&) = delete;
protected:
	virtual void core_init(pw_filter *filter);

	virtual void on_process(size_t nr_samples);

	void add_audio_port(PortDirection direction, const char *name);
	void rem_audio_port(AudioPort *port);

	float *get_input_buffer(size_t index, size_t nr_samples);
	float *get_output_buffer(size_t index, size_t nr_samples);

	std::vector<AudioPort *> i_audio_ports;
	std::vector<AudioPort *> o_audio_ports;
private:
	void setup_filter_events();

	pw_filter *filter = nullptr;

	spa_hook filter_listener;
	FilterEventsUserData feud;

	static void on_process(void *data, struct spa_io_position *position);

	static pw_filter_events filter_events;
};

}
