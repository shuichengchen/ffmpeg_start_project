// ffmpeg_start_project.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "FFMuxerObject.h"

int main() 
{
	const char*input_h264_filename = "test.h264";
	const char*input_audio_filename = "Audioout.aac";
	const char*out_filename = "muxer.mp4";
	FFMuxerObject object(input_h264_filename, input_audio_filename, out_filename);
	object.InitFFmpeg();
	object.Muxering();
	return 1;
}