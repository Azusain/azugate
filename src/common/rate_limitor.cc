// this module is used for controlling bursty data.
// ref:
// https://www.geeksforgeeks.org/token-bucket-algorithm/
// https://www.geeksforgeeks.org/leaky-bucket-algorithm/
#include <boost/asio/detail/chrono.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/system/detail/error_code.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>

#include "rate_limiter.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>

namespace azugate {

TokenBucketRateLimiter::TokenBucketRateLimiter(
    const boost::shared_ptr<boost::asio::io_context> io_context_ptr)
    : io_context_ptr_(io_context_ptr),
      timer_(*io_context_ptr,
             boost::asio::chrono::seconds(kDftTokenGenIntervalSec)) {};

void tick(boost::asio::steady_timer &t) {
  SPDLOG_INFO("tick!");
  t.expires_at(t.expiry() +
               boost::asio::chrono::seconds(kDftTokenGenIntervalSec));
  t.async_wait([&](const boost::system::error_code &) { tick(t); });
}

void TokenBucketRateLimiter::Start() {
  timer_.async_wait([&](const boost::system::error_code &) { tick(timer_); });
}

} // namespace azugate