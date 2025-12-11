#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

struct line
{
    int valid;          // 有效位
    int tag;            // 标记
    int last_used_time; // 最后使用时间，用于LRU替换策略
};

// 定义组，每组有E行，一行有一块
typedef struct line *set;

// 定义缓存，有S个组
set *cache;

// 定义全局缓存参数
int v = 0;         // 可选的详细信息标志，用于显示跟踪信息
int s;             // 缓存的组数为S=2^s
int E;             // 缓存每组的行数
int b;             // 每个高速缓存块的有B=2^b个字节
int timestamp = 0; // 时间戳，记录当前时间

// 定义全局返回参数
unsigned hit = 0;      // 命中数
unsigned miss = 0;     // 未命中数
unsigned eviction = 0; // 替换数

// 打印使用说明
void printUsage()
{
    puts("Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>");
    puts("Options:");
    puts("  -h         Print this help message.");
    puts("  -v         Optional verbose flag.");
    puts("  -s <num>   Number of set index bits.");
    puts("  -E <num>   Number of lines per set.");
    puts("  -b <num>   Number of block offset bits.");
    puts("  -t <file>  Trace file.");
    puts("");
    puts("Examples:");
    puts("  linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace");
    puts("  linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace");
}

// 访问缓存
void useCache(size_t address, int is_modify)
{
    int set_pos = address >> b & ((1 << s) - 1);
    int tag = address >> (b + s);
    set cur_set = cache[set_pos];
    int lru_pos = 0, lru_time = cur_set[0].last_used_time;

    for (int i = 0; i < E; i++)
    {
        if (cur_set[i].tag == tag)
        {
            ++hit;
            // 如果是修改操作还需要增加一次命中，因为既有读又有写
            hit += is_modify;
            cur_set[i].last_used_time = timestamp;
            if (v)
            {
                printf("hit\n");
            }
            return;
        }
        if (cur_set[i].last_used_time < lru_time)
        {
            lru_time = cur_set[i].last_used_time;
            lru_pos = i;
        }
    }
    miss++;
    // 如果是修改操作，必有一次写的命中
    hit += is_modify;
    // 如果lru_time != -1，说明当前缓存已满，需要驱逐
    eviction += (lru_time != -1);
    if (v)
    {
        if (lru_time != -1)
        {
            if (is_modify)
                printf("miss eviction hit\n");
            else
                printf("miss eviction\n");
        }
        else
        {
            printf("miss\n");
        }
    }
    // 驱逐
    cur_set[lru_pos].last_used_time = timestamp;
    cur_set[lru_pos].tag = tag;
    return;
}

int main(int argc, char *argv[])
{
    int option;
    FILE *trace_file;
    if (argc == 1)
    {
        printUsage();
        exit(0);
    }
    // 读取参数
    while ((option = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (option)
        {
        case 'h':
            printUsage();
            exit(0);
            break;
        case 'v':
            v = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            trace_file = fopen(optarg, "r");
            break;
        default:
            printUsage();
            exit(0);
        }
    }

    // 校验参数
    // 注意这里内存地址是64位的，而地址格式为：(Tag, Set Index, Block Offset)这三个加起来要等于64，所以s + b不能超过64
    if (s <= 0 || E <= 0 || b <= 0 || s + b > 64 || trace_file == NULL)
    {
        printUsage();
        exit(1);
    }

    // 初始化缓存
    cache = (set *)malloc(sizeof(set) * (1 << s));
    for (int i = 0; i < (1 << s); i++)
    {
        cache[i] = (set)malloc(sizeof(struct line) * E);
        for (int j = 0; j < E; j++)
        {
            cache[i][j].valid = -1;
            cache[i][j].tag = -1;
            cache[i][j].last_used_time = -1;
        }
    }

    // 解析trace文件
    int size;
    char operation;
    size_t address; // size_t大小与系统有关

    // %lx：读取无符号十六进制整数
    while (fscanf(trace_file, "%s %lx,%d", &operation, &address, &size) == 3)
    {
        ++timestamp;
        if (v)
        {
            printf("%c %lx,%d", operation, address, size);
        }
        switch (operation)
        {
        case 'I':
            continue;
        case 'M': // Modify = Load + Store
            useCache(address, 1);
            break;
        case 'L':
        case 'S':
            useCache(address, 0);
        }
    }

    free(cache);
    printSummary(hit, miss, eviction);
    return 0;
}
