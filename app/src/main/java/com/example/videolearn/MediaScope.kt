package com.example.videolearn

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers

object MediaScope : CoroutineScope by CoroutineScope(Dispatchers.IO)