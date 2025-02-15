package com.norman.android.hdrsample.opengl;

import android.opengl.EGL14;
import android.opengl.EGLContext;

/**
 * EGLContext管理器
 */
public interface GLEnvContextManager {
    /**
     * 必须attach以后才能在当前线程使用
     */
    void attach();

    /**
     * 必须在attach的线程detach
     */
    void detach();

    /**
     * 获取绑定的线程
     * @return
     */
    Thread getAttachThread();

    /**
     * 绑定线程是否在当前线程
     * @return
     */
    boolean isCurrentThread();

    /**
     * 是否已经绑定
     * @return
     */
    boolean isAttach();

    void release();

    boolean isRelease();

    GLEnvContext getEnvContext();

    static GLEnvContextManager create() {
        Builder builder = new Builder();
        return builder.build();
    }

    static GLEnvContextManager create(@GLEnvVersion int version) {
        Builder builder = new Builder();
        builder.setClientVersion(version);
        return builder.build();
    }

    static GLEnvContextManager create(GLEnvConfigChooser configChooser) {
        Builder builder = new Builder(configChooser);
        return builder.build();
    }

    static GLEnvContextManager create(GLEnvDisplay envDisplay,GLEnvConfig envConfig) {
        Builder builder = new Builder(envDisplay,envConfig);
        return builder.build();
    }

    static GLEnvContextManager create(GLEnvDisplay envDisplay,GLEnvConfigChooser configChooser) {
        Builder builder = new Builder(envDisplay,configChooser);
        return builder.build();
    }

    static GLEnvContextManager create(@GLEnvVersion int version, GLEnvConfigChooser configChooser) {
        Builder builder = new Builder(configChooser);
        builder.setClientVersion(version);
        return builder.build();
    }

    class Builder {

        final GLEnvContext.Builder envContextBuilder;

        public Builder() {
            this(EGL14.EGL_NO_CONTEXT);
        }

        public Builder(EGLContext shareContext) {
            this(GLEnvConfigSimpleChooser.create(), shareContext);
        }

        public Builder(GLEnvConfigChooser configChooser) {
            this(configChooser, EGL14.EGL_NO_CONTEXT);
        }

        public Builder(GLEnvConfigChooser configChooser, EGLContext shareContext) {
            envContextBuilder = new GLEnvContext.Builder(configChooser, shareContext);
        }

        public Builder(GLEnvDisplay envDisplay,GLEnvConfigChooser configChooser) {
            this(envDisplay,envDisplay.chooseConfig(configChooser));
        }

        public Builder(GLEnvDisplay envDisplay,GLEnvConfig envConfig) {
            this(envDisplay,envConfig,EGL14.EGL_NO_CONTEXT);
        }

        public Builder(GLEnvDisplay envDisplay, GLEnvConfig envConfig, EGLContext shareContext) {
            envContextBuilder = new GLEnvContext.Builder(envDisplay, envConfig, shareContext);
        }

        /**
         * 设置版本
         * @param version
         */


        public void setClientVersion(@GLEnvVersion int version) {
            envContextBuilder.setClientVersion(version);
        }

        /**
         * 设置属性
         * @param key
         * @param value
         */
        public void setContextAttrib(int key, int value) {
            envContextBuilder.setContextAttrib(key,value);
        }

        public GLEnvContextManager build() {
            return new EnvContextManagerImpl(envContextBuilder.build());
        }
    }
}
