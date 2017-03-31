#include "pointer_tool.h"
#include "shader_program.h"
#include "vr_system.h"
#include <glm.hpp>
#include <gtc/type_ptr.hpp>

PointerTool::PointerTool() :
	VRTool( VRToolType::Pointer )
{}

PointerTool::~PointerTool()
{}

bool PointerTool::init()
{
	initialised_ = true;
	bool success = true;

	glGenVertexArrays( 1, &vao_ );
	glBindVertexArray( vao_ );
	glGenBuffers( 1, &vbo_ );
	glBindBuffer( GL_ARRAY_BUFFER, vbo_ );

	GLuint stride = 2 * 3 * sizeof( GLfloat );
	GLuint offset = 0;

	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offset );

	offset += sizeof( GLfloat ) * 3;
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offset );

	shader_modl_mat_location_ = shader_->getUniformLocation( "model" );
	shader_view_mat_location_ = shader_->getUniformLocation( "view" );
	shader_proj_mat_location_ = shader_->getUniformLocation( "projection" );

	return success;
}

void PointerTool::shutdown()
{
	if( vao_ )
	{
		glDeleteVertexArrays( 1, &vao_ );
		vao_ = 0;
	}
	if( vbo_ )
	{
		glDeleteBuffers( 1, &vbo_ );
		vbo_ = 0;
	}
}

void PointerTool::activate()
{
	active_ = true;
}

void PointerTool::deactivate()
{
	active_ = false;
}

void PointerTool::update( float dt )
{
	if( controller_->isButtonDown( vr::k_EButton_SteamVR_Touchpad ) )
	{
		float length = -0.8 + -0.6 * controller_->axis( vr::k_EButton_Axis0 ).y;

		GLfloat verts[] = {
			0,0,0, 1,1,1,
			0,0,length, 1,0,1
		};

		// TODO: have a little circle at the end of the pointer

		glBufferData( GL_ARRAY_BUFFER, sizeof( verts ), verts, GL_STREAM_DRAW );
		num_verts_ = (sizeof( verts ) / sizeof( verts[0] )) / 6;
	}
	else
	{
		num_verts_ = 0;
	}
}

void PointerTool::render( vr::EVREye eye )
{
	shader_->bind();
	glUniformMatrix4fv( shader_view_mat_location_, 1, GL_FALSE, glm::value_ptr( vr_system_->viewMatrix( eye ) ) );
	glUniformMatrix4fv( shader_proj_mat_location_, 1, GL_FALSE, glm::value_ptr( vr_system_->projectionMartix( eye ) ) );
	glUniformMatrix4fv( shader_modl_mat_location_, 1, GL_FALSE, glm::value_ptr( controller_->deviceToAbsoluteTracking() ) );

	glBindVertexArray( vao_ );
	glDrawArrays( GL_LINES, 0, num_verts_ );
}