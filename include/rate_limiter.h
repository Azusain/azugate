#ifndef __RATE_LIMITER_H
#define __RATE_LIMITER_H

#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>

namespace azugate {

constexpr size_t kDftTokenGenIntervalSec = 1;
// TODO: for testing purpose.
constexpr size_t kTokenGenNum = 1;
constexpr size_t kMaxTokenNum = 10;

class TokenBucketRateLimiter {
public:
  explicit TokenBucketRateLimiter(
      const boost::shared_ptr<boost::asio::io_context> io_context_ptr);

  void Start();

  bool GetToken();

private:
  void performTask();
  void tick(boost::asio::steady_timer &timer);

  size_t token_gen_interval_sec_;
  boost::shared_ptr<boost::asio::io_context> io_context_ptr_;
  boost::asio::steady_timer timer_;
  std::atomic<size_t> n_token_;
};
} // namespace azugate

#endif