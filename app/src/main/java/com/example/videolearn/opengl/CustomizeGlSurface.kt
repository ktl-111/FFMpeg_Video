package com.example.videolearn.opengl

import android.content.Context
import android.opengl.GLSurfaceView

class CustomizeGlSurface(context: Context) : GLSurfaceView(context) {
    init {
        setEGLContextClientVersion(2);
        setRenderer(CustomizeRender());
        setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
    }
}