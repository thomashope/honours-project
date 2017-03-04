#include "vr_system.h"
#include <SDL.h>
#include <iostream>

// Static member delcarations
VRSystem* VRSystem::self_ = nullptr;

// Constructor
VRSystem::VRSystem() :
	vr_system_(nullptr),
	render_target_width_(0),
	render_target_height_(0),
	near_clip_plane_(0.1f),
	far_clip_plane_(100.0f)
{
}

// Destructor
VRSystem::~VRSystem()
{
	glDeleteFramebuffers( 1, &eye_buffers_[0].render_frame_buffer );
	glDeleteFramebuffers( 1, &eye_buffers_[0].resolve_frame_buffer );
	glDeleteFramebuffers( 1, &eye_buffers_[1].render_frame_buffer );
	glDeleteFramebuffers( 1, &eye_buffers_[1].resolve_frame_buffer );

	vr::VR_Shutdown();
	// TODO: should i manually delete the vr_system ptr?
	vr_system_ = nullptr;

	self_ = nullptr;
}

VRSystem* VRSystem::get()
{
	// Check if the window exists yet
	if( self_ == nullptr )
	{
		// If the window does not yet exist, create it
		self_ = new VRSystem();
		bool success = self_->init();

		// Check everything went OK
		if( success == false )
		{
			delete self_;
		}
	}

	return self_;
}

bool VRSystem::init()
{
	bool success = true;

	/* PRELIMINARY CHECKS */
	{
		bool is_hmd_present = vr::VR_IsHmdPresent();
		std::cout << "Found HMD: " << (is_hmd_present ? "yes" : "no") << std::endl;
		if( !is_hmd_present ) success = false;

		bool is_runtime_installed = vr::VR_IsRuntimeInstalled();
		std::cout << "Found OpenVR runtime: " << (is_runtime_installed ? "yes" : "no") << std::endl;
		if( !is_runtime_installed ) success = false;

		if( !success )
		{
			SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "error", "Something is missing...", NULL );
			return success;
		}
	}

	/* INIT THE VR SYSTEM */
	vr::EVRInitError error = vr::VRInitError_None;
	vr_system_ = vr::VR_Init( &error, vr::VRApplication_Scene );
	if( error != vr::VRInitError_None )
	{
		vr_system_ = nullptr;
		char buf[1024];
		sprintf_s( buf, sizeof( buf ), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription( error ) );
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "VR_Init Failed", buf, NULL );
		success = false;
		return success;
	}

	/* PRINT SYSTEM INFO */
	std::cout << "Tracking System: " << getDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String, NULL ) << std::endl;
	std::cout << "Serial Number: " << getDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, NULL ) << std::endl;

	vr_system_->GetRecommendedRenderTargetSize( &render_target_width_, &render_target_height_ );

	/* SETUP FRAME BUFFERS */
	for( int i = 0; i < 2; i++ )
	{
		// Create render frame buffer
		glGenFramebuffers( 1, &eye_buffers_[i].render_frame_buffer );						// Create a FBO
		glBindFramebuffer( GL_FRAMEBUFFER, eye_buffers_[i].render_frame_buffer );			// Bind the FBO
		// Attach colour component
		glGenTextures( 1, &eye_buffers_[i].render_texture );																				// Generate a colour texture
		
		glBindTexture( GL_TEXTURE_2D, eye_buffers_[i].render_texture );																		// Bind the texture
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, render_target_width_, render_target_height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );			// Create texture data
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eye_buffers_[i].render_texture, 0 );					// Attach the texture to the bound FBO

		//glBindTexture( GL_TEXTURE_2D_MULTISAMPLE, eye_buffers_[i].render_texture );															// Bind the multisampled texture
		//glTexImage2DMultisample( GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, render_target_width_, render_target_height_, true );				// Create multisampled data
		//glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, eye_buffers_[i].render_texture, 0 );		// Attach the multisampled texture to the bound FBO

		// Attach depth component
		glGenRenderbuffers( 1, &eye_buffers_[i].render_depth );																				// Generate a render buffer
		glBindRenderbuffer( GL_RENDERBUFFER, eye_buffers_[i].render_depth );																// Bind the render buffer
		//glRenderbufferStorageMultisample( GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, render_target_width_, render_target_height_ );			// Enable multisampling
		glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT, render_target_width_, render_target_height_ );
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, eye_buffers_[i].render_depth );			// Attach the the render buffer as a depth buffer 

		// Check everything went OK
		if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
			std::cout << "ERROR: incomplete render frame buffer!" << std::endl;
			success = false;
		}

		// Create resolve frame buffer
		glGenFramebuffers( 1, &eye_buffers_[i].resolve_frame_buffer );
		glBindFramebuffer( GL_FRAMEBUFFER, eye_buffers_[i].resolve_frame_buffer );
		// Attach colour component
		glGenTextures( 1, &eye_buffers_[i].resolve_texture );
		glBindTexture( GL_TEXTURE_2D, eye_buffers_[i].resolve_texture );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, render_target_width_, render_target_height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eye_buffers_[i].resolve_texture, 0 );

		// Check everything went OK
		if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )	{
			std::cout << "ERROR: incomplete resolve frame buffer!" << std::endl;
			success = false;
		}

		// Unbind any bound frame buffer
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		success = true;
	}

	return success;
}

