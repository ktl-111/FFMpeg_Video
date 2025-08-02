package com.norman.android.hdrsample.player;

import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.util.Log;

import androidx.annotation.CallSuper;

import com.norman.android.hdrsample.opengl.GLMatrix;
import com.norman.android.hdrsample.player.shader.TextureFragmentShader;
import com.norman.android.hdrsample.player.shader.TextureVertexShader;
import com.norman.android.hdrsample.util.GLESUtil;
import com.norman.android.hdrsample.util.LogUtils;

import java.nio.FloatBuffer;
import java.util.Arrays;

/**
 * 支持3种格式 2D OES Y2Y渲染到frameBuffer上
 */
public class GLTextureRenderer extends GLRenderer {


    private final float[] textureMatrix = new GLMatrix().get();
    private float[] positionMatrixScale = new GLMatrix().get();

    protected FloatBuffer textureCoordinateBuffer;
    private final FloatBuffer positionCoordinateBuffer;

    private int positionCoordinateAttribute;
    private int textureCoordinateAttribute;
    private int textureUnitUniform;
    private int textureMatrixUniform;
    private int positionMatrixUniformScale;

    private int rotation = -1;
    private float scale = 1.0f;
    private float translationX = 0f;
    private float translationY = 0f;
    private int textureId;

    private final @TextureFragmentShader.TextureType int textureType;


    public GLTextureRenderer(@TextureFragmentShader.TextureType int type) {
        textureType = type;
        positionCoordinateBuffer = GLESUtil.createPositionFlatBuffer();//平面的顶点坐标
        textureCoordinateBuffer = GLESUtil.createTextureFlatBufferUpsideDown();//纹理坐标
        positionMatrixScale = getPositionMatrix(scale, translationX, translationY);
        setVertexShader(new TextureVertexShader());
        setFrameShader(new TextureFragmentShader(textureType));
    }

    public void setRotation(int rotation) {
        this.rotation = rotation;
    }

    public void updatePositionMatrix(float scale, float x, float y) {
        boolean change = false;

        if (this.scale != scale) {
            this.scale = scale;
            change = true;
        }
        if (translationX != x) {
            translationX = x;
            change = true;
        }
        if (translationY != y) {
            translationY = y;
            change = true;
        }
        if (change) {
            positionMatrixScale = getPositionMatrix(scale, translationX, translationY);
            Log.i("gltex", "updatePositionMatrix: scale:" + scale + " translationX:" + translationX + " translationY:" + translationY + " positionMatrixScale:" + Arrays.toString(positionMatrixScale));
        }

    }

    /**
     * opengl列优先存储机制
     *
     * @param scale
     * @param translationX
     * @param translationY
     * @return
     */
    private float[] getPositionMatrix(float scale, float translationX, float translationY) {
        //xyzw矩阵
        return new float[]{
                scale, 0f, 0, 0,
                0f, scale, 0, 0,
                0f, 0f, 1f, 0,
                translationX, translationY, 0, 1,
        };
    }

    public void setTextureId(int textureId) {
        this.textureId = textureId;
    }


    public float[] getTextureMatrix() {
        return textureMatrix;
    }

    @CallSuper
    @Override
    protected void onProgramChange(int programId) {
        positionCoordinateAttribute = GLES20.glGetAttribLocation(programId, TextureVertexShader.POSITION);
        textureMatrixUniform = GLES20.glGetUniformLocation(programId, TextureVertexShader.TEXTURE_MATRIX);
        positionMatrixUniformScale = GLES20.glGetUniformLocation(programId, TextureVertexShader.POSITION_MATRIX_SCALE);
        textureCoordinateAttribute = GLES20.glGetAttribLocation(programId, TextureVertexShader.INPUT_TEXTURE_COORDINATE);
        textureUnitUniform = GLES20.glGetUniformLocation(programId, TextureFragmentShader.INPUT_IMAGE_TEXTURE);
    }

    @Override
    protected boolean onRenderStart() {
        return textureId > 0;
    }

    private int preRotation = -1;
    private String mTag = "GLTexture";

    @Override
    protected void onRender() {
        positionCoordinateBuffer.clear();
        textureCoordinateBuffer.clear();
        if (preRotation != rotation) {
            LogUtils.i(mTag, "onRender rotation:" + rotation + " preRotation:" + preRotation);
            if (rotation == 90) {
                textureCoordinateBuffer = GLESUtil.createTextureFlatBufferLeft90();
            } else if (rotation == 270) {
                textureCoordinateBuffer = GLESUtil.createTextureFlatBufferRight90();
            } else if (rotation == 180) {
                textureCoordinateBuffer = GLESUtil.createTextureFlatBuffer();
            } else if (rotation == 0) {
                textureCoordinateBuffer = GLESUtil.createTextureFlatBufferUpsideDown();
            }
            preRotation = rotation;
        }
        GLES20.glEnableVertexAttribArray(positionCoordinateAttribute);
        GLES20.glVertexAttribPointer(positionCoordinateAttribute,
                GLESUtil.FLAT_VERTEX_LENGTH,
                GLES20.GL_FLOAT, false, 0,
                positionCoordinateBuffer);
        GLES20.glEnableVertexAttribArray(textureCoordinateAttribute);
        GLES20.glVertexAttribPointer(textureCoordinateAttribute,
                GLESUtil.FLAT_VERTEX_LENGTH,
                GLES20.GL_FLOAT, false, 0,
                textureCoordinateBuffer);
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        if (textureType != TextureFragmentShader.TYPE_TEXTURE_2D) {
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureId);
        } else {
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        }
        GLES20.glUniform1i(textureUnitUniform, 0);
        GLES20.glUniformMatrix4fv(textureMatrixUniform, 1, false, textureMatrix, 0);
        GLES20.glUniformMatrix4fv(positionMatrixUniformScale, 1, false, positionMatrixScale, 0);
        onTextureRender();
        GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
        GLES20.glDisableVertexAttribArray(positionCoordinateAttribute);
        GLES20.glDisableVertexAttribArray(textureCoordinateAttribute);
        if (textureType != TextureFragmentShader.TYPE_TEXTURE_2D) {
            GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureId);
        } else {
            GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
        }
    }

    protected void onTextureRender() {

    }

}
