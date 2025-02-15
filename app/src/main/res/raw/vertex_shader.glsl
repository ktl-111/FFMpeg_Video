precision highp float;
uniform sampler2D inputImageTexture;
uniform mediump mat3 colorConversionMatrix;
uniform mediump int isSt2084;
uniform mediump int isAribB67;
varying highp vec2 textureCoordinate;

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMAX3(a,b,c) FFMAX(FFMAX(a,b),c)

highp vec3 YuvConvertRGB_BT2020(highp vec3 yuv, int normalize) {
    highp vec3 rgb;
    // [64, 960]
    float r = float(yuv.x - 64.) * 1.164384                                  - float(yuv.z - 512.) * -1.67867;
    float g = float(yuv.x - 64.) * 1.164384 - float(yuv.y - 512.) * 0.187326 - float(yuv.z - 512.) * 0.65042;
    float b = float(yuv.x - 64.) * 1.164384 - float(yuv.y - 512.) * -2.14177;
    rgb.r = r;
    rgb.g = g;
    rgb.b = b;
    if (normalize == 1) { 
        rgb /= 1024.0; 
    }
    return rgb;
}

// [arib b67 eotf
const highp float ARIB_B67_A = 0.17883277;
const highp float ARIB_B67_B = 0.28466892;
const highp float ARIB_B67_C = 0.55991073;
highp float arib_b67_inverse_oetf(highp float x)
{
    // Prevent negative pixels expanding into positive values.
    x = max(x, 0.0);
    if (x <= 0.5)
    x = (x * x) * (1.0 / 3.0);
    else
    x = (exp((x - ARIB_B67_C) / ARIB_B67_A) + ARIB_B67_B) / 12.0;
    return x;
}
highp float ootf_1_2(highp float x)
{
    return x < 0.0 ? x : pow(x, 1.2);
}
highp float arib_b67_eotf(highp float x)
{
    return ootf_1_2(arib_b67_inverse_oetf(x));
}
// arib b67 eotf]


// [st 2084 eotf
highp float ST2084_M1 = 0.1593017578125;
const float ST2084_M2 = 78.84375;
const float ST2084_C1 = 0.8359375;
const float ST2084_C2 = 18.8515625;
const float ST2084_C3 = 18.6875;
highp float FLT_MIN = 1.17549435082228750797e-38;
highp float st_2084_eotf(highp float x)
{
    highp float xpow = pow(x, float(1.0 / ST2084_M2));
    highp float num = max(xpow - ST2084_C1, 0.0);
    highp float den = max(ST2084_C2 - ST2084_C3 * xpow, FLT_MIN);
    return pow(num/den, 1.0 / ST2084_M1);
}
// st 2084 eotf]

// [tonemap hable
highp float hableF(highp float inVal)
{
    highp float a = 0.15, b = 0.50, c = 0.10, d = 0.20, e = 0.02, f = 0.30;
    return (inVal * (inVal * a + b * c) + d * e) / (inVal * (inVal * a + b) + d * f) - e / f;
}
// tonemap hable]

// [bt709 
highp float rec_1886_inverse_eotf(highp float x)
{
    return x < 0.0 ? 0.0 : pow(x, 1.0 / 2.4);
}

highp float rec_1886_eotf(float x)
{
    return x < 0.0 ? 0.0 : pow(x, 2.4);
}
// bt709]

void main() {
    highp vec3 rgb10bit = texture2D(inputImageTexture, textureCoordinate).rgb;

    // 1、HDR 非线性电信号转为 HDR 线性光信号（EOTF）
    float peak_luminance = 100.0;
    float ST2084_PEAK_LUMINANCE = 10000.0;
    float to_linear_scale;
    highp vec3 fragColor;
    if (isSt2084 == 1) {
        to_linear_scale = 10000.0 / peak_luminance;
        fragColor = to_linear_scale * vec3(st_2084_eotf(rgb10bit.r), st_2084_eotf(rgb10bit.g), st_2084_eotf(rgb10bit.b));
    } else if (isAribB67 == 1) {
        to_linear_scale = 1000.0 / peak_luminance;
        fragColor = to_linear_scale * vec3(arib_b67_eotf(rgb10bit.r), arib_b67_eotf(rgb10bit.g), arib_b67_eotf(rgb10bit.b));
    } else {
        fragColor = vec3(rec_1886_eotf(rgb10bit.r), rec_1886_eotf(rgb10bit.g), rec_1886_eotf(rgb10bit.b));
    }

    // 2、HDR 线性光信号做颜色空间转换（Color Space Converting）
    // color-primaries REC_2020 to REC_709
    mat3 rgb2xyz2020 = mat3(0.6370, 0.1446, 0.1689,
                            0.2627, 0.6780, 0.0593,
                            0.0000, 0.0281, 1.0610);
    mat3 xyz2rgb709 = mat3(3.2410, -1.5374, -0.4986,
                           -0.9692, 1.8760, 0.0416,
                           0.0556, -0.2040, 1.0570);                                   
    fragColor *= rgb2xyz2020 * xyz2rgb709;

    // 3、HDR 线性光信号色调映射为 SDR 线性光信号（Tone Mapping）
    highp float sig = FFMAX(FFMAX3(fragColor.r, fragColor.g, fragColor.b), 1e-6);
    highp float sig_orig = sig;
    float peak = 10.0;
    sig = hableF(sig) / hableF(peak);
    fragColor *= sig / sig_orig;

    // 4、SDR 线性光信号转 SDR 非线性电信号（OETF）
    fragColor = vec3(rec_1886_inverse_eotf(fragColor.r), rec_1886_inverse_eotf(fragColor.g), rec_1886_inverse_eotf(fragColor.b));
    gl_FragColor = vec4(fragColor, 1.0);
}