#ifndef RENDERING_H
#define RENDERING_H

struct CommandBuffer
{
	void* buffer;
	unsigned int size;
	unsigned int current_pos;
};

extern struct CommandBuffer command_buffer;
extern struct CommandBuffer back_command_buffer;
#endif