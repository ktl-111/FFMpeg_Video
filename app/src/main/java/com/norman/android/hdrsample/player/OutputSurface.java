package com.norman.android.hdrsample.player;

import android.os.Build;
import android.view.Surface;

import androidx.annotation.Nullable;

import com.norman.android.hdrsample.opengl.GLEnvColorSpace;
import com.norman.android.hdrsample.opengl.GLEnvContext;
import com.norman.android.hdrsample.opengl.GLEnvDisplay;
import com.norman.android.hdrsample.opengl.GLEnvWindowSurface;
import com.norman.android.hdrsample.player.color.ColorSpace;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class OutputSurface {
    private GLEnvWindowSurface windowSurface;

    private Surface surface;

    private int lastColorSpace;
    private GLEnvContext envContext;

    public OutputSurface(GLEnvContext envContext) {
        this.envContext = envContext;
    }

    public void setSurface(Surface surface) {
        synchronized (this) {
            this.surface = surface;
            if (surface == null || !surface.isValid()) {
                release();
            }
        }

    }


    public void release() {
        synchronized (this) {
            if (windowSurface == null) {
                return;
            }
            windowSurface.release();
            windowSurface = null;
        }
    }

    public boolean isValid() {
        synchronized (this) {
            return surface != null && surface.isValid();
        }
    }


    public @Nullable GLEnvWindowSurface getWindowSurface(@ColorSpace int requestColorSpace) {
        synchronized (this) {
            if (surface == null) {
                release();
                return null;
            }
            if (windowSurface == null ||
                    surface != windowSurface.getSurface() ||
                    this.lastColorSpace != requestColorSpace) {//surface不同或者色域不同就要重新创建WindowSurface
                release();
                GLEnvDisplay envDisplay = envContext.getEnvDisplay();
                GLEnvWindowSurface.Builder builder = new GLEnvWindowSurface.Builder(envContext, surface);
                if (isSupportHDR(surface)) {//判断Surface是否支持HDR
                    if (requestColorSpace == ColorSpace.VIDEO_BT2020_PQ) {
                        if (envDisplay.isSupportBT2020PQ()) {
                            builder.setColorSpace(GLEnvColorSpace.BT2020_PQ);
                        }
                    } else if (requestColorSpace == ColorSpace.VIDEO_BT2020_HLG) {
                        if (envDisplay.isSupportBT2020HLG()) {
                            builder.setColorSpace(GLEnvColorSpace.BT2020_HLG);
                        }
                    } else if (requestColorSpace == ColorSpace.VIDEO_BT2020_LINEAR) {
                        if (envDisplay.isSupportBT2020Linear()) {
                            builder.setColorSpace(GLEnvColorSpace.BT2020_LINEAR);
                        }
                    }
                }
                windowSurface = builder.build();
                this.lastColorSpace = requestColorSpace;
            }
            if (!windowSurface.isValid()) {
                release();
            }
            return windowSurface;
        }
    }

    /**
     * 如果是SurfaceView的Surface(通过判断toString是否包含字符串null)或者版本13以上Surface就支持HDR
     *
     * @param surface
     * @return
     */
    private boolean isSupportHDR(Surface surface) {
        if (surface == null) {
            return false;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return true;
        }
        String str = surface.toString();
        String pattern = "Surface\\(name=([^)]+)\\)";
        Pattern regexPattern = Pattern.compile(pattern);
        Matcher matcher = regexPattern.matcher(str);
        if (!matcher.find()) {
            return false;
        }
        String extractedValue = matcher.group(1);
        return extractedValue.equals("null");
    }
}
