package com.example.lib

object MyClass {
    @JvmStatic
    fun main(args: Array<String>) {
//       000  00101
//        5      00101    --- 4
        val data = (7 and 0xFF).toByte() //得到一个字节 假设上一个值 读取成功   000   00101
        var mStartBit = 3
        var mZeroNum = 0
        // 字节 8位
        while (mStartBit < 8) {
            if (data.toInt() and (0x80 shr mStartBit) != 0) {
                break
            }
            mZeroNum++
            mStartBit++
        }
        //1 -1  =0  1232   542 35 43  12321321     二进制 0 0000001
        mStartBit++
        //        先统计  出现0 的个数
        println("mZeroNum :  $mZeroNum")

//        左 右 1
//        右  左 2
//         往后面读取相应的多少位  还原对应  把知还原 001   01    n位
//        分两部分还原  最高位   1   00个数       后面尾数  1<<2  =4  +1=5

//1<<2  1*2*2 =4+1  =5 -1  -=4
        var dwRet = 0
        for (i in 0 until mZeroNum) {
            dwRet = dwRet shl 1
            if (data.toInt() and (0x80 shr mStartBit) % 8 != 0) {
                dwRet += 1
            }
            mStartBit++
        }
        val value = (1 shl mZeroNum) + dwRet - 1
        println("value:  $value")
    }

}