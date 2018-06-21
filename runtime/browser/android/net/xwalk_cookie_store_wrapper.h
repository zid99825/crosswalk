// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef XWALK_RUNTIME_BROWSER_ANDROID_NET_XWALK_COOKIE_STORE_WRAPPER_H_
#define XWALK_RUNTIME_BROWSER_ANDROID_NET_XWALK_COOKIE_STORE_WRAPPER_H_

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_store.h"

namespace xwalk {

// A cross-threaded cookie store implementation that wraps the CookieManager's
// CookieStore. It posts tasks to the CookieStore's thread, and invokes
// callbacks on the originating thread. Deleting it cancels pending callbacks.
// This is needed to allow Webview to run the CookieStore on its own thread, to
// enable synchronous calls into the store on the IO Thread from Java.
//
// XWalkCookieStoreWrapper will only grab the CookieStore pointer from the
// CookieManager when it's needed, allowing for lazy creation of the
// CookieStore.
//
// XWalkCookieStoreWrapper may only be called from the thread on which it's
// created.
//
// The CookieManager must outlive the XWalkCookieStoreWrapper.
class XWalkCookieStoreWrapper : public net::CookieStore {
 public:
  XWalkCookieStoreWrapper();
  ~XWalkCookieStoreWrapper() override;
  // CookieStore implementation:
  void SetCookieWithOptionsAsync(const GURL& url,
                                 const std::string& cookie_line,
                                 const net::CookieOptions& options,
                                 SetCookiesCallback callback) override;

  void SetCanonicalCookieAsync(std::unique_ptr<net::CanonicalCookie> cookie,
                                         bool secure_source,
                                         bool modify_http_only,
                                         SetCookiesCallback callback) override;

  void GetCookiesWithOptionsAsync(const GURL& url,
                                  const net::CookieOptions& options,
                                  GetCookiesCallback callback) override;
  void GetCookieListWithOptionsAsync(
      const GURL& url,
      const net::CookieOptions& options,
      GetCookieListCallback callback) override;
  void GetAllCookiesAsync(GetCookieListCallback callback) override;
  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         base::OnceClosure callback) override;
  void DeleteCanonicalCookieAsync(const net::CanonicalCookie& cookie,
                                  DeleteCallback callback) override;
  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    DeleteCallback callback) override;
  void DeleteAllCreatedBetweenWithPredicateAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const net::CookieStore::CookiePredicate& predicate,
      DeleteCallback callback) override;
  void DeleteSessionCookiesAsync(DeleteCallback callback) override;
  void FlushStore(base::OnceClosure callback) override;
  void SetForceKeepSessionState() override;
  std::unique_ptr<CookieChangedSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const CookieChangedCallback& callback) override;

  std::unique_ptr<CookieChangedSubscription> AddCallbackForAllChanges(
        const CookieChangedCallback& callback) override;

  bool IsEphemeral() override;

  void TriggerCookieFetch() override;

 private:
  // Used by CreateWrappedCallback below. Takes an arugment of Type and posts
  // a task to |task_runner| to invoke |callback| with that argument. If
  // |weak_cookie_store| is deleted before the task is run, the task will not
  // be run.
  template <class Type>
  static void RunCallbackOnClientThread(
      base::TaskRunner* task_runner,
      base::WeakPtr<XWalkCookieStoreWrapper> weak_cookie_store,
      base::Callback<void(Type)> callback,
      Type argument) {
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(&XWalkCookieStoreWrapper::RunClosureCallback,
                   weak_cookie_store, base::Bind(callback, argument)));
  }

  // Returns a base::Callback that takes an argument of Type and posts a task to
  // the |client_task_runner_| to invoke |callback| with that argument.
  template <class Type>
  base::Callback<void(Type)> CreateWrappedCallback(
      base::Callback<void(Type)> callback) {
    if (callback.is_null())
      return callback;
    return base::Bind(&XWalkCookieStoreWrapper::RunCallbackOnClientThread<Type>,
                      base::RetainedRef(client_task_runner_),
                      weak_factory_.GetWeakPtr(), callback);
  }

  // Returns a base::Closure that posts a task to the |client_task_runner_| to
  // invoke |callback|.
  base::Closure CreateWrappedClosureCallback(const base::Closure& callback);

  // Runs |callback|. Used to prevent callbacks from being invoked after the
  // XWalkCookieStoreWrapper has been destroyed.
  void RunClosureCallback(const base::Closure& callback);

  scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;

  base::WeakPtrFactory<XWalkCookieStoreWrapper> weak_factory_;
};

}  // namespace xwalk

#endif  // XWALK_RUNTIME_BROWSER_ANDROID_NET_XWALK_COOKIE_STORE_WRAPPER_H_
