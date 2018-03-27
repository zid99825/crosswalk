// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "xwalk/runtime/browser/android/xwalk_content.h"

#include <memory>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/base_paths_android.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
//#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"

#include "ui/gfx/geometry/rect_f.h"
#include "xwalk/application/common/application_manifest_constants.h"
#include "xwalk/application/common/manifest.h"
#include "xwalk/runtime/browser/android/net_disk_cache_remover.h"
#include "xwalk/runtime/browser/android/state_serializer.h"
#include "xwalk/runtime/browser/android/xwalk_autofill_client_android.h"
#include "xwalk/runtime/browser/android/xwalk_content_lifecycle_notifier.h"
#include "xwalk/runtime/browser/android/xwalk_contents_client_bridge.h"
#include "xwalk/runtime/browser/android/xwalk_contents_client_bridge_base.h"
#include "xwalk/runtime/browser/android/xwalk_contents_io_thread_client_impl.h"
#include "xwalk/runtime/browser/android/xwalk_web_contents_delegate.h"
#include "xwalk/runtime/browser/runtime_resource_dispatcher_host_delegate_android.h"
#include "xwalk/runtime/browser/xwalk_autofill_manager.h"
#include "xwalk/runtime/browser/xwalk_browser_context.h"
#include "xwalk/runtime/browser/xwalk_runner.h"
#include "jni/XWalkContent_jni.h"
//#include "xwalk/third_party/tenta/file_blocks/db_fs_manager.h"
//#include "xwalk/third_party/tenta/file_blocks/db_file_system.h"
//#include "xwalk/third_party/tenta/file_blocks/db_file.h"
#include "xwalk/third_party/tenta/meta_fs/meta_errors.h"
#include "xwalk/third_party/tenta/meta_fs/meta_db.h"
#include "xwalk/third_party/tenta/meta_fs/meta_file.h"
#include "xwalk/third_party/tenta/meta_fs/a_cancellable_read_listener.h"
#include "xwalk/third_party/tenta/meta_fs/jni/meta_fs_manager.h"
#include "xwalk/third_party/tenta/meta_fs/jni/meta_file_system.h"
#include "xwalk/third_party/tenta/meta_fs/jni/meta_virtual_file.h"
#include "xwalk/third_party/tenta/meta_fs/jni/java_byte_array.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;
using content::WebContents;
using navigation_interception::InterceptNavigationDelegate;
using xwalk::application_manifest_keys::kDisplay;

namespace keys = xwalk::application_manifest_keys;
namespace metafs = ::tenta::fs;

namespace xwalk {

namespace {
const int cBlkSize = 4 * 1024;

// database name for history
static const std::string cHistoryDb = "c22b0c42-90d5-4a1e-9565-79cbab3b22dd";
static const int cHistoryBlockSize = ::tenta::fs::MetaDb::BLOCK_64K;

const void* kXWalkContentUserDataKey = &kXWalkContentUserDataKey;

// TODO make a template AutoCallClose
/**
 *
 */
class AutoCloseMetaFile {
 public:
  AutoCloseMetaFile(scoped_refptr<metafs::MetaFile>& mvf)
      : _mvf(mvf) {
  }
  ~AutoCloseMetaFile() {
    if (_mvf.get() != nullptr) {
      _mvf->Close();
    }
  }
 private:
  scoped_refptr<metafs::MetaFile>& _mvf;
};

class AutoCloseMetaDb {
 public:
  AutoCloseMetaDb(scoped_refptr<metafs::MetaDb>& db)
      : _db(db) {
  }
  ~AutoCloseMetaDb() {
    if (_db.get() != nullptr) {
      ::tenta::fs::MetaFsManager * mng = ::tenta::fs::MetaFsManager::GetInstance();
      mng->CloseDb(cHistoryDb);
    }
  }
 private:
  scoped_refptr<metafs::MetaDb>& _db;
};

/**
 *
 */
class MetaReadToPickle : public metafs::ACancellableReadListener {
 public:
  MetaReadToPickle(base::Pickle& pickle)
      : ACancellableReadListener(ScopedJavaGlobalRef<jobject>()),
        _pickle_to_fill(pickle) {

  }

  void OnData(const char *data, int length) {
    _pickle_to_fill.WriteBytes(data, length);
  }
  void OnStart(const std::string &guid, int length) {
  }
  void OnProgress(float percent) {
  }
  void OnDone(int status) {
  }
  bool wasCancelledNotify(int* outStatus = nullptr) {
    return false;
  }
 private:
  base::Pickle& _pickle_to_fill;
};

/**
 *
 */
class XWalkContentUserData : public base::SupportsUserData::Data {
 public:
  explicit XWalkContentUserData(XWalkContent* ptr)
      : content_(ptr) {
  }
  static XWalkContent* GetContents(content::WebContents* web_contents) {
    if (!web_contents)
      return nullptr;
    XWalkContentUserData* data = reinterpret_cast<XWalkContentUserData*>(web_contents->GetUserData(
        kXWalkContentUserDataKey));
    return data ? data->content_ : nullptr;
  }

