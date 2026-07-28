// Tiny self-contained unit-test framework (no external dependencies).
//
// Usage:
//   TEST(group, name) { CHECK(cond); CHECK_EQ(a, b); }
//   int main() { return roboeyes_test::run_all(); }
#ifndef ROBOEYES_TEST_FRAMEWORK_H
#define ROBOEYES_TEST_FRAMEWORK_H

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace roboeyes_test {

struct TestCase {
  std::string group;
  std::string name;
  std::function<void(int &)> fn;  // increments the failure counter it is given
};

inline std::vector<TestCase> &registry() {
  static std::vector<TestCase> tests;
  return tests;
}

struct Registrar {
  Registrar(const char *group, const char *name,
            std::function<void(int &)> fn) {
    registry().push_back({group, name, std::move(fn)});
  }
};

inline int run_all() {
  int total = 0;
  int failed_tests = 0;
  for (auto &tc : registry()) {
    int local_failures = 0;
    tc.fn(local_failures);
    total++;
    if (local_failures > 0) {
      failed_tests++;
      std::printf("[FAIL] %s :: %s (%d assertion failure%s)\n",
                  tc.group.c_str(), tc.name.c_str(), local_failures,
                  local_failures == 1 ? "" : "s");
    } else {
      std::printf("[ OK ] %s :: %s\n", tc.group.c_str(), tc.name.c_str());
    }
  }
  std::printf("\n%d/%d tests passed.\n", total - failed_tests, total);
  return failed_tests == 0 ? 0 : 1;
}

}  // namespace roboeyes_test

// The failure counter is named __failures inside each test body.
#define TEST(group, name)                                                    \
  static void test_##group##_##name(int &__failures);                        \
  static roboeyes_test::Registrar registrar_##group##_##name(                \
      #group, #name, test_##group##_##name);                                 \
  static void test_##group##_##name(int &__failures)

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      __failures++;                                                          \
      std::printf("    CHECK failed: %s (%s:%d)\n", #cond, __FILE__,         \
                  __LINE__);                                                 \
    }                                                                        \
  } while (0)

#define CHECK_EQ(a, b)                                                       \
  do {                                                                       \
    auto __va = (a);                                                         \
    auto __vb = (b);                                                         \
    if (!(__va == __vb)) {                                                   \
      __failures++;                                                          \
      std::printf("    CHECK_EQ failed: %s == %s -> (%lld != %lld) (%s:%d)\n", \
                  #a, #b, (long long)(__va), (long long)(__vb), __FILE__,    \
                  __LINE__);                                                 \
    }                                                                        \
  } while (0)

#endif  // ROBOEYES_TEST_FRAMEWORK_H
