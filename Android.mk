LOCAL_PATH := $(call my-dir)
ENGINE_DIR = ../nenuzhno-engine_iter1/src/
PCC_DIR = ME3-Formats
include $(CLEAR_VARS)

LOCAL_MODULE    := nenuzhno-engine
LOCAL_CFLAGS    := -Wall -std=c++11
LOCAL_C_INCLUDES := $(LOCAL_PATH)/. $(LOCAL_PATH)/../libs/glm/glm $(LOCAL_PATH)/$(ENGINE_DIR) $(LOCAL_PATH)/$(PCC_DIR)

SOUND = 0

PCC_SRCS := PCCViewer.cpp LevelRenderer.cpp $(PCC_DIR)/pcc.cpp $(PCC_DIR)/PCCSystem.cpp $(PCC_DIR)/PCCProperties.cpp $(PCC_DIR)/PCCTexture2D.cpp $(PCC_DIR)/PCCStaticMesh.cpp $(PCC_DIR)/PCCSkeletalMesh.cpp $(PCC_DIR)/PCCAnim.cpp $(PCC_DIR)/PCCLevel.cpp
LOCAL_SRC_FILES := $(ENGINE_DIR)android_backend.cpp $(ENGINE_DIR)log.cpp $(ENGINE_DIR)system/FileSystem.cpp $(ENGINE_DIR)system/Config.cpp $(ENGINE_DIR)game/IGame.cpp $(ENGINE_DIR)cull/frustum.cpp $(ENGINE_DIR)cull/BoundingBox.cpp \
	$(ENGINE_DIR)graphics/gl_utils.cpp $(ENGINE_DIR)graphics/gl_ext.cpp $(ENGINE_DIR)graphics/glsl_prog.cpp $(ENGINE_DIR)graphics/texture.cpp $(ENGINE_DIR)graphics/vbo.cpp \
	$(ENGINE_DIR)renderer/mesh.cpp $(ENGINE_DIR)renderer/Model.cpp $(ENGINE_DIR)renderer/font.cpp $(ENGINE_DIR)renderer/camera.cpp \
	$(ENGINE_DIR)resource/ResourceManager.cpp $(ENGINE_DIR)resource/vtf_loader.cpp $(ENGINE_DIR)resource/dds_loader.cpp $(ENGINE_DIR)resource/nmf_loader.cpp $(ENGINE_DIR)resource/mesh_loader.cpp $(ENGINE_DIR)resource/obj_loader.cpp \
	editor.cpp ModelViewer.cpp ModelEditor.cpp ModelUtils.cpp BinkPlayer.cpp BinkFile.cpp vlc.cpp GraphUtil.cpp dds_export.cpp smd_export.cpp $(PCC_SRCS)
LOCAL_LDLIBS := -llog -lGLESv2 -lm -lEGL -lz

ifeq ($(SOUND),1)
LOCAL_SRC_FILES += $(ENGINE_DIR)sound/sound_al.cpp $(ENGINE_DIR)sound/AudioClip.cpp \
	sound_editor.cpp 
LOCAL_LDLIBS += -lopenal -lOpenSLES
LOCAL_LDFLAGS += -L../libs/openal/libs/armeabi-v7a
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../libs/openal/include/AL
else
LOCAL_CFLAGS += -DNO_SOUND=1
endif

include $(BUILD_SHARED_LIBRARY)

#

#$(call import-add-path,/sdcard/AppProjects/libs)
#$(call import-module,openal)

