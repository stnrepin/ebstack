#include <atomic>
#include <cassert>
#include <ctime>
#include <iostream>
#include <thread>
#include <random>
#include <stack>
#include <vector>

#include "stack_wrapper.hpp"

//#define STD_STACK_MUTEX

#ifdef STD_STACK_MUTEX
using Stack = std::stack< int >;
#else
using Stack = eb::Stack< int >;
#endif // STD_STACK_MUTEX

static StackWrapper< Stack > g_shared;
static std::atomic< int > g_thread_count;

std::vector< std::vector< int > > BuildTestStacks( int thread_count, int elems_count )
{
    constexpr auto kMax = 1'000;
    std::random_device dev;
    std::mt19937 rng( dev() );
    std::uniform_int_distribution< std::mt19937::result_type > dist( 0, kMax );

    std::vector< std::vector< int > > res;

    for (auto i = 0; i < thread_count; ++i )
    {
        auto s = std::vector< int >();
        for (auto j = 0; j < elems_count; ++j)
        {
            auto r = i * kMax + dist(rng);
            s.push_back(r);
        }
        res.push_back( std::move( s ) );
    }
    return res;
}

struct Stopwatch {
public:
    static Stopwatch Start()
    {
        Stopwatch sw;
        clock_gettime(CLOCK_REALTIME, &sw.start_);
        return sw;
    }

    void Stop()
    {
        clock_gettime(CLOCK_REALTIME, &end_);
    }

    uint64_t GetNs() const
    {
        const uint64_t billion = 1'000'000'000;
        return ( end_.tv_sec - start_.tv_sec ) * billion + ( end_.tv_nsec - start_.tv_nsec );
    }

private:
    Stopwatch() {}

    timespec start_;
    timespec end_;
};

void process( std::vector< int >* vals, int pop_count )
{
    eb::InitThread();

    g_thread_count.fetch_sub( 1 );
    while( g_thread_count.load() > 0 )
    {
        eb::Pause();
    }

    for( const auto v : *vals )
    {
        g_shared.Push( v );
    }

    for( auto i = 0; i < pop_count; ++i )
    {
        g_shared.Pop();
    }
}

int main( int argc, char* argv[] )
{
    if( argc != 2 )
    {
        std::cout << "invalid args\n";
        return 1;
    }

    constexpr auto kElemsCount = 100'000;
    constexpr int kPopCount = kElemsCount * 0.3;

    auto thread_count = std::stoi( argv[ 1 ] );
    auto src = BuildTestStacks( thread_count, kElemsCount );

    eb::InitSystem();

    g_thread_count = thread_count;
    std::vector< std::thread > ths;

    auto sw = Stopwatch::Start();
    for( auto i = 0; i < thread_count; ++i )
    {
        auto th = std::thread( process, &src[ i ], kPopCount );
        ths.push_back( std::move( th ) );
    }

    for( auto& t : ths )
    {
        if( t.joinable() )
        {
            t.join();
        }
    }
    sw.Stop();

    size_t total_size = thread_count * ( kElemsCount - kPopCount ); 
    assert( g_shared.GetSize() == total_size );

    std::cout << "Took: " << sw.GetNs() << "\n";
}
