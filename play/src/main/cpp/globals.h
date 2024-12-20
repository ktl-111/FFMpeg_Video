
#ifndef VIDEOLEARN_GLOBALS_H
#define VIDEOLEARN_GLOBALS_H
#include "string"
const int TimeBaseDiff = 1500;

const int ERRORCODE_COMMON = 1;
const int ERRORCODE_OPENINPUT = 2;
const int ERRORCODE_FIND_STERAM = 3;
const int ERRORCODE_NOT_FIND_VIDEO_STERAM = 4;
const int ERRORCODE_NOT_FIND_DECODER = 5;
const int ERRORCODE_PARAMETERS_TO_CONTEXT = 6;
const int ERRORCODE_AVCODEC_OPEN2 = 7;
const int ERRORCODE_GET_AVFILTER = 8;
const int ERRORCODE_CREATE_FILTER = 9;
const int ERRORCODE_FILTER_ERROR = 10;
const int ERRORCODE_FILTER_WRITE_ERROR = 11;
const int ERRORCODE_ALLOC_OUTPUT_CONTEXT = 12;
const int ERRORCODE_NOT_FIND_ENCODE = 13;
const int ERRORCODE_OPEN_FILE = 14;
const int ERRORCODE_PREPARE_FILE = 15;
const std::string RESULT_SUCCESS = "success";
#endif //VIDEOLEARN_GLOBALS_H
