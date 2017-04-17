/*
 *  Created on: Mar 28, 2016
 *      Author: xing
 */

#ifndef COMMON_INC_TIMER_H_
#define COMMON_INC_TIMER_H_

#include <chrono>

#include "base/common.h"
#include "base/macro_utils.h"

namespace base{

	class Timer final{
	public:
		typedef std::chrono::steady_clock::time_point Point;
		typedef std::chrono::steady_clock::duration Duration;
		typedef std::chrono::steady_clock::period Period;

		Timer(): start{std::chrono::steady_clock::now()} {}

		inline void Reset() noexcept {start = std::chrono::steady_clock::now();}

		inline Duration Lap() const noexcept{
			return std::chrono::steady_clock::now() - start;
		}

		/**
		 * Has time passed duration till now?
		 * @param[in] duration - time duration
		 * @return OK - time is up, and ts has been updated
		 * 		NO - time is not up, and ts stays the same
		 */
		Ret TimeUp(const Duration &duration) noexcept;

		/**
		 * Has time passed duration till now?
		 * @param[in] duration - time duration
		 * @return OK - time is up, NO - time is not up
		 * @note This call will ALWAYS update the value of timer to now
		 */
		Ret TimeUpAlwaysUpdate(const Duration &duration) noexcept;

		void Inspect() const noexcept;

#define BASE_TIMER_HELPER_CAST(unit) \
		static inline intmax_t of_##unit(const Duration &du) noexcept {\
			return std::chrono::duration_cast<std::chrono::unit>(du).count();\
		}

#define BASE_TIMER_HELPER_STATE(unit) \
		static inline Duration unit(int64_t val) noexcept{\
			return std::chrono::unit(val);\
		}

#define BASE_TIMER_HELPER(unit) \
	BASE_TIMER_HELPER_STATE(unit)\
	BASE_TIMER_HELPER_CAST(unit)\

		BASE_TIMER_HELPER(nanoseconds)
		BASE_TIMER_HELPER(microseconds)
		BASE_TIMER_HELPER(milliseconds)
		BASE_TIMER_HELPER(seconds)
		BASE_TIMER_HELPER(minutes)
		BASE_TIMER_HELPER(hours)

	private:
		BASE_READER(Point, start);
		Point tmp{};
	};

}

#endif /* COMMON_INC_TIMER_H_ */