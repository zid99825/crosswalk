package org.xwalk.core.internal;

@XWalkAPI(createInternally = true)
public class RewriteUrlValueInternal {
    private String url;
    
    /**
     * C++ ui/base/page_transition_types.h
     * Java org.chromium.ui.base.PageTransition
     */
    private int transitionType;
    
    private void nativeInit(String url, int trType) {
        this.url = url;
        this.transitionType = trType;
    }
    
    @XWalkAPI
    public String getUrl() {
        return url;
    }
    
    @XWalkAPI
    public void setUrl(String url) {
        this.url = url;
    }
    
    @XWalkAPI
    public int getTransitionType() {
        return transitionType;
    }
    
    @XWalkAPI
    public void setTransitionType(int transitionType) {
        this.transitionType = transitionType;
    }
    
    
}
