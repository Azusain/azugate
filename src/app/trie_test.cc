#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>


// ==============================
// Trie 实现（最长前缀匹配）
// ==============================

struct TrieNode {
  std::unordered_map<char, TrieNode *> children;
  bool isEnd = false;
};

class Trie {
public:
  Trie() : root(new TrieNode) {}

  void insert(const std::string &word) {
    TrieNode *node = root.get();
    for (char ch : word) {
      if (!node->children.count(ch))
        node->children[ch] = new TrieNode();
      node = node->children[ch];
    }
    node->isEnd = true;
  }

  // 返回最长匹配前缀
  std::string longestPrefixMatch(const std::string &path) {
    TrieNode *node = root.get();
    std::string matched;
    std::string current;

    for (char ch : path) {
      if (!node->children.count(ch))
        break;
      node = node->children[ch];
      current += ch;
      if (node->isEnd)
        matched = current;
    }

    return matched;
  }

private:
  std::unique_ptr<TrieNode> root;
};

// ==============================
// 工具函数
// ==============================

std::string random_path(int len) {
  static const char charset[] = "abcdefghijklmnopqrstuvwxyz/";
  static std::default_random_engine engine(std::random_device{}());
  static std::uniform_int_distribution<int> dist(0, sizeof(charset) - 2);

  std::string path = "/";
  for (int i = 0; i < len; ++i) {
    path += charset[dist(engine)];
    if (i % 5 == 0)
      path += '/';
  }
  return path;
}

// ==============================
// 性能测试
// ==============================

void performance_test(int m, int n) {
  Trie trie;
  std::vector<std::string> rules;

  for (int i = 0; i < m; ++i) {
    auto rule = random_path(n);
    rules.push_back(rule);
    trie.insert(rule);
  }

  // 暴力字符串前缀查找（用 vector + std::find_if）
  auto prefix_match = [&rules](const std::string &path) -> std::string {
    std::string matched;
    for (const auto &rule : rules) {
      if (path.compare(0, rule.length(), rule) == 0 &&
          rule.length() > matched.length()) {
        matched = rule;
      }
    }
    return matched;
  };

  // 构造测试路径（部分与规则前缀相同）
  std::vector<std::string> test_paths;
  for (int i = 0; i < 10000; ++i) {
    std::string base = rules[i % rules.size()];
    test_paths.push_back(base + "/extra" + std::to_string(i)); // 模拟真实路径
  }

  // Trie 匹配测试
  auto start = std::chrono::high_resolution_clock::now();
  int trie_match = 0;
  for (const auto &path : test_paths) {
    if (!trie.longestPrefixMatch(path).empty())
      ++trie_match;
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "Trie 前缀匹配耗时: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << " ms, 命中: " << trie_match << std::endl;

  // 暴力字符串前缀匹配测试
  start = std::chrono::high_resolution_clock::now();
  int brute_match = 0;
  for (const auto &path : test_paths) {
    if (!prefix_match(path).empty())
      ++brute_match;
  }
  end = std::chrono::high_resolution_clock::now();
  std::cout << "暴力字符串前缀匹配耗时: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << " ms, 命中: " << brute_match << std::endl;
}

// ==============================
// 主函数
// ==============================

int main() {
  int m = 100000; // 路由规则数量
  int n = 15;     // 平均字符串长度

  performance_test(m, n);
  return 0;
}
