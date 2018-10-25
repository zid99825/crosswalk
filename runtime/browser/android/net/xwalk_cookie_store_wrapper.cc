// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/android/net/xwalk_cookie_store_wrapper.h"

#include <string>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "url/gurl.h"
#include "xwalk/runtime/browser/android/net/init_native_callback.h"

namespace xwalk {

namespace {

// Posts |task| to the thread that the global CookieStore lives on.
void PostTaskToCookieStoreTaskRunner(base::OnceClosure task) {
  GetCookieStoreTaskRunner()->PostTask(FROM_HERE, std::move(task));
}

class XwalkCookieChangedSubscription
    : public net::CookieStore::CookieChangedSubscription {
 public:
  explicit XwalkCookieChangedSubscription(
      std::unique_ptr<net::CookieStore::CookieChangedCallbackList::Subscription>
          subscription)
      : subscription_(std::move(subscription)) {}

 private:
  std::unique_ptr<net::CookieStore::CookieChangedCallbackList::Subscription>
      subscription_;

  DISALLOW_COPY_AND_ASSIGN(XwalkCookieChangedSubscription);
};

// Wraps a subscription to cookie change notifications for the global
// CookieStore for a consumer that lives on another thread. Handles passing
// messages between thread, and destroys itself when the consumer unsubscribes.
// Must be created on the consumer's thread. Each instance only supports a
// single subscription.
class SubscriptionWrapper {
 public:
  SubscriptionWrapper() : weak_factory_(this) {}

  std::unique_ptr<net::CookieStore::CookieChangedSubscription> Subscribe(
      const GURL& url, const std::string& name, const net::CookieStore::CookieChangedCallback& callback) {
    // This class is only intended to be used for a single subscription.
    DCHECK(callback_list_.empty());

    nested_subscription_ = new NestedSubscription(url, name, weak_factory_.GetWeakPtr());
    return std::make_unique < XwalkCookieChangedSubscription > (callback_list_.Add(callback));
  }

 private:
  // The NestedSubscription is responsible for creating and managing the
  // underlying subscription to the real CookieStore, and posting notifications
  // back to |callback_list_|.
  class NestedSubscription
      : public base::RefCountedDeleteOnSequence<NestedSubscription> {
   public:
    NestedSubscription(const GURL& url,
                       const std::string& name,
                       base::WeakPtr<SubscriptionWrapper> subscription_wrapper)
        : base::RefCountedDeleteOnSequence<NestedSubscription>(
              GetCookieStoreTaskRunner()),
          subscription_wrapper_(subscription_wrapper),
          client_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
      PostTaskToCookieStoreTaskRunner(
          base::Bind(&NestedSubscription::Subscribe, this, url, name));
    }

   private:
    friend class base::RefCountedDeleteOnSequence<NestedSubscription>;
    friend class base::DeleteHelper<NestedSubscription>;

    ~NestedSubscription() {}

    void Subscribe(const GURL& url, const std::string& name) {
      GetCookieStore()->AddCallbackForCookie(
          url, name, base::Bind(&NestedSubscription::OnChanged, this));
    }

    void OnChanged(const net::CanonicalCookie& cookie, net::CookieStore::ChangeCause cause) {
      client_task_runner_->PostTask(
          FROM_HERE, base::Bind(&SubscriptionWrapper::OnChanged,
                                subscription_wrapper_, cookie, cause));
    }

    base::WeakPtr<SubscriptionWrapper> subscription_wrapper_;
    scoped_refptr<base::TaskRunner> client_task_runner_;

    std::unique_ptr<net::CookieStore::CookieChangedSubscription> subscription_;

    DISALLOW_COPY_AND_ASSIGN(NestedSubscription);
  };

  void OnChanged(const net::CanonicalCookie& cookie, net::CookieStore::ChangeCause cause) {
    callback_list_.Notify(cookie, cause);
  }

  // The "list" only had one entry, so can just clean up now.
  void OnUnsubscribe() { delete this; }

  scoped_refptr<NestedSubscription> nested_subscription_;
  net::CookieStore::CookieChangedCallbackList callback_list_;
  base::WeakPtrFactory<SubscriptionWrapper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SubscriptionWrapper);
};

