#include <viewAVTrans.h>
#include <viewLog.h>


void hls_test() {
    viewAVTrans::HLSConfig settings = {
        .hls_time = 10,
        .hls_playlist_type = "vod",
        .hls_base_url = "/video/"
    };
    viewAVTrans::HLSTransCoder transcoder(settings);
    bool ret = transcoder.Transcode("./Lesson1.mp4", "./dest.m3u8");
    if (ret == false) {
        viewLog::ERROR("转码失败!");
        abort();
    }
}
void m3u8_test() {
    viewAVTrans::M3U8Info info("./dest.m3u8");
    bool ret = info.Parse();
    if (ret == false) {
        viewLog::ERROR("m3u8文件解析失败!");
        return;
    }
    auto &headers = info.GetHeaders();
    for (auto &h : headers) {
        std::cout << "[" << h << "]" << std::endl;
    }
    std::cout << "==========================\n";
    auto &pieces = info.GetUrlPairs();
    for (int i = 0; i < pieces.size(); i++) {
        std::cout << "[" << pieces[i].first << "]" << std::endl;
        std::cout << "[" << pieces[i].second << "]" << std::endl;
        pieces[i].second = "/hello" + pieces[i].second;
    }
    ret = info.Write();
    if (ret == false) {
        viewLog::ERROR("m3u8文件重写失败!");
        return;
    }
}

int main()
{
    viewLog::init_logger();
    hls_test();
    m3u8_test();
    return 0;
}
