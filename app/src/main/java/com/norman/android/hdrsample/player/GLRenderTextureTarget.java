//================================================================================================================================
//
// Copyright (c) 2015-2022 VisionStar Information Technology (Shanghai) Co., Ltd. All Rights Reserved.
// EasyAR is the registered trademark or trademark of VisionStar Information Technology (Shanghai) Co., Ltd in China
// and other countries for the augmented reality technology developed by VisionStar Information Technology (Shanghai) Co., Ltd.
//
//================================================================================================================================

package com.norman.android.hdrsample.player;


import android.opengl.GLES20;
import android.opengl.GLES30;

import androidx.annotation.NonNull;
import androidx.compose.ui.state.ToggleableState;

import com.norman.android.hdrsample.player.color.ColorSpace;
import com.norman.android.hdrsample.util.GLESUtil;
import com.norman.android.hdrsample.util.LogUtils;


public class GLRenderTextureTarget extends GLRenderTarget {
    private String mTAG;

    public GLRenderTextureTarget(String tag) {
        mTAG = tag;
    }

    @NonNull
    @Override
    public String toString() {
        return "GLRenderTextureTarget(" + mTAG + ")";
    }

    int frameBufferId;
    int textureId;

    int bitDepth = 8;

    @ColorSpace
    int colorSpace = ColorSpace.VIDEO_SDR;

    int maxContentLuminance;
    int maxFrameAverageLuminance;
    int maxMasteringLuminance;

    public void setBitDepth(int bitDepth) {
        if (this.bitDepth != bitDepth) {
            this.bitDepth = bitDepth;
            requestRecreate();
        }
    }

    private String TAG = "GLRender";

    public void setColorSpace(@ColorSpace int colorSpace) {
        LogUtils.i(TAG, "setColorSpace " + colorSpace);
        this.colorSpace = colorSpace;
    }


    public void setMaxContentLuminance(int maxContentLuminance) {
        this.maxContentLuminance = maxContentLuminance;
    }

    public void setMaxFrameAverageLuminance(int maxFrameAverageLuminance) {
        this.maxFrameAverageLuminance = maxFrameAverageLuminance;
    }

    public void setMaxMasteringLuminance(int maxMasteringLuminance) {
        this.maxMasteringLuminance = maxMasteringLuminance;
    }

    /**
     * 根据位数和宽高创建纹理并绑定到frameBuffer
     */
    @Override
    void onCreate() {
        frameBufferId = GLESUtil.createFrameBufferId();
        textureId = GLESUtil.createTextureId(width, height, bitDepth);
        GLESUtil.attachTexture(frameBufferId, textureId);
    }

    @Override
    void onDestroy() {
        GLESUtil.delTextureId(textureId);
        GLESUtil.deleteFrameBufferId(frameBufferId);
    }

    @Override
    void onStart() {
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, frameBufferId);
        GLES30.glViewport(0, 0, width, height);
    }

    @Override
    void onFinish() {
        GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
    }

    @Override
    void onClearColor() {
        // 清空frameBuffer中的数据
        GLES30.glClearColor(0.0f, 0.f, 0.f, 0.0f);
        GLES30.glClear(GLES30.GL_COLOR_BUFFER_BIT | GLES30.GL_DEPTH_BUFFER_BIT);
    }
}