 private:
  XWalkContent* content_;
};

// FIXME(wang16): Remove following methods after deprecated fields
// are not supported any more.
void PrintManifestDeprecationWarning(std::string field) {
  LOG(WARNING) << "\"" << field << "\" is deprecated for Crosswalk. " << "Please follow "
               << "https://www.crosswalk-project.org/#documentation/manifest.";
}

bool ManifestHasPath(const xwalk::application::Manifest& manifest, const std::string& path,
                     const std::string& deprecated_path) {
  if (manifest.HasPath(path))
    return true;
  if (manifest.HasPath(deprecated_path)) {
    PrintManifestDeprecationWarning(deprecated_path);
    return true;
  }
  return false;
}

bool ManifestGetString(const xwalk::application::Manifest& manifest, const std::string& path,
                       const std::string& deprecated_path, std::string* out_value) {
  if (manifest.GetString(path, out_value))
    return true;
  if (manifest.GetString(deprecated_path, out_value)) {
    PrintManifestDeprecationWarning(deprecated_path);
    return true;
  }
  return false;
}

}  // namespace

// static
XWalkContent* XWalkContent::FromID(int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::RenderViewHost* rvh = content::RenderViewHost::FromID(render_process_id, render_view_id);
  if (!rvh)
    return NULL;
  content::WebContents* web_contents = content::WebContents::FromRenderViewHost(rvh);
  if (!web_contents)
    return NULL;
  return FromWebContents(web_contents);
}

// static
XWalkContent* XWalkContent::FromWebContents(content::WebContents* web_contents) {
  return XWalkContentUserData::GetContents(web_contents);
}

XWalkContent::XWalkContent(std::unique_ptr<content::WebContents> web_contents)
    : web_contents_(std::move(web_contents)) {
  xwalk_autofill_manager_.reset(new XWalkAutofillManager(web_contents_.get()));
  XWalkContentLifecycleNotifier::OnXWalkViewCreated();
}

void XWalkContent::SetXWalkAutofillClient(jobject client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_XWalkContent_setXWalkAutofillClient(env, obj.obj(), client);
}

void XWalkContent::SetSaveFormData(bool enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  xwalk_autofill_manager_->InitAutofillIfNecessary(enabled);
  // We need to check for the existence, since autofill_manager_delegate
  // may not be created when the setting is false.
  if (auto client = XWalkAutofillClientAndroid::FromWebContents(web_contents_.get()))
    client->SetSaveFormData(enabled);
}

XWalkContent::~XWalkContent() {
  XWalkContentLifecycleNotifier::OnXWalkViewDestroyed();
}

void XWalkContent::SetJavaPeers(JNIEnv* env, jobject obj, jobject xwalk_content, jobject web_contents_delegate,
                                jobject contents_client_bridge, jobject io_thread_client,
                                jobject intercept_navigation_delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  java_ref_ = JavaObjectWeakGlobalRef(env, xwalk_content);

  web_contents_delegate_.reset(new XWalkWebContentsDelegate(env, web_contents_delegate));
  contents_client_bridge_.reset(new XWalkContentsClientBridge(env, contents_client_bridge, web_contents_.get()));

  web_contents_->SetUserData(kXWalkContentUserDataKey, new XWalkContentUserData(this));

  XWalkContentsIoThreadClientImpl::RegisterPendingContents(web_contents_.get());

  // XWalk does not use disambiguation popup for multiple targets.
  content::RendererPreferences* prefs = web_contents_->GetMutableRendererPrefs();
  prefs->tap_multiple_targets_strategy = content::TAP_MULTIPLE_TARGETS_STRATEGY_NONE;

  XWalkContentsClientBridgeBase::Associate(web_contents_.get(), contents_client_bridge_.get());
  XWalkContentsIoThreadClientImpl::Associate(web_contents_.get(), ScopedJavaLocalRef<jobject>(env, io_thread_client));
  int render_process_id = web_contents_->GetRenderProcessHost()->GetID();
  int render_frame_id = web_contents_->GetRenderViewHost()->GetRoutingID();
  RuntimeResourceDispatcherHostDelegateAndroid::OnIoThreadClientReady(render_process_id, render_frame_id);
  InterceptNavigationDelegate::Associate(
      web_contents_.get(), base::WrapUnique(new InterceptNavigationDelegate(env, intercept_navigation_delegate)));
  web_contents_->SetDelegate(web_contents_delegate_.get());

  render_view_host_ext_.reset(new XWalkRenderViewHostExt(web_contents_.get()));
}

base::android::ScopedJavaLocalRef<jobject> XWalkContent::GetWebContents(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(web_contents_);
  if (!web_contents_)
    return base::android::ScopedJavaLocalRef<jobject>();
  return web_contents_->GetJavaWebContents();
}

void XWalkContent::SetPendingWebContentsForPopup(std::unique_ptr<content::WebContents> pending) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (pending_contents_.get()) {
    // TODO(benm): Support holding multiple pop up window requests.
    LOG(WARNING) << "Blocking popup window creation as an outstanding " << "popup window is still pending.";
    base::MessageLoop::current()->task_runner()->DeleteSoon(FROM_HERE, pending.release());
    return;
  }
  pending_contents_.reset(new XWalkContent(std::move(pending)));
}

jlong XWalkContent::ReleasePopupXWalkContent(JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return reinterpret_cast<intptr_t>(pending_contents_.release());
}

void XWalkContent::ClearCache(JNIEnv* env, jobject obj, jboolean include_disk_files) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  render_view_host_ext_->ClearCache();

  if (include_disk_files)
    RemoveHttpDiskCache(web_contents_->GetRenderProcessHost(), std::string());
}

void XWalkContent::ClearCacheForSingleFile(JNIEnv* env, jobject obj, jstring url) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string key = base::android::ConvertJavaStringToUTF8(env, url);

  if (!key.empty())
    RemoveHttpDiskCache(web_contents_->GetRenderProcessHost(), key);
}

ScopedJavaLocalRef<jstring> XWalkContent::DevToolsAgentId(JNIEnv* env, jobject obj) {
  scoped_refptr<content::DevToolsAgentHost> agent_host(content::DevToolsAgentHost::GetOrCreateFor(web_contents_.get()));
  return base::android::ConvertUTF8ToJavaString(env, agent_host->GetId());
}

void XWalkContent::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void XWalkContent::RequestNewHitTestDataAt(JNIEnv* env, jobject obj, jfloat x, jfloat y, jfloat touch_major) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  gfx::PointF touch_center(x, y);
  gfx::SizeF touch_area(touch_major, touch_major);
  render_view_host_ext_->RequestNewHitTestDataAt(touch_center, touch_area);
}

