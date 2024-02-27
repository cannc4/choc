
include_directories(./)

if (APPLE)
    ADD_LIBRARY(choc STATIC gui/im_MacOS_Webview.mm)
else()
    ADD_LIBRARY(choc STATIC gui/im_Win_Webview.cpp)
endif()
