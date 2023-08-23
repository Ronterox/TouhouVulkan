#include <chrono>
#include <functional>
#include <cstring>
#include <set>

#include "utils.h"

void timeit(std::function<void()> f)
{
    auto start = std::chrono::high_resolution_clock::now();
    f();
    auto end = std::chrono::high_resolution_clock::now();

    LOG("Time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() << "ns");
}

int main()
{
    const list<const char*> l = { "Hello", "World", "!",
                      "This", "is", "a", "test",
                      "of", "the", "emergency","broadcast","system",
                      "just", "kidding", "it's", "just", "a", "test"
    };

    timeit([&l]() {
        list<const char*> required = { "emergency", "system", "just", "test" };
        for (const char* const& word : l) {
            const auto wordFound = [word](auto& currentWord) {
                return strcmp(word, currentWord) == 0;
            };
            if (!std::any_of(required.begin(), required.end(), wordFound)) return;
        }
    });

    timeit([&l]() {
        std::set<const char*> required = { "emergency", "system", "just", "test" };
        for (const char* const& word : l) {
            required.erase(word);
        }
        return required.empty();
    });

    return 0;
}
