// ffmpeg_start_project.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "FFMuxerObject.h"
#include "FFEncodePcmToAAc.h"
#include "FFAudioConversionObject.h"

int main() 
{
	const char*input_h264_filename = "test.h264";
	const char*input_audio_filename = "Audioout.aac";
	const char*out_filename = "muxer.mp4";

	const char*input_pcm_filename = "input.pcm";
	const char*out_aac_filename = "pcmtoaac.aac";

// 	FFMuxerObject object(input_h264_filename, input_audio_filename, out_filename);
// 	if (object.InitFFmpeg())
// 	{
// 		object.Muxering();
// 	}	
	/*FFEncodePcmToAAc encodePcmToAAcObject(input_pcm_filename, out_aac_filename);
	if (encodePcmToAAcObject.InitFFmpeg())
	{
		encodePcmToAAcObject.EnCode();
	}*/

	FFAudioConversionObject audioConversionObject("Audio.G711", "Audio.mp3");
	audioConversionObject.Initialize(_enAudioCodec_G711_A);
	audioConversionObject.DataConversion();

	return 1;
}