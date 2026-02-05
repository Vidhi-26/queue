#include "q3buffer.h"
#include "q3consumer.h"
#include "q3producer.h"
#include <iostream>

static void usage() {
    std::cerr
      << "Usage: buffer [cons|'d' [prods|'d' [produce|'d' [buffersize|'d' "
         "[delay|'d' [processors|'d']]]]]]\n"
      << "  cons       : [1,100], default 5\n"
      << "  prods      : [1,100], default 3\n"
      << "  produce    : [1,50000], default 10\n"
      << "  buffersize : [1,5000], default 10\n"
      << "  delay      : [0,100], default cons+prods\n"
      << "  processors : [1,128], default 1\n";
}

static bool parseInt(const char* s, int &out) {
    if (!s) return false;
    char *end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (*s == '\0' || *end != '\0' || v < INT_MIN || v > INT_MAX)
        return false;
    out = static_cast<int>(v);
    return true;
}

static bool inRange(int val, int lo, int hi) { 
    return val >= lo && val <= hi; 
}

int main( int argc, char *argv[] ) {
    /* Defaults */
    int cons = 5, prods = 3;
    int produce = 10, buffersize = 10;
    int delay = -1;
    int processors = 1;

    /* Parse args */
    try {
        if (argc >= 2 && (!argv[1] || std::string(argv[1]) != "d")) {
            if (!parseInt(argv[1], cons)) {
                throw std::runtime_error("Error: Unsupported consumers");
            }
        }
        if (argc >= 3 && (!argv[2] || std::string(argv[2]) != "d")) {
            if (!parseInt(argv[2], prods)) {
                throw std::runtime_error("Error: Unsupported producers");
            }
        }
        if (argc >= 4 && (!argv[3] || std::string(argv[3]) != "d")) {
            if (!parseInt(argv[3], produce)) {
                throw std::runtime_error("Error: Unsupported produce");
            }
        }
        if (argc >= 5 && (!argv[4] || std::string(argv[4]) != "d")) {
            if (!parseInt(argv[4], buffersize)) {
                throw std::runtime_error("Error: Unsupported buffersize");
            }
        }
        if (argc >= 6 && (!argv[5] || std::string(argv[5]) != "d")) {
            if (!parseInt(argv[5], delay)) {
                throw std::runtime_error("Error: Unsupported delay");
            }
        }
        if (argc >= 7 && (!argv[6] || std::string(argv[6]) != "d")) {
            if (!parseInt(argv[6], processors)) {
                throw std::runtime_error("Error: Unsupported processors");
            }
        }
        if (argc > 7) {
            throw std::runtime_error("Error: Too many args");
        }
    } catch (...) {
        usage();
        return 1;
    }

    /* Default delay */
    if (delay < 0) delay = cons + prods;
    /* Check ranges */
    if (!inRange(cons, 1, 100) || !inRange(prods, 1, 100) ||
        !inRange(produce, 1, 50000) || !inRange(buffersize, 1, 5000) ||
        !inRange(delay, 0, 100) ||
        !inRange(processors, 1, 128)) {
        usage();
        return 1;
    }

    uProcessor p[(processors > 0 ? processors - 1 : 0)]
        __attribute__(( unused ));

    /* Create buffer */
    BoundedBuffer<int> buffer( static_cast<unsigned int>(buffersize) );

    /* Create tasks */
    std::vector<int> sums(cons, 0);
    std::vector<Consumer*> consumers;
    std::vector<Producer*> producers;
    for (int i = 0; i < cons; ++i) {
        consumers.push_back( new Consumer( buffer, delay, sums[i] ) );
    }
    for (int i = 0; i < prods; ++i) {
        producers.push_back( new Producer( buffer, produce, delay ) );
    }

    /* Clean up */
    for (auto *pr : producers) { 
        delete pr; 
    }
    buffer.poison();
    for (auto *co : consumers) { 
        delete co; 
    }

    /* Print total of all consumer sums */
    long long total = 0;
    for (int s : sums) total += s;
    std::cout << "total: " << total << std::endl;
    return 0;
}
