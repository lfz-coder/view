/**
 * @file viewAVTrans.cc
 * @brief HLS 转码模块实现 —— M3U8Info 解析/生成 / HLSTransCoder
 */

#include "viewAVTrans.h"
#include "viewUtil.h"
#include "viewLog.h"

namespace viewAVTrans {
    M3U8Info::M3U8Info(const std::string& fileName) : _fileName(fileName) {}

    bool M3U8Info::Parse() {
        std::string content;
        bool ret = viewUtil::FileUtil::Read(_fileName, content);
        if(!ret) {
            viewLog::ERROR("M3U8Info:Parse read file failed: {}", _fileName);
            return false; // 读取文件失败
        }
        // 解析内容
        std::vector<std::string> lines;
        size_t count = viewUtil::StrUtil::Split(content, "\n", lines);
        for(size_t i = 0; i < lines.size(); i++) {
            if(lines[i].empty()) continue;
            
            if(lines[i].find(HLS_ENDLIST) != std::string::npos) {
                _headers.push_back(lines[i]);  // 也保留 ENDLIST 行
                break;
            }
            
            if(lines[i].find(HLS_EXTINF) != std::string::npos) {
                // 检查是否有下一行
                if(i + 1 >= lines.size()) {
                    viewLog::WARN("M3U8Info:Parse: EXTINF without URL");
                    break;
                }
                _urlPairs.push_back({lines[i], lines[i+1]});
                i++;  // 跳过 URL 行
                continue;
            }
            
            _headers.push_back(lines[i]);
        }

        return true;
    }

    bool M3U8Info::Write() {
        std::stringstream ss;
        for(const auto& header : _headers) {
            ss << header << "\n";
        }
        for(const auto& pair : _urlPairs) {
            ss << pair.first << "\n" << pair.second << "\n";
        }
        bool ret = viewUtil::FileUtil::Write(_fileName, ss.str());
        if(!ret) {
            viewLog::ERROR("M3U8Info:Write write file failed: {}", _fileName);
            return false; // 写入文件失败
        }
        return true;
    }

    std::vector<std::string>& M3U8Info::GetHeaders() {
        return _headers;
    }

    std::vector<M3U8Info::StrPair>& M3U8Info::GetUrlPairs() {
        return _urlPairs;
    }

    // *********************************************************************************************** //

    HLSTransCoder::HLSTransCoder(const HLSConfig& config) : _config(config) {}

    bool HLSTransCoder::Transcode(const std::string& inputFile, const std::string& outputFile) {
        // ==================== 变量定义 ====================
        int ret = 0;                              // FFmpeg 函数返回值（0 表示成功，负数表示错误）
        AVFormatContext* inputContext = nullptr;  // 输入文件上下文（包含格式、流信息、时长等）
        AVFormatContext* outputContext = nullptr; // 输出文件上下文（HLS 格式）
        AVDictionary* options = nullptr;          // 输出格式选项（HLS 配置参数）
        std::vector<int> stream_mapping;          // 输入流索引 -> 输出流索引 的映射表
        NetworkGuard networkGuard;                // 网络初始化和清理的 RAII 对象
        av_log_set_level(AV_LOG_QUIET);           // 关闭日志
        
        // ==================== 步骤1: 打开输入文件 ====================
        // avformat_open_input: 打开媒体文件并读取头部信息
        // 参数说明:
        //   &inputContext  - 输出参数，指向新分配的 AVFormatContext
        //   argv[1]        - 输入文件路径
        //   nullptr        - 指定输入格式（nullptr 表示自动检测）
        //   nullptr        - 额外选项（nullptr 表示无）
        ret = avformat_open_input(&inputContext, inputFile.c_str(), nullptr, nullptr);
        if (ret < 0) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: {}", inputFile, AvError(ret));
            return false;
        }
        viewLog::INFO("HLSTransCoder::Transcode: {}: ✓ open file", inputFile);
        
