#pragma once


/*
1、改方法适用于合成mp4\fmp4\m3u8\ts等。
2、部分封装格式 ，可以通过av_dict_set进行设置
//const bool low_delay = true;
	//if (low_delay) {
	//	// In case of low delay, set fragment duration to 200 ms.
	//	av_dict_set(&movflags, "movflags", "empty_moov+default_base_moof", 0);
	//	av_dict_set_int(&movflags, "frag_duration", 200 * 1000, 0);
	//}
	//else {
	//	// Only produce fragment until we have next key frame.
	//	av_dict_set(&movflags, "movflags", "empty_moov+default_base_moof+frag_keyframe", 0);
	//}
	//avformat_open_input(&addofmt_ctx, addinputAacFileName, 0, &movflags);

	av_dict_set(&movflags, "hls_time", "14", 0);
	av_dict_set(&movflags, "start_number", "1", 0);
	av_dict_set(&movflags, "hls_list_size", "200", 0);
3、mp4\Fmp4 音频为aac的时候 要去掉adts头部，封装m3u8或者ts的时候 是需要adts头的。
*/

class FFMuxerObject
{
public:
	FFMuxerObject(const char*szInputH264FileName,const char*szInputAudioFileName,const char*szOutMp4FileName);
	~FFMuxerObject();
public:
	bool InitFFmpeg();
	void Muxering();
private:
	int get_sr_index(unsigned int sampling_frequency);
	void make_dsi(unsigned int sampling_frequency_index, unsigned int channel_configuration, unsigned char* dsi);
private:
	string m_strInputH264FileName;
	string m_strInputAudioFileName;
	string m_strOutMp4FileName;
	AVOutputFormat *m_ofmt;
	AVFormatContext *m_ifmt_ctx_v;
	AVFormatContext *m_ifmt_ctx_a;
	AVFormatContext *m_ofmt_ctx;
	AVBSFContext *m_aacAbsCtx;
	int videoindex_input;
	int videoindex_out;
	int audioindex_input;
	int audioindex_out;
	AVPacket pkt;
};

