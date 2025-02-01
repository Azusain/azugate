#ifndef __RATE_LIMITER_H
#define __RATE_LIMITER_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>

namespace azugate {

constexpr size_t kDftTokenGenIntervalSec = 1;

class TokenBucketRateLimiter {
public:
  explicit TokenBucketRateLimiter(
      const boost::shared_ptr<boost::asio::io_context> io_context_ptr);

  void Start();

private:
  size_t token_gen_interval_;
  boost::shared_ptr<boost::asio::io_context> io_context_ptr_;
  boost::asio::steady_timer timer_;
};
} // namespace azugate

#endif