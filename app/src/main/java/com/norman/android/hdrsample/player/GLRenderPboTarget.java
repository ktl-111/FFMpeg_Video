//================================================================================================================================
//
// Copyright (c) 2015-2022 VisionStar Information Technology (Shanghai) Co., Ltd. All Rights Reserved.
// EasyAR is the registered trademark or trademark of VisionStar Information Technology (Shanghai) Co., Ltd in China
// and other countries for the augmented reality technology developed by VisionStar Information Technology (Shanghai) Co., Ltd.
//
//================================================================================================================================

package com.norman.android.hdrsample.player;


import static android.opengl.GLES30.GL_MAP_READ_BIT;
import static android.opengl.GLES30.GL_PIXEL_PACK_BUFFER;
import static android.opengl.GLES30.GL_STREAM_READ;

import android.graphics.Bitmap;
import android.opengl.GLES30;

import com.example.play.utils.DecodeUtils;
import com.norman.android.hdrsample.util.AppUtil;
import com.norman.android.hdrsample.util.LogUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;


public class GLRenderPboTarget extends GLRenderTarget {

    private String TAG = "PboTarget";

    int currentPBOIndex = 0;
    private long time = 0;
    int[] pboIds = new int[2];

    /**
     * 根据位数和宽高创建纹理并绑定到frameBuffer
     */
    @Override
    void onCreate() {// 双PBO初始化
        GLES30.glGenBuffers(pboIds.length, pboIds, 0);
        // 初始化两个PBO
        for (int i = 0; i < pboIds.length; i++) {
            GLES30.glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[i]);
            GLES30.glBufferData(GL_PIXEL_PACK_BUFFER, width * height * 4, null, GL_STREAM_READ);
        }
        GLES30.glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

    @Override
    void onDestroy() {
        GLES30.glDeleteBuffers(pboIds.length, pboIds, 0);
    }

    private boolean writeDone = false;

    @Override
    void onStart() {
        LogUtils.i(TAG, "onStart " + width + "*" + height);
        // 绑定当前PBO用于下一帧
        int nextPBOIndex = (currentPBOIndex + 1) % 2;
        GLES30.glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[currentPBOIndex]);
        GLES30.glReadPixels(0, 0, width, height, GLES30.GL_RGBA, GLES30.GL_UNSIGNED_BYTE, 0);

        // 处理前一帧的PBO数据
        GLES30.glBindBuffer(GL_PIXEL_PACK_BUFFER, pboIds[nextPBOIndex]);
        ByteBuffer buffer = (ByteBuffer) GLES30.glMapBufferRange(
                GL_PIXEL_PACK_BUFFER, 0, width * height * 4,
                GL_MAP_READ_BIT);
        if (buffer != null) {
//            encodeBuffer(buffer);
            saveBufferToLocal(buffer);
        }

        currentPBOIndex = nextPBOIndex;
    }

    private void encodeBuffer(ByteBuffer buffer) {
        buffer.rewind();
        if (!writeDone) {
            int result = DecodeUtils.INSTANCE.writeData(buffer, time);
            LogUtils.i(TAG, "glMapBufferRange " + buffer.getClass().getSimpleName() + " time:" + time + " result:" + result);
            if (result == 10000) {
                DecodeUtils.INSTANCE.endDecode();
                writeDone = true;
            }

        }
    }

    private void saveBufferToLocal(ByteBuffer buffer) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        buffer.rewind();
        bitmap.copyPixelsFromBuffer(buffer);
        saveBitmap(bitmap);        // 保存到本地
        bitmap.recycle();
    }

    // 保存到本地方法
    private void saveBitmap(Bitmap bitmap) {
        String path = new File(AppUtil.getAppContext().getExternalCacheDir(), System.currentTimeMillis() + ".png").getAbsolutePath();
        try (FileOutputStream out = new FileOutputStream(path)) {
            bitmap.compress(Bitmap.CompressFormat.JPEG, 100, out);
            LogUtils.i(TAG, "save bitmap to " + path + " success");
        } catch (IOException e) {
            LogUtils.e("PBO", "Save failed " + e.getMessage());
        }
    }

    @Override
    void onFinish() {
        GLES30.glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        GLES30.glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

    @Override
    void onClearColor() {
    }

    public void setTime(long time) {
        this.time = time;
    }
}