void XWalkContent::UpdateLastHitTestData(JNIEnv* env, jobject obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!render_view_host_ext_->HasNewHitTestData())
    return;

  const XWalkHitTestData& data = render_view_host_ext_->GetLastHitTestData();
  render_view_host_ext_->MarkHitTestDataRead();

  // Make sure to null the java object if data is empty/invalid
  ScopedJavaLocalRef<jstring> extra_data_for_type;
  if (data.extra_data_for_type.length())
    extra_data_for_type = ConvertUTF8ToJavaString(env, data.extra_data_for_type);

  ScopedJavaLocalRef<jstring> href;
  if (data.href.length())
    href = ConvertUTF16ToJavaString(env, data.href);

  ScopedJavaLocalRef<jstring> anchor_text;
  if (data.anchor_text.length())
    anchor_text = ConvertUTF16ToJavaString(env, data.anchor_text);

  ScopedJavaLocalRef<jstring> img_src;
  if (data.img_src.is_valid())
    img_src = ConvertUTF8ToJavaString(env, data.img_src.spec());
  Java_XWalkContent_updateHitTestData(env, obj, data.type, extra_data_for_type.obj(), href.obj(), anchor_text.obj(),
                                      img_src.obj());
}

ScopedJavaLocalRef<jstring> XWalkContent::GetVersion(JNIEnv* env, jobject obj) {
  return base::android::ConvertUTF8ToJavaString(env, XWALK_VERSION);
}

static ScopedJavaLocalRef<jstring> GetChromeVersion(JNIEnv* env, const base::android::JavaParamRef<jclass>& jcaller) {
  LOG(INFO) << "GetChromeVersion=" << version_info::GetVersionNumber();
  return base::android::ConvertUTF8ToJavaString(env, version_info::GetVersionNumber());
}

void XWalkContent::SetJsOnlineProperty(JNIEnv* env, jobject obj, jboolean network_up) {
  render_view_host_ext_->SetJsOnlineProperty(network_up);
}

jboolean XWalkContent::SetManifest(JNIEnv* env, jobject obj, jstring path, jstring manifest_string) {
  std::string path_str = base::android::ConvertJavaStringToUTF8(env, path);
  std::string json_input = base::android::ConvertJavaStringToUTF8(env, manifest_string);

  std::unique_ptr<base::Value> manifest_value = base::JSONReader::Read(json_input);
  if (!manifest_value || !manifest_value->IsType(base::Value::Type::DICTIONARY))
    return false;

  xwalk::application::Manifest manifest(
      base::WrapUnique(static_cast<base::DictionaryValue*>(manifest_value.release())));

  std::string url;
  if (manifest.GetString(keys::kStartURLKey, &url)) {
    std::string scheme = GURL(url).scheme();
    if (scheme.empty())
      url = path_str + url;
  } else if (manifest.GetString(keys::kLaunchLocalPathKey, &url)) {
    PrintManifestDeprecationWarning(keys::kLaunchLocalPathKey);
    // According to original proposal for "app:launch:local_path", the "http"
    // and "https" schemes are supported. So |url| should do nothing when it
    // already has "http" or "https" scheme.
    std::string scheme = GURL(url).scheme();
    if (scheme != url::kHttpScheme && scheme != url::kHttpsScheme)
      url = path_str + url;
  } else if (manifest.GetString(keys::kLaunchWebURLKey, &url)) {
    PrintManifestDeprecationWarning(keys::kLaunchWebURLKey);
  } else {
    NOTIMPLEMENTED()
    ;
  }

  std::string match_patterns;
  const base::ListValue* xwalk_hosts = NULL;
  if (manifest.GetList(xwalk::application_manifest_keys::kXWalkHostsKey, &xwalk_hosts)) {
    base::JSONWriter::Write(*xwalk_hosts, &match_patterns);
  }
  render_view_host_ext_->SetOriginAccessWhitelist(url, match_patterns);

  std::string csp;
  ManifestGetString(manifest, keys::kCSPKey, keys::kDeprecatedCSPKey, &csp);
  XWalkBrowserContext* browser_context = XWalkRunner::GetInstance()->browser_context();
  CHECK(browser_context);
  browser_context->SetCSPString(csp);

  ScopedJavaLocalRef<jstring> url_buffer = base::android::ConvertUTF8ToJavaString(env, url);

  if (manifest.HasPath(kDisplay)) {
    std::string display_string;
    if (manifest.GetString(kDisplay, &display_string)) {
      // TODO(David): update the handling process of the display strings
      // including fullscreen etc.
      bool display_as_fullscreen = base::LowerCaseEqualsASCII(display_string, "fullscreen");
      Java_XWalkContent_onGetFullscreenFlagFromManifest(env, obj, display_as_fullscreen ? JNI_TRUE : JNI_FALSE);
    }
  }

  // Check whether need to display launch screen. (Read from manifest.json)
  if (ManifestHasPath(manifest, keys::kXWalkLaunchScreen, keys::kLaunchScreen)) {
    std::string ready_when;
    // Get the value of 'ready_when' from manifest.json
    ManifestGetString(manifest, keys::kXWalkLaunchScreenReadyWhen, keys::kLaunchScreenReadyWhen, &ready_when);
    ScopedJavaLocalRef<jstring> ready_when_buffer = base::android::ConvertUTF8ToJavaString(env, ready_when);

    // Get the value of 'image_border'
    // 1. When 'launch_screen.[orientation]' was defined, but no 'image_border'
    //    The value of 'image_border' will be set as 'empty'.
    // 2. Otherwise, there is no 'launch_screen.[orientation]' defined,
    //    The value of 'image_border' will be empty.
    const char empty[] = "empty";
    std::string image_border_default;
    ManifestGetString(manifest, keys::kXWalkLaunchScreenImageBorderDefault, keys::kLaunchScreenImageBorderDefault,
                      &image_border_default);
    if (image_border_default.empty()
        && ManifestHasPath(manifest, keys::kXWalkLaunchScreenDefault, keys::kLaunchScreenDefault)) {
      image_border_default = empty;
    }

    std::string image_border_landscape;
    ManifestGetString(manifest, keys::kXWalkLaunchScreenImageBorderLandscape, keys::kLaunchScreenImageBorderLandscape,
                      &image_border_landscape);
    if (image_border_landscape.empty()
        && ManifestHasPath(manifest, keys::kXWalkLaunchScreenLandscape, keys::kLaunchScreenLandscape)) {
      image_border_landscape = empty;
    }

    std::string image_border_portrait;
    ManifestGetString(manifest, keys::kXWalkLaunchScreenImageBorderPortrait, keys::kLaunchScreenImageBorderPortrait,
                      &image_border_portrait);
    if (image_border_portrait.empty()
        && ManifestHasPath(manifest, keys::kXWalkLaunchScreenPortrait, keys::kLaunchScreenPortrait)) {
      image_border_portrait = empty;
    }

    std::string image_border = image_border_default + ';' + image_border_landscape + ';' + image_border_portrait;
    ScopedJavaLocalRef<jstring> image_border_buffer = base::android::ConvertUTF8ToJavaString(env, image_border);

    Java_XWalkContent_onGetUrlAndLaunchScreenFromManifest(env, obj, url_buffer.obj(), ready_when_buffer.obj(),
                                                          image_border_buffer.obj());
  } else {
    // No need to display launch screen, load the url directly.
    Java_XWalkContent_onGetUrlFromManifest(env, obj, url_buffer.obj());
  }
  std::string view_background_color;
  ManifestGetString(manifest, keys::kXWalkViewBackgroundColor, keys::kViewBackgroundColor, &view_background_color);

  if (view_background_color.empty())
    return true;
  unsigned int view_background_color_int = 0;
  if (!base::HexStringToUInt(view_background_color.substr(1), &view_background_color_int)) {
    LOG(ERROR) << "Background color format error! Valid background color"
               "should be(Alpha Red Green Blue): #ff01abcd";
    return false;
  }
  Java_XWalkContent_setBackgroundColor(env, obj, view_background_color_int);
  return true;
}

