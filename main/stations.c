#include "stations.h"

#include <stdio.h>

// Live MP3 streams, checked from a Mac on 2026-11-02: 32 kbps, 44.1 or 48 kHz stereo. Plenty for a mono 24 kHz
// speaker, and half the load of the 64 kbps streams, which the device could not always keep ahead of real time.
// For testing on our own device; the rights to these streams are not cleared for a store release. Each city's
// stations are sorted by frequency: the next station is the next one to the right on the dial.
#define QT(id) "http://lhttp.qingting.fm/live/" #id "/24k.mp3"
#define BJ "北京", "Beijing"
#define SH "上海", "Shanghai"
#define GZ "广州", "Guangzhou"
#define SZ "深圳", "Shenzhen"
#define HZ "杭州", "Hangzhou"
#define NJ "南京", "Nanjing"
#define CD "成都", "Chengdu"
#define WH "武汉", "Wuhan"
#define CS "长沙", "Changsha"
#define CQ "重庆", "Chongqing"
#define XA "西安", "Xi'an"
#define S(city, name, name_en, mhz10, id) {city, name, name_en, mhz10, QT(id)}  // city: both its names

static const station_t BEIJING[] = {
    S(BJ, "北京文艺广播", "Beijing Arts", 876, 333),
    S(BJ, "北京音乐广播", "Beijing Music", 974, 332),
    S(BJ, "北京新闻广播", "Beijing News", 1006, 339),
    S(BJ, "北京交通广播", "Beijing Traffic", 1039, 336),
    S(BJ, "北京城市广播", "Beijing City", 1075, 345),
};
static const station_t SHANGHAI[] = {
    S(SH, "上海新闻广播", "Shanghai News", 934, 270),
    S(SH, "上海经典947", "Classic 947", 947, 267),
    S(SH, "上海动感101", "Dynamic 101", 1017, 274),
    S(SH, "上海流行音乐 LoveRadio", "Love Radio", 1037, 273),
    S(SH, "上海交通广播", "Shanghai Traffic", 1057, 266),
};
static const station_t GUANGZHOU[] = {
    S(GZ, "广州 FM88.0", "Guangzhou FM 88.0", 880, 20194),
    S(GZ, "广东新闻广播", "Guangdong News", 914, 1254),
    S(GZ, "广州新闻电台", "Guangzhou News", 962, 4848),
    S(GZ, "珠江经济电台", "Pearl River Radio", 974, 1259),
    S(GZ, "广东音乐之声", "Guangdong Music", 993, 1260),
    S(GZ, "广州交通广播", "Guangzhou Traffic", 1061, 4955),
};
static const station_t SHENZHEN[] = {
    S(SZ, "深圳先锋898", "Pioneer 898", 898, 1270),
    S(SZ, "深圳生活广播", "Shenzhen Life", 942, 1273),
    S(SZ, "深圳飞扬971", "Shenzhen Feiyang 971", 971, 1271),
    S(SZ, "深圳交通广播", "Shenzhen Traffic", 1062, 1272),
};
static const station_t HANGZHOU[] = {
    S(HZ, "浙江之声", "Voice of Zhejiang", 880, 4518),
    S(HZ, "杭州交通广播", "Hangzhou Traffic", 918, 1133),
    S(HZ, "浙江交通之声", "Zhejiang Traffic", 930, 4522),
    S(HZ, "浙江音乐调频", "Zhejiang Music", 968, 4866),
    S(HZ, "西湖之声", "West Lake Radio", 1054, 1163),
};
static const station_t NANJING[] = {
    S(NJ, "江苏音乐广播", "Jiangsu Music", 897, 4936),
    S(NJ, "江苏新闻广播", "Jiangsu News", 937, 4944),
    S(NJ, "江苏交通广播", "Jiangsu Traffic", 1011, 4054),
    S(NJ, "南京音乐广播", "Nanjing Music", 1058, 4963),
};
static const station_t CHENGDU[] = {
    S(CD, "成都交通文艺广播", "Chengdu Traffic", 914, 4891),
    S(CD, "成都电台946", "Chengdu 946", 946, 4892),
    S(CD, "亚洲音乐成都", "Asia Music Chengdu", 965, 4581),
    S(CD, "成都经济广播", "Chengdu Business", 1056, 1121),
};
static const station_t WUHAN[] = {
    S(WH, "武汉交通广播", "Wuhan Traffic", 896, 4665),
    S(WH, "楚天交通广播", "Chutian Traffic", 927, 1291),
    S(WH, "武汉经典音乐广播", "Wuhan Classic Music", 1018, 1297),
    S(WH, "湖北之声", "Voice of Hubei", 1046, 1303),
};
static const station_t CHANGSHA[] = {
    S(CS, "长沙音乐广播", "Changsha Music", 886, 20847),
    S(CS, "金鹰955", "Golden Eagle 955", 955, 4937),
    S(CS, "长沙城市之声", "Changsha City", 1017, 4237),
    S(CS, "长沙新闻广播", "Changsha News", 1050, 4877),
    S(CS, "长沙交通广播", "Changsha Traffic", 1061, 3967),
};
static const station_t CHONGQING[] = {
    S(CQ, "重庆音乐广播", "Chongqing Music", 881, 647),
    S(CQ, "重庆私家车广播", "Chongqing Car Radio", 938, 1502),
    S(CQ, "重庆交通广播", "Chongqing Traffic", 955, 1500),
    S(CQ, "重庆之声", "Voice of Chongqing", 968, 1498),
};
static const station_t XIAN[] = {
    S(XA, "陕西交通广播", "Shaanxi Traffic", 916, 1601),
    S(XA, "西安音乐广播", "Xi'an Music", 931, 1612),
    S(XA, "陕西音乐广播", "Shaanxi Music", 988, 4873),
    S(XA, "陕西新闻广播", "Shaanxi News", 1066, 1600),
};

