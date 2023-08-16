#pragma once

#include <utility>
#include <optional>

namespace aeq::utils
{

template<typename F>
class Defer
{
	static_assert(std::is_invocable_r_v<void, F>, "F must be a callable object that returns void.");
	std::optional<F> f;
public:
	Defer() noexcept = default;
	Defer(F &&f) noexcept : f(std::move(f)) {}

	inline void cancel() noexcept { f = std::nullopt; }

	~Defer() noexcept
	{
		if (f) (*f)();
	}
};

} // namespace aeq::utils