jint XWalkContent::GetRoutingID(JNIEnv* env, jobject obj) {
  DCHECK(web_contents_.get());
  return web_contents_->GetRenderViewHost()->GetRoutingID();
}

base::android::ScopedJavaLocalRef<jbyteArray> XWalkContent::GetState(JNIEnv* env, jobject obj) {
  if (!web_contents_->GetController().GetEntryCount())
    return ScopedJavaLocalRef<jbyteArray>();

  base::Pickle pickle;
  if (!WriteToPickle(*web_contents_, &pickle)) {
    return ScopedJavaLocalRef<jbyteArray>();
  } else {
    return base::android::ToJavaByteArray(env, reinterpret_cast<const uint8_t*>(pickle.data()), pickle.size());
  }
}

jboolean XWalkContent::SetState(JNIEnv* env, jobject obj, jbyteArray state) {
  std::vector<uint8_t> state_vector;
  base::android::JavaByteArrayToByteVector(env, state, &state_vector);

  base::Pickle pickle(reinterpret_cast<const char*>(&state_vector[0]), state_vector.size());
  base::PickleIterator iterator(pickle);

  return RestoreFromPickle(&iterator, web_contents_.get());
}

/************** History in MetaFs *********************/
/**
 *
 */
int XWalkContent::OpenHistoryFile(JNIEnv* env, const JavaParamRef<jstring>& id, const JavaParamRef<jstring>& key,
                                  scoped_refptr<::tenta::fs::MetaFile>& fileOut,
                                  scoped_refptr<::tenta::fs::MetaDb>& dbOut, int mode) {

  using namespace metafs;
  using namespace ::base::android;

  MetaFsManager * mng = MetaFsManager::GetInstance();
  if (mng == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "MetaFsManager::GetInstance() NULL";
#endif
    return ERR_NULL_POINTER;
  }

  std::string keyStr;

  ConvertJavaStringToUTF8(env, key, &keyStr);

  int result;

  result = mng->OpenDb(cHistoryDb, keyStr, cHistoryBlockSize, dbOut);
  if (result != FS_OK) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "OpenDb failed";
#endif
    return mng->error();
  }

  if (dbOut.get() == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "MetaDb NULL";
#endif
    return ERR_INVALID_POINTER;
  }

  std::string idStr;
  ConvertJavaStringToUTF8(env, id, &idStr);

  scoped_refptr<MetaFile> weak_file;

  int status;
  status = dbOut->OpenFile(idStr, "", weak_file, mode);

  if (status != FS_OK) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "OpenFile '" << idStr << "' failed " << status;
#endif
    return status;
  }

  fileOut = weak_file;
  if (fileOut.get() == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "File pointer NULL";
#endif
    return ERR_INVALID_POINTER;
  }

  return FS_OK;
}

/**
 *
 */
