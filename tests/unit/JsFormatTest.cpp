// JavaScript-compatible number formatting used by the composeIcon() port.
#include <gtest/gtest.h>

#include "rvhl/JsFormat.h"
#include "rvhl/Sha256.h"

using namespace roados::rvhl::detail;

TEST(JsFormat, NumberMatchesJavaScriptToString) {
    EXPECT_EQ(jsNumber(20), "20");
    EXPECT_EQ(jsNumber(-20), "-20");
    EXPECT_EQ(jsNumber(46.5), "46.5");
    EXPECT_EQ(jsNumber(0.5), "0.5");
    EXPECT_EQ(jsNumber(-0.0), "0");
    EXPECT_EQ(jsNumber(6.5), "6.5");
    EXPECT_EQ(jsNumber(0.1 + 0.2), "0.30000000000000004");
    EXPECT_EQ(jsNumber(1e21), "1e+21");
}

TEST(JsFormat, ToFixedRoundsTiesAwayFromZero) {
    // 66/64 = 1.03125 exactly. JS gives "1.0313"; printf("%.4f") gives "1.0312".
    EXPECT_EQ(jsToFixed(66.0 / 64, 4), "1.0313");
    EXPECT_EQ(jsToFixed(31, 2), "31.00");
    EXPECT_EQ(jsToFixed(1.005, 2), "1.00");  // 1.005 is really 1.00499999…
    EXPECT_EQ(jsToFixed(9.96, 1), "10.0");
    EXPECT_EQ(jsToFixed(0.05, 1), "0.1");    // 0.05 is really 0.05000000000000000277…
    EXPECT_EQ(jsToFixed(-1.25, 1), "-1.3");
    EXPECT_EQ(jsToFixed(0, 1), "0.0");
}

TEST(JsFormat, RoundMatchesMathRound) {
    EXPECT_EQ(jsRound(2.5), 3);
    EXPECT_EQ(jsRound(-2.5), -2);
    EXPECT_EQ(jsRound(15.4999), 15);
}

TEST(JsFormat, EscapeXmlMatchesStudio) {
    EXPECT_EQ(escapeXml(R"(a&b<c>"d'e)"), "a&amp;b&lt;c&gt;&quot;d'e");
}

TEST(Sha256, KnownVectors) {
    EXPECT_EQ(sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(sha256Hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    EXPECT_EQ(sha256Hex(std::string(1000, 'a')),
              "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
}