#define N(a) ((int)(sizeof(a) / sizeof(a[0])))
typedef struct {
    const station_t *stations;
    int count;
} city_t;
// The order of the city picker: the biggest first, then roughly north to south.
static const city_t CITIES[] = {
    {BEIJING, N(BEIJING)},   {SHANGHAI, N(SHANGHAI)}, {GUANGZHOU, N(GUANGZHOU)}, {SHENZHEN, N(SHENZHEN)},
    {HANGZHOU, N(HANGZHOU)}, {NANJING, N(NANJING)},   {CHENGDU, N(CHENGDU)},     {WUHAN, N(WUHAN)},
    {CHANGSHA, N(CHANGSHA)}, {CHONGQING, N(CHONGQING)}, {XIAN, N(XIAN)},
};

static int wrap(int i, int n) { return ((i % n) + n) % n; }

int cities_count(void) { return N(CITIES); }

int city_wrap(int city) { return wrap(city, N(CITIES)); }

const char *city_name(int city, bool english) { return station_city(CITIES[city_wrap(city)].stations, english); }

int city_stations(int city) { return CITIES[city_wrap(city)].count; }

const station_t *station_at(int city, int i) {
    const city_t *c = &CITIES[city_wrap(city)];
    return &c->stations[wrap(i, c->count)];
}

int station_step(int city, int i, int delta) { return wrap(i + delta, city_stations(city)); }

void station_freq_text(const station_t *st, char *out, size_t size) {
    snprintf(out, size, "%d.%d", st->mhz10 / 10, st->mhz10 % 10);
}

int dial_permille(int mhz10) {
    if (mhz10 < FM_MIN_10) mhz10 = FM_MIN_10;
    if (mhz10 > FM_MAX_10) mhz10 = FM_MAX_10;
    return (mhz10 - FM_MIN_10) * 1000 / (FM_MAX_10 - FM_MIN_10);
}

const char *station_city(const station_t *st, bool english) { return english ? st->city_en : st->city; }

const char *station_name(const station_t *st, bool english) { return english ? st->name_en : st->name; }

void listen_sentence(const station_t *st, int seconds, bool english, char *out, size_t size) {
    const char *name = station_name(st, english);
    const int minutes = (seconds + 30) / 60;
    if (english) {
        if (seconds < 60) snprintf(out, size, "Listened to %s for a moment", name);
        else snprintf(out, size, "Listened to %s for %d minute%s", name, minutes, minutes == 1 ? "" : "s");
    } else {
        if (seconds < 60) snprintf(out, size, "听了一会儿%s", name);
        else snprintf(out, size, "听了%s %d 分钟", name, minutes);
    }
}
