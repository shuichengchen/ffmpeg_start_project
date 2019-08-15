#pragma once


class FFEncodePcmToAAc
{
public:
	FFEncodePcmToAAc(const char*szPcmFileName,const char*szOutPutAAcName);
	~FFEncodePcmToAAc();
public:
	bool InitFFmpeg();
	void EnCode();
private:
	string m_strPcmName;
	string m_strAAcName;
	AVFormatContext* m_pFormatCtx;
	AVOutputFormat* m_pfmt;
	FILE *m_pInPutFile;
	AVCodec*m_pAACCodec;
	AVCodecContext* m_pAACCodecCtx;
	SwrContext *m_Swrctx;
	unsigned int m_nAAcDataSize;
	uint8_t **convert_data;         //存储转换后的数据，再编码AAC  
	uint8_t* m_pAACframe_buf;
	AVFrame* m_pPCMInputFrame;
	AVPacket m_AACPkt;
	AVStream* audio_st;
};

