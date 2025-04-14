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

#include "config.h"
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
             boost::asio::chrono::seconds(kDftTokenGenIntervalSec)) {
  auto [num_token_max, _] = GetRateLimitorConfig();
  n_token_ = num_token_max;
};

// TODO: memory order.
bool TokenBucketRateLimiter::GetToken() {
  size_t current_tokens = n_token_.fetch_sub(1, std::memory_order_acq_rel);
  if (current_tokens > 0) {
    return true;
  }
  n_token_.fetch_add(1, std::memory_order_acq_rel);
  return false;
}

void TokenBucketRateLimiter::performTask() {
  // TODO: low performance.
  auto [num_token_max, num_token_per_sec] = GetRateLimitorConfig();

  size_t new_token_num = n_token_.load() + num_token_per_sec;
  if (new_token_num > num_token_max) {
    new_token_num = num_token_max;
  }
  n_token_.store(new_token_num);
  // TODO: rm this line.
  // SPDLOG_INFO("current tokens number: {}", new_token_num);
}

void TokenBucketRateLimiter::tick(boost::asio::steady_timer &t) {
  performTask();
  t.expires_at(t.expiry() +
               boost::asio::chrono::seconds(kDftTokenGenIntervalSec));
  t.async_wait([&](const boost::system::error_code &) { tick(t); });
}

void TokenBucketRateLimiter::Start() {
  timer_.async_wait([&](const boost::system::error_code &) { tick(timer_); });
}

} // namespace azugate