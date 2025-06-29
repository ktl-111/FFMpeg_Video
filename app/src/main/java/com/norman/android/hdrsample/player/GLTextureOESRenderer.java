package com.norman.android.hdrsample.player;

import com.norman.android.hdrsample.player.shader.TextureFragmentShader;

public class GLTextureOESRenderer extends GLTextureRenderer {
    public GLTextureOESRenderer() {
        super(TextureFragmentShader.TYPE_TEXTURE_OES);
    }

}
