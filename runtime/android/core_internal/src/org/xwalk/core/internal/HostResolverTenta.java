
package org.xwalk.core.internal;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * @author iotto
 */
@JNINamespace("xwalk::tenta")
public class HostResolverTenta {

    public static final int OK = 0; // success; all went ok
    public static final int ERR_DNS_CACHE_MISS = -804; // cache not available
    public static final int ERR_DNS_SERVER_FAILED = -802;
    public static final int ERR_CACHE_MISS = -400; // not found in cache
    public static final int ERR_NAME_NOT_RESOLVED = -105; // name resolution failed

    private static HostResolverTenta sInstance = null;
    private long mNativeHostResolverTenta; // native host resolver tenta reference pointer

    private IDelegate delegate;

    /**
     * @return Java single instance
     */
    public static HostResolverTenta getInstance() {
        if (sInstance == null) {
            sInstance = new HostResolverTenta();
        }

        return sInstance;
    }

    /**
     * @param nativeClassPointer
     * @return
     */
    @CalledByNative
    public static HostResolverTenta getInstanceNative(long nativeClassPointer) {
        if (sInstance == null) {
            sInstance = new HostResolverTenta();
        }
        sInstance.mNativeHostResolverTenta = nativeClassPointer;

        return sInstance;
    }

    /**
     * Resolve hostname from cache. !NOTE! Blocking action, don't make any network requests
     * 
     * @param hostname
     * @return ip list from cache
     */
    @CalledByNative
    public byte[][] resolveCache(String hostname) {
        if (delegate != null) {
            return delegate.resolveCache(hostname);
        }

        return null;
    }

    /**
     * Try resolve host name. Return 0 if have resolvers registered Resolvers must call
     * onResolved(list<address>, requestId)
     * 
     * @return 0 if request acknowledged and will be resolved; -1 if error occured
     */
    @CalledByNative
    public int resolve(String hostname, long requestId) {
        if (delegate != null) {
            delegate.resolve(hostname, requestId);
            return OK;
        } else
            return ERR_DNS_SERVER_FAILED;
    }

    /**
     * Called by registered resolver to notify host name resolved
     * 
     * @param statusCode see above de declared valid codes (OK if address resolved)
     * @param addrList byte array if ip byte arrays (ex.[0] = [192,168,0,10])
     * @param requestID id of this request from resolve
     */
    public void onResolved(int statusCode, byte[][] addrList, long requestID) {
        if (mNativeHostResolverTenta == 0)
            return;

        nativeOnResolved(mNativeHostResolverTenta, statusCode, addrList, requestID);
    }

    // @CalledByNative
    public void onIPAddressChanged() {
        if (delegate != null) {
            delegate.onIPAddressChanged();
        }
    }

    // @CalledByNative
    public void onConnectionTypeChanged(int type) {
        if (delegate != null) {
            delegate.onConnectionTypeChanged(type);
        }
    }

    // @CalledByNative
    public void onDNSChanged() {
        if (delegate != null) {
            delegate.onDNSChanged();
        }
    }

    // @CalledByNative
    public void onInitialDNSConfigRead() {
        if (delegate != null) {
            delegate.onInitialDNSConfigRead();
        }
    }

    public IDelegate getDelegate() {
        return delegate;
    }

    public void setDelegate(IDelegate delegate) {
        this.delegate = delegate;
    }

    // native functions
    private native void nativeOnResolved(long nativeHostResolverTenta, int status, byte[][] result,
            long forRequestId);

    private HostResolverTenta() {
        // TODO Auto-generated constructor stub
    }

    /**
     * @author iotto
     */
    public static abstract class IDelegate {
        /**
         * Called when new name resolution required When finished call the
         * HostResolverTenta.onResolved with requestId provided here
         * 
         * @param hostname Name to resolve
         * @param requestId Id of this request
         */
        protected abstract void resolve(String hostname, long requestId);

        /**
         * Resolve host from cache !Note! Blocking request!
         * 
         * @param hostname
         * @return
         */
        protected abstract byte[][] resolveCache(String hostname);

        protected void onIPAddressChanged() {
        }

        protected void onConnectionTypeChanged(int type) {
        }

        protected void onDNSChanged() {
        }

        protected void onInitialDNSConfigRead() {
        }
    }

}
