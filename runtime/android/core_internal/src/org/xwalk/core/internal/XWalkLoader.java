package org.xwalk.core.internal;

import android.content.Context;

@XWalkAPI(noInstance = true)
public class XWalkLoader {
    
    @XWalkAPI
    public static void loadXWalkLib(Context ctx) {
        XWalkViewDelegate.init(null, ctx);
    }
}
