#pragma once

#define  AUDIO_MAX_FRAME_SIZE  1024*40
const int AUDIO_PER_DEC_FRAME_SIZE = 160;

enum _enAudioCodec // 音频编码器
{
	_enAudioCodec_G711_A = 6,
	_enAudioCodec_G711_U = 7,
	_enAudioCodec_G726 = 8,
	_enAudioCodec_AAC = 19,
};


class FFAudioConversionObject
{
public:
	FFAudioConversionObject(const char *szSrcFilePath, const char *szDstMp3Path);
	~FFAudioConversionObject();

public:
	bool Initialize(uint8_t _byAudioType);
	void DataConversion(/*const unsigned char *_pData, const long _iDataLen*/);
protected:
	bool InitFfmpeg();
	long DecodeDataToPcmEx(const unsigned char *pInputData, const unsigned int uiDataLen, unsigned char*pOutData);
	int  FfmpegDecodeMethodEx(const unsigned char *pInputData, const unsigned int uiDataLen, unsigned char*pOutData);
	int  GetADTSframe(unsigned char* buffer, unsigned int &buf_size, unsigned char* data, unsigned int* data_size);
	void EnCodePcmToMp3(unsigned char*pData, const unsigned int iDataLen);
private:
	string m_strSrcFilePath;
	string m_strDstFilePath;
	unsigned short m_usLastAudioLen;
	uint8_t m_AudioBuffer[AUDIO_MAX_FRAME_SIZE];
	uint8_t m_AudioType;
	FILE*pInputFileHandle = nullptr;
	int nPcmLen;
private:
	AVCodec *m_pSrcDataCodec;//对应解码器
	AVCodecID m_srcDataCodecId;
	AVCodecContext *m_psrcDataCodecCtx;//解码方式相关数据
	AVFrame	*m_pPCMFrame;//解码后数据
	AVPacket m_srcDataPacket;//解码前数据
	uint8_t  *m_pOutDataCache;
	int m_swrtNb;
	int m_iPcmChannels;
	int64_t m_ui64PcmChannelsLayout;
	SwrContext *m_pSwrctx = nullptr;
	SwrContext *m_SwrctxMp3;
	uint8_t **convert_data;
private:
	AVFormatContext* m_pOutMp3FormatCtx; //mp3输出上下文
	AVOutputFormat* m_pMp3OutputFmt;
	AVStream* m_pMp3AudioStream;
	AVCodecContext* m_pMp3CodecCtx;
	AVCodec*m_pMp3Codec;

	uint8_t* m_pMp3frame_buf;
	AVFrame* m_pPCMInputFrame;
	AVPacket m_Mp3Pkt;
	unsigned int m_nMp3DataSize;
};

