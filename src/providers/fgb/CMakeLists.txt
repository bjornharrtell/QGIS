
########################################################
# Files

SET (FGB_SRCS
  qgsfgbfeatureiterator.cpp
  qgsfgbprovider.cpp
)

SET (DTEXT_MOC_HDRS
  qgsfgbprovider.h
)

IF (WITH_GUI)
  SET(FGB_SRCS ${FGB_SRCS}
    qgsfgbsourceselect.cpp
  )
  SET(DTEXT_MOC_HDRS ${DTEXT_MOC_HDRS}
    qgsfgbsourceselect.h
  )
ENDIF ()

########################################################
# Build

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/core/expression
  ${CMAKE_SOURCE_DIR}/src/core/geometry
  ${CMAKE_SOURCE_DIR}/src/core/metadata
  ${CMAKE_SOURCE_DIR}/src/gui

  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
  ${CMAKE_BINARY_DIR}/src/ui
)
INCLUDE_DIRECTORIES (SYSTEM
)

QT5_WRAP_CPP(DTEXT_MOC_SRCS ${DTEXT_MOC_HDRS})

ADD_LIBRARY(fgbprovider MODULE ${FGB_SRCS} ${DTEXT_MOC_SRCS})

TARGET_LINK_LIBRARIES(fgbprovider
  qgis_core
)

IF (WITH_GUI)
  TARGET_LINK_LIBRARIES (fgbprovider
    qgis_gui
  )
ENDIF ()

# clang-tidy
IF(CLANG_TIDY_EXE)
  SET_TARGET_PROPERTIES(
    fgbprovider PROPERTIES
    CXX_CLANG_TIDY "${DO_CLANG_TIDY}"
  )
ENDIF(CLANG_TIDY_EXE)

########################################################
# Install

INSTALL (TARGETS fgbprovider
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})
