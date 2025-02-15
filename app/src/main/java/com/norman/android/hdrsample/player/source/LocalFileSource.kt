package com.norman.android.hdrsample.player.source

import android.os.ParcelFileDescriptor
import java.io.File
import java.io.FileDescriptor

class LocalFileSource(private val filePath: String) : FileSource {
    override fun createFileDescriptor(): FileSource.Descriptor {
        return LocalFileSourceDescriptor(filePath)
    }

    override fun getPath(): String = filePath
    class LocalFileSourceDescriptor(private val filePath: String) : FileSource.Descriptor {
        private val fileDescriptor: ParcelFileDescriptor = ParcelFileDescriptor.open(File(filePath), ParcelFileDescriptor.MODE_READ_WRITE)
        var close = false

        override fun getFileDescriptor(): FileDescriptor {
            return fileDescriptor.fileDescriptor
        }

        override fun getLength(): Long {
            return fileDescriptor.statSize
        }

        override fun getStartOffset(): Long {
            return 0
        }

        override fun close() {
            fileDescriptor.close()
            close = true
        }

        override fun isClose(): Boolean {
            return close
        }

    }
}