jint XWalkContent::SaveOldHistory(JNIEnv* env, const JavaParamRef<jobject>& jcaller,
                                  const JavaParamRef<jbyteArray>& state, const JavaParamRef<jstring>& id,
                                  const JavaParamRef<jstring>& key) {
  using namespace metafs;

  scoped_refptr<MetaDb> db;
  scoped_refptr<MetaFile> file;
  AutoCloseMetaDb mdbClose(db);
  AutoCloseMetaFile mvfClose(file);

  int status;
  status = OpenHistoryFile(env, id, key, file, db, MetaDb::IO_CREATE_IF_NOT_EXISTS | MetaDb::IO_TRUNCATE_IF_EXISTS);

  if (status != FS_OK) {
    return status;
  }

  std::unique_ptr<JavaByteArray> data(new JavaByteArray());
  if (data.get() == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "SaveOldHistory: JavaByteArray not created, out of memory";
#endif
    return ERR_OUT_MEMORY;
  }

  status = data->init(env, state);
  if (status != FS_OK) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "SaveOldHistory: JavaByteArray init failed " << status;
#endif
    return status;
  }

  int iolen = data->len();
  status = file->Append(data->data(), iolen);
  if (status != FS_OK) {
    return status;
  }

#if TENTA_LOG_ENABLE == 1
  LOG(INFO) << __func__ << "_OK length=" << iolen;
#endif
  return iolen;
}

/**
 *
 */
jint XWalkContent::SaveHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& id,
                               const JavaParamRef<jstring>& key) {

  using namespace metafs;
  using namespace ::base::android;

  std::unique_ptr<base::Pickle> pickle(new base::Pickle());

  if (pickle.get() == nullptr) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "SaveHistory: Pickle not created, out of memory";
#endif
    return ERR_OUT_MEMORY;
  }

  if (!WriteToPickle(*web_contents_, pickle.get())) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "SaveHistory failed";
#endif
    return ERR_XWALK_INTERNAL;  // error occured
  }

  scoped_refptr<MetaDb> db;
  scoped_refptr<MetaFile> file;
  AutoCloseMetaDb mdbClose(db);
  AutoCloseMetaFile mvfClose(file);

  int status;
  status = OpenHistoryFile(env, id, key, file, db, MetaDb::IO_CREATE_IF_NOT_EXISTS | MetaDb::IO_TRUNCATE_IF_EXISTS);

  if (status != FS_OK) {
    return status;
  }

  if (pickle->payload_size() == 0) {  // file truncated allready
    return FS_OK;  // no data to save
  }

  int iolen = pickle->payload_size();
  status = file->Append(pickle->payload(), iolen);
  if (status != FS_OK) {
    return status;
  }

#if TENTA_LOG_ENABLE == 1
  LOG(ERROR) << __func__ << "_OK length=" << iolen;
#endif
  return iolen;
}

/**
 *
 */
jint XWalkContent::RestoreHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& id,
                                  const JavaParamRef<jstring>& key) {
  using namespace metafs;
  using namespace ::base::android;

  scoped_refptr<MetaDb> db;
  scoped_refptr<MetaFile> file;
  AutoCloseMetaDb mdbClose(db);
  AutoCloseMetaFile mvfClose(file);

  int status;
  status = OpenHistoryFile(env, id, key, file, db, MetaDb::IO_CREATE_IF_NOT_EXISTS | MetaDb::IO_OPEN_EXISTING);

  if (status != FS_OK) {
    return status;
  }

  int length = file->length();

  if (length < 0) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "RestoreHistory get file length " << length;
#endif
    return ERR_INVALID_FILE_DATA;
  }

  if (length == 0) {
#if TENTA_LOG_ENABLE == 1
    LOG(INFO) << "RestoreHistory empty " << length;
#endif
    return FS_OK;  // zero data
    // Safe to return since this webView was just created
    // so it has no history to be cleared
  }

  base::Pickle pickle;
  MetaReadToPickle intoPickle(pickle);

  pickle.Reserve(length);  // makes write faster

  status = file->Read(0, nullptr, length, &intoPickle);
  if (status != FS_OK) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "RestoreHistory read error " << status;
#endif
    return status;
  }

  base::PickleIterator iterator(pickle);
  if (!RestoreFromPickle(&iterator, web_contents_.get())) {
#if TENTA_LOG_ENABLE == 1
    LOG(ERROR) << "Restore pickle error " << pickle.size() << " payLoad: " << pickle.payload_size();
#endif
    return ERR_XWALK_INTERNAL;
  }

#if TENTA_LOG_ENABLE == 1
  LOG(ERROR) << __func__ << "_OK length=" << pickle.payload_size();
#endif

  return pickle.payload_size();
}

/**
 *
 */
jint XWalkContent::NukeHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& id,
                               const JavaParamRef<jstring>& key) {
  using namespace metafs;

  scoped_refptr<MetaDb> db;
  scoped_refptr<MetaFile> file;
  AutoCloseMetaDb mdbClose(db);
  AutoCloseMetaFile mvfClose(file);

  int status;
  status = OpenHistoryFile(env, id, key, file, db, MetaDb::IO_OPEN_EXISTING);

  if (status != FS_OK) {
    return status;
  }

  status = file->Delete();

  if ( status != FS_OK ) {
    return status;
  }

#if TENTA_LOG_ENABLE == 1
  LOG(ERROR) << __func__ << "_OK length=" << 0;
#endif

  return FS_OK;
}

/**
 *
 */
jint XWalkContent::ReKeyHistory(JNIEnv* env, const JavaParamRef<jobject>& obj, const JavaParamRef<jstring>& oldKey,
                                const JavaParamRef<jstring>& newKey) {
  using namespace ::tenta::fs;
  using namespace ::base::android;

  MetaFsManager * mng = MetaFsManager::GetInstance();
  if (mng == nullptr) {
    return ERR_NULL_POINTER;
  }

  std::string oldKeyStr;

  ConvertJavaStringToUTF8(env, oldKey, &oldKeyStr);

  scoped_refptr<MetaDb> db;
  AutoCloseMetaDb dbClose(db);
  int result;

  result = mng->OpenDb(cHistoryDb, oldKeyStr, cHistoryBlockSize, db);
  if (result != FS_OK) {
    return mng->error();
  }

  if (db.get() == nullptr) {
    // TODO log error
    return ERR_INVALID_POINTER;
  }

  std::string newKeyStr;

  ConvertJavaStringToUTF8(env, newKey, &newKeyStr);

  result = db->ReKey(newKeyStr);

  return result;
}

