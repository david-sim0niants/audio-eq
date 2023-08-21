#include <audioeq/filter.h>


namespace aeq {

Filter::~Filter()
{
	if (filter)
		pw_filter_destroy(filter);
}


void Filter::core_init(pw_filter *filter)
{
	this->filter = filter;
	setup_filter_events();
}


void Filter::on_process(size_t nr_samples)
{
}


void Filter::add_audio_port(PortDirection direction, const char *name)
{
	pw_properties *props = pw_properties_new(
			PW_KEY_FORMAT_DSP, "32 bit float mono audio",
			PW_KEY_PORT_NAME, name, NULL);

	std::vector<AudioPort *> *ports;
	spa_direction spa_dir;
	if (direction == PortDirection::Input) {
		ports = &i_audio_ports;
		spa_dir = SPA_DIRECTION_INPUT;
	} else {
		ports = &o_audio_ports;
		spa_dir = SPA_DIRECTION_OUTPUT;
	}

	AudioPort *port = static_cast<AudioPort *>(
				pw_filter_add_port(filter,
				spa_dir,
				PW_FILTER_PORT_FLAG_MAP_BUFFERS,
				sizeof(void *),
				props, nullptr, 0));
	ports->push_back(port);
}


void Filter::rem_audio_port(AudioPort *port)
{
	auto do_remove = [](auto& port_it, auto& ports)
	{
		AudioPort *port = *port_it;
		ports.erase(port_it);
		pw_filter_remove_port(port);
	};

	auto port_it = std::find(i_audio_ports.begin(), i_audio_ports.end(), port);
	if (port_it != i_audio_ports.end()) {
		do_remove(port_it, i_audio_ports);
		return;
	}

	port_it = std::find(o_audio_ports.begin(), o_audio_ports.end(), port);
	if (port_it != o_audio_ports.end()) {
		do_remove(port_it, o_audio_ports);
		return;
	}
}


float *Filter::get_input_buffer(size_t index, size_t nr_samples)
{
	return static_cast<float *>(pw_filter_get_dsp_buffer(i_audio_ports[index], nr_samples));
}


float *Filter::get_output_buffer(size_t index, size_t nr_samples)
{
	return static_cast<float *>(pw_filter_get_dsp_buffer(o_audio_ports[index], nr_samples));
}


void Filter::setup_filter_events()
{
	feud.self = this;
	spa_zero(filter_listener);
	pw_filter_add_listener(filter, &filter_listener, &filter_events, &feud);
}


void Filter::on_process(void *data, struct spa_io_position *position)
{
	FilterEventsUserData *feud = static_cast<FilterEventsUserData *>(data);
	feud->self->on_process(position->clock.duration);
}


pw_filter_events Filter::filter_events = {
	.version = PW_VERSION_FILTER_EVENTS,
	.process = on_process,
};

}
