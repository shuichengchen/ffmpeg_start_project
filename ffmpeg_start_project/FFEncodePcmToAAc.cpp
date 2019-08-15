#include "pch.h"
#include "FFEncodePcmToAAc.h"


FFEncodePcmToAAc::FFEncodePcmToAAc(const char*szPcmFileName, const char*szOutPutAAcName)
	:m_strPcmName(szPcmFileName)
	,m_strAAcName(szOutPutAAcName)
{
	m_pFormatCtx = nullptr;
	m_pfmt = nullptr;
	m_pInPutFile = nullptr;
	m_pAACCodec = nullptr;
	m_pAACCodecCtx = nullptr;
	m_Swrctx = nullptr;
	m_nAAcDataSize = 0;
	convert_data = 0;         //存储转换后的数据，再编码AAC  
	m_pAACframe_buf = nullptr;
	m_pPCMInputFrame = 0;
	audio_st = nullptr;
	av_init_packet(&m_AACPkt);
}


FFEncodePcmToAAc::~FFEncodePcmToAAc()
{
	if (m_pFormatCtx)
	{
		av_write_trailer(m_pFormatCtx);
	}
	if (m_pInPutFile)
	{
		fclose(m_pInPutFile);
		m_pInPutFile = nullptr;
	}
	if (m_Swrctx)
	{
		swr_free(&m_Swrctx);
		m_Swrctx = nullptr;
	}
	
	if (m_pAACCodecCtx)
	{
		avcodec_free_context(&m_pAACCodecCtx);
		m_pAACCodecCtx = nullptr;
	}
	if (m_pFormatCtx)
	{
		avio_close(m_pFormatCtx->pb);
		avformat_free_context(m_pFormatCtx);
		m_pFormatCtx = nullptr;
	}
	if (m_pAACframe_buf)
	{
		av_free(m_pAACframe_buf);
		m_pAACframe_buf = nullptr;
	}
	if (convert_data)
	{
		av_freep(&convert_data[0]);
		convert_data[0] = nullptr;
		free(convert_data);
		convert_data = nullptr;
	}
	
}

bool FFEncodePcmToAAc::InitFFmpeg()
{
	 if (fopen_s(&m_pInPutFile,m_strPcmName.c_str(), "rb") != 0)
	 {
		 return false;
	 }
	 m_pFormatCtx = avformat_alloc_context();
	 if (!m_pFormatCtx)
	 {
		 return false;
	 }
	 m_pfmt = av_guess_format(NULL, m_strAAcName.c_str(), NULL);
	 if (!m_pfmt)
	 {
		 return false;
	 }
	 m_pFormatCtx->oformat = m_pfmt;
	
	 if (avio_open(&m_pFormatCtx->pb, m_strAAcName.c_str(), AVIO_FLAG_READ_WRITE) < 0) {
		 printf("Failed to open output file!\n");
		 return false;
	 }
	 m_pAACCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);//音频为audio_codec
	 if (!m_pAACCodec) {
		 printf("Can not find encoder!\n");
		 return false;
	 }

	 audio_st = avformat_new_stream(m_pFormatCtx, nullptr);
	 if (audio_st == NULL) {
		 return false;
	 }
	 m_pAACCodecCtx = avcodec_alloc_context3(m_pAACCodec);//需要使用avcodec_free_context释放
	 if (!m_pAACCodecCtx)
	 {
		 return false;
	 }
	
	 m_pAACCodecCtx->codec_id = AV_CODEC_ID_AAC;
	 m_pAACCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	 m_pAACCodecCtx->sample_fmt = m_pAACCodec->sample_fmts[0];
	 m_pAACCodecCtx->sample_rate = 8000;
	 m_pAACCodecCtx->channel_layout = AV_CH_LAYOUT_MONO;
	 m_pAACCodecCtx->channels = av_get_channel_layout_nb_channels(m_pAACCodecCtx->channel_layout);
	 m_pAACCodecCtx->profile = FF_PROFILE_AAC_LOW;
	 m_pAACCodecCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

	 int  ret = 0;
	 if ((ret = avcodec_open2(m_pAACCodecCtx, m_pAACCodec, NULL)) < 0) {
		 printf("Failed to open encoder!\n");
		 return false;
	 }
	 avcodec_parameters_from_context(audio_st->codecpar, m_pAACCodecCtx);

	 m_Swrctx = swr_alloc();
	 if (!m_Swrctx)
	 {
		 return false;
	 }
	 av_opt_set_int(m_Swrctx, "in_channel_layout", AV_CH_LAYOUT_MONO, 0);
	 av_opt_set_int(m_Swrctx, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
	 av_opt_set_int(m_Swrctx, "in_sample_rate", 8000, 0);
	 av_opt_set_int(m_Swrctx, "out_sample_rate", 8000, 0);
	 av_opt_set_sample_fmt(m_Swrctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	 av_opt_set_sample_fmt(m_Swrctx, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
	 swr_init(m_Swrctx);

	 convert_data = (uint8_t**)calloc(m_pAACCodecCtx->channels,sizeof(*convert_data));
	 if (!convert_data)
	 {
		 return false;
	 }
	 av_samples_alloc(convert_data, NULL,m_pAACCodecCtx->channels, m_pAACCodecCtx->frame_size,
		 m_pAACCodecCtx->sample_fmt, 0);
	 m_nAAcDataSize = av_samples_get_buffer_size(NULL, m_pAACCodecCtx->channels, m_pAACCodecCtx->frame_size, m_pAACCodecCtx->sample_fmt, 0);

	 m_pPCMInputFrame = av_frame_alloc();
	 if (!m_pPCMInputFrame)
	 {
		 return false;
	 }

	 m_pAACframe_buf = (uint8_t *)av_malloc(m_nAAcDataSize);
	 memset(m_pAACframe_buf, 0, m_nAAcDataSize);
	 if (!m_pAACframe_buf)
	 {
		 return false;
	 }
	 m_pPCMInputFrame->channels = 1;
	 m_pPCMInputFrame->nb_samples = m_pAACCodecCtx->frame_size;
	 m_pPCMInputFrame->sample_rate = m_pAACCodecCtx->sample_fmt;
	 int rest = avcodec_fill_audio_frame(m_pPCMInputFrame, m_pAACCodecCtx->channels, m_pAACCodecCtx->sample_fmt, (const uint8_t*)m_pAACframe_buf, m_nAAcDataSize, 0);
	 avformat_write_header(m_pFormatCtx, NULL);
	 return true;
}

void FFEncodePcmToAAc::EnCode()
{
	for (auto i = 0; i < 10000; i++)
	{
		//Read PCM
		if (fread(m_pAACframe_buf, 1, m_nAAcDataSize / 2, m_pInPutFile) <= 0) {//如果按4096读取 编码的aac快一倍，单通道读一半就可以了？
			printf("Failed to read raw data! \n");
			break;
		}
		else if (feof(m_pInPutFile)) {
			break;
		}

		int count = swr_convert(m_Swrctx, convert_data, m_pAACCodecCtx->frame_size, (const uint8_t **)&m_pAACframe_buf, m_pAACCodecCtx->frame_size);//len 为4096
		m_pPCMInputFrame->data[0] = convert_data[0];
		int ret = avcodec_send_frame(m_pAACCodecCtx, m_pPCMInputFrame);
		while (avcodec_receive_packet(m_pAACCodecCtx,&m_AACPkt) ==0)
		{
			ret = av_write_frame(m_pFormatCtx, &m_AACPkt);
			av_packet_unref(&m_AACPkt);
		}
	}
}