/************ End MetaFS ****************/
///**
// *
// */
//jboolean XWalkContent::PushStateWitkKey(JNIEnv* env, jobject obj,
//                                        const JavaParamRef<jbyteArray>& state,
//                                        jstring id, jstring key) {
//  jbyte* _bytes = nullptr;
//
//  if (state.obj() != nullptr) {
//    jsize len = env->GetArrayLength(state.obj());  // array length
//
//    if (len > 0) {
//      _bytes = env->GetByteArrayElements(state.obj(), nullptr);
//
//      // do the dooda
//
//      bool result;
//      result = SaveArrayToDb(reinterpret_cast<const char*>(_bytes), len, env,
//                             id,
//                             key);
//
//      env->ReleaseByteArrayElements(state.obj(), _bytes, JNI_ABORT);
//      return result;
//
//    } else {
//      LOG(WARNING) << "PushStateWitkKey zero length";
//    }
//
//  } else {
//    LOG(WARNING) << "PushStateWitkKey NULL array";
//  }
//
//  return false;
//}
//
///**
// * Save byte array as id's content to DB
// */
//bool XWalkContent::SaveArrayToDb(const char * data, int data_len, JNIEnv* env,
//                                 jstring id,
//                                 jstring key) {
////  std::string strId;  // virtual filename
////  std::string strKey;  // db encryption key
////
////  base::android::ConvertJavaStringToUTF8(env, id, &strId);
////  base::android::ConvertJavaStringToUTF8(env, key, &strKey);
////
////  if (data == nullptr || data_len == 0 || strId.empty() || strKey.empty()) {
////    LOG(WARNING) << "SaveArrayToDb Invalid data len/id/key " << data_len << "/"
////                    << strId << "/" << strKey;
////    return false;
////  }
////
////  std::string data_path;
////  if (!GetHistoryDbPath(data_path))
////                        {
////    return false;
////  }
////
////  using namespace ::tenta::fs;
////
////  bool bResult;
////  DbFsManager * mng = DbFsManager::GetInstance();
////
////  LOG(INFO) << " got instance";
////
////  std::weak_ptr < DbFile > file;
////  std::shared_ptr<DbFileSystem> fs = mng->NewFs(data_path, strKey,
////                                                "73523-019-0000012-53523");
////  // TODO lic move safe place 73523-019-0000012-53523
////
////  if (fs.get() == nullptr) {
////    LOG(ERROR) << "FileSystem not created!";
////    return false;
////  }
////
////  bResult = fs->FileOpen(strId, file,
////                         (DbFileSystem::IO_CREATE_IF_NOT_EXISTS
////                             | DbFileSystem::IO_TRUNCATE_IF_EXISTS));
////
////  if (!bResult) {
////    LOG(ERROR) << "FileOpen failed " << fs->error();
////    return false;
////  }
////
////  std::shared_ptr<DbFile> sfile = file.lock();
////
////  if (file.expired()) {
////    LOG(ERROR) << "FileSystem gone!";
////    return false;
////  }
////
////  bResult = sfile->Append(data, data_len);
////
////  if (!bResult) {
////    LOG(ERROR) << "Error on write " << sfile->error();
////    return false;
////  }
////
////  return sfile->Close();
//  return false;
//}
//
///**
// *
// */
//jboolean XWalkContent::NukeStateWithKey(JNIEnv* env, jobject obj, jstring id,
//                                        jstring key) {
//
////  std::string strId;  // virtual filename
////  std::string strKey;  // db encryption key
////
////  base::android::ConvertJavaStringToUTF8(env, id, &strId);
////  base::android::ConvertJavaStringToUTF8(env, key, &strKey);
////
////  if (strId.empty() || strKey.empty()) {
////    LOG(WARNING) << "NukeStateWithKey Invalid data id/key " << strId << "/"
////                    << strKey;
////    return false;
////  }
////
////  std::string data_path;
////  if (!GetHistoryDbPath(data_path))
////                        {
////    return false;
////  }
////
////  using namespace ::tenta::fs;
////
////  bool bResult;
////  DbFsManager * mng = DbFsManager::GetInstance();
////
////  std::weak_ptr < DbFile > file;
////  std::shared_ptr<DbFileSystem> fs = mng->NewFs(data_path, strKey,
////                                                "73523-019-0000012-53523");
////  // TODO lic move safe place 73523-019-0000012-53523
////
////  if (fs.get() == nullptr) {
////    LOG(ERROR) << "FileSystem not created!";
////    return false;
////  }
////
////  bResult = fs->FileDelete(strId);
////  if (!bResult) {
////    LOG(ERROR) << "NukeStateWithKey delete result " << fs->error();
////    return false;
////  }
////
////  return true;
//  return false;
//}
//
///**
// *
// * @param env
// * @param obj
// * @param oldKey
// * @param newKey
// * @return
// */
//jboolean XWalkContent::RekeyStateWithKey(JNIEnv* env, jobject obj,
//                                         jstring oldKey,
//                                         jstring newKey) {
////  std::string oldKeyStr;  // virtual filename
////  std::string newKeyStr;  // db encryption key
////
////  base::android::ConvertJavaStringToUTF8(env, oldKey, &oldKeyStr);
////  base::android::ConvertJavaStringToUTF8(env, newKey, &newKeyStr);
////
////  if (/*oldKeyStr.empty() || */newKeyStr.empty()) {  // old key can be null/empty if db just created
////#if TENTA_LOG_ENABLE == 1
////    LOG(WARNING) << "RekeyStateWithKey Invalid data old/new key " << oldKeyStr
////                    << "/"
////                    << newKeyStr;
////#endif
////    return false;
////  }
////
////  std::string data_path;
////  if (!GetHistoryDbPath(data_path))
////                        {
////    return false;
////  }
////
////  using namespace ::tenta::fs;
////
////  bool bResult;
////  DbFsManager * mng = DbFsManager::GetInstance();
////
////  std::shared_ptr<DbFileSystem> fs = mng->NewFs(data_path, oldKeyStr,
////                                                "73523-019-0000012-53523");
////  // TODO lic move safe place 73523-019-0000012-53523
////
////  if (fs.get() == nullptr) {
////    LOG(ERROR) << "FileSystem not created!";
////    return false;
////  }
////
////  bResult = fs->Rekey(newKeyStr);
////
////  if (!bResult) {
////#if TENTA_LOG_ENABLE == 1
////    LOG(ERROR) << "Rekey failed " << fs->error();
////#endif
////    return false;
////  }
////
////  return true;
//  return false;
//}
//
///**
// *
// */
//jboolean XWalkContent::SaveStateWithKey(JNIEnv* env, jobject obj, jstring id,
//                                        jstring key) {
////  // TODO make non blocking?!
////  if (!web_contents_->GetController().GetEntryCount()) {
////    return true;  // nothing to save
////  }
////
////  // time to read the pickle
////  base::Pickle pickle;
////  if (!WriteToPickle(*web_contents_, &pickle)) {
////    LOG(ERROR) << "SaveStateWithKey pickle";
////    return false;  // error occured
////  }
////
////  return SaveArrayToDb(pickle.payload(), pickle.payload_size(), env, id, key);
//  return false;
//}
//
///**
// *
// */
//jboolean XWalkContent::RestoreStateWithKey(JNIEnv* env, jobject obj, jstring id,
//                                           jstring key) {
////  std::string strId;  // virtual filename
////  std::string strKey;  // db encryption key
////
////  base::android::ConvertJavaStringToUTF8(env, id, &strId);
////  base::android::ConvertJavaStringToUTF8(env, key, &strKey);
////
////  if (strKey.empty() || strId.empty()) {
////    LOG(ERROR) << "Invalid key ";
////    return false;
////  }
////
////  std::string data_path;
////  if (!GetHistoryDbPath(data_path))
////                        {
////    return false;
////  }
////
////  using namespace ::tenta::fs;
////
////  bool bResult;
////  DbFsManager * mng = DbFsManager::GetInstance();
////
////  std::weak_ptr < DbFile > file;
////  std::shared_ptr<DbFileSystem> fs = mng->NewFs(data_path, strKey,
////                                                "73523-019-0000012-53523");
////  // TODO lic move safe place 73523-019-0000012-53523
////
////  if (fs.get() == nullptr) {
////    LOG(ERROR) << "FileSystem not created!";
////    return false;
////  }
////
////  bResult = fs->FileOpen(strId, file, (DbFileSystem::IO_OPEN_EXISTING));
////
////  if (!bResult) {
////    LOG(ERROR) << "FileOpen failed " << fs->error();
////    return false;
////  }
////
////  std::shared_ptr<DbFile> sfile = file.lock();
////
////  if (file.expired()) {
////    LOG(ERROR) << "FileSystem gone!";
////    return false;
////  }
////
////  int dataLength = 0;
////  dataLength = sfile->get_length();
////
////  if (dataLength <= 0) {
////    LOG(WARNING) << "Won't restore zero length";
////    return false;
////  }
////
////  if (bResult) {
////    // time to read and save to pickle
////
////    char buff[cBlkSize];
////    base::Pickle pickle;
////
////    int inOutLen;
////    int offset = 0;
////
////    while (dataLength) {
////      inOutLen = cBlkSize;
////      bResult = sfile->Read(buff, &inOutLen, offset);
////
////      if (!bResult) {
////        LOG(ERROR) << "Restore read error " << sfile->error();
////        return false;
////      }
////
////      pickle.WriteBytes(buff, inOutLen);
////      offset += inOutLen;
////      dataLength -= inOutLen;
////
////    }  // while
////
////    if (bResult) {  // all ok
////      base::PickleIterator iterator(pickle);
////      if (!RestoreFromPickle(&iterator, web_contents_.get())) {
////        LOG(ERROR) << "Restore pickle error "
////                      << pickle.size() << " pl: " << pickle.payload_size();
////        return false;
////      }
////    }
////  }
////
////  return sfile->Close();
//  return false;
//}
//
///**
// * Fill |out| with app path + history.db
// * @param out storage for file+path storage
// * @return true if success; false otherwise
// */
//bool XWalkContent::GetHistoryDbPath(std::string& out) {
//  // get application path
//  base::FilePath data_path;
//  bool path_ok = base::PathService::Get(base::DIR_ANDROID_APP_DATA, &data_path);
//
//  if (!path_ok) {
//#if TENTA_LOG_ENABLE == 1
//    LOG(ERROR) << "Get app data failed";
//#endif
//    return false;
//  }
//
//  data_path = data_path.Append("history.db");
//  out.assign(data_path.value());
//
//  return true;
//}
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  std::unique_ptr<WebContents> web_contents(
      content::WebContents::Create(content::WebContents::CreateParams(XWalkRunner::GetInstance()->browser_context())));
  return reinterpret_cast<intptr_t>(new XWalkContent(std::move(web_contents)));
}

