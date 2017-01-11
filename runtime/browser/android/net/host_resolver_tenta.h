/*
 * host_resolver_tenta.h
 *
 *  Created on: Dec 20, 2016
 *      Author: iotto
 */

#ifndef XWALK_RUNTIME_NET_HOST_RESOLVER_TENTA_H_
#define XWALK_RUNTIME_NET_HOST_RESOLVER_TENTA_H_

#include <jni.h>
#include <map>

#include "base/macros.h"
#include "base/android/scoped_java_ref.h"
#include "base/android/jni_weak_ref.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "net/dns/host_resolver.h"

using base::android::ScopedJavaLocalRef;
using base::android::JavaParamRef;
using net::AddressList;
using net::HostResolver;

//class base::WaitableEvent;

namespace xwalk {
namespace tenta {

using namespace net;

class HostResolverTenta : public HostResolver {
 protected:
  class SavedRequest;

 public:
  HostResolverTenta(std::unique_ptr<HostResolver> backup_resolver);
  virtual ~HostResolverTenta();

// Resolves the given hostname (or IP address literal), filling out the
// |addresses| object upon success.  The |info.port| parameter will be set as
// the sin(6)_port field of the sockaddr_in{6} struct.  Returns OK if
// successful or an error code upon failure.  Returns
// ERR_NAME_NOT_RESOLVED if hostname is invalid, or if it is an
// incompatible IP literal (e.g. IPv6 is disabled and it is an IPv6
// literal).
//
// If the operation cannot be completed synchronously, ERR_IO_PENDING will
// be returned and the real result code will be passed to the completion
// callback.  Otherwise the result code is returned immediately from this
// call.
//
// If |out_req| is non-NULL, then |*out_req| will be filled with a handle to
// the async request. This handle is not valid after the request has
// completed.
//
// Profiling information for the request is saved to |net_log| if non-NULL.
  virtual int Resolve(const RequestInfo& info, net::RequestPriority priority,
                      net::AddressList* addresses,
                      const CompletionCallback& callback,
                      RequestHandle* out_req, const BoundNetLog& net_log)
                          override;

// Resolves the given hostname (or IP address literal) out of cache or HOSTS
// file (if enabled) only. This is guaranteed to complete synchronously.
// This acts like |Resolve()| if the hostname is IP literal, or cached value
// or HOSTS entry exists. Otherwise, ERR_DNS_CACHE_MISS is returned.
  virtual int ResolveFromCache(const RequestInfo& info, AddressList* addresses,
                               const BoundNetLog& net_log) override;

  // Post a task to another thread to complete the request
  int ResolveFromCacheWithTask(const RequestInfo& info, AddressList* addresses,
                               const BoundNetLog& net_log);

  // JNI direct call to complete the request
  int ResolveFromCacheDirect(const RequestInfo& info, AddressList* addresses,
                             const BoundNetLog& net_log);

// Cancels the specified request. |req| is the handle returned by Resolve().
// After a request is canceled, its completion callback will not be called.
// CancelRequest must NOT be called after the request's completion callback
// has already run or the request was canceled.
  virtual void CancelRequest(RequestHandle req) override;

  /**
   * Called from java to notify request was handled
   */
  virtual void OnResolved(JNIEnv* env, jobject caller, jint status,
                          jobjectArray result, jlong forRequestId);

  /**
   * Called when internal error occured
   */
  virtual void OnError(int64_t key_id, int error);

  /**
   * Call Java to resolve the name
   */
  virtual void DoResolveInJava(SavedRequest *request, int64_t key_id);

  virtual void DoResolveCacheInJava(const RequestInfo& info,
                                    AddressList* addresses,
                                    const BoundNetLog& net_log,
                                    base::WaitableEvent* completion,
                                    bool *success);

 protected:
  AddressList * ConvertIpJava2Native(JNIEnv* env, jobjectArray ipArray);

 protected:

  typedef base::Callback<void(int64_t, int)> OnErrorCallback;
  OnErrorCallback on_error_call_;

  base::WeakPtrFactory<HostResolverTenta> weak_ptr_factory_;

  std::unique_ptr<HostResolver> backup_resolver_;  // chromium implementation for host resolve
  JavaObjectWeakGlobalRef j_host_resolver_;  // java instance for host resolve

// mapping id to resolver
  typedef std::map<int64_t, SavedRequest *> RequestsMap;
  RequestsMap requests_;
  base::Lock mapGuard;

  /**
   * Thread to run dns requests
   */
  scoped_refptr<base::TaskRunner> task_runner_;

  scoped_refptr<base::TaskRunner> orig_runner_;

 private:
  /**
   * Do real work on original thread
   */
  void origOnResolved(int64_t forRequestId, int error, AddressList* addr_list);

  DISALLOW_COPY_AND_ASSIGN(HostResolverTenta)
  ;

};

bool RegisterHostResolverTentaNative(JNIEnv* env);

} /* namespace tenta */
} /* namespace xwalk */

#endif /* XWALK_RUNTIME_NET_HOST_RESOLVER_TENTA_H_ */
