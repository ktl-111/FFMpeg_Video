## 音视频

![整体图](img\4.png)

### 基础

[参考](https://juejin.cn/post/7005493877272477726)  
[音视频学习路线](https://juejin.cn/user/3245414055944045)  
[音视频开发进阶指南](http://xcodep.oss-cn-beijing.aliyuncs.com/2020/09/20200912093929907.pdf)  

#### 封装格式(容器)

> 封装格式也叫容器,将已经编码好的音频和视频按照一定格式放到一个文件中.

| 视频文件格式                             | 视频封装格式                       | 场景                                                                                                                                                                                |
| ---------------------------------------- | ---------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| .avi                                     | AVI(Audio Video Interleave)        | 图像质量好，但体积过于庞大，压缩标准不统一，存在高低版本兼容问题。                                                                                                                  |
| .wmv                                     | WMV(Windows Media Video)           | 可边下载边播放，很适合网上播放和传输                                                                                                                                                |
| .mpg .mpeg .mpe .dat .vob .asf .3gp .mp4 | MPEG(Moving Picture Experts Group) | 由运动图像专家组制定的视频格式，有三个压缩标准，分别是 MPEG-1、MPEG-2、和 MPEG-4，它为了播放流式媒体的高质量视频而专门设计的，以求使用最少的数据获得最佳的图像质量。                |
| .mkv                                     | Matroska                           | 一种新的视频封装格式，它可将多种不同编码的视频及 16 条以上不同格式的音频和不同语言的字幕流封装到一个 Matroska Media 文件当中。                                                      |
| .rm、.rmvb                               | Real Video                         | Real Networks 公司所制定的音频视频压缩规范称为 Real Media。用户可以使用 RealPlayer 根据不同的网络传输速率制定出不同的压缩比率，从而实现在低速率的网络上进行影像数据实时传送和播放。 |
| .mov                                     | QuickTime File Format              | Apple 公司开发的一种视频格式，默认的播放器是苹果的 QuickTime。这种封装格式具有较高的压缩比率和较完美的视频清晰度等特点，并可以保存 alpha 通道。                                     |
| .flv                                     | Flash Video                        | 由 Adobe Flash 延伸出来的一种网络视频封装格式。这种格式被很多视频网站所采用。                                                                                                       |

#### 编码格式(zip文件)

> 将视频像素压缩成视频流,从而降低视频的大小,类似zip

| 名称            | 场景                                                         |
| --------------- | ------------------------------------------------------------ |
| **HEVC(H.265)** | 高效率视频编码(High Efficiency Video Coding，简称 HEVC)是一种视频压缩标准，是 H.264 的继任者。HEVC 被认为不仅提升图像质量，同时也能达到 H.264 两倍的压缩率（等同于同样画面质量下比特率减少了 50%），可支持 4K 分辨率甚至到超高画质电视，最高分辨率可达到 8192×4320（8K 分辨率），这是目前发展的趋势。 |
| **AVC(H.264)**  | 等同于 MPEG-4 第十部分，也被称为高级视频编码(Advanced Video Coding，简称 AVC)，是一种视频压缩标准，一种被广泛使用的高精度视频的录制、压缩和发布格式。该标准引入了一系列新的能够大大提高压缩性能的技术，并能够同时在高码率端和低码率端大大超越以前的诸标准。 |
| **MPEG4**       | 等同于`H.264`，是这两个编码组织合作诞生的标准。              |
| **MPEG2**       | 等同于`H.262`，使用在 DVD、SVCD 和大多数数字视频广播系统和有线分布系统中。 |
| **VP9**         | google                                                       |
| **VP8**         |                                                              |
| **VC-1**        |                                                              |

#### 有了编码为什么还需要封装格式
一张图片有很多种算法,比如jpeg,png等,视频是动态图片,用怎样的方式表达1s内30张图片,就是编码格式,比如h264,mpeg等,后面发展了需要音频,字幕,这时就需要个容器将他们结合起来,就诞生了mp4,mkv等



编码格式只是将数据压缩,封装格式相当于把容器里所有的编码格式/字幕/音频等整理好

![MP4结构](img/Snipaste_2023-04-26_22-50-17.png)

#### **不选择H265的原因**

> **iOS11.0**之后才支持**H265**。
> 相对于**H264**，**H265**对CPU造成的负荷更大，CPU发热更严重。

#### 编码格式发展史
![](img/Snipaste_2023-04-26_00-06-18.png)
![](img/Snipaste_2023-04-26_00-06-07.png)

由于iso名声比较大,所以对外命名以iso项目命名,但是ITU技术比较好,在程序员圈中都以他为准  
![](img/Snipaste_2023-04-26_00-22-00.png)


#### 编解码方式和封装格式的关系

> **「视频封装格式」= 视频 + 音频 +视频编解码方式 等信息的容器。**

一种「视频封装格式」可以支持多种「视频编解码方式」。比如：QuickTime File Format(.MOV) 支持几乎所有的「视频编解码方式」，MPEG(.MP4)
也支持相当广的「视频编解码方式」。

比较专业的说法可能是以 **A/B** 这种方式，**A 是「视频编解码方式」，B 是「视频封装格式」**。比如：一个 H.264/MOV 的视频文件，它的封装方式就是 QuickTime File
Format，编码方式是 H.264。

### RBG&YUV

> rgb
> ![](img\1610710285279041278.png)


> YUV同样使用三个分量来存储数据，他们分别是
> * Y：用于表示明亮度（Luminance或Luma）
> * U： 用于表示色度（Chrominance或Chroma）
> * V：用于表示色度（Chrominance或Chroma）
> ![](img/Snipaste_2023-04-25_23-19-10.png)

> 下图为YUV图片的原图，下面的图片分别为只有Y分量、只有U分量、只有V分量数据的图片。可以看到只有Y分量的图片能够看清楚图片的轮廓，但图片是黑白的。[参考](https://www.jianshu.com/p/4fa388739e05)
> ![](img\13434832-e8177c0534e240e8.png)
> ![](img\efe87d1b93314a1593ff013813fb45fa_tplv-k3u1fbpfcp-zoom-in-crop-mark_4536_0_0_0.png)  
> yuv格式详解[参考](https://blog.csdn.net/xkuzhang/article/details/115423061)

#### 为什么视频不用rgb要用yuv?
> rgb基本用在图像的存储,并且十分简单,但是在视频和相机领域中就基本不可能,因为视频都是一张张连续的图片排列组成的
> 假设视频是一分钟30帧1080p(1920*1080),用rgb存储需要(1920 * 1080 * (3 * 8) * 30 * 60)=834G
> yuv(nv21,不进行h264编码)需要(1920 * 1080 * 8 + (1920 * 1080 * 8 * 1/4 *2)) * 30 * 60 = 41G

### 码率(比特率)
指平均每秒多少bit,比如一个视频100分钟,大小1g
100M=100X60S=6000s
1GB=1024MB= 1024X1024KB=1024X1024X1024Byte=1024X1024X1024X8bit=8589934592bit
那么这个视频的码率大概就是1.4Mbit/s(8589934592/6000)

### 帧

* **I帧**
> 关键帧,完整数据

* **P帧**
> 与前面一张图片的区别的区域 

* **B帧**
> 与前面以及后面的图片的区别区域

![](img/Snipaste_2023-04-26_23-05-34.png)
[图讲解I,B,P](http://www.tlcement.com/327667.html)
[图解I,B,P-2](https://www.nxrte.com/jishu/492.html)

**两个I帧之间称为GOP,图像序列**
![](img\31.png)

#### 场编码和帧编码
* 帧编码
> 完整的编码一帧  

* 场编码
>  CRT(大屁股)显示器传输频宽不够，逐行扫描的方法通常从上到下地扫描每帧图像。这个过程消耗的时间比较长，阴极射线的荧光衰减将造成人视觉的闪烁感觉。当频宽受限，以至于不可能快到使用逐行扫描而且没有闪烁效应时，通常采用一种折中的办法，即每次只传输和显示一半的扫描线，即场。一场只包含偶数行（即偶场）或者奇数行（即奇场）扫描线。由于视觉暂留效应，人眼不会注意到两场只有一半的扫描行，而会看到完整的一帧。
>  ![](img/Snipaste_2023-04-25_23-20-39.png)  
>  ![](img/Snipaste_2023-04-25_22-12-34.png)
>  在视频里看到的1080P的P指的是逐行扫描(Progressive Scanning),并非分辨率
>  [参考](https://zhuanlan.zhihu.com/p/439780995)

### 码流

> 其实就是视频的原始编码后的二进制数据,非封装后的数据
> 在码流中,**I帧**后面跟着**P帧**,接着再是**B帧**,但实际播放的顺序是根据**pts**,解码顺序根据**dts**
> 录视频时,获取到的都是yuv数据,编码时内部算法会判断哪些是I,P,B帧,此时就会设置对应的dts和pts

![](img/Snipaste_2023-03-11_17-49-45.png)
#### 为什么需要pts和dts
> P帧依赖I帧,B帧依赖I帧和P帧,这样就带来了一个问题：在视频流中，先到来的 B 帧无法立即解码，需要等待它依赖的后面的 I、P 帧先解码完成，这样一来播放时间与解码时间不一致

### 编码流程

#### 底层元器件

* 视频解码器
  ![](img\图片1.png)
* 信源编解码器
  ![](img\图片2.png)

#### dsp

> cpu中的某个元器件
> ![](img/QQ截图20230304140730.png)
> 有cpu为什么要单独设计个dsp?
> 因为cpu擅长计算,由于视频处理很复杂,数据量很庞大,如果把视频相关的数据放在cpu中,会占用资源,导致很卡,所以需要dsp
> **软解**:写代码解码,跑cpu,兼容性高,但是容易发烫,耗电量高,卡顿(FFmpeg)
> **硬解**:把数据传递给dsp,靠硬件解码,兼容性差,不发烫...(MediaPlayer,MediaCodec)
> 先硬解再软解[参考](https://www.cnblogs.com/nmj1986/p/9288674.html)

### 编码原理

#### 区分IPB
> 节约空间,上面有讲

#### 宏块划分与分组
* 宏块
> 宏块就是把视频的每一帧（相当于一张图片）划分成16*16的小块，一块一块的依次压缩，而不是对整张图片一起压缩。一个宏块又可以分成16*16,16*8,8*16,8*8,8*4,4*8,4*4,等大小不等的块。具体怎么划分块大小，要看画面有多复杂。一般来说，运动多，细节多的部分，划分成小块来编码；大片的平坦的无变化的，划分成16*16的大块
> ![](img/Snipaste_2023-04-25_23-05-37.png)

#### 帧内预测
> 帧内预测可以进一步压缩文件大小,也可以防止视频产生锯齿现象  
> 对于一幅图像，相邻的两个像素的亮度和色度值之间经常是比较接近的，也就是颜色是逐渐变化的，不会一下子突变成完全不一样的颜色
> 所以一个宏块只存上和左的数据
> ![4X4亮度快预测模式](img/Snipaste_2023-04-25_23-59-48.png)


#### 参考
[I,P,B大概的编码流程](https://www.cnblogs.com/sddai/p/14360950.html)
[帧内预测](https://www.jianshu.com/p/d5d92421186a)

### 视频数据解析

#### h264

![](img/QQ截图20230304142522.png)

##### 数据分析

* 分隔符,每个分隔符之间称为NALU
  ![](img/Snipaste_2023-04-26_21-47-24.png)
  00 00 00 01/00 00 01,避免出现数据刚好相同,被误判,导致解码失败,如果编码器检测到NAL数据存在以下4种数据时，编码器会在最后个字节前插入一个新的字节0x03。

  ```
  0x 00 00 00 -->0x 00 00 03 00  --> 针对 0x 00 00 00 01的case
  0x 00 00 01 -->0x 00 00 03 01  --> 针对 0x 00 00 01的case
  0x 00 00 02 -->0x 00 00 03 02  --> 0x 00 00 02是协议的保留字段，将来可能会使用，所以也要加入防竞争字节
  0x 00 00 03 -->0x 00 00 03 03  --> 为了避免对原始数据中的0x 00 00 03进行脱壳操作，从而造成数据丢失
  ```
* 头信息,NALU Header
  sps(基础配置,宽高帧率等)
  pps(全量配置,基本是算法的一些配置)
  ![](img/QQ截图20230304152202.png)
  ![](img/微信截图_20230304161800.png)
* forbidden_bit：禁止位(1)

> 编码中默认值为0，当网络识别此单元中存在比特错误时，可将其设为1，以便接收方丢掉该单元

* nal_reference_bit：重要性指示位(2)

> 值越大,越重要,sps/pps/I->11,p->10,b->01

* nal_unit_type：NALU类型位 (5)

常见的type  
![](img/Snipaste_2023-04-26_21-44-57.png)  
![](img/QQ截图20230304162412.png)

抖音下载的视频提取h264,基本都有sel
1.并不是解码过程的必须项；
2.有可能对解码过程（容错、纠错）有帮助
[sel讲解](https://www.jianshu.com/p/4d9120dfcd69)

##### 例子

0x67-0110 0111

##### 参考链接

[参考](http://blog.yundiantech.com/?log=blog&id=40)
[参考](https://zhuanlan.zhihu.com/p/511793341)
[参考](https://juejin.cn/post/7172132096733872158)
[参考](http://blog.yundiantech.com/?log=blog&id=40)
**[参考](https://yangandmore.github.io/2021/11/02/%E9%9F%B3%E8%A7%86%E9%A2%91%E7%BC%96%E8%A7%A3%E7%A0%81%E6%8A%80%E6%9C%AF%E5%9F%BA%E7%A1%80/)**
**[参考](https://juejin.cn/post/7009100729994444837)**

#### h265

![](img/微信截图_20230304180910.png)

> h265和h264差别
> H.265仍然采用混合编解码，编解码结构域H.264基本一致，
> 主要的不同在于：

* 编码块划分结构：采用CU (CodingUnit)、PU(PredictionUnit)和TU(TransformUnit)的递归结构。
* 基本细节：各功能块的内部细节有很多差异
* 并行工具：增加了Tile以及WPP等并行工具集以提高编码速度

##### 宏块划分

> 在H.265中，将宏块的大小从H.264的16×16扩展到了64×64，以便于高分辨率视频的压缩。
> 较复杂图片H.264和H.265区别不大,大面积相识度高的图片H.265比H.264好,并且压缩体积更优秀
> ![](img/微信截图_20230304182554.png)

##### 帧内预测模式

> 本质上H.265是在H.264的预测方向基础上增加了更多的预测方向
> H.265：所有尺寸的CU块，亮度有35种预测方向，色度有5种预测方向
> H.264：亮度 4x4块9个方向，8x8块9个方向，16x16块4种方向，色度4种方向

##### 数据分析

> 分隔符和H264一样,头信息增加,两字节表示
> 相比H264,H265移除了nal_reference_bit,将此信息合并到nal_unit_type中
> ![](img/微信截图_20230304183358.png)
> ![](img/微信截图_20230304183548.png)

##### 例子

0x40 0x01-01000000 000000001

##### 参考链接

**[参考](https://blog.csdn.net/u014470361/article/details/89541544)**
**[参考](https://zhuanlan.zhihu.com/p/589739772)**
**[参考](https://blog.51cto.com/u_15077549/3366546)**


### 软件
![](img/Snipaste_2023-04-26_23-21-03.png)

### 码流结构

> 宏观结构,图像-片组-片-NALU-宏块 -像素
> ![](img/QQ截图20230304190453.png)

#### H264编码分层

* **NAL层:（Network Abstraction Layer,视频数据网络抽象层）**：
  根据不同的网络把数据打包成相应的格式，将VCL产生的比特字符串适配到各种各样的网络和多元环境中
* **VCL层:（Video Coding Layer,视频数据编码层）**： 对视频原始数据进行压缩  
* ![](img/Snipaste_2023-04-26_21-39-33.png)

#### H264码流分层
![](img/Snipaste_2023-04-26_22-21-25.png)

**[参考](https://blog.csdn.net/weixin_45112340/article/details/115542769)**
**[详解每层类型](https://blog.csdn.net/luoyaxing0812/article/details/111352155)**
**[详解宏块类型](https://blog.csdn.net/zhoutaopower/article/details/127338752)**


## 哥伦布编码

>
无损压缩,变长算法,比较适合小数字比大数字出现概率高的场景编码,并不是一味的为了减少内存占用,[wiki百科](https://zh.wikipedia.org/wiki/%E6%8C%87%E6%95%B0%E5%93%A5%E4%BC%A6%E5%B8%83%E7%A0%81)

### 原理

| 源数据 | step1 | step2     | 编码结果  |
|-----|-------|-----------|-------|
| 0   | 0+1=1 | 1->二进制1   | 1     |
| 1   | 1+1=2 | 2->二进制10  | 010   |
| 3   | 3+1=4 | 4->二进制100 | 00100 |

step1.源数据+1,得到结果X  
step2.X转二进制,看1后面有Y位,在1前补Y个0  
**伦布编码优势**    
如果按字节表示,浪费空间较多(小数字>大数字的情况,两个1,一个255,字节表示需要3个字节,哥伦布编码只需要2个字节+1位)  
如果按位表示,只有0/1,限制较大,并且该编码方便读取和写入,参考H264Codec.kt

### 文档

doc/H.264视频编码官方中文帮助文档.pdf  
在H.264中，指数哥伦布编码有四个描述子，分别为ue(v)、se(v)、me(v)、te(v)。其中me(v)是最简单的，它直接靠查表来实现。而剩余的se(v)和te(v)，是在ue(v)的基础上来实现的

* **ue(v)无符号整数指数哥伦布码编码**  
  上述原理
* **se(v)有符号整数指数哥伦布码编码**  
  se(v)需要先调用ue(v)得到codeNum，然后再调用se(v)的过程.  
  value = (-1)^(codeNum+1) * (codeNum+1)/2;  
  (-1)^(codeNum+1)：表示如果codeNum为奇数那么是1，偶数为-1
* **te(v)舍位指数哥伦布码编码语**  
  te(v)需要先判断范围
  ```c
  // 1.判断取值上限
  if( x == 1 ) // 如果为1则将读取到的比特值取反
  {
  return 1 - bs_read_u1( b );
  }
  else if( x > 1 ) // 否则按照ue(v)进行解码
  {
  return bs_read_ue( b );
  }
  return 0;
  ```

### 实例

[参考](https://zhuanlan.zhihu.com/p/27896239)

> **videlfile/16.h264 16*16**
>
> 42:01000010  
> profile_idc u(8):66,编码等级,直播
>
> C0:11000000  
> flag u(1+1+1+1+4):
>
> 29:00101001  
> level_idc u(8):41,最大支持码流范围,Supports 2Kx1K format
>
> 8D:10001101  
> seq_parameter_set_id ue(v):1->0,sps id,通过该id值,图像参数集pps可以引用其代表的sps中的参数  
> log2_max_frame_num_minus4 ue(v):0001101->12,用于计算MaxFrameNum的值  
> ``计算公式为MaxFrameNum = 2^(log2_max_frame_num_minus4 + 4)。MaxFrameNum是frame_num的上限值，frame_num是图像序号的一种表示方法，在帧间编码中常用作一种参考帧标记的手段。``
>
> 69:01101001  
> pic_order_cnt_type ue(v):011->2,表示解码picture order count(POC)
> 的方法。POC是另一种计量图像序号的方式，与frame_num有着不同的计算方法。该语法元素的取值为0、1或2。    
> num_ref_frames ue(v):010->1,用于表示参考帧的最大数目。    
> gaps_in_frame_num_value_allowed_flag u(1):0,标识位，说明frame_num中是否允许不连续的值。  
> pic_width_in_mbs_minus1 ue(v):1->0,(0+1)*16=16,宽度.由于宽度不可能为0,所以定义需要+1
>
> E9:11101001  
> pic_height_in_map_units_minus1 ue(v):1->0,(0+1)*16=16,高度  
> frame_mbs_only_flag u(1):1,当该标识位为0时，宏块可能为帧编码或场编码；该标识位为1时，所有宏块都采用帧编码  
> direct_8x8_inference_flag u(1):1,  
> frame_cropping_flag u(1):0,没有额外裁切  
> ![](img/Snipaste_2023-03-10_23-28-12.png)

> **videofile/24.h264 24*24**  
>
> 67:01100111  
> f(1)0:可用  
> u(2)11:高  
> u(5)00111:十进制7,序列集参数
>
> 64:01100100  
> u(8)编码等级,十进制100,hight  
>
> 00:00000000  
> u(1+1+1+1+4)扩展  
>
> 1F:00011111  
> level_idc u(8) 最大支持码流范围,十进制31,Supports 720p HD format  
>
> AC:10101100  
> seq_parameter_set_id ue(v)1:序列参数集的id,解码后为0  
> chroma_format_idc ue(v):010->1,与亮度取样对应的色度取样,4:2:0,**如果profileidc不成立,chroma_format_idc默认值id为1**  
> bit_depth_luma_minus8 ue(v):1->0,视频位深,High 只支持8bit  
> bit_depth_chroma_minus8 ue(v)1:profile定义  
> qpprime_y_zero_transform_bypass_flag u(1)0:??  
> seq_scaling_matrix_present_flag u(1)0:  
>
> 2C:00101100  
> log2_max_frame_num_minus4 ue(v):00101->4  
> pic_order_cnt_type ue(v):1->0  
>
> A5:10100101  
> log2_max_pic_order_cnt_lsb_minus4 ue(v):00(前一个字节的00)101  
> num_ref_frames ue(v):00101->4  
>
> 25:00100101  
> gaps_in_frame_num_value_allowed_flag u(1)0:  
> pic_width_in_mbs_minus1 ue(v):010->1,(1+1) * 16=32,由于24不是16的倍数,所以还需要获取frame_crop_xxx的值进行计算  
> pic_height_in_map_units_minus1 ue(v):010->1,(1+1)*16=32,由于24不是16的倍数,所以还需要获取frame_crop_xxx的值进行计算  
> ```
> 由于出现需要裁切的情况,需要考虑几点
> 1.在场编码中,一帧等于两场,高度是帧的一半,所以裁切时,crop 1 一像素，相当于对帧 crop 2 个像素???
> 2.yuv格式,在yuv420情况下,无法去除单个行,所以只能去除偶数个行,所以在yuv420上需要crop*2
> 3.ChromaArrayType,非语法表格中的内容,residual_colour_transform_flag(默认值为0) 和 chroma_format_idc 共同作用推导出来的
>   if (residual_colour_transform_flag == 0){
>       ChromaArrayType = chroma_format_idc;
>   }else{
>     ChromaArrayType = 0;
>   }
> 4.SubWidthC 和 SubHeightC 表示的是 YUV 分量中，Y 分量和 UV 分量在水平和竖直方向上的比值。
> 当 ChromaArrayType 等于 0 的时候，表示只有 Y 分量或者表示 YUV 444 的独立模式，所以 SubWidthC 和 SubHeightC 没有意义。
>   if (ChromaArrayType == 1) {
>     SubWidthC = 2;
>     SubHeightC = 2;
>   }
>   else if (ChromaArrayType == 2) {
>     SubWidthC = 2;
>     SubHeightC = 1;
>   }
>   else if (ChromaArrayType == 3) {
>     SubWidthC = 1;
>     SubHeightC = 1;
>   }
> 
> 最终宽高计算公式为
> width = (pic_width_in_mbs_minus1 + 1) * 16;
> height = (2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16;  
> if(frame_cropping_flag){
>   int crop_unit_x = 0;
>   int crop_unit_y = 0;
> 
>     if(ChromaArrayType == 0){
>         crop_unit_x = 1;
>         crop_unit_y = 2 - frame_mbs_only_flag;
>     }
>     else if(ChromaArrayType == 1 || ChromaArrayType == 2 || ChromaArrayType == 3){
>         crop_unit_x = SubWidthC;
>         crop_unit_y = SubHeightC * (2 - frame_mbs_only_flag);
>     }
> 
>     width -= crop_unit_x * (frame_crop_left_offset + frame_crop_right_offset);
>     height -= crop_unit_y * (frame_crop_top_offset + frame_crop_bottom_offset);
> }
> 
> width:32 = (1+1)*16
> height:32 = (2-1)*(1+1)*16
> if(1){
>   int crop_unit_x = 0;
>   int crop_unit_y = 0;
>   int ChromaArrayType = 1;//由于residual_colour_transform_flag=0,所以ChromaArrayType=chroma_format_idc=1
>   SubWidthC = 2
>   SubHeightC = 2*(2-1)
>   width:32-=SubWidthC*(4+0)
>   height:32-=SubHeightC*(4+0)
> }
> 
>```
> frame_mbs_only_flag u(1):1, 等于 1 的时候，表示都是帧编码，等于0的时候，表示有可能存在场编码
>
> E5:11100101  
> direct_8x8_inference_flag u(1):1,  
> frame_cropping_flag u(1):1,是否需要对输出的图像帧进行裁剪  
> frame_crop_left_offset ue(v):1->0  
> frame_crop_right_offset ue(v):00101->4  
>
> 96:10010110  
> frame_crop_top_offset ue(v):1->0  
> frame_crop_bottom_offset ue(v):00101->4  

> ![](img/Snipaste_2023-03-11_23-09-28.png)

> **videofile/test.h264 3840*2160**
>
> 64:01100100  
> profile_idc u(8):100,编码等级,High (FRExt)
>
> 00:00000000  
> flag u(1+1+1+1+4):
>
> 33:00110011  
> level_idc u(8):51,最大支持码流范围,Supports 4096x2304 format
>
> AC:10101100  
> seq_parameter_set_id ue(v):1->0,序列参数集的id  
> chroma_format_idc ue(v):010->1,与亮度取样对应的色度取样,4:2:0  
> bit_depth_luma_minus8 ue(v):1->0,视频位深,0 High 只支持8bit  
> bit_depth_chroma_minus8 ue(v):1->0,  
> qpprime_y_zero_transform_bypass_flag u(1):0  
> seq_scaling_matrix_present_flag u(1):0
>
> B4:10110100  
> log2_max_frame_num_minus4 ue(v):1->0,  
> pic_order_cnt_type ue(v):011->2  
> num_ref_frames ue(v):010->1  
> gaps_in_frame_num_value_allowed_flag u(1):0
>
> 01:00000001  
> E0:11100000  
> pic_width_in_mbs_minus1 ue(v):000000011110000->239,(239+1)*16=3480宽度
>
> 02:00000010  
> 1F:00011111  
> pic_height_in_map_units_minus1 ue(v):000000010000111->134,(134+1)*16=2160
> ![](img/Snipaste_2023-03-10_23-53-31.png)


#### 区分I,P,B  
![](img/Snipaste_2023-03-12_17-22-51.png)
* I,type:5
* PB,type:1
需要读取rbsp里的slice_type区分P,B  
![](img/Snipaste_2023-03-12_17-29-25.png)


h264visa工具分析  
![](img/Snipaste_2023-03-12_00-42-44.png)  
对于宽高非16整数倍需要结合其他信息进行计算[参考](https://juejin.cn/post/6956445593933709319)  
![](img/v2-5b11cd2e81c4b134730c9856468a4f69_r.jpg)
#### 参考链接
**[手写解码器](https://www.zzsin.com/catalog/write_avc_decoder.html)**  
[参考](https://blog.csdn.net/houxiaoni01/article/details/99844945)    
[参考](https://blog.51cto.com/u_12204415/3804218)    
[参考](https://blog.51cto.com/u_13267193/5377091)

## 音频

[维基百科](https://zh.wikipedia.org/wiki/%E5%90%AC%E8%A7%89)
[声音长什么样](https://maintao.com/2021/sound/)
[声音图形并茂](https://www.lyratone.net/blog/about-sound-1015)

### 声音的产生

> 声音是由物体振动产生的，通过空气、固体、液体等介质进行传输的一种声波，可以被人耳识别的声波的范围是 20Hz~20000Hz
> 之间，也叫做可听声波，这种声波称之为声音，根据声波频率的不同可以主要分为:
>* 可听声波：20Hz~20kHz
>* 超声波：> 20kHz
>* 次声波：< 20Hz
>
>波长越短,间距越小,人的发声范围一般是85Hz~1100Hz,人耳能够听到的频率范围是20hz(17米的波长)-20Khz(1.7厘米的波长),部分特殊人群能听到22.05khz

### 声音的三要素

> 声音的三要素分别是音调、音量、音色，具体如下：
> * 音调：指的是声音**频率**(声音1秒内周期性变化的次数)的高低，表示人的听觉分辨一个声音的调子高低的程度，物体振动的快，发出的声音的音调就高，振动的慢，发出的音调就低。
> * 音量：又称音强、响度，指声音的**振幅**大小，表示人耳对所听到的声音大小强弱的主观感受。
> * 音色：又称音品，指不同声音表现在波形方面总是有与众不同的特性，不同的物体振动都有不同的特点，反映每个物体发出的声音的特有的品质，音色具体由谐波决定，好听的声音绝不仅仅是一个正弦波，而是谐波。[音色由什么决定?](https://zhuanlan.zhihu.com/p/62835952)

### 模数转换

> 声音是一个模拟音频信号，如果要将声音数字化，则需要将模拟音频信号转换为数字信号，这就是模数转换，主要流程包括采样、量化、编码
![](img/Snipaste_2023-03-11_15-43-44.png)

#### 采样率

![](img/Snipaste_2023-03-11_15-49-16.png)
> 1s多少个点,采样率44.1Khz,1s采样44100个点,该采样率为音频cd所用  
> ![](img/Snipaste_2023-03-11_15-21-46.png)
> **为什么是44.1?**   
> 根据采样定理,按比人能听到的最大频率的2倍进行采样可以保证声音在被数字化处理后,还能有质量保障

#### 量化格式

![](img/Snipaste_2023-03-11_15-50-09.png)
>
经过采样后，我们发现图中的纵坐标是没有值的，无法表示每段样本的数字大小，这时候就需要引入量化的概念。通俗易懂地讲「量化」就是在沿水平方向再将信号图按照一定数字范围切断，保证每段样本能用数字描述。这个数字的最终物理意义是反应在音响振膜位置，比如用[0-10万]
进行量化，最终反应在振膜的位置就是 0-10万。  
> 那么CD的量化标准是什么呢？采用16bit(short)，也就是2的16次方，总共65536，然后为了由于振膜是可以发生正向和负向位移，所以用[-32767,32768]进行量化。  
> 所以图中虚线范围就代表了量化的数字范围，最终的红色曲线就是量化的结果，数字信号

#### 编码

> 经过量化后，每一个采样都是一个数字，那这么多的数字该如何存储呢？这就需要第三个概念：「编码」，所谓编码，就是按照一定的格式记录采样和量化后的数据，比如顺序存储或压缩存储等。  
> 这里涉及很多种格式，通常所说的音频的裸数据格式就是脉冲编码调制数据，简称 PCM （Pulse Code Modulation)。描述一段 PCM 通常需要以下三个概念
> * 量化格式（SampleFormat）
> * 采样率（SampleRate)
> * 声道数（Channel)  
    > 平时所谓的双声道、单声道其实可以理解为需要记录几个信号，比如磁带，双声道就是同一时刻记录两个轨道的信息，一个负责记录左耳机振膜位置，一个负责记录右耳机振膜位置，以此类推，多个声道也是类似

##### 大小
>称为数据比特率（bitRate),即1s内的比特数目,单位为千比特每秒kbps(kb per second)[比特率设置](https://blog.csdn.net/android_cai_niao/article/details/110153682)  

公式=采样率 * 量化格式 * 声道数  
如:采样率 44.1KHz,量化格式为 16bit,双声道  
44.1 * 16 * 2=1411.2kbps,1s0.17m+,一分钟10m  
大小对于磁盘来说可以接受,但是对于网络传输不行,所以需要压缩

#### 压缩编码
> pcm,一种编码方式，在音视频领域则理解为原始音频数据裸流  
> Android 中使用 AudioRecord、MediaRecord等采集到的音频数据就是 PCM 数据

压缩是为了减小编码后的数据存储空间，那么就应该去掉音频的“冗余信息”，从以下两个方面去衡量哪些数据是冗余的

* 人耳所能察觉的声音信号的频率范围为20Hz-20KHz，除此之外的其它频率人耳无法察觉，都可视为冗余信号
* 当一个强音频信号和一个弱音频信号同时存在时，弱信号会被强信号掩蔽，可视为冗余

其中第二点涉及另两个概念「频谱掩蔽效应」和「时域掩蔽效应」，名字看起来高深莫测，其实不难理解。
相当于高dB声音覆盖了低dB声音
![](img/Snipaste_2023-03-11_16-20-02.png)

#### 压缩编码格式

|编码    |实现简介|    特点|    适用场景|
|---|---|---|---|
|WAV|    无损压缩，其中一种实现方式是在 PCM 数据格式前加上 44 字节，分别描述采样率、声道数、数据格式等信息。|    音质非常好，大量软件都支持    |多媒体开发的中间文件、保存音乐和音效|
|MP3|    具有不错的压缩比，使用 LAME 编码（MP3 编码格式的一种实现）的中高码率的 MP3 文件    |音质在 128Kbit/s 以上表现还不错，压缩比比较高，大量软硬件都支持|    高比特率下对兼容性有要求的音乐鉴赏|
|AAC|    新一代有损压缩技术，通过一些附加的编码技术（PS、SBR 等），衍生出了 LC-AAC、HE-AAC、HE-AAC v2三种主要编码格式    |小于 128Kbit/s 表现优异，多用于视频中的音频编码    |128Kbit/s 一下的音频编码，多用于视频中的音频编码|
|Ogg|    一种非常有潜力的编码，各种码率下都有比较优秀的表现，尤其是低码率场景下。可以在低码率的场景下仍然保持不错的音质，但目前软件硬件支持情况较差|    可用比 MP3 更小的码率实现比 MP3 更好的音质，但兼容性不好    |语音聊天的音频消息场景|

### 参考链接

**[参考](https://zhuanlan.zhihu.com/p/69901270)**
**[参考](https://zhuanlan.zhihu.com/p/161453747)**
**[傅里叶分析](https://zhuanlan.zhihu.com/p/19763358)**
[参考](https://www.newvfx.com/forums/topic/21333)


## 直播推流

### RTMP
> RTMP:实时消息传输协议,基于TCP,默认使用1935端口的“明文”协议[维基百科](https://zh.wikipedia.org/wiki/%E5%AE%9E%E6%97%B6%E6%B6%88%E6%81%AF%E5%8D%8F%E8%AE%AE)
> 直播推流流程 : 手机采集视频 / 音频数据 , 视频数据使用 H.264 编码 , 音频数据使用 AAC 编码 , 最后将音视频数据都打包到 RTMP 数据包中 , 使用 RTMP 协议上传到 RTMP 服务器中 ;

#### RTMP-API
<img src="img/31113620_624521b4e2e1f3803.png" style="zoom:50%;" />

#### RTMP 包格式
结构体,按照格式设置参数到body里
![](img/Snipaste_2023-03-28_22-40-52.png)
* **视频包格式**
![](img/Snipaste_2023-03-28_22-36-48.png)

* **音频包格式**
![](img/Snipaste_2023-03-29_22-32-51.png)


#### librtmp
> 封装了socket,建立tcp通信,实现RTMP数据发送的so库[下载链接](http://rtmpdump.mplayerhq.hu/)

#### 参考
[Android RTMP 直播](https://www.jianshu.com/p/02b6eb0e3498)
[RTMP 数据格式](https://blog.csdn.net/heli200482128/article/details/127198244)  
[RTMP详讲](https://zhuanlan.zhihu.com/p/191542130)  



## FFMPEG
[官网](https://ffmpeg.org/)  
> FFmpeg 是一个音视频软解so库

### 主要流程
<img src="img/559adc64edfa49809396ba6258df8bac.png" style="zoom:60%;" />

### 常用api
![](img/Snipaste_2023-04-13_23-34-26.png)

### ffmpeg获取视频文件h264码流
> ffmpeg filter(过滤器，名字取的很难理解)  
> 将视频从1080 * 1920缩小到520 * 960，也属于过滤，音频混音也属于过滤
控制台查询支持的filters列表
![](img/Snipaste_2023-04-24_23-44-13.png)

#### 参考
[使用过滤器获取mp4中的h264码流](https://www.cnblogs.com/vczf/p/13818609.html)

### 解码耗时
硬解:
![](img/Snipaste_2023-04-26_23-57-18.png)

ffmpeg软解:
![](img/Snipaste_2023-04-26_23-51-42.png)

### 剪辑
快影/剪映:重编码,原视频时长29s,2m,剪辑成13s,2+m,

sdk:丢帧,只留IP

#### 参考
[ffmpeg实现重封装](https://codeantenna.com/a/LavUIN2K2P)  
[ffmpeg解码](https://www.nxrte.com/jishu/13254.html)  
[ffmpeg硬解](https://cloud.tencent.com/developer/article/2115712) +[ffmpeg直接渲染Surface](https://cloud.tencent.com/developer/article/1901883?from=article.detail.1928893)
[ffmpeg硬解2](https://zhuanlan.zhihu.com/p/168240163)   
[ffmpeg硬解3](https://blog.csdn.net/lakebobo/article/details/79619584)
[ffmpeg官方demo](https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/hw_decode.c)
[大佬分析ffmpeg源码/命令行](https://ffmpeg.xianwaizhiyin.net/)
[ffmpeg ftiler讲解](https://cloud.tencent.com/developer/article/2168943?areaSource=&traceId=)+[抽帧](https://cloud.tencent.com/developer/article/2168938?areaSource=&traceId=)

### 参考
[一文读懂 FFmpeg](https://juejin.cn/post/7072198840035573790#heading-19)

## open系列
* OpenGL（OpenGraphics Library）
OpenGL是业界最广泛采用的二维和三维图形API，将成千上万的应用程序应用到各种各样的计算机平台上。它是独立于窗口系统和操作系统以及网络透明。OpenGL使软件开发人员为PC，工作站和超级计算机硬件创造高性能，视觉上引人注目的图形软件应用程序，在市场，如CAD，内容创作，能源，娱乐，游戏开发，制造，医疗和虚拟现实。OpenGL公开了最新图形硬件的所有功能。

* OpenCL（OpenComputing Language）
开放运算语言opencl并行计算，面向异构系统通用目的并行编程的开放式、免费标准，也是一个统一的编程环境，便于软件开发人员为高性能计算服务器、桌面计算系统、手持设备编写高效轻便的代码，而且广泛适用于多核心处理器(CPU)、图形处理器(GPU)、Cell类型架构以及数字信号处理器(DSP)等其他并行处理器，在游戏、娱乐、科研、医疗等各种领域都有广阔的发展前景。

* OpenAL（Open AudioLibrary）
是自由软件界的跨平台音效API。它设计给多通道三维位置音效的特效表现。其 API 风格模仿自OpenGL。 

* OpenGL ES (OpenGL for EmbeddedSystems)
嵌入式系统采用的OpenGL集，是 OpenGL 三维图形 API 的子集，针对手机、PDA和游戏主机等嵌入式设备而设计。该API由Khronos集团定义推广，Khronos是一个图形软硬件行业协会，该协会主要关注图形和多媒体方面的开放标准。 

* OpenVG（Vector GraphicsAcceleration）
矢量图形设备加速器，OpenVG是免费的，跨平台的API，提供了一个低级别的硬件加速接口的矢量图形库如Flash和SVG。OpenVG是主要针对手持设备，需要移动加速引人注目的用户界面和小屏幕设备上的文本高质量的矢量图形，使硬件加速，在非常低的功率水平提供流畅的交互性能。

* OpenSL ES (Open Sound Library for Embedded Systems)
OpenSL ES 是无授权费、跨平台、针对嵌入式系统精心优化的硬件音频加速API。它为嵌入式移动多媒体设备上的本地应用程序开发者提供标准化, 高性能,低响应时间的音频功能实现方法，并实现软/硬件音频性能的直接跨平台部署，降低执行难度，促进高级音频市场的发展。

### opensl es(音频播放)
#### 为什么用opensl es播放视频
java层提供了AudioTrack,但是只支持部分封装格式的音频,并且如果是使用ffmpeg进行解码的话,还得从C回调到java,如果用opensl es可以直接在C操作

#### api设计
> OpenSL ES 是基于 c 语言实现的，但其提供的接口是采用面向对象的方式实现，OpenSL ES 的大多数 API 是通过对象来调用的

opensl es 使用三步骤
1.创建 create
2.实例化 realize
3.使用对应接口 GetInterface(xxx)
4.销毁 Destroy  

![](img/Snipaste_2023-04-20_22-57-21.png)

#### 对象(Object)与接口(Interface)
> Object和Interface是OpenSL ES库的两个非常重要的概念，OpenSL ES的整个框架就是基于这两个概念构成的  

1. 每个Object可能存在一个或者多个Interface，而每个Interface封装了相关的功能函数  
    比如当我们获取一个Audio Player对象后，可以通过该对象得到音频播放Interface、音量Interface、缓存队列Interface，然后调用这些Interface的功能函数实现音频播放、音量调节等功能

  ```c++
SLEngineItf pEngineItf = NULL; // OpenSL ES引擎Interface
SLObjectItf pPlayerObject = NULL;  // Audio Player对象
SLPlayItf pPlayerItf = NULL;	   // 播放接口
SLVolumeItf pVolumeItf = NULL;	   // 音量接口
SLAndroidSimpleBufferQueueItf pBufferItf = NULL; // 缓存队列接口

//创建播放器
(*pEngineItf)->CreateAudioPlayer(pEngineItf,&pPlayerObject,..);
//实例化
(*pPlayerObject)->Realize(pPlayerObject,SL_BOOLEAN_FALSE);               
//获取播放,暂停,恢复控制器
(*pPlayerObject)->GetInterface(pPlayerObject, SL_IID_PLAY,&pPlayerItf); 
//获取音量控制器
(*pPlayerObject)->GetInterface(pPlayerObject,SL_IID_VOLUME,&pVolumeItf); 

  ```

2. 每个Object对象提供了一些最基础的"管理"操作，比如它的Realize、Destory函数用于分配、释放资源，Resume函数用于结束SL_OBJECT_STATE_SUSPENED状态等等。如果系统使用该对象支持的功能函数，就需要通过该对象的GetInterface函数获取相应的Interface接口，然后通过该Interface接口来访问功能函数。下面以调节音量为例：
```c++
// OpenSL ES引擎Interface
SLEngineItf pEngineItf = NULL;
SLObjectItf pPlayerObject = NULL;  
// 首先，创建Audio Player对象
(*pEngineItf)->CreateAudioPlayer(pEngineItf,&pPlayerObject,..); 
// 其次，初始化Audio Player对象，即分配资源
(*pPlayerObject)->Realize(pPlayerObject,SL_BOOLEAN_FALSE); 
// 第三，获取Audio Player对象的音量(Volume)Interface
(*pPlayerObject)->GetInterface(pPlayerObject, SL_IID_VOLUME,&pVolumeItf);  
// 最后，调用Volume Interface的调节音量功能函数
(*pVolumeItf)->SetVolumeLevel(&pVolumeItf,level);
```

#### 对象状态机制
> OpenSL ES 的 Object 一般有三种状态，分别是：UNREALIZED （不可用），REALIZED（可用），SUSPENDED（挂起）。  

![](img/Snipaste_2023-04-20_23-10-40.png)
Object 处于 UNREALIZED （不可用）状态时，系统不会为其分配资源；调用 Realize 方法后便进入 REALIZED（可用）状态，此时对象的各个功能和资源可以正常访问；当系统音频相关的硬件设备被其他进程占用时，OpenSL ES Object 便会进入 SUSPENDED （挂起）状态，随后调用 Resume 方法可使对象重回 REALIZED（可用）状态；当 Object 使用结束后，调用 Destroy 方法释放资源，是对象重回 UNREALIZED （不可用）状态。



#### 参考
[opensl es api讲解以及ffmpeg解码音频](https://juejin.cn/post/7031848037311840293)
[opensl es播放音频](https://www.jianshu.com/p/5ab9a339346f)
[opensl es播放与采集](https://mp.weixin.qq.com/s/BSCmvkVVTEh2UuPcZhk3dA)

### opengl es(视频播放)