void VRSystem::processVREvents()
{
	vr::VREvent_t event;

	while( vr_system_->PollNextEvent( &event, sizeof( event ) ) )
	{
		// TODO: handle events
	}
}

void VRSystem::updatePoses()
{
	// Update the pose list
	vr::VRCompositor()->WaitGetPoses( poses_, vr::k_unMaxTrackedDeviceCount, NULL, 0 );

	for( int device = 0; device < vr::k_unMaxTrackedDeviceCount; device++ )
	{
		if( poses_[device].bPoseIsValid )
		{
			// Translate the pose matrix to a glm::mat4 for ease of use
			// The stored matrix is from device to absolute tracking position, we will probably want that inverted
			transforms_[device] = glm::inverse( convertHMDmat3ToGLMMat4( poses_[device].mDeviceToAbsoluteTracking ) );
		}
	}
}

void VRSystem::bindEyeTexture( vr::EVREye eye )
{
	glBindFramebuffer( GL_FRAMEBUFFER, renderEyeTexture(eye) );
	glViewport( 0, 0, render_target_width_, render_target_height_ );
	glEnable( GL_MULTISAMPLE );
	glEnable( GL_DEPTH );
}

void VRSystem::blitEyeTextures()
{
	for( int i = 0; i < 2; i++ )
	{
		// Blit from the render frame buffer to the resolve
		glViewport( 0, 0, render_target_width_, render_target_height_ );
		glBindFramebuffer( GL_READ_BUFFER, eye_buffers_[i].render_frame_buffer );
		glBindFramebuffer( GL_DRAW_BUFFER, eye_buffers_[i].resolve_frame_buffer );
		glBlitFramebuffer(
			0, 0, render_target_width_, render_target_height_,
			0, 0, render_target_width_, render_target_height_,
			GL_COLOR_BUFFER_BIT, GL_LINEAR );
	}
}

void VRSystem::submitEyeTextures()
{
	if( hasInputFocus() )
	{
		// NOTE: to find out what the error codes mean Ctal+F 'enum EVRCompositorError' in 'openvr.h'
		vr::EVRCompositorError error = vr::VRCompositorError_None;

		vr::Texture_t left = { (void*)eye_buffers_[0].resolve_texture, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
		error = vr::VRCompositor()->Submit( vr::Eye_Left, &left, NULL );
		if( error != vr::VRCompositorError_None ) std::cout << "ERROR: left eye  " << error << std::endl;

		vr::Texture_t right = { (void*)eye_buffers_[1].resolve_texture, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
		error = vr::VRCompositor()->Submit( vr::Eye_Right, &right, NULL );
		if( error != vr::VRCompositorError_None ) std::cout << "ERROR: right eye " << error << std::endl;

		// Added on advice from comments in IVRCompositor::submit in openvr.h
		glFlush();
	}
}

glm::mat4 VRSystem::projectionMartix( vr::Hmd_Eye eye )
{
	vr::HmdMatrix44_t matrix = vr_system_->GetProjectionMatrix( eye, near_clip_plane_, far_clip_plane_ );
	return convertHMDmat4ToGLMmat4( matrix );
}

glm::mat4 VRSystem::eyePoseMatrix( vr::Hmd_Eye eye )
{
	vr::HmdMatrix34_t matrix = vr_system_->GetEyeToHeadTransform( eye );
	return glm::inverse( convertHMDmat3ToGLMMat4( matrix ) );
}
std::string VRSystem::getDeviceString(
	vr::TrackedDeviceIndex_t device,
	vr::TrackedDeviceProperty prop,
	vr::TrackedPropertyError* error )
{
	uint32_t buffer_length = vr_system_->GetStringTrackedDeviceProperty( device, prop, NULL, 0, error );
	if( buffer_length == 0 ) return "";

	char* buffer = new char[buffer_length];
	vr_system_->GetStringTrackedDeviceProperty( device, prop, buffer, buffer_length, error );
	std::string result = buffer;
	delete[] buffer;
	return result;
}

glm::mat4 VRSystem::convertHMDmat3ToGLMMat4( vr::HmdMatrix34_t matrix )
{
	return glm::mat4(
		matrix.m[0][0], matrix.m[1][0], matrix.m[2][0], 0.0,
		matrix.m[0][1], matrix.m[1][1], matrix.m[2][1], 0.0,
		matrix.m[0][2], matrix.m[1][2], matrix.m[2][2], 0.0,
		matrix.m[0][3], matrix.m[1][3], matrix.m[2][3], 1.0f
	);
}

glm::mat4 VRSystem::convertHMDmat4ToGLMmat4( vr::HmdMatrix44_t matrix )
{
	return glm::mat4(
		matrix.m[0][0], matrix.m[1][0], matrix.m[2][0], matrix.m[3][0],
		matrix.m[0][1], matrix.m[1][1], matrix.m[2][1], matrix.m[3][1],
		matrix.m[0][2], matrix.m[1][2], matrix.m[2][2], matrix.m[3][2],
		matrix.m[0][3], matrix.m[1][3], matrix.m[2][3], matrix.m[3][3]
	);
}