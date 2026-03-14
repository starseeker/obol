# source files
set(OBOL_MISC_FILES
	SoBase.cpp
	SoBaseP.cpp
	SoChildList.cpp
	SoCompactPathList.cpp
	SoConfigSettings.cpp
	SoContextHandler.cpp
	SoDB.cpp
	SoDebug.cpp
	SoFullPath.cpp
	SoGenerate.cpp
	SoInteraction.cpp
	SoLightPath.cpp
	SoLockManager.cpp
	SoNormalGenerator.cpp
	SoNotRec.cpp
	SoNotification.cpp
	SoPath.cpp
	SoPick.cpp
	SoPickedPoint.cpp
	SoPrimitiveVertex.cpp
	SoProto.cpp
	SoProtoInstance.cpp
	SoSceneManager.cpp
	SoSceneManagerP.cpp
	SoShaderGenerator.cpp
	SoState.cpp
	SoTempPath.cpp
	SoType.cpp
	CoinResources.cpp
	SoDBP.cpp
	SoEventManager.cpp
	SoViewport.cpp
	SoQuadViewport.cpp
	SoRaytracerSceneCollector.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_MISC_INTERNAL_FILES
	SbHash.h
	SoBaseP.h
	SoBaseP.cpp
	SoCompactPathList.h
	SoCompactPathList.cpp
	SoConfigSettings.h
	SoConfigSettings.cpp
	SoDBP.h
	SoDBP.cpp
	SoGenerate.h
	SoGenerate.cpp
	SoPick.h
	SoPick.cpp
	SoSceneManagerP.h
	SoSceneManagerP.cpp
	SoShaderGenerator.h
	SoShaderGenerator.cpp
)

# build library
foreach(SRCFILE ${OBOL_MISC_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} misc/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)
if(OBOL_USE_OSMESA)
	set(OBOL_SOURCE ${OBOL_SOURCE} misc/SoDBOSMesa.cpp CACHE INTERNAL "OBOL_SOURCE")
elseif(OBOL_BUILD_DUAL_GL)
	set(OBOL_SOURCE ${OBOL_SOURCE} misc/SoDBOSMesa.cpp CACHE INTERNAL "OBOL_SOURCE")
elseif(OBOL_USE_PORTABLEGL)
	set(OBOL_SOURCE ${OBOL_SOURCE}
		misc/portablegl_impl.cpp
		misc/portablegl_compat_funcs.cpp
		misc/portablegl_shader_registry.cpp
		misc/SoDBPortableGL.cpp
		CACHE INTERNAL "OBOL_SOURCE")
elseif(OBOL_BUILD_DUAL_PORTABLEGL)
	# Dual-portablegl build: PortableGL sources are compiled in alongside
	# system GL.  portablegl_impl.cpp defines PORTABLEGL_IMPLEMENTATION with
	# PGL_PREFIX_GL so the compiled symbols are pgl_gl* (not gl*), avoiding
	# linker collisions with system OpenGL.
	set(OBOL_SOURCE ${OBOL_SOURCE}
		misc/portablegl_impl.cpp
		misc/portablegl_compat_funcs.cpp
		misc/portablegl_shader_registry.cpp
		misc/SoDBPortableGL.cpp
		CACHE INTERNAL "OBOL_SOURCE")
endif()

# define documentation
foreach(SRCFILE ${OBOL_MISC_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/misc/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_MISC_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/misc/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
