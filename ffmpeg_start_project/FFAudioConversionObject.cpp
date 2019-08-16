#include "pch.h"
#include "FFAudioConversionObject.h"


FFAudioConversionObject::FFAudioConversionObject(const char *szSrcFilePath, const char *szDstMp3Path)
	:m_strSrcFilePath(szSrcFilePath)
	, m_strDstFilePath(szDstMp3Path)
{
	m_pSwrctx = nullptr;
	m_usLastAudioLen = 0;
	m_SwrctxMp3 = nullptr;
	convert_data = nullptr;
	nPcmLen = 0;
}


FFAudioConversionObject::~FFAudioConversionObject()
{
	av_write_trailer(m_pOutMp3FormatCtx);
}

bool FFAudioConversionObject::Initialize(uint8_t _byAudioType)
{
	int ret = fopen_s(&pInputFileHandle,m_strSrcFilePath.c_str(), "rb");
	if (ret!= 0)
	{
		return false;
	}
	m_AudioType = _byAudioType;
	return InitFfmpeg();
}

void FFAudioConversionObject::DataConversion(/*const unsigned char *_pData, const long _iDataLen*/)
{
	while (!feof(pInputFileHandle))/*判断是否结束，这里会有个文件结束符*/
	{
		unsigned char v_pAudioData[256] = { 0 };
		fread(v_pAudioData, 256, 1, pInputFileHandle);
		unsigned char szDecodeBuf[1024 * 40] = { 0 };
		int iDataLen = DecodeDataToPcmEx(v_pAudioData, sizeof(v_pAudioData), szDecodeBuf);
		if (iDataLen > 0)
		{
			EnCodePcmToMp3(szDecodeBuf, iDataLen);
		}
	}
}