void SetCookieWithOptionsAsyncOnCookieThread(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    net::CookieStore::SetCookiesCallback callback) {
  DLOG(INFO) << "Setting cookie Async in cookie_store_wrapper";
  GetCookieStore()->SetCookieWithOptionsAsync(url, cookie_line, options,
                                              std::move(callback));
}

void SetCanonicalCookieAsyncOnCookieThread(std::unique_ptr<net::CanonicalCookie> cookie,
                                           bool secure_source,
                                           bool modify_http_only,
                                           net::CookieStore::SetCookiesCallback callback ) {

  GetCookieStore()->SetCanonicalCookieAsync(std::move(cookie), secure_source, modify_http_only, std::move(callback));
}

void GetCookiesWithOptionsAsyncOnCookieThread(
    const GURL& url,
    const net::CookieOptions& options,
    net::CookieStore::GetCookiesCallback callback) {
  GetCookieStore()->GetCookiesWithOptionsAsync(url, options, std::move(callback));
}

void GetCookieListWithOptionsAsyncOnCookieThread(
    const GURL& url,
    const net::CookieOptions& options,
    net::CookieStore::GetCookieListCallback callback) {
  GetCookieStore()->GetCookieListWithOptionsAsync(url, options, std::move(callback));
}

void GetAllCookiesAsyncOnCookieThread(
    net::CookieStore::GetCookieListCallback callback) {
  GetCookieStore()->GetAllCookiesAsync(std::move(callback));
}

void DeleteCookieAsyncOnCookieThread(const GURL& url,
                                     const std::string& cookie_name,
                                     base::OnceClosure callback) {
  GetCookieStore()->DeleteCookieAsync(url, cookie_name, std::move(callback));
}

void DeleteCanonicalCookieAsyncOnCookieThread(
    const net::CanonicalCookie& cookie,
    net::CookieStore::DeleteCallback callback) {
  GetCookieStore()->DeleteCanonicalCookieAsync(cookie, std::move(callback));
}

void DeleteAllCreatedBetweenAsyncOnCookieThread(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    net::CookieStore::DeleteCallback callback) {
  GetCookieStore()->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                                 std::move(callback));
}

void DeleteAllCreatedBetweenWithPredicateAsyncOnCookieThread(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const net::CookieStore::CookiePredicate& predicate,
    net::CookieStore::DeleteCallback callback) {
  GetCookieStore()->DeleteAllCreatedBetweenWithPredicateAsync(
      delete_begin, delete_end, predicate, std::move(callback));
}

void DeleteSessionCookiesAsyncOnCookieThread(
    net::CookieStore::DeleteCallback callback) {
  GetCookieStore()->DeleteSessionCookiesAsync(std::move(callback));
}

void FlushStoreOnCookieThread(base::OnceClosure callback) {
  GetCookieStore()->FlushStore(std::move(callback));
}

/**
 *
 */
void AddCallbackForAllChangesOnCookieThread(
    base::WaitableEvent* completion,
    net::CookieStore::CookieChangedSubscription **result,
    const net::CookieStore::CookieChangedCallback& callback) {

  if ( result != nullptr ) {
    *result = GetCookieStore()->AddCallbackForAllChanges(callback).release();
  }
  completion->Signal();
}

void SetForceKeepSessionStateOnCookieThread() {
  GetCookieStore()->SetForceKeepSessionState();
}

void TriggerCookieFetchOnCookieThread() {
  GetCookieStore()->TriggerCookieFetch();
}

}  // namespace

XWalkCookieStoreWrapper::XWalkCookieStoreWrapper()
    : client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {}

XWalkCookieStoreWrapper::~XWalkCookieStoreWrapper() {}

void XWalkCookieStoreWrapper::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    net::CookieStore::SetCookiesCallback callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&SetCookieWithOptionsAsyncOnCookieThread, url, cookie_line,
                 options, std::move(callback)));
}

/**
 *
 */
void XWalkCookieStoreWrapper::SetCanonicalCookieAsync(std::unique_ptr<net::CanonicalCookie> cookie,
                                         bool secure_source,
                                         bool modify_http_only,
                                         SetCookiesCallback callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
        base::BindOnce(&SetCanonicalCookieAsyncOnCookieThread, std::move(cookie), secure_source,
                   modify_http_only,
                   std::move(callback)));
}

void XWalkCookieStoreWrapper::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookiesCallback callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&GetCookiesWithOptionsAsyncOnCookieThread, url, options,
                 std::move(callback)));
}

