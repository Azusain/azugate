#ifndef __HTTP_WRAPPER_H
#define __HTTP_WRAPPER_H
#include "crequest.h"

namespace azugate {
namespace network {
template <typename T>
inline bool SendHttpRequest(const CRequest::HttpRequest &request,
                            const boost::shared_ptr<T> &sock_ptr);

template <typename T>
inline bool SendHttpRequest(const CRequest::HttpRequest &request,
                            const boost::shared_ptr<T> &sock_ptr);
} // namespace network
} // namespace azugate
#endif