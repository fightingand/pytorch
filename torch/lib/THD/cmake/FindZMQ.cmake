FIND_LIBRARY(ZMQ_LIBRARIES zmq)
FIND_PATH(ZMQ_INCLUDE_DIRS zmq.h)

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(ZMQ DEFAULT_MSG ZMQ_LIBRARIES ZMQ_INCLUDE_DIRS)
