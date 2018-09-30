
########################################################
# Files

SET (FGB_SRCS
  qgsfgbprovider.cpp
  qgsfgbfeatureiterator.cpp
)

SET (FGB_MOC_HDRS
	qgsfgbprovider.h
)

########################################################
# Build

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}/src/core/geometry
  ${CMAKE_SOURCE_DIR}/src/core/metadata
  ${CMAKE_SOURCE_DIR}/src/core/expression

  ${CMAKE_BINARY_DIR}/src/core
  ${CMAKE_BINARY_DIR}/src/gui
)
INCLUDE_DIRECTORIES (SYSTEM
  ${EXPAT_INCLUDE_DIR}
)

QT5_WRAP_CPP(FGB_MOC_SRCS ${FGB_MOC_HDRS})

ADD_LIBRARY(fgbprovider MODULE ${FGB_SRCS} ${FGB_MOC_SRCS})

TARGET_LINK_LIBRARIES(fgbprovider
  ${EXPAT_LIBRARY}
  qgis_core
)


########################################################
# Install

INSTALL (TARGETS fgbprovider
  RUNTIME DESTINATION ${QGIS_PLUGIN_DIR}
  LIBRARY DESTINATION ${QGIS_PLUGIN_DIR})
