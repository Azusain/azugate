#include "crequest.h"

namespace azugate {
namespace network {
template <typename T>
inline bool SendHttpRequest(const CRequest::HttpRequest &request,
                            const boost::shared_ptr<T> &sock_ptr) {
  return true;
};

template <typename T>
inline bool SendHttpResponse(const CRequest::HttpResponse &response,
                             const boost::shared_ptr<T> &sock_ptr) {
  return true;
};

} // namespace network
} // namespace azugate