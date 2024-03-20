package com.example.play.utils

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers

object MediaScope : CoroutineScope by CoroutineScope(Dispatchers.IO)