void XWalkCookieStoreWrapper::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&GetCookieListWithOptionsAsyncOnCookieThread, url, options,
                 std::move(callback)));
}

void XWalkCookieStoreWrapper::GetAllCookiesAsync(
    GetCookieListCallback callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&GetAllCookiesAsyncOnCookieThread,
                 std::move(callback)));
}

void XWalkCookieStoreWrapper::DeleteCookieAsync(const GURL& url,
    const std::string& cookie_name,
    base::OnceClosure callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&DeleteCookieAsyncOnCookieThread, url, cookie_name,
                 std::move(callback)));
}

void XWalkCookieStoreWrapper::DeleteCanonicalCookieAsync(
    const net::CanonicalCookie& cookie,
    DeleteCallback callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&DeleteCanonicalCookieAsyncOnCookieThread, cookie,
                 std::move(callback)));
}

void XWalkCookieStoreWrapper::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin, const base::Time& delete_end,
    DeleteCallback callback) {

  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&DeleteAllCreatedBetweenAsyncOnCookieThread, delete_begin,
                     delete_end, std::move(callback)));
}

void XWalkCookieStoreWrapper::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const net::CookieStore::CookiePredicate& predicate,
    DeleteCallback callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(base::BindOnce(
      &DeleteAllCreatedBetweenWithPredicateAsyncOnCookieThread,
      delete_begin,
      delete_end,
      predicate,
      std::move(callback)));
}

void XWalkCookieStoreWrapper::DeleteSessionCookiesAsync(
    DeleteCallback callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
      base::BindOnce(&DeleteSessionCookiesAsyncOnCookieThread,
                 std::move(callback)));
}

void XWalkCookieStoreWrapper::FlushStore(base::OnceClosure callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(base::BindOnce(
      &FlushStoreOnCookieThread, std::move(callback)));
}

void XWalkCookieStoreWrapper::SetForceKeepSessionState() {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  PostTaskToCookieStoreTaskRunner(
      base::Bind(&SetForceKeepSessionStateOnCookieThread));
}

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
XWalkCookieStoreWrapper::AddCallbackForCookie(
    const GURL& url,
    const std::string& name,
    const CookieChangedCallback& callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());

  // The SubscriptionWrapper is owned by the subscription itself, and has no
  // connection to the XWalkCookieStoreWrapper after creation. Other CookieStore
  // implementations DCHECK if a subscription outlasts the cookie store,
  // unfortunately, this design makes DCHECKing if there's an outstanding
  // subscription when the XWalkCookieStoreWrapper is destroyed a bit ugly.
  // TODO(mmenke):  Still worth adding a DCHECK?
  SubscriptionWrapper* subscription = new SubscriptionWrapper();
  return subscription->Subscribe(url, name, callback);
}

/**
 *
 */
std::unique_ptr<net::CookieStore::CookieChangedSubscription> XWalkCookieStoreWrapper::AddCallbackForAllChanges(
        const CookieChangedCallback& callback) {
  base::WaitableEvent completion(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED);

  net::CookieStore::CookieChangedSubscription * result;

  GetCookieStoreTaskRunner()->PostTask(FROM_HERE, base::Bind(
        &AddCallbackForAllChangesOnCookieThread,
        &completion,
        &result,
        callback));
  completion.Wait();

  return base::WrapUnique(result);
}

bool XWalkCookieStoreWrapper::IsEphemeral() {
  return GetCookieStore()->IsEphemeral();
}

void XWalkCookieStoreWrapper::TriggerCookieFetch() {
  PostTaskToCookieStoreTaskRunner(base::Bind(&TriggerCookieFetchOnCookieThread));
}

base::Closure XWalkCookieStoreWrapper::CreateWrappedClosureCallback(
    const base::Closure& callback) {
  if (callback.is_null())
    return callback;
  return base::Bind(base::IgnoreResult(&base::TaskRunner::PostTask),
                    client_task_runner_, FROM_HERE,
                    base::Bind(&XWalkCookieStoreWrapper::RunClosureCallback,
                               weak_factory_.GetWeakPtr(), callback));
}

void XWalkCookieStoreWrapper::RunClosureCallback(
    const base::Closure& callback) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  callback.Run();
}

}  // namespace xwalk
