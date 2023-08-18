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
	~Filter();
protected:
	Filter(Filter &&) = default;
	Filter& operator=(Filter &&) = default;

	virtual void on_process(size_t nr_samples);

	void add_audio_port(PortDirection direction, const char *name);
	void rem_audio_port(AudioPort *port);

	float *get_input_buffer(size_t index, size_t nr_samples);
	float *get_output_buffer(size_t index, size_t nr_samples);

	std::vector<AudioPort *> i_audio_ports;
	std::vector<AudioPort *> o_audio_ports;
private:
	explicit Filter(pw_filter *filter);

	Filter(const Filter&) = delete;
	Filter& operator=(const Filter&) = delete;

	void setup_filter_events();

	pw_filter *filter;

	spa_hook filter_listener;
	FilterEventsUserData feud;

	static void on_process(void *userdata, struct spa_io_position *position);

	static pw_filter_events filter_events;
};

}