bool FFAudioConversionObject::InitFfmpeg()
{
	m_pOutDataCache = NULL;
	m_swrtNb = 0;
	m_iPcmChannels = 0;
	m_ui64PcmChannelsLayout = 0;
	switch (m_AudioType)
	{
	case _enAudioCodec_G711_A:
		m_srcDataCodecId = AV_CODEC_ID_PCM_ALAW;
		break;
	case _enAudioCodec_AAC:
		m_srcDataCodecId = AV_CODEC_ID_AAC;
		break;
	case _enAudioCodec_G726:
		m_srcDataCodecId = AV_CODEC_ID_ADPCM_G726LE;
		break;
	default:
		m_srcDataCodecId = AV_CODEC_ID_PCM_ALAW;
	}

	m_pSrcDataCodec = avcodec_find_decoder(m_srcDataCodecId);
	if (!m_pSrcDataCodec)
	{
		return false;
	}

	m_psrcDataCodecCtx = avcodec_alloc_context3(m_pSrcDataCodec);
	if (!m_psrcDataCodecCtx)
	{
		return false;
	}

	if (m_AudioType == _enAudioCodec_G726)
		m_psrcDataCodecCtx->bits_per_coded_sample = 4;

	m_psrcDataCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	m_psrcDataCodecCtx->channels = 1;//通道数
	m_psrcDataCodecCtx->sample_rate = 8000;//采样率（音频）
	m_psrcDataCodecCtx->sample_fmt = m_pSrcDataCodec->sample_fmts[0];//AV_SAMPLE_FMT_S16;采样位数
	m_psrcDataCodecCtx->channel_layout = AV_CH_LAYOUT_MONO;//单通道
	int ret = avcodec_open2(m_psrcDataCodecCtx, m_pSrcDataCodec, NULL);
	if (ret < 0)
	{
		return false;
	}
	av_init_packet(&m_srcDataPacket);
	//为解码帧分配内存
	m_pPCMFrame = av_frame_alloc();
	if (!m_pPCMFrame)
	{
		return false;
	}

	//设置MP3 编码参数

	avformat_alloc_output_context2(&m_pOutMp3FormatCtx, NULL, NULL, m_strDstFilePath.c_str());
	if (!m_pOutMp3FormatCtx)
	{
		return false;
	}

	if (avio_open(&m_pOutMp3FormatCtx->pb, m_strDstFilePath.c_str(), AVIO_FLAG_READ_WRITE) < 0) {
		printf("Failed to open output file!\n");
		return false;
	}

	m_pMp3Codec = avcodec_find_encoder(m_pOutMp3FormatCtx->oformat->audio_codec);//音频为audio_codec
	if (!m_pMp3Codec) {
		printf("Can not find encoder!\n");
		return false;
	}


	m_pMp3CodecCtx = avcodec_alloc_context3(m_pMp3Codec);//需要使用avcodec_free_context释放
	if (!m_pMp3CodecCtx)
	{
		return false;
	}
	m_pMp3CodecCtx->codec_id = m_pOutMp3FormatCtx->oformat->audio_codec;
	m_pMp3CodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	m_pMp3CodecCtx->sample_fmt = m_pMp3Codec->sample_fmts[0];
	m_pMp3CodecCtx->sample_rate = 8000;
	m_pMp3CodecCtx->channel_layout = AV_CH_LAYOUT_MONO;
	m_pMp3CodecCtx->channels = av_get_channel_layout_nb_channels(m_pMp3CodecCtx->channel_layout);

	m_pMp3CodecCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	if (avcodec_open2(m_pMp3CodecCtx, m_pMp3Codec, NULL) < 0) {
		printf("Failed to open encoder!\n");
		return false;
	}
	m_pMp3AudioStream = avformat_new_stream(m_pOutMp3FormatCtx, m_pMp3Codec);
	if (m_pMp3AudioStream == NULL) {
		return false;
	}

	if (avcodec_parameters_from_context(m_pMp3AudioStream->codecpar, m_pMp3CodecCtx) < 0)
	{
		return false;
	}
	m_SwrctxMp3 = swr_alloc();
	av_opt_set_int(m_SwrctxMp3, "in_channel_layout", AV_CH_LAYOUT_MONO, 0);
	av_opt_set_int(m_SwrctxMp3, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
	av_opt_set_int(m_SwrctxMp3, "in_sample_rate", 8000, 0);
	av_opt_set_int(m_SwrctxMp3, "out_sample_rate", 8000, 0);
	av_opt_set_sample_fmt(m_SwrctxMp3, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_sample_fmt(m_SwrctxMp3, "out_sample_fmt", AV_SAMPLE_FMT_S32P, 0);
	swr_init(m_SwrctxMp3);

	/* 分配空间 */
	convert_data = (uint8_t**)calloc(m_pMp3CodecCtx->channels,
		sizeof(*convert_data));
	av_samples_alloc(convert_data, NULL,
		m_pMp3CodecCtx->channels, m_pMp3CodecCtx->frame_size,
		m_pMp3CodecCtx->sample_fmt, 0);

	m_pPCMInputFrame = av_frame_alloc();
	if (!m_pPCMInputFrame)
	{
		return false;
	}
	m_pPCMInputFrame->nb_samples = m_pMp3CodecCtx->frame_size;
	m_pPCMInputFrame->format = m_pMp3Codec->sample_fmts[0];

	m_nMp3DataSize = av_samples_get_buffer_size(NULL, m_pMp3CodecCtx->channels, m_pMp3CodecCtx->frame_size, m_pMp3CodecCtx->sample_fmt, 1);
	if (m_nMp3DataSize <= 0)
	{
		return false;
	}
	m_pMp3frame_buf = (uint8_t *)av_malloc(m_nMp3DataSize);
	if (!m_pMp3frame_buf)
	{
		return false;
	}

	avcodec_fill_audio_frame(m_pPCMInputFrame, m_pMp3CodecCtx->channels, m_pMp3CodecCtx->sample_fmt, (const uint8_t*)m_pMp3frame_buf, m_nMp3DataSize, 1);

	//Write Header
	avformat_write_header(m_pOutMp3FormatCtx, NULL);

	av_init_packet(&m_Mp3Pkt);

	return true;
}

long FFAudioConversionObject::DecodeDataToPcmEx(const unsigned char *pInputData, const unsigned int uiDataLen, unsigned char*pOutData)
{
	if (m_AudioType == _enAudioCodec_AAC)
	{
		int iOUt = 0;
		unsigned char p_OutADTS[1024 * 8] = { 0 };
		unsigned int p_OutADTS_size = 0;
		uint8_t temp[AUDIO_MAX_FRAME_SIZE] = { 0 };
		memcpy(temp, m_AudioBuffer, m_usLastAudioLen);
		memcpy(temp + m_usLastAudioLen, pInputData, uiDataLen);
		unsigned int allDataLen = m_usLastAudioLen + uiDataLen;
		while (GetADTSframe(temp, allDataLen, p_OutADTS, &p_OutADTS_size))
		{
			iOUt += FfmpegDecodeMethodEx(p_OutADTS, p_OutADTS_size, pOutData);
			memset(p_OutADTS, 0, sizeof(p_OutADTS));
		}
		if (allDataLen >= 0)
		{
			memcpy(m_AudioBuffer, temp, allDataLen);
			m_usLastAudioLen = allDataLen;
		}
		return iOUt;
	}
	else
	{
		return FfmpegDecodeMethodEx(pInputData, uiDataLen, pOutData);
	}
}

int FFAudioConversionObject::FfmpegDecodeMethodEx(const unsigned char *pInputData, const unsigned int uiDataLen, unsigned char*pOutData)
{
	av_packet_unref(&m_srcDataPacket);
	m_srcDataPacket.size = uiDataLen;
	m_srcDataPacket.data = (uint8_t*)pInputData;
	int data_size = 0;

	int ret = avcodec_send_packet(m_psrcDataCodecCtx, &m_srcDataPacket);
	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
		av_packet_unref(&m_srcDataPacket);
		data_size = 0;
		return data_size;
	}
	while (avcodec_receive_frame(m_psrcDataCodecCtx, m_pPCMFrame) == 0)
	{
		AVSampleFormat dst_format = AV_SAMPLE_FMT_S16;
		// 设置转换参数
		if (!m_pSwrctx)
		{

			if (m_pPCMFrame->channels > 0 && m_pPCMFrame->channel_layout == 0)
			{
				m_ui64PcmChannelsLayout = av_get_default_channel_layout(m_pPCMFrame->channels);
				m_iPcmChannels = m_pPCMFrame->channels;
			}
			else if (m_pPCMFrame->channels == 0 && m_pPCMFrame->channel_layout > 0)
			{
				m_iPcmChannels = av_get_channel_layout_nb_channels(m_pPCMFrame->channel_layout);
				m_ui64PcmChannelsLayout = m_pPCMFrame->channel_layout;
			}
			else {
				m_iPcmChannels = m_pPCMFrame->channels;
				m_ui64PcmChannelsLayout = m_pPCMFrame->channel_layout;
			}

			uint64_t dst_layout = av_get_default_channel_layout(m_iPcmChannels);
			m_pSwrctx = swr_alloc_set_opts(NULL, dst_layout, dst_format, m_pPCMFrame->sample_rate,
				m_ui64PcmChannelsLayout, (AVSampleFormat)m_pPCMFrame->format, m_pPCMFrame->sample_rate, 0, NULL);
			m_pOutDataCache = (uint8_t  *)malloc(1024 * 40);
			if (!m_pSwrctx || swr_init(m_pSwrctx) < 0 || !m_pOutDataCache)
			{
				return data_size;
			}

		}

		// 计算转换后的sample个数 a * b / c
		uint64_t dst_nb_samples = av_rescale_rnd(swr_get_delay(m_pSwrctx, m_pPCMFrame->sample_rate) +
			m_pPCMFrame->nb_samples, m_pPCMFrame->sample_rate, m_pPCMFrame->sample_rate, AVRounding(1));
		// 转换，返回值为转换后的sample个数

		m_swrtNb = swr_convert(m_pSwrctx, &m_pOutDataCache, static_cast<int>(dst_nb_samples),
			(const uint8_t**)m_pPCMFrame->data, m_pPCMFrame->nb_samples);
		memcpy(pOutData, m_pOutDataCache, m_iPcmChannels * m_swrtNb * av_get_bytes_per_sample(dst_format
		));
		data_size += m_iPcmChannels * m_swrtNb * av_get_bytes_per_sample(dst_format);
	}
	return data_size;
}

int FFAudioConversionObject::GetADTSframe(unsigned char* buffer, unsigned int &buf_size, unsigned char* data, unsigned int* data_size)
{
	unsigned int size = 0;
	unsigned char * TempBuffer[1024] = { 0 };
	int TempBuSize = 0;
	if (!buffer || !data || !data_size) {
		return 0;
	}
	while (1) {
		if (buf_size < 7) {
			return 0;
		}
		//Sync words
		if ((buffer[0] == 0xff) && ((buffer[1] & 0xf0) == 0xf0)) {

			size |= ((buffer[3] & 0x03) << 11);     //high 2 bit
			size |= buffer[4] << 3;                //middle 8 bit
			size |= ((buffer[5] & 0xe0) >> 5);        //low 3bit
													  //buffer[1]最后一位为protection_absent: 
													  //当这个字段为“0”的时候，在ADTS header后面会跟两个字节的CRC_check字段。
													  //如果为’1’ 则没有CRC_check字段。 
			if (((buffer[1]) & 0x01) == 0)
			{
				size += 2;
			}
			break;
		}
		memcpy(TempBuffer, buffer + 1, buf_size - 1);
		memset(buffer, 0, buf_size);
		--buf_size;
		memcpy(buffer, TempBuffer, buf_size);

	}
	if (size >= (AUDIO_PER_DEC_FRAME_SIZE * 6))
	{
		// ATLTRACE("获取该帧帧长data_size=%d,原数据长度剩buf_size = %d\n",size,buf_size);
		memset(TempBuffer, 0, sizeof(TempBuffer));
		memcpy(TempBuffer, buffer + 7, buf_size - 7);
		memset(buffer, 0, buf_size);
		buf_size -= 7;
		memcpy(buffer, TempBuffer, buf_size);
		return 0;
	}

	if (buf_size < size) {
		return 0;
	}
	memset(TempBuffer, 0, sizeof(TempBuffer));
	memcpy(data, buffer, size);
	TempBuSize = buf_size - size;
	memcpy(TempBuffer, buffer + size, TempBuSize);
	memset(buffer, 0, buf_size);
	buf_size = TempBuSize;
	memcpy(buffer, TempBuffer, buf_size);
	*data_size = size;
	return 1;
}


void FFAudioConversionObject::EnCodePcmToMp3(unsigned char*pData, const unsigned int iDataLen)
{
	if (m_nMp3DataSize/2 - nPcmLen > iDataLen)
	{
		::memcpy_s(m_pMp3frame_buf + nPcmLen, m_nMp3DataSize / 2 - nPcmLen, pData, iDataLen);
		nPcmLen += iDataLen;
	}
	else
	{
		::memcpy_s(m_pMp3frame_buf + nPcmLen, m_nMp3DataSize / 2 - nPcmLen, pData, m_nMp3DataSize / 2 - nPcmLen);

		swr_convert(m_SwrctxMp3, convert_data, m_pMp3CodecCtx->frame_size, (const uint8_t **)&m_pMp3frame_buf, m_pMp3CodecCtx->frame_size);

		m_pPCMInputFrame->data[0] = (uint8_t*)convert_data[0];

		if (avcodec_send_frame(m_pMp3CodecCtx, m_pPCMInputFrame) < 0)
		{
			return;
		}
		while (avcodec_receive_packet(m_pMp3CodecCtx, &m_Mp3Pkt) == 0)
		{

			m_Mp3Pkt.stream_index = m_pMp3AudioStream->index;
			int ret = av_write_frame(m_pOutMp3FormatCtx, &m_Mp3Pkt);
			av_packet_unref(&m_Mp3Pkt);
		}
	
		::memcpy_s(m_pMp3frame_buf, m_nMp3DataSize / 2, (uint8_t*)pData + (m_nMp3DataSize / 2 - nPcmLen), iDataLen - (m_nMp3DataSize / 2 - nPcmLen));
		nPcmLen = iDataLen - (m_nMp3DataSize / 2 - nPcmLen);
	}
	
}

