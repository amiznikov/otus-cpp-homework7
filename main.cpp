#include <algorithm>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#include "CRC32.hpp"
#include "IO.hpp"

/// @brief Переписывает последние 4 байта значением value
void replaceLastFourBytes(std::vector<char> &data, uint32_t value) {
  std::copy_n(reinterpret_cast<const char *>(&value), 4, data.begin());
}

void computeRightCRC32(size_t start, size_t end, const uint32_t& baseCrc32, std::vector<char>& result, const uint32_t &originalCrc32, bool &found) {
    std::vector<char> modifiedResult = result;
    for (size_t i = start; i < end; ++i) {
      if (found) {
        break;
      }
      replaceLastFourBytes(modifiedResult, uint32_t(i));
      auto currentCrc32 = crc32(modifiedResult.data() , 4, ~baseCrc32);
      if (currentCrc32 == originalCrc32) {
        result = std::move(modifiedResult);
        std::cout << "Found at " << i << '\n';
        found = true;
        break;
      }
    }
};

/**
 * @brief Формирует новый вектор с тем же CRC32, добавляя в конец оригинального
 * строку injection и дополнительные 4 байта
 * @details При формировании нового вектора последние 4 байта не несут полезной
 * нагрузки и подбираются таким образом, чтобы CRC32 нового и оригинального
 * вектора совпадали
 * @param original оригинальный вектор
 * @param injection произвольная строка, которая будет добавлена после данных
 * оригинального вектора
 * @return новый вектор
 */
std::vector<char> hack(const std::vector<char> &original,
                       const std::string &injection) {
  const uint32_t originalCrc32 = crc32(original.data(), original.size());

  std::vector<char> injected(original.size() + injection.size());
  std::vector<char> solt(4);
  auto it = std::copy(original.begin(), original.end(), injected.begin());
  std::copy(injection.begin(), injection.end(), it);
  bool found{false};
  const uint32_t baseCrc32 = crc32(injected.data(), injected.size());

  size_t maxVal = std::numeric_limits<uint32_t>::max();
  //самое быстрое выполнение происходит в 5,12,19 потоков
  const size_t numThreads = 5;
  const size_t step = maxVal / numThreads;
  std::vector<std::thread> threads;
  for (size_t i = 0; i < numThreads; ++i) {
    const size_t start = numThreads - 1 == i ? 0 : maxVal - step;
    const size_t end = maxVal;
    maxVal -= step;
    std::thread t(computeRightCRC32, start, end, std::ref(baseCrc32), std::ref(solt), std::ref(originalCrc32), std::ref(found));
    threads.push_back(std::move(t));
  }
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }
  if (!found) {
    throw std::logic_error("Can't hack");
  } else {
    injected.insert(injected.end(), solt.begin(), solt.end());
    return injected;
  }

}

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Call with two args: " << argv[0]
              << " <input file> <output file>\n";
    return 1;
  }

    try {
      const std::vector<char> data = readFromFile(argv[1]);
      const std::vector<char> badData = hack(data, "He-he-he");
      writeToFile(argv[2], badData);
    } catch (std::exception &ex) {
      std::cerr << ex.what() << '\n';
      return 2;
    }

  return 0;
}