bool RegisterXWalkContent(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

namespace {

void ShowGeolocationPromptHelperTask(const JavaObjectWeakGlobalRef& java_ref, const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_ref = java_ref.get(env);
  if (j_ref.obj()) {
    ScopedJavaLocalRef<jstring> j_origin(ConvertUTF8ToJavaString(env, origin.spec()));
    Java_XWalkContent_onGeolocationPermissionsShowPrompt(env, j_ref.obj(), j_origin.obj());
  }
}

void ShowGeolocationPromptHelper(const JavaObjectWeakGlobalRef& java_ref, const GURL& origin) {
  JNIEnv* env = AttachCurrentThread();
  if (java_ref.get(env).obj()) {
    content::BrowserThread::PostTask(content::BrowserThread::UI,
    FROM_HERE,
    base::Bind(&ShowGeolocationPromptHelperTask,
        java_ref,
        origin));
  }
}
}
// anonymous namespace

                                     void XWalkContent::ShowGeolocationPrompt(const GURL& requesting_frame, const base::Callback<void(bool)>& callback) {  // NOLINT
      GURL origin = requesting_frame.GetOrigin();
      bool show_prompt = pending_geolocation_prompts_.empty();
      pending_geolocation_prompts_.push_back(OriginCallback(origin, callback));
      if (show_prompt) {
        ShowGeolocationPromptHelper(java_ref_, origin);
      }
    }

// Called by Java.
void XWalkContent::InvokeGeolocationCallback(JNIEnv* env, jobject obj, jboolean value, jstring origin) {
  GURL callback_origin(base::android::ConvertJavaStringToUTF16(env, origin));
  if (callback_origin.GetOrigin() == pending_geolocation_prompts_.front().first) {
    pending_geolocation_prompts_.front().second.Run(value);
    pending_geolocation_prompts_.pop_front();
    if (!pending_geolocation_prompts_.empty()) {
      ShowGeolocationPromptHelper(java_ref_, pending_geolocation_prompts_.front().first);
    }
  }
}

void XWalkContent::HideGeolocationPrompt(const GURL& origin) {
  bool removed_current_outstanding_callback = false;
  std::list<OriginCallback>::iterator it = pending_geolocation_prompts_.begin();
  while (it != pending_geolocation_prompts_.end()) {
    if ((*it).first == origin.GetOrigin()) {
      if (it == pending_geolocation_prompts_.begin()) {
        removed_current_outstanding_callback = true;
      }
      it = pending_geolocation_prompts_.erase(it);
    } else {
      ++it;
    }
  }

  if (removed_current_outstanding_callback) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> j_ref = java_ref_.get(env);
    if (j_ref.obj()) {
      Java_XWalkContent_onGeolocationPermissionsHidePrompt(env, j_ref.obj());
    }
    if (!pending_geolocation_prompts_.empty()) {
      ShowGeolocationPromptHelper(java_ref_, pending_geolocation_prompts_.front().first);
    }
  }
}

