# http://ros.org/doc/groovy/api/catkin/html/user_guide/supposed.html
cmake_minimum_required(VERSION 2.8)
project(depthimage_to_laserscan)

# Load catkin and all dependencies required for this package
find_package(catkin REQUIRED dynamic_reconfigure image_geometry image_transport nodelet roscpp sensor_msgs)
#find_package(OpenCV REQUIRED)

catkin_package(DEPENDS sensor_msgs roscpp nodelet image_transport image_geometry dynamic_reconfigure
               INCLUDE_DIRS include
               LIBRARIES ${PROJECT_NAME}
)

include_directories(include ${catkin_INCLUDE_DIRS})

# Dynamic reconfigure support
generate_dynamic_reconfigure_options(cfg/Depth.cfg)

add_library(DepthImageToLaserScan src/DepthImageToLaserScan.cpp)
target_link_libraries(DepthImageToLaserScan ${catkin_LIBRARIES})

add_library(DepthImageToLaserScanROS src/DepthImageToLaserScanROS.cpp)
target_link_libraries(DepthImageToLaserScanROS DepthImageToLaserScan ${catkin_LIBRARIES})

add_library(DepthImageToLaserScanNodelet src/DepthImageToLaserScanNodelet.cpp)
add_dependencies(DepthImageToLaserScanNodelet ${PROJECT_NAME}_gencfg)
target_link_libraries(DepthImageToLaserScanNodelet DepthImageToLaserScanROS ${catkin_LIBRARIES})

add_executable(depthimage_to_laserscan src/depthimage_to_laserscan.cpp)
add_dependencies(depthimage_to_laserscan ${PROJECT_NAME}_gencfg)
target_link_libraries(depthimage_to_laserscan DepthImageToLaserScanROS ${catkin_LIBRARIES})

# Test the library
catkin_add_gtest(libtest test/DepthImageToLaserScanTest.cpp)
target_link_libraries(libtest DepthImageToLaserScan ${catkin_LIBRARIES})

# add the test executable, keep it from being built by "make all"
add_executable(test_dtl EXCLUDE_FROM_ALL test/depthimage_to_laserscan_rostest.cpp)

# Install targets
install(TARGETS DepthImageToLaserScan DepthImageToLaserScanROS DepthImageToLaserScanNodelet DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION})

install(DIRECTORY include/${PROJECT_NAME}/
        DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)
install(FILES nodelets.xml
        DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)
install(TARGETS depthimage_to_laserscan
        DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)