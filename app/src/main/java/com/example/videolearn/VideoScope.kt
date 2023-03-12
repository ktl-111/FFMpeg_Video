package com.example.videolearn

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers

object VideoScope : CoroutineScope by CoroutineScope(Dispatchers.IO)