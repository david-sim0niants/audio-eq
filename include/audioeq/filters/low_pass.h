#pragma once

#include <audioeq/filter.h>
#include <audioeq/err.h>

namespace aeq::filters {

class LowPassFilter : public Filter {
public:
	LowPassFilter(Filter&& base, float cutoff_freq, int sample_rate, int nr_channels);

	void set_cutoff_freq(float cutoff_freq);
private:
	void on_process(size_t nr_samples) override;
	void single_channel_process(float *in_buf, float *out_buf, size_t nr_samples);

	float cuttoff_freq;
	int sample_rate;

	float alpha;
	float prev_output;

	static float calc_alpha(float cuttoff_freq, int sample_rate);
};

struct LowPassFilterErr : AudioEqErr {
	LowPassFilterErr(AudioEqErr&& base) : AudioEqErr(std::move(base)) {}
};

}