        // ==================== 步骤2: 解析流信息 ====================
        // avformat_find_stream_info: 读取文件中的包数据，填充流信息
        // 这一步会读取一小部分数据来确认编码格式、时长、码率等元信息
        ret = avformat_find_stream_info(inputContext, nullptr);
        if (ret < 0) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: {}", inputFile, AvError(ret));
            avformat_close_input(&inputContext);  // 关闭输入文件
            return false;
        }
        
        // 打印输入文件的基本信息（用于调试）
        viewLog::INFO("HLSTransCoder::Transcode: {}: 时长: {}", inputFile, (inputContext->duration / (double)AV_TIME_BASE));
        viewLog::INFO("HLSTransCoder::Transcode: {}: 媒体流数量: {}", inputFile, inputContext->nb_streams);
        
        // ==================== 步骤3: 创建输出上下文（HLS 格式）====================
        // avformat_alloc_output_context2: 根据输出格式名称创建 AVFormatContext
        // 参数说明:
        //   &outputContext - 输出参数，指向新分配的 AVFormatContext
        //   nullptr        - 指定输出格式驱动（nullptr 表示自动查找）
        //   "hls"          - 输出格式名称（hls = HTTP Live Streaming）
        //   argv[2]        - 输出文件路径（会保存在 outputContext->url）
        ret = avformat_alloc_output_context2(&outputContext, nullptr, "hls", outputFile.c_str());
        if (ret < 0 || !outputContext) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: 创建输出上下文失败", outputFile);
            avformat_close_input(&inputContext);
            return false;
        }
        viewLog::INFO("HLSTransCoder::Transcode: {}: ✓ 成功创建 HLS 输出上下文", outputFile);
        
        // ==================== 步骤4: 创建输出流并复制编码参数 ====================
        // 遍历输入的所有流（视频流、音频流、字幕流等），为每个流在输出文件中创建对应的流
        // 注意：HLS 通常只需要视频流和音频流，但这里处理所有流以保持兼容性
        stream_mapping.resize(inputContext->nb_streams, -1);  // 初始化为 -1
        
        for (unsigned int i = 0; i < inputContext->nb_streams; i++) {
            AVStream* inStream = inputContext->streams[i];      // 输入流
            AVStream* outStream = nullptr;                      // 输出流
            
            // avformat_new_stream: 在输出上下文中创建新流
            // 参数:
            //   outputContext - 输出上下文
            //   nullptr       - 编解码器（nullptr 表示稍后设置）
            outStream = avformat_new_stream(outputContext, nullptr);
            if (!outStream) {
                viewLog::ERROR("HLSTransCoder::Transcode: {}: 创建输出流失败 (索引: {})", outputFile, i);
                avformat_close_input(&inputContext);
                avformat_free_context(outputContext);
                return false;
            }
            
            // avcodec_parameters_copy: 复制编码参数
            // 将输入流的编码参数（编码类型、分辨率、采样率等）完整复制到输出流
            // 这是 remux 的关键：不重新编码，只是复制参数
            ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);
            if (ret < 0) {
                viewLog::ERROR("HLSTransCoder::Transcode: {}: 复制编码参数失败 (流索引: {}): {}", outputFile, i, AvError(ret));
                avformat_close_input(&inputContext);
                avformat_free_context(outputContext);
                return false;
            }
            
            // 注意：暂不设置 outStream->time_base，后面会从输入流自动导出
            // 记录映射关系：输入流的索引 i 对应输出流的索引 outStream->index
            stream_mapping[i] = outStream->index;
        }
        
        // ==================== 步骤5: 配置 HLS 参数 ====================
        // HLS (HTTP Live Streaming) 将视频切成多个小片段 (.ts 文件)
        // 并生成一个索引文件 (.m3u8) 供播放器读取
        
        // hls_time: 设置每个分片的时长（秒）
        // 注意：分片时长不是精确的，FFmpeg 会在关键帧处切分，实际时长可能略有差异
        av_dict_set_int(&options, "hls_time", _config.hls_time, 0);      // 每个分片时间
        
        // hls_base_url: 设置 TS 分片文件的 URL 前缀
        // 播放器会将 base_url + 分片文件名 拼接成完整 URL 来下载 TS 文件
        // 例如: base_url = "http://192.168.1.100:9000/video/"
        //       分片文件 = "hls0.ts"
        //       完整 URL = "http://192.168.1.100:9000/video/hls0.ts"
        av_dict_set(&options, "hls_base_url", _config.hls_base_url.c_str(), 0);
        
        // hls_playlist_type: 播放列表类型
        //   "vod"    - 点播模式（Video On Demand），所有分片在 m3u8 中完整列出，适合已完整录制的视频
        //   "event"  - 事件模式，分片边生成边列出，适合直播结束后保留
        //   nullptr  - 直播模式，动态更新列表
        av_dict_set(&options, "hls_playlist_type", _config.hls_playlist_type.c_str(), 0);
        
        // 可选参数（已注释，按需启用）:
        // av_dict_set(&options, "hls_list_size", "0", 0);     // 0 表示在 m3u8 中包含所有分片
        // av_dict_set(&options, "hls_segment_filename", "output/segment_%05d.ts", 0); // 自定义分片文件名
        // av_dict_set(&options, "hls_flags", "delete_segments", 0); // 自动删除旧分片（直播用）
        
        viewLog::INFO("HLSTransCoder::Transcode: {}: ✓ HLS 参数配置完成: 分片时长={}, 类型={}", outputFile, _config.hls_time, _config.hls_playlist_type);
        viewLog::INFO("HLSTransCoder::Transcode: {}:  Base URL: {}", outputFile, _config.hls_base_url);
        
        // ==================== 步骤6: 写入文件头 ====================
        // 如果输出协议需要（如本地文件），打开输出文件；HLS 会创建 .m3u8 和 .ts 文件
        if (!(outputContext->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open2(&outputContext->pb, outputContext->url, 
                            AVIO_FLAG_WRITE, nullptr, nullptr);
            if (ret < 0) {
                viewLog::ERROR("HLSTransCoder::Transcode: {}: 打开输出文件失败", outputFile);
                avformat_close_input(&inputContext);
                avformat_free_context(outputContext);
                return false;
            }
        }

        // avformat_write_header: 写入容器头部信息
        ret = avformat_write_header(outputContext, &options);
        if (ret < 0) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: 写入文件头失败", outputFile);
            // 这里不能直接 goto cleanup，因为后面有变量定义
            // 改为手动清理
            av_dict_free(&options);
            avformat_close_input(&inputContext);
            if (outputContext && !(outputContext->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&outputContext->pb);
            }
            avformat_free_context(outputContext);
            return false;
        }
        viewLog::INFO("HLSTransCoder::Transcode: {}: ✓ 成功写入 HLS 头部，开始转换...", outputFile);

        // ==================== 步骤7: 读取并写入数据包（核心循环）====================
        AVPacket packet;
        int frame_count = 0;           
        int64_t last_progress = 0;     

        while (av_read_frame(inputContext, &packet) >= 0) {
            // 检查流索引是否有效
            unsigned int in_index = packet.stream_index;
            if (in_index >= stream_mapping.size()) {
                viewLog::WARN("HLSTransCoder::Transcode: {}: 警告: 无效的流索引 {}", outputFile, in_index);
                av_packet_unref(&packet);
                continue;
            }
            
            // 获取输出流索引
            int out_index = stream_mapping[in_index];
            if (out_index < 0) {
                viewLog::WARN("HLSTransCoder::Transcode: {}: 警告: 无效的输出流索引 {}", outputFile, out_index);
                av_packet_unref(&packet);
                continue;
            }
            
            // 获取输入流和输出流对象
            AVStream* inStream = inputContext->streams[in_index];
            AVStream* outStream = outputContext->streams[out_index];
            
            // 转换时间戳
            if (packet.pts != AV_NOPTS_VALUE) {
                packet.pts = av_rescale_q_rnd(packet.pts, inStream->time_base, 
                                            outStream->time_base,
                                            (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            }
            
            if (packet.dts != AV_NOPTS_VALUE) {
                packet.dts = av_rescale_q_rnd(packet.dts, inStream->time_base, 
                                            outStream->time_base,
                                            (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            }
            
            if (packet.duration > 0) {
                packet.duration = av_rescale_q(packet.duration, inStream->time_base, 
                                            outStream->time_base);
            }
            
            packet.pos = -1;
            packet.stream_index = out_index;
            
            // 写入数据包
            ret = av_interleaved_write_frame(outputContext, &packet);
            if (ret < 0) {
                viewLog::ERROR("HLSTransCoder::Transcode: {}: 写入数据包失败 (帧 {}): {}", outputFile, frame_count, AvError(ret));
                av_packet_unref(&packet);
                break;
            }
            
            av_packet_unref(&packet);
            
            // 进度显示
            frame_count++;
            // if (frame_count % 30 == 0) {
            //     int64_t current_time = (inputContext->streams[0]->cur_dts * 
            //                             inputContext->streams[0]->time_base.num * 1000) /
            //                             inputContext->streams[0]->time_base.den;
            //     if (current_time > last_progress + 1000) {
            //         last_progress = current_time;
            //         double progress_percent = (current_time / (double)inputContext->duration) * 100;
            //         viewLog::INFO("HLSTransCoder::Transcode: {}:  进度: {}% (已处理 {} 包)", outputFile, progress_percent, frame_count);
            //     }
            // }
        }

        viewLog::INFO("HLSTransCoder::Transcode: {}: ✓ 数据处理完成，共处理 {} 个数据包", outputFile, frame_count);

        // ==================== 步骤8: 写入文件尾 ====================
        ret = av_write_trailer(outputContext);
        if (ret < 0) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: 写入文件尾失败", outputFile);
            // 清理资源
            av_dict_free(&options);
            avformat_close_input(&inputContext);
            if (outputContext && !(outputContext->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&outputContext->pb);
            }
            avformat_free_context(outputContext);
            return false;
        }

        viewLog::INFO("HLSTransCoder::Transcode: {}: ✓ 成功写入文件尾，转换完成！", outputFile);

        // ==================== 步骤9: 清理资源 ====================
        av_dict_free(&options);
        avformat_close_input(&inputContext);
        if (outputContext && !(outputContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&outputContext->pb);
        }
        avformat_free_context(outputContext);

        return true;
    }

    const char *HLSTransCoder::AvError(int errnum) {
        thread_local char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(errnum, errbuf, sizeof(errbuf));
        return errbuf;
    }
}