// Called by Java.
void XWalkContent::SetBackgroundColor(JNIEnv* env, jobject obj, jint color) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->SetBackgroundColor(color);
}

void XWalkContent::SetOriginAccessWhitelist(JNIEnv* env, jobject obj, jstring url, jstring match_patterns) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  render_view_host_ext_->SetOriginAccessWhitelist(base::android::ConvertJavaStringToUTF8(env, url),
                                                  base::android::ConvertJavaStringToUTF8(env, match_patterns));
}

base::android::ScopedJavaLocalRef<jbyteArray> XWalkContent::GetCertificate(JNIEnv* env,
                                                                           const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NavigationEntry* entry = web_contents_->GetController().GetLastCommittedEntry();
  if (!entry || !entry->GetSSL().certificate) {
    return ScopedJavaLocalRef<jbyteArray>();
  }

  // Convert the certificate and return it
  std::string der_string;
  net::X509Certificate::GetDEREncoded(entry->GetSSL().certificate->os_cert_handle(), &der_string);
  return base::android::ToJavaByteArray(env, reinterpret_cast<const uint8_t*>(der_string.data()), der_string.length());
}

/**
 *
 */
base::android::ScopedJavaLocalRef<jobjectArray> XWalkContent::GetCertificateChain(JNIEnv* env,
                                                                                  const JavaParamRef<jobject>& obj) {

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::NavigationEntry* entry = web_contents_->GetController().GetLastCommittedEntry();
  if (!entry || !entry->GetSSL().certificate) {
    return ScopedJavaLocalRef<jobjectArray>();
  }

  // Get the certificate

  scoped_refptr<net::X509Certificate> cert = entry->GetSSL().certificate;

  std::vector<std::string> cert_chain;
  // Convert the certificate and return it
  std::string der_string;
  net::X509Certificate::GetDEREncoded(cert->os_cert_handle(), &der_string);

  cert_chain.push_back(der_string);  // store main cert

  // iterate over the list of intermediates
  const net::X509Certificate::OSCertHandles& intermediates = cert->GetIntermediateCertificates();

  for (size_t i = 0; i < intermediates.size(); ++i) {
    der_string.clear();

    net::X509Certificate::GetDEREncoded(intermediates[i], &der_string);
    cert_chain.push_back(der_string);  // store intermediates

  }
  return base::android::ToJavaArrayOfByteArray(env, cert_chain);
}

FindHelper* XWalkContent::GetFindHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!find_helper_.get()) {
    find_helper_.reset(new FindHelper(web_contents_.get()));
    find_helper_->SetListener(this);
  }
  return find_helper_.get();
}

void XWalkContent::FindAllAsync(JNIEnv* env, const JavaParamRef<jobject>& obj,
                                const JavaParamRef<jstring>& search_string) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->FindAllAsync(ConvertJavaStringToUTF16(env, search_string));
}

void XWalkContent::FindNext(JNIEnv* env, const JavaParamRef<jobject>& obj, jboolean forward) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->FindNext(forward);
}

void XWalkContent::ClearMatches(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetFindHelper()->ClearMatches();
}

void XWalkContent::OnFindResultReceived(int active_ordinal, int match_count,
bool finished) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_XWalkContent_onFindResultReceived(env, obj.obj(), active_ordinal, match_count, finished);
}

}  // namespace xwalk
