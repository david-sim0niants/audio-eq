#pragma once

#include "audioeq/filter.h"

#include <atomic>

namespace aeq::filters {

class LowPassFilter : public Filter {
public:
	LowPassFilter(float cutoff_freq, int sample_rate, unsigned int nr_channels);

	void core_init(pw_filter *filter) override;
	void set_cutoff_freq(float cutoff_freq);
private:
	void on_process(size_t nr_samples) override;
	void single_channel_process(float *in_buf, float *out_buf, size_t nr_samples);

	int nr_channels;
	float cuttoff_freq;
	int sample_rate;

	std::atomic<float> alpha;

	static float calc_alpha(float cuttoff_freq, int sample_rate);
};

struct LowPassFilterErr : FilterErr {
	LowPassFilterErr(FilterErr&& base) : FilterErr(std::move(base)) {}
};

}
