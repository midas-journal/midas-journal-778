cmake_minimum_required(VERSION 2.4)

PROJECT( FastMarchingImageFilter )

#The following lines are required to use Dart
ENABLE_TESTING()
INCLUDE(Dart)

# Set up ITK
IF(USE_ITK)
  FIND_PACKAGE(ITK)
  IF(ITK_FOUND)
    INCLUDE(${ITK_USE_FILE})
  ELSE(ITK_FOUND)
    MESSAGE(FATAL_ERROR
      "Cannot build without ITK.  Please set ITK_DIR.")
  ENDIF(ITK_FOUND)
ENDIF(USE_ITK)

INCLUDE (${CMAKE_ROOT}/Modules/FindITK.cmake) 
IF (USE_ITK_FILE) 
  INCLUDE(${USE_ITK_FILE}) 
ENDIF(USE_ITK_FILE) 

#SET(IJ_INCLUDE_DIRS
#)

#INCLUDE_DIRECTORIES(${IJ_INCLUDE_DIRS})

LINK_DIRECTORIES(${ITK_LIBRARY_PATH})

IF(BUILD_TESTING)
# non-templated class -- this should be stored in a library and linked in...
ADD_EXECUTABLE( itkFastMarchingImageFilterTest itkFastMarchingImageFilterTest.cxx)
TARGET_LINK_LIBRARIES( itkFastMarchingImageFilterTest ITKIO )

ENDIF(BUILD_TESTING)

###
#  Perform testing
###

## 2-D

ADD_TEST( TEST_Brain2D_singleSeed_NoTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 2 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_speed.nii.gz 
  ${CMAKE_BINARY_DIR}/test_Brain2D_singleSeed_NoTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_singleSeed.nii.gz 
  150 0 )

ADD_TEST( TEST_Brain2D_singleSeed_StrictTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 2 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_speed.nii.gz 
  ${CMAKE_BINARY_DIR}/test_Brain2D_singleSeed_StrictTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_singleSeed.nii.gz 
  150 1 )

ADD_TEST( TEST_Brain2D_singleSeed_NoHandlesTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 2 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_speed.nii.gz 
  ${CMAKE_BINARY_DIR}/test_Brain2D_singleSeed_NoHandlesTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_singleSeed.nii.gz 
  150 2 )

ADD_TEST( TEST_Brain2D_multipleSeeds_NoTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 2 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_speed.nii.gz 
  ${CMAKE_BINARY_DIR}/test_Brain2D_multipleSeeds_NoTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_multipleSeeds.nii.gz 
  150 0 )

ADD_TEST( TEST_Brain2D_multipleSeeds_StrictTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 2 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_speed.nii.gz 
  ${CMAKE_BINARY_DIR}/test_Brain2D_multipleSeeds_StrictTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_multipleSeeds.nii.gz 
  150 1 )

ADD_TEST( TEST_Brain2D_multipleSeeds_NoHandlesTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 2 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_speed.nii.gz 
  ${CMAKE_BINARY_DIR}/test_Brain2D_multipleSeeds_NoHandlesTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/BrainProtonDensitySlice_multipleSeeds.nii.gz 
  150 2 )

## 3-D

ADD_TEST( TEST_torus_multipleSeeds_NoTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 3 
  ${CMAKE_SOURCE_DIR}/Images/torus.nii.gz 
  ${CMAKE_BINARY_DIR}/test_torus_multipleSeeds_NoTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/torus_multipleSeeds.nii.gz 
  150 0 )

ADD_TEST( TEST_torus_multipleSeeds_StrictTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 3 
  ${CMAKE_SOURCE_DIR}/Images/torus.nii.gz 
  ${CMAKE_BINARY_DIR}/test_torus_multipleSeeds_StrictTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/torus_multipleSeeds.nii.gz 
  150 1 )

ADD_TEST( TEST_torus_multipleSeeds_NoHandlesTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 3 
  ${CMAKE_SOURCE_DIR}/Images/torus.nii.gz 
  ${CMAKE_BINARY_DIR}/test_torus_multipleSeeds_NoHandlesTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/torus_multipleSeeds.nii.gz 
  150 2 )

ADD_TEST( TEST_wm_multipleSeeds_NoTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 3 
  ${CMAKE_SOURCE_DIR}/Images/wm.nii.gz 
  ${CMAKE_BINARY_DIR}/test_wm_multipleSeeds_NoTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/wm_multipleSeeds.nii.gz 
  150 0 )

ADD_TEST( TEST_wm_multipleSeeds_StrictTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 3 
  ${CMAKE_SOURCE_DIR}/Images/wm.nii.gz 
  ${CMAKE_BINARY_DIR}/test_wm_multipleSeeds_StrictTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/wm_multipleSeeds.nii.gz 
  150 1 )

ADD_TEST( TEST_wm_multipleSeeds_NoHandlesTopo ${CMAKE_BINARY_DIR}/itkFastMarchingImageFilterTest 3 
  ${CMAKE_SOURCE_DIR}/Images/wm.nii.gz 
  ${CMAKE_BINARY_DIR}/test_wm_multipleSeeds_NoHandlesTopo.nii.gz 
  ${CMAKE_SOURCE_DIR}/Images/wm_multipleSeeds.nii.gz 
  150 2 )

