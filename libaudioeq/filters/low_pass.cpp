#include <audioeq/filters/low_pass.h>

#include <cmath>


namespace aeq::filters {

LowPassFilter::LowPassFilter(float cutoff_freq, int sample_rate, int nr_channels)
	: nr_channels(nr_channels), cuttoff_freq(cutoff_freq), sample_rate(sample_rate),
	alpha(calc_alpha(cutoff_freq, sample_rate))
{
	if (nr_channels <= 0)
		nr_channels = 0;
	else if (nr_channels > 2)
		nr_channels = 2;
}


void LowPassFilter::core_init(pw_filter *filter)
{
	Filter::core_init(filter);
	if (nr_channels <= 0) {
		return;
	} else if (nr_channels == 1) {
		add_audio_port(PortDirection::Input, "lp-in");
		add_audio_port(PortDirection::Output, "lp-out");
	} else {
		add_audio_port(PortDirection::Input, "lp-in_L");
		add_audio_port(PortDirection::Input, "lp-in_R");
		add_audio_port(PortDirection::Output, "lp-out_L");
		add_audio_port(PortDirection::Output, "lp-out_R");
	}
}


void LowPassFilter::set_cutoff_freq(float cutoff_freq)
{
	this->cuttoff_freq = cutoff_freq;
	this->alpha = calc_alpha(cuttoff_freq, sample_rate);
}


void LowPassFilter::on_process(size_t nr_samples)
{
	for (int i = 0; i < nr_channels; ++i) {
		float *in_buf = get_input_buffer(i, nr_samples);
		if (in_buf == nullptr)
			continue;

		float *out_buf = get_output_buffer(i, nr_samples);
		if (out_buf == nullptr)
			continue;

		single_channel_process(in_buf, out_buf, nr_samples);
	}
}


void LowPassFilter::single_channel_process(float *in_buf, float *out_buf, size_t nr_samples)
{
	for (size_t i = 0; i < nr_samples; ++i) {
		out_buf[i] = alpha * in_buf[i] + (1 - alpha) * out_buf[i];
	}
}


float LowPassFilter::calc_alpha(float cutoff_freq, int sample_rate)
{
	if (sample_rate <= 0)
		throw LowPassFilterErr(FilterErr({"Non-positive sample rate."}));
	if (cutoff_freq <= 0.0F)
		throw LowPassFilterErr(FilterErr({"Non-positive cuttoff frequency."}));
	float dt = 1.0F / sample_rate;
	float rc = 1.0F / (2 * M_PI * cutoff_freq);
	return dt / (dt + rc);
}